#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bt3_runtime_addresses.h"
#include "earth_collision.h"

namespace {
struct alignas(16) Vec {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;
};

std::uintptr_t ImageBase() {
    static const auto address = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return address;
}

template<class Return = void, class... Args>
Return Game(std::uintptr_t rva, Args... args) {
    return reinterpret_cast<Return (*)(Args...)>(ImageBase() + rva)(args...);
}

template<class T>
T& Field(void* block, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(block) + offset);
}

// The game's x64 translation passes PS2 floats as raw bits in integer registers.
float AsFloat(std::uint32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

std::uint32_t AsWord(float value) {
    std::uint32_t word;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}

void FlightLog(const char* message, int frames = 0, float value = 0) {
    FILE* file = nullptr;
    if (fopen_s(&file, "adventure-flight.log", "a") == 0) {
        std::fprintf(file, "%s | frames=%d | value=%.2f\n", message, frames, value);
        std::fclose(file);
    }
}

EarthCollision earthCollision;
void* markerResource = nullptr;
struct MarkerSprite {
    int flags, x0, y0, x1, y1, u0, v0, u1, v1, texture, r, g, b, a;
};
MarkerSprite* markerSprites = nullptr;

bool LoadRaditzMarker() {
    FILE* f = nullptr;
    if (fopen_s(&f, "raditz-marker.bin", "rb")) return false;
    constexpr size_t entrySize = 64, packetSize = 16512, totalSize = 128 + packetSize;
    auto* data = Game<unsigned char*>(rva::Port_LowAlloc, totalSize);
    auto* sprites = Game<MarkerSprite*>(rva::Port_LowAlloc, 2 * sizeof(MarkerSprite));
    bool ok = data && sprites && std::fread(data + 64, 1, entrySize, f) == entrySize &&
              std::fread(data + 128, 1, packetSize, f) == packetSize && std::fgetc(f) == EOF;
    std::fclose(f);
    if (!ok) {
        if (data) Game(rva::Port_LowFree, data);
        if (sprites) Game(rva::Port_LowFree, sprites);
        return false;
    }
    std::memset(data, 0, 64);
    Field<std::uint32_t>(data, 0x10) = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data + 64));
    Field<std::uint32_t>(data, 0x40) = 1; // Pixels present.
    Field<std::uint32_t>(data, 0x44) = 0; // 32-bit colour, no palette.
    Field<std::uint32_t>(data, 0x48) = static_cast<std::uint32_t>(packetSize);
    Field<std::uint32_t>(data, 0x78) = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data + 128));
    Field<std::uint32_t>(data, 0x7c) = 0;
    markerResource = data;
    markerSprites = sprites;
    return true;
}

void FreeRaditzMarker() {
    if (markerResource) Game(rva::Port_LowFree, markerResource);
    if (markerSprites) Game(rva::Port_LowFree, markerSprites);
    markerResource = nullptr;
    markerSprites = nullptr;
}
float earthScale = 1.0f;
constexpr float OriginalEarthTreeSize = 9000.0f;

std::uint32_t& StageResource() {
    const auto common = *reinterpret_cast<const std::uint32_t*>(ImageBase() + rva::gCommonRes);
    return Field<std::uint32_t>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(common)), 0x24);
}

void* LoadEarthStage() {
    FILE* file = nullptr;
    if (fopen_s(&file, "earth-adventure-map.unk", "rb") != 0) return nullptr;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    void* stage = nullptr;
    if (size >= 128 && size <= 16 * 1024 * 1024) {
        stage = Game<void*>(rva::Port_LowAlloc, static_cast<std::size_t>(size));
        if (stage) {
            const bool read = std::fread(stage, 1, size, file) == static_cast<std::size_t>(size);
            const auto* offsets = static_cast<const std::uint32_t*>(stage);
            const bool valid = read && offsets[0] == 25 && offsets[1] == 128
                && std::is_sorted(offsets + 1, offsets + 27)
                && offsets[2] >= 128 + 0x60 && offsets[3] - offsets[2] >= 0xB0
                && offsets[26] <= static_cast<std::uint32_t>(size)
                && std::memcmp(static_cast<const char*>(stage) + offsets[2], "MAPD", 4) == 0;
            FlightLog("load valid", valid ? 1 : 0, static_cast<float>(offsets[0]));
            // The converter scales the tree's root size together with the geometry.
            const float scale = valid ? Field<float>(stage, offsets[1] + 0x4C) / OriginalEarthTreeSize : 0.0f;
            if (std::isfinite(scale) && scale > 0.0f) {
                earthScale = scale;
            } else {
                Game(rva::Port_LowFree, stage);
                stage = nullptr;
            }
        }
    }
    std::fclose(file);
    return stage;
}

constexpr std::uint32_t Left = 1 << 0;
constexpr std::uint32_t Right = 1 << 1;
constexpr std::uint32_t Down = 1 << 2;
constexpr std::uint32_t Up = 1 << 3;
constexpr std::uint32_t Cross = 1 << 9;
constexpr std::uint32_t Start = 1 << 12;
constexpr std::uint32_t L2 = 1 << 15;
constexpr std::uint32_t Ascend = 1 << 16;
constexpr std::uint32_t Descend = 1 << 17;
constexpr std::uint32_t Select = 1 << 23; // SELECT: cycle camera distance preset

struct Pilot {
    Vec position;
    Vec goal;
    Vec velocity{0, 0, 0, 0};
    float heading = 0;     // facing requested by the turn input
    float modelYaw = 0;    // model facing, turned towards heading at a limited rate
    float pitch = 0;       // flight pitch, engine sign: positive = descending
    float bank = 0;        // sideways lean while turning (disabled: siempre vertical)
    float verticalInput = 0; // input de subir/bajar (Ascend/Descend) para la animación
    int cameraDistMode = 0; // distance preset cycled with SELECT (0..2)
    float cameraScale = 1.0f;
    float radius = 0;
    float ceiling = 0;
    float floor = 0;
    int motion = -1;
    bool boosting = false;
    bool fastFlight = false;
    bool dashReverseHeld = false;
    bool dashEffects = false;
    float markerX = 0.0f;
    float markerY = 0.0f;
    bool markerVisible = false;
    bool arrived = false;   // reached the destination and locked the flight controls
    int arriveFrames = 0;
};

constexpr float Tau = 6.2831853f;
constexpr float Deg36 = 0.62831853f;

// Relative pose measured on the first visible frame of BT2 Dragon Adventure.
constexpr float AdventureCameraHeight = 23.26146f;
constexpr float AdventureCameraBack = 29.156279f;
constexpr float AdventureCameraTilt = 0.30f;
constexpr int FastFlightStart = 0x0C;
constexpr int FastFlightLoop = 0x0D;
constexpr int DragonDashStart = 0x2D;
constexpr int DragonDashLoop = 0x19;
constexpr int AscendMotion = 0x2B;
constexpr int DescendMotion = 0x2C;
constexpr float ArrivalRadius = 65.0f;
constexpr int ArrivalFrames = 90;

// Same rule as BtlMove_TurnModelYaw: step = |delta| * rate, clamped, never overshooting.
float TurnToward(float current, float target, float rate, float minStep, float maxStep) {
    const float delta = std::remainder(target - current, Tau);
    float step = std::clamp(std::fabs(delta) * rate, minStep, maxStep);
    step = std::min(step, std::fabs(delta));
    return std::remainder(current + std::copysign(step, delta), Tau);
}

Vec QuatAxisAngle(float x, float y, float z, float angle) {
    const float half = angle * 0.5f;
    const float sine = std::sin(half);
    return {x * sine, y * sine, z * sine, std::cos(half)};
}

Vec QuatMul(const Vec& a, const Vec& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// Mirrors BtlChar_UpdateLean: the object's extra node-2 rotation is built from
// angles (-leanX, 0, leanZ), which is what makes the body tilt in flight.
void SetLean(void* avatar, float leanX, float leanZ) {
    const Vec qx = QuatAxisAngle(1.0f, 0.0f, 0.0f, -leanX);
    const Vec qz = QuatAxisAngle(0.0f, 0.0f, 1.0f, leanZ);
    Field<Vec>(avatar, 0xD20) = QuatMul(qx, qz);
}

float GroundY(float x, float z, float fallback) {
    float hit=1.0f;
    constexpr float top=-2000.0f, bottom=2000.0f;
    return earthCollision.trace({x,top,z},{x,bottom,z},hit) ? top+(bottom-top)*hit : fallback;
}

float Distance(const Vec& a, const Vec& b) {
    return std::hypot(std::hypot(a.x - b.x, a.z - b.z), a.y - b.y);
}

void SetPilotPose(Pilot& pilot, void* fighter, void* avatar, float ground) {
    const float speed = std::hypot(pilot.velocity.x, pilot.velocity.z);
    // Animaciones nativas de vuelo libre del motor (acción 0xD):
    //  movimiento -> motion 3 (loop), inicio -> motion 2.
    //  Vuelo quieto -> 0x26 / 0x27 (según el gatillo de gauge).
    //  En suelo -> 0.
    int motion;
    if (pilot.boosting) {
        if (pilot.motion == DragonDashLoop) {
            Game(rva::BtlAnim_AdvanceLoop, fighter, 0);
            motion = DragonDashLoop;
        } else if (pilot.motion == DragonDashStart) {
            motion = Game<int>(rva::BtlAnim_Advance, fighter, 0) ? DragonDashLoop : DragonDashStart;
        } else {
            motion = DragonDashStart;
        }
    } else if (pilot.fastFlight) {
        if (pilot.motion == FastFlightLoop) {
            motion = FastFlightLoop;
        } else if (pilot.motion == FastFlightStart) {
            motion = Game<int>(rva::BtlAnim_Advance, fighter, 0) ? FastFlightLoop : FastFlightStart;
        } else {
            motion = FastFlightStart;
        }
    } else if (pilot.verticalInput < -0.01f) {
        motion = AscendMotion;
    } else if (pilot.verticalInput > 0.01f) {
        motion = DescendMotion;
    } else if (speed > 1.0f) {
        motion = 3;
    } else if (ground - pilot.position.y > 7.0f || std::abs(pilot.verticalInput) > 0.01f || std::abs(pilot.velocity.y) > 0.5f) {
        motion = 0x26;
    } else {
        motion = 0;
    }
    const bool verticalMotion = motion == AscendMotion || motion == DescendMotion;
    if (pilot.motion == motion && (verticalMotion || motion == FastFlightLoop)) {
        Game(rva::BtlAnim_AdvanceLoop, fighter, 0);
    } else if (pilot.motion != motion || (!pilot.boosting && !pilot.fastFlight && Game<int>(rva::BtlAnim_Advance, fighter, 0))) {
        const float blend = motion == DragonDashStart ? 0.15f : motion == DragonDashLoop ? 0.2f : 0.12f;
        Game(rva::BtlAnim_Play, fighter, motion, AsWord(blend));
        pilot.motion = motion;
    }

    const Vec rotation{0, pilot.modelYaw, 0};
    // The engine's placement function updates both the current and prior model roots.
    Game(rva::BtlChar_Place, fighter, &pilot.position, &rotation, 0);
    // Keep the pose lean fields in sync with the object quaternion, as BtlChar_UpdateLean does.
    const float leanX = std::clamp(pilot.pitch, -1.0f, 1.0f) * 0.6f;
    const float leanZ = std::clamp(pilot.bank, -0.6f, 0.6f);
    Field<float>(fighter, 0xB0) = leanX;
    Field<float>(fighter, 0xB4) = leanZ;
    SetLean(avatar, leanX, leanZ);
    Game(rva::BtlObjXf_Update, avatar);

    const float sine = std::sin(pilot.modelYaw);
    const float cosine = std::cos(pilot.modelYaw);
    const float wantedScale = 1.0f + 0.25f * pilot.cameraDistMode;
    pilot.cameraScale += (wantedScale - pilot.cameraScale) * 0.1f;
    const float back = AdventureCameraBack * pilot.cameraScale;
    const Vec desired{
        pilot.position.x - sine * back,
        pilot.position.y - AdventureCameraHeight * pilot.cameraScale,
        pilot.position.z - cosine * back,
    };
    const Vec pivot{pilot.position.x, pilot.position.y - 12.0f, pilot.position.z};
    Vec& eye = Field<Vec>(fighter, 0x430);
    float cameraHit=1.0f;
    if (earthCollision.trace({pivot.x,pivot.y,pivot.z},{desired.x,desired.y,desired.z},cameraHit)) {
        const float t=std::max(0.0f,cameraHit-0.02f);
        eye={pivot.x+(desired.x-pivot.x)*t,pivot.y+(desired.y-pivot.y)*t,pivot.z+(desired.z-pivot.z)*t};
    } else eye=desired;
    // Use the resolved eye so a stage hit changes the distance without changing the tilt.
    const Vec look{eye.x + sine * AdventureCameraBack,
                   eye.y + std::tan(AdventureCameraTilt) * AdventureCameraBack,
                   eye.z + cosine * AdventureCameraBack};
    Game(rva::Vec3_DiffToEuler, &Field<Vec>(fighter, 0x440), &look, &eye);
    Field<float>(fighter, 0x4A0) = pilot.modelYaw;
}

float Move(Pilot& pilot, void* pad) {
    const auto held = Field<std::uint32_t>(pad, 0x188);
    static std::uint32_t lastHeld = 0;
    const auto btn = [held](std::uint32_t mask) { return (held & mask) ? 1.0f : 0.0f; };
    const bool selPressed = (held & Select) && !(lastHeld & Select);
    lastHeld = held;

    if (pilot.arrived) return GroundY(pilot.position.x, pilot.position.z, pilot.floor);

    const auto pressed = Field<std::uint32_t>(pad, 0x18C);
    if ((pressed & Start) && Distance(pilot.position, pilot.goal) < ArrivalRadius) {
        pilot.arrived = true;
        pilot.boosting = false;
        pilot.fastFlight = false;
        pilot.velocity = {0, 0, 0, 0};
        pilot.pitch = 0.0f;
        pilot.bank = 0.0f;
        pilot.verticalInput = 0.0f;
        return GroundY(pilot.position.x, pilot.position.z, pilot.floor);
    }

    // Turbo follows L2 directly; releasing L2 returns to the appropriate flight mode.
    const bool wasBoosting = pilot.boosting;
    const bool wasFastFlight = pilot.fastFlight;
    pilot.boosting = (held & L2) != 0;
    const float forward = std::clamp(-Field<float>(pad, 0x1A8) + btn(Up) - btn(Down), -1.0f, 1.0f);
    pilot.fastFlight = !pilot.boosting && (held & Cross) && forward > 0.1f;
    const float turn = std::clamp(Field<float>(pad, 0x1A4) + Field<float>(pad, 0x1AC) +
                                  btn(Right) - btn(Left), -1.0f, 1.0f);
    // Stick derecho vertical (RY) controla subir/bajar en vuelo.
    const float verticalStick = Field<float>(pad, 0x1B0);
    const float verticalInput = std::clamp(btn(Descend) - btn(Ascend) + verticalStick, -1.0f, 1.0f);
    pilot.verticalInput = verticalInput;
    pilot.heading += turn * 0.055f;
    // A backward input during turbo reverses the heading once per press.
    const bool dashReverseHeld = pilot.boosting && forward < -0.5f;
    if (dashReverseHeld && !pilot.dashReverseHeld) {
        pilot.heading += Tau * 0.5f;
    }
    pilot.dashReverseHeld = dashReverseHeld;
    pilot.heading = std::remainder(pilot.heading, Tau);
    pilot.modelYaw = TurnToward(pilot.modelYaw, pilot.heading, 0.3f, 0.0314f, Deg36);

    // Cycle camera distance preset with SELECT (BT2 ChrCam_UpdateInput).
    if (selPressed) {
        pilot.cameraDistMode = (pilot.cameraDistMode + 1) % 3;
    }

    const float sine = std::sin(pilot.modelYaw);
    const float cosine = std::cos(pilot.modelYaw);
    constexpr float cruiseSpeed = 4.0f;
    constexpr float fastSpeed = 12.0f;
    if (pilot.boosting) {
        // Keep the dash velocity aligned with the model as it turns.
        const float speed = std::hypot(pilot.velocity.x, pilot.velocity.z);
        pilot.velocity.x = sine * speed;
        pilot.velocity.z = cosine * speed;
    }
    const float rate = pilot.boosting ? 22.0f : (pilot.fastFlight ? fastSpeed : cruiseSpeed) * forward;
    const Vec desired{sine * rate,
                      verticalInput * 6.0f,
                      cosine * rate, 0};
    pilot.velocity.x += (desired.x - pilot.velocity.x) * 0.25f;
    pilot.velocity.y += (desired.y - pilot.velocity.y) * 0.25f;
    pilot.velocity.z += (desired.z - pilot.velocity.z) * 0.25f;
    // A short kick at dash entry, then the existing smoothing settles at fastSpeed.
    if (pilot.fastFlight && !wasFastFlight && !wasBoosting) {
        constexpr float entryImpulse = 16.0f;
        pilot.velocity.x = sine * entryImpulse * forward;
        pilot.velocity.z = cosine * entryImpulse * forward;
    }
    if (pilot.fastFlight && !wasFastFlight) {
        // BT3 normal dash startup sound, common sound bank (mask 4), id 0x1E.
        Game<int>(rva::Snd_PlaySe, 4, 0x1E);
    }
    if (wasBoosting && !pilot.boosting) {
        const float speed = std::hypot(pilot.velocity.x, pilot.velocity.z);
        const float maxSpeed = pilot.fastFlight ? fastSpeed : cruiseSpeed;
        if (speed > maxSpeed) {
            pilot.velocity.x *= maxSpeed / speed;
            pilot.velocity.z *= maxSpeed / speed;
        }
    }

    // Pitch basado en la entrada de subir/bajar (no en la velocidad vertical,
    // que cambia cada frame por el suavizado y provoca el balanceo del cuerpo).
    // Convención del motor: pitch positivo = nariz hacia abajo (descendiendo).
    const float wantedPitch = pilot.boosting ? verticalInput * 0.45f : 0.0f;
    pilot.pitch = TurnToward(pilot.pitch, wantedPitch, 0.2f, 0.0f, Deg36);

    // Inclinación lateral desactivada: el personaje se mantiene siempre vertical
    // al girar a izquierda o derecha.
    pilot.bank = 0.0f;

    Vec next{pilot.position.x + pilot.velocity.x,
             pilot.position.y + pilot.velocity.y,
             pilot.position.z + pilot.velocity.z};
    // Límites en las cuatro direcciones para no salirse del mapa
    // y evitar que el cielo se torne negro en los bordes del escenario.
    // Se usa un límite circular (distancia radial) para que las esquinas
    // se redondeen automáticamente: en rectangular (±5150) las esquinas
    // llegaban a ~7283 del origen; en circular el radio es 5150 en todas
    // direcciones, incluidas las diagonales.
    const float radialLimit = 5700.0f;
    const float radial = std::hypot(next.x, next.z);
    if (radial > radialLimit) {
        const float scale = radialLimit / radial;
        next.x *= scale;
        next.z *= scale;
        // Detener la componente outward para no seguir empujando contra el límite.
        const float nx = next.x / radialLimit;
        const float nz = next.z / radialLimit;
        const float outward = pilot.velocity.x * nx + pilot.velocity.z * nz;
        if (outward > 0.0f) {
            pilot.velocity.x -= outward * nx;
            pilot.velocity.z -= outward * nz;
        }
    }
    const float groundNow = GroundY(pilot.position.x, pilot.position.z, pilot.floor);
    const float lowest = std::max(pilot.ceiling, std::min(pilot.floor, groundNow));
    next.y = std::clamp(next.y, pilot.ceiling, lowest);

    const auto blocked = [&pilot](const Vec& target) {
        float hit;
        return earthCollision.trace({pilot.position.x,pilot.position.y-25.0f,pilot.position.z},
                                    {target.x,target.y-25.0f,target.z},hit);
    };

    if (!blocked(next)) {
        pilot.position = next;
    } else {
        // Slide along blocked surfaces instead of losing all momentum.
        Vec attempt = pilot.position;
        const Vec tryX{attempt.x + pilot.velocity.x, attempt.y, attempt.z};
        if (!blocked(tryX)) {
            attempt.x = tryX.x;
        } else {
            pilot.velocity.x = 0.0f;
        }
        const Vec tryZ{attempt.x, attempt.y, attempt.z + pilot.velocity.z};
        if (!blocked(tryZ)) {
            attempt.z = tryZ.z;
        } else {
            pilot.velocity.z = 0.0f;
        }
        const float wantedY = std::clamp(attempt.y + pilot.velocity.y, pilot.ceiling, lowest);
        const Vec tryY{attempt.x, wantedY, attempt.z};
        if (!blocked(tryY)) {
            attempt.y = wantedY;
        } else {
            pilot.velocity.y = 0.0f;
        }
        pilot.position = attempt;
    }
    return GroundY(pilot.position.x, pilot.position.z, pilot.floor);
}

void UpdateDashEffects(Pilot& pilot, void* fighter) {
    const int player = Field<int>(fighter, 0x0C);
    constexpr int battleSoundBank = 4;
    const bool dashLoop = pilot.motion == DragonDashLoop;
    if (dashLoop != pilot.dashEffects) {
        pilot.dashEffects = dashLoop;
        if (dashLoop) {
            Game<int>(rva::EftGlow_Request, player, 0); // Same glow as Dragon Dash's effect bit 4.
            const int sound = Game<int>(rva::BtlParam_GetDashSound, fighter);
            Game<int>(rva::Snd_PlaySe, battleSoundBank, sound);
        } else {
            // Remove the glow before drawing the first frame outside the dash.
            Game<int>(rva::EftGlow_Request, player, 2);
        }
    }
    // Keep all scene tasks and their texture uploads current for BtlScene_Draw.
    Game(rva::BtlScene_Update);
    Game(rva::BtlScene_PostUpdate);
}

void DrawMarker(Pilot& pilot, char* label) {
    constexpr std::size_t labelSize = 112;
    char* marker = label + labelSize;
    std::memcpy(marker, "[ C ]", 6);
    const float remaining = Distance(pilot.position, pilot.goal);
    const char* controls = remaining < ArrivalRadius && !pilot.arrived
        ? "ENTER / START PARA ENTRAR"
        : "O SUBE   9 BAJA   L2 TURBO";
    std::snprintf(label, labelSize, "RADITZ: %d m   %s", static_cast<int>(remaining), controls);
    const auto color = Game<std::uint32_t>(rva::Font_GetColor);
    const auto scale = Game<std::uint32_t>(rva::Font_GetScale);
    const int enabled = Game<int>(rva::Font_IsEnabled);
    Game(rva::Font_SetEnabled, 1);
    Game(rva::Font_SetScale, AsWord(1.0f));
    Game(rva::Font_SetColor, 0x80FFFFFFu);
    Game(rva::Font_PrintAsciiAt, 20, 19, label);

    alignas(16) float screen[4][4];
    Game(rva::Vu0Screen_StoreMtx, &screen[0][0]);
    const Vec goal{pilot.goal.x, pilot.goal.y - 14.0f, pilot.goal.z};
    const auto dot = [&](int col) {
        return goal.x * screen[0][col] + goal.y * screen[1][col] + goal.z * screen[2][col] + screen[3][col];
    };
    const float depth = dot(3);
    const bool inFront = std::isfinite(depth) && depth > 1.0f;
    if (inFront) {
        const float targetX = dot(0) / depth - 1792.0f;
        const float targetY = dot(1) / depth - 1824.0f;
        if (std::isfinite(targetX) && std::isfinite(targetY)) {
            const float clampedX = std::clamp(targetX, 24.0f, 488.0f);
            const float clampedY = std::clamp(targetY, 52.0f, 410.0f);
            if (!pilot.markerVisible) {
                pilot.markerX = clampedX;
                pilot.markerY = clampedY;
                pilot.markerVisible = true;
            } else {
                // Smooth the projection without changing the destination's world position.
                pilot.markerX += (clampedX - pilot.markerX) * 0.22f;
                pilot.markerY += (clampedY - pilot.markerY) * 0.22f;
            }
            const int x = static_cast<int>(std::lround(pilot.markerX));
            const int y = static_cast<int>(std::lround(pilot.markerY));
            if (markerResource && markerSprites) {
                markerSprites[0] = {1, x - 24, y - 46, x + 24, y + 2, 0, 0, 64, 64, 0, 128, 128, 128, 128};
                markerSprites[1] = {};
                markerSprites[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResource, 0, 0, markerSprites);
            } else {
                Game(rva::Font_SetColor, 0x8000E8FFu);
                Game(rva::Font_PrintAsciiAt, x - 18, y, marker);
            }
        } else {
            pilot.markerVisible = false;
        }
    } else {
        // Do not pretend a destination behind the camera is on the left or right.
        pilot.markerVisible = false;
    }
    Game(rva::Font_FlushAll);
    Game(rva::Font_SetEnabled, enabled);
    Game(rva::Font_SetScale, scale);
    Game(rva::Font_SetColor, color);
}

void Draw(Pilot& pilot, char* overlayText) {
    const auto view = *reinterpret_cast<std::uint32_t*>(ImageBase() + rva::gBtlCamView);
    Game(rva::Gfx_MarkPass, 1);
    Game(rva::StgModel_Cull, view);
    Game(rva::StgModel_Draw, view);
    Game(rva::Gfx_MarkPass, 3);
    Game(rva::StgFx_DrawPre);
    Game(rva::Gfx_MarkPass, 2);
    Game(rva::BtlObjDraw_Draw);
    Game(rva::Gfx_MarkPass, 4);
    Game(rva::Gfx_AddDefaultEnv);
    Game(rva::BtlScene_Draw, 1);
    Game(rva::Ot_Draw);
    Game(rva::Gfx_MarkPass, 3);
    Game(rva::StgFx_DrawPost);
    Game(rva::Gfx_MarkPass, 0);
    DrawMarker(pilot, overlayText);
    Game(rva::Gfx_MarkPass, 3);
    Game(rva::StgFx_DrawOverlay);
    Game(rva::Gfx_MarkPass, 0);
}

bool AdventureCycle() {
    if (!Game<int>(rva::BtlStage_IsReady)) return false;
    auto* fighter = Game<void*>(rva::BtlChar_Get, 0);
    if (!fighter) return false;
    auto* avatar = Game<void*>(rva::BtlChar_GetObj, fighter);
    if (!avatar) return false;
    auto* opponent = Game<void*>(rva::BtlChar_Get, 1);
    auto* rival = opponent ? Game<void*>(rva::BtlChar_GetObj, opponent) : nullptr;

    Pilot pilot;
    // Adventure limits are independent of the template arena's invisible bounds.
    pilot.radius = 2850.0f * earthScale;
    pilot.ceiling = -700.0f * earthScale;
    pilot.floor = 0.0f; // Sea-level fallback where the terrain has no triangles.

    // Coordenadas propias del mapa de aventura: punto de aparición y destino.
    // Crater identified from the BT2 Earth map; coordinates use original BT2 scale.
    pilot.position = { 1452.0f * earthScale, 0.0f, 650.0f * earthScale };
    pilot.goal = { 300.0f * earthScale, 0.0f, 300.0f * earthScale };
    float highestGround = 0.0f;
    for (int sample = 0; sample <= 16; ++sample) {
        const float fraction = sample / 16.0f;
        const float x = pilot.position.x + (pilot.goal.x - pilot.position.x) * fraction;
        const float z = pilot.position.z + (pilot.goal.z - pilot.position.z) * fraction;
        highestGround = std::min(highestGround, GroundY(x, z, highestGround));
    }
    // Keep the prototype route and its marker above the ground during level flight.
    pilot.goal.y = std::max(pilot.ceiling, highestGround - 90.0f);
    pilot.position.y = std::max(pilot.ceiling, GroundY(pilot.position.x, pilot.position.z, pilot.floor) - 30.0f);
    const float headingToGoal = std::atan2(pilot.goal.x - pilot.position.x, pilot.goal.z - pilot.position.z);
    if (!std::isfinite(headingToGoal)) return false;
    pilot.heading = pilot.modelYaw = headingToGoal;

    auto* overlayText = Game<char*>(rva::Port_LowAlloc, std::size_t{128});
    if (!overlayText) return false;

    if (rival) Field<int>(rival, 8) = 0;
    Field<int>(avatar, 8) = 1;
    int& avatarNoAnim = Field<int>(avatar, 0xC70);
    avatarNoAnim = 0;
    Field<float>(avatar, 0xA20) = 1.0f;
    auto& avatarFlags = Field<std::uint32_t>(avatar, 0xA40);
    Game(rva::BtlScene_SetSingleView, 1);
    auto* pad = reinterpret_cast<void*>(ImageBase() + rva::gPad);
    const bool recorded = GetEnvironmentVariableW(L"BT3_PAD_PLAY", nullptr, 0) != 0;
    FlightLog("Adventure started", recorded ? Game<int>(rva::Port_PadPlayPos) : 0, Distance(pilot.position, pilot.goal));

    for (int frames = 1;; ++frames) {
        Game(rva::Job_Run);
        Game(rva::Gfx_BeginFrame);
        Game(rva::Pad_Update);
        Game(rva::Snd_Update);
        const Vec previousPosition = pilot.position;
        const float ground = Move(pilot, pad);
        SetPilotPose(pilot, fighter, avatar, ground);
        // Native speed trails use actual per-frame displacement and travel direction.
        const Vec moved{pilot.position.x - previousPosition.x,
                        pilot.position.y - previousPosition.y,
                        pilot.position.z - previousPosition.z, 0};
        Field<Vec>(fighter, 0x50) = moved;
        const float movedLength = std::sqrt(moved.x*moved.x + moved.y*moved.y + moved.z*moved.z);
        if (movedLength > 0.01f) {
            Field<Vec>(fighter, 0x90) = {moved.x/movedLength, moved.y/movedLength, moved.z/movedLength, 0};
        }
        Game(rva::BtlStage_Update);
        Game(rva::BtlCam_SelectView, 0);
        Game(rva::BtlCam_UpdateView, 0);
        avatarFlags |= 2;  // BTL_OBJ_FLAG_VISIBLE
        avatarNoAnim = 0;
        Game(rva::BtlObj_UpdateAll);
        Game(rva::BtlObj_UpdateVisibility, 0);
        Game(rva::BtlObj_FinishVisibility, 0);
        if (pilot.fastFlight && movedLength > 0.5f) {
            Game<int>(rva::EftSpdLine_SpawnBodyTrails, Field<int>(fighter, 0x0C));
        }
        UpdateDashEffects(pilot, fighter);
        Draw(pilot, overlayText);
        Game(rva::Gfx_EndFrame, 2);
        Game(rva::Dma_Flush);

        const float remaining = Distance(pilot.position, pilot.goal);
        if (recorded && frames % 60 == 0 && frames <= 240) {
            FlightLog("Distance to goal", frames, remaining);
            FlightLog("Altitude", frames, pilot.position.y);
        }
        if (pilot.arrived && ++pilot.arriveFrames >= ArrivalFrames) {
            FlightLog("Adventure destination reached", frames, remaining);
            break;
        }
        if (recorded && !pilot.arrived && frames >= 900) {
            FlightLog("Adventure recording timeout", frames, remaining);
            break;
        }
    }

    Game(rva::Port_LowFree, overlayText);
    FlightLog("Adventure cycle ended");
    return pilot.arrived;
}
} // namespace

void RunRaditzAdventure() {
    if (!LoadRaditzMarker()) FlightLog("Raditz marker unavailable; using C");
    if (!earthCollision.load("earth-collision.bin")) {
        FlightLog("Earth collision data unavailable");
        FreeRaditzMarker();
        return;
    }
    FlightLog("Earth collision data loaded", static_cast<int>(earthCollision.triangles.size()));
    void* stage = LoadEarthStage();
    if (!stage) {
        FlightLog("BT2 Earth stage unavailable");
        FreeRaditzMarker();
        return;
    }

    // The mission setup is already selected. Preserve it across this temporary load.
    auto* work = Game<void*>(rva::Battle_GetWork);
    std::uint8_t savedWork[0x1A00];
    std::memcpy(savedWork, work, sizeof(savedWork));
    Field<std::uint64_t>(work, 0x19F0) = 0;

    FlightLog("Adventure loading before Battle_Init", 0, earthScale);
    Game<int>(rva::Battle_Load);
    // Swap temporary sides after the story script has populated and loaded them.
    // Keep the player's input assignment; the complete setup is restored on exit.
    auto* side0 = static_cast<std::uint8_t*>(work) + 0xC0;
    auto* side1 = side0 + 0x270;
    constexpr int RaditzCharacter = 77;
    if (Field<int>(side1, 4) != RaditzCharacter) {
        FlightLog("Raditz not found in mission setup; adventure cancelled");
        Game<int>(rva::Battle_Unload);
        Game(rva::Port_LowFree, stage);
        std::memcpy(work, savedWork, sizeof(savedWork));
        FreeRaditzMarker();
        return;
    }
    std::uint8_t tempSide[0x270];
    const int playerPad = Field<int>(side0, 0x204);
    const int playerControl = Field<int>(side0, 0x208);
    std::memcpy(tempSide, side0, sizeof(tempSide));
    std::memcpy(side0, side1, sizeof(tempSide));
    std::memcpy(side1, tempSide, sizeof(tempSide));
    Field<int>(side0, 0x204) = playerPad;
    Field<int>(side0, 0x208) = playerControl;
    FlightLog("Raditz selected for Adventure", RaditzCharacter);
    const auto arena = std::exchange(StageResource(),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stage)));

    // Set up the presentation resources needed for flight. Battle_Main starts
    // the story script, sequence, and AI after this hook returns.
    Game(rva::StgFx_Init);
    Game(rva::StgModel_InitStage);
    Game(rva::BtlChar_AllocAll, 2);
    Game(rva::BtlCam_Init);
    Game(rva::Ot_Init);
    Game(rva::BtlScene_Init, 0);
    Game(rva::StgFx_Reset);
    Game(rva::Fade_ResetAll);

    const bool arrived = AdventureCycle();
    FreeRaditzMarker();

    Game(rva::BtlChar_FreeAll);
    Game(rva::BtlCam_Term);
    Game(rva::BtlScene_Term);
    Game(rva::Ot_Term);
    Game(rva::StgFx_Term);
    Game(rva::BtlStage_Term);
    StageResource() = arena; // Battle_Unload owns the arena buffer, not our map.
    Game(rva::Port_LowFree, stage);
    Game<int>(rva::Battle_Unload);
    Game(rva::Fade_ResetAll);
    std::memcpy(work, savedWork, sizeof(savedWork));
    FlightLog(arrived ? "Adventure confirmed, ready for Battle_Init" : "Adventure ended before confirmation");
}

// Compatibility with the existing executable's two battle call-site hooks.
extern "C" __declspec(dllexport) void RaditzInitStage() {
    Game(rva::StgModel_InitStage);
}

extern "C" __declspec(dllexport) void RaditzBattleLoop() {
    Game(rva::Battle_Loop);
}
