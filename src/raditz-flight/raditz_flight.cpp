#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "raditz_flight_addresses.h"

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
    if (fopen_s(&file, "raditz-flight.log", "a") == 0) {
        std::fprintf(file, "%s | frames=%d | value=%.2f\n", message, frames, value);
        std::fclose(file);
    }
}

bool flightPending = false;
std::uint32_t originalStage = 0;
void* earthStage = nullptr;

std::uint32_t& StageResource() {
    const auto common = *reinterpret_cast<const std::uint32_t*>(ImageBase() + rva::gCommonRes);
    return Field<std::uint32_t>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(common)), 0x24);
}

void* LoadEarthStage() {
    FILE* file = nullptr;
    if (fopen_s(&file, "raditz-earth.unk", "rb") != 0) return nullptr;
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
            if (!valid) {
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
constexpr std::uint32_t Boost = 1 << 14;
constexpr std::uint32_t Ascend = 1 << 16;
constexpr std::uint32_t Descend = 1 << 17;

struct Pilot {
    Vec position;
    Vec goal;
    Vec velocity{0, 0, 0, 0};
    float heading = 0;     // direction of travel, from the actual velocity
    float modelYaw = 0;    // model facing, turned towards heading at a limited rate
    float pitch = 0;       // flight pitch, engine sign: positive = descending
    float bank = 0;        // sideways lean while turning
    float cameraYaw = 0;
    float radius = 0;
    float ceiling = 0;
    float floor = 0;
    int motion = -1;
};

constexpr float Tau = 6.2831853f;
constexpr float Deg36 = 0.62831853f;

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
    const Vec sample{x, -700.0f, z};
    alignas(16) float hit[12]{};
    Game<int>(rva::BtlStage_ProbeGround, -1, &sample, hit);
    return std::isfinite(hit[1]) && hit[1] > sample.y + 1.0f ? hit[1] : fallback;
}

float Distance(const Vec& a, const Vec& b) {
    return std::hypot(std::hypot(a.x - b.x, a.z - b.z), a.y - b.y);
}

void SetPilotPose(Pilot& pilot, void* fighter, void* avatar, float ground) {
    const float speed = std::hypot(pilot.velocity.x, pilot.velocity.z);
    const int motion = speed > 1.0f ? 0xD : (ground - pilot.position.y > 7.0f ? 0x26 : 0);
    if (pilot.motion != motion || Game<int>(rva::BtlAnim_Advance, fighter, 0)) {
        Game(rva::BtlAnim_Play, fighter, motion, AsWord(0.12f));
        pilot.motion = motion;
    }

    const Vec rotation{0, pilot.modelYaw, 0};
    // The engine's placement function updates both the current and prior model roots.
    Game(rva::BtlChar_Place, fighter, &pilot.position, &rotation, 0);
    // Keep the pose lean fields in sync with the object quaternion, as BtlChar_UpdateLean does.
    const float leanX = std::clamp(pilot.pitch, -1.2f, 1.2f) * 0.72f;
    const float leanZ = std::clamp(pilot.bank, -0.6f, 0.6f);
    Field<float>(fighter, 0xB0) = leanX;
    Field<float>(fighter, 0xB4) = leanZ;
    SetLean(avatar, leanX, leanZ);
    Game(rva::BtlObjXf_Update, avatar);

    const Vec look{pilot.position.x, pilot.position.y - 16.0f, pilot.position.z};
    const Vec desired{pilot.position.x - std::sin(pilot.cameraYaw) * 165.0f,
                      pilot.position.y - 64.0f,
                      pilot.position.z - std::cos(pilot.cameraYaw) * 165.0f};
    Vec& eye = Field<Vec>(fighter, 0x430);
    Game<int>(rva::BtlCam_TraceStage, &eye, &desired, &look,
              static_cast<float*>(nullptr), static_cast<int*>(nullptr));
    Game(rva::Vec3_DiffToEuler, &Field<Vec>(fighter, 0x440), &look, &eye);
    Field<float>(fighter, 0x4A0) = pilot.cameraYaw;
}

float Move(Pilot& pilot, void* pad) {
    const auto held = Field<std::uint32_t>(pad, 0x188);
    const auto button = [held](std::uint32_t mask) { return (held & mask) ? 1.0f : 0.0f; };
    const float forward = std::clamp(-Field<float>(pad, 0x1A8) + button(Up) - button(Down), -1.0f, 1.0f);
    const float strafe = Field<float>(pad, 0x1A4);
    const float turn = std::clamp(Field<float>(pad, 0x1AC) + button(Right) - button(Left), -1.0f, 1.0f);
    pilot.cameraYaw = std::remainder(pilot.cameraYaw + turn * 0.055f, Tau);
    const float sine = std::sin(pilot.cameraYaw);
    const float cosine = std::cos(pilot.cameraYaw);
    const float rate = (held & Boost) ? 16.0f : 7.5f;
    const float diagonal = std::max(1.0f, std::hypot(forward, strafe));
    const Vec desired{(sine * forward + cosine * strafe) * rate / diagonal,
                      (button(Descend) - button(Ascend)) * 6.0f,
                      (cosine * forward - sine * strafe) * rate / diagonal, 0};
    pilot.velocity.x += (desired.x - pilot.velocity.x) * 0.25f;
    pilot.velocity.y += (desired.y - pilot.velocity.y) * 0.25f;
    pilot.velocity.z += (desired.z - pilot.velocity.z) * 0.25f;

    // Keep the camera trailing the model while the player is not turning it,
    // with the gentle rate the engine's free camera uses.
    const float horizontalSpeed = std::hypot(pilot.velocity.x, pilot.velocity.z);
    if (std::fabs(turn) < 0.05f && horizontalSpeed > 1.0f) {
        const float delta = std::remainder(pilot.modelYaw - pilot.cameraYaw, Tau);
        const float follow = std::min(std::fabs(std::sin(delta)) * 0.05f, 0.0333f);
        const float step = std::min(follow, std::fabs(delta));
        pilot.cameraYaw = std::remainder(pilot.cameraYaw + std::copysign(step, delta), Tau);
    }

    // Face the direction the character is actually flying; while the velocity is
    // still building up, follow the requested direction so the turn starts cleanly.
    float wantedHeading = pilot.heading;
    if (horizontalSpeed > 0.5f) {
        wantedHeading = std::atan2(pilot.velocity.x, pilot.velocity.z);
    } else if (std::hypot(desired.x, desired.z) > 0.1f) {
        wantedHeading = std::atan2(desired.x, desired.z);
    }
    const float previousYaw = pilot.modelYaw;
    pilot.heading = wantedHeading;
    pilot.modelYaw = TurnToward(pilot.modelYaw, pilot.heading, 0.3f, 0.0314f, Deg36);

    // Engine pitch: positive when descending (forward direction y = -sin(pitch)).
    const float verticalSpeed = pilot.velocity.y;
    const float flightSpeed = std::hypot(horizontalSpeed, verticalSpeed);
    const float wantedPitch = flightSpeed > 1.0f
        ? std::atan2(-verticalSpeed, horizontalSpeed)
        : 0.0f;
    pilot.pitch = TurnToward(pilot.pitch, wantedPitch, 0.3f, 0.0f, Deg36);

    // Lean into the turn; the model yaw error gives the yaw rate for this frame.
    const float yawError = std::remainder(pilot.modelYaw - previousYaw, Tau);
    const float wantedBank = std::clamp(-yawError * 0.6f, -0.55f, 0.55f);
    pilot.bank += (wantedBank - pilot.bank) * 0.15f;

    Vec next{pilot.position.x + pilot.velocity.x,
             pilot.position.y + pilot.velocity.y,
             pilot.position.z + pilot.velocity.z};
    const float radial = std::hypot(next.x, next.z);
    if (radial > pilot.radius) {
        const float scale = pilot.radius / radial;
        next.x *= scale;
        next.z *= scale;
        // Drop the outward component so the character slides along the boundary.
        const float nx = next.x / pilot.radius;
        const float nz = next.z / pilot.radius;
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
        const Vec from{pilot.position.x, pilot.position.y - 25.0f, pilot.position.z};
        const Vec to{target.x, target.y - 25.0f, target.z};
        Vec contact;
        return Game<int>(rva::BtlCam_TraceStage, &contact, &to, &from,
                         static_cast<float*>(nullptr), static_cast<int*>(nullptr)) != 0;
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

void DrawMarker(const Pilot& pilot, char* label) {
    constexpr std::size_t labelSize = 112;
    char* marker = label + labelSize;
    std::memcpy(marker, "[ C ]", 6);
    std::snprintf(label, labelSize, "C: %d m   O SUBE   9 BAJA   U TURBO",
                  static_cast<int>(Distance(pilot.position, pilot.goal)));
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
    int x;
    int y;
    if (depth > 1.0f) {
        x = static_cast<int>(std::clamp(dot(0) / depth - 1792.0f, 24.0f, 488.0f));
        y = static_cast<int>(std::clamp(dot(1) / depth - 1824.0f, 52.0f, 410.0f));
    } else {
        const float side = (goal.x - pilot.position.x) * std::cos(pilot.cameraYaw) -
                           (goal.z - pilot.position.z) * std::sin(pilot.cameraYaw);
        x = side < 0 ? 24 : 488;
        y = 216;
    }
    Game(rva::Font_SetColor, 0x8000E8FFu);
    Game(rva::Font_PrintAsciiAt, x - 18, y, marker);
    Game(rva::Font_FlushAll);
    Game(rva::Font_SetEnabled, enabled);
    Game(rva::Font_SetScale, scale);
    Game(rva::Font_SetColor, color);
}

void Draw(const Pilot& pilot, char* overlayText) {
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

bool Explore() {
    if (!Game<int>(rva::BtlStage_IsReady)) return false;
    auto* fighter = Game<void*>(rva::BtlChar_Get, 0);
    auto* opponent = Game<void*>(rva::BtlChar_Get, 1);
    if (!fighter || !opponent) return false;
    auto* rival = Game<void*>(rva::BtlChar_GetObj, opponent);
    auto* avatar = Game<void*>(rva::BtlChar_GetObj, fighter);
    if (!rival || !avatar) return false;

    Pilot pilot;
    pilot.radius = AsFloat(Game<std::uint32_t>(rva::BtlStage_GetInnerRadius));
    pilot.ceiling = AsFloat(Game<std::uint32_t>(rva::BtlStage_GetTop)) + 30.0f;
    pilot.floor = AsFloat(Game<std::uint32_t>(rva::BtlStage_GetBottom));
    if (!std::isfinite(pilot.radius) || pilot.radius < 150.0f || pilot.ceiling >= pilot.floor) return false;

    Vec heading;
    Vec rivalHeading;
    Game(rva::BtlStage_GetStartPlace, 0, &pilot.position, &heading, 0);
    Game(rva::BtlStage_GetStartPlace, 1, &pilot.goal, &rivalHeading, 0);
    const float headingToGoal = std::atan2(pilot.goal.x - pilot.position.x, pilot.goal.z - pilot.position.z);
    if (!std::isfinite(headingToGoal)) return false;
    const float route = std::min(600.0f, pilot.radius * 0.6f);
    const float startX = pilot.goal.x - std::sin(headingToGoal) * route;
    const float startZ = pilot.goal.z - std::cos(headingToGoal) * route;
    if (std::hypot(startX, startZ) < pilot.radius - 45.0f) {
        pilot.position.y = GroundY(startX, startZ, pilot.position.y);
        pilot.position.x = startX;
        pilot.position.z = startZ;
    }
    // Pick an unobstructed starting altitude above the terrain along the route.
    float highestGround = std::min(pilot.position.y, pilot.goal.y);
    for (int sample = 1; sample < 16; ++sample) {
        const float weight = sample / 16.0f;
        const float x = pilot.position.x + (pilot.goal.x - pilot.position.x) * weight;
        const float z = pilot.position.z + (pilot.goal.z - pilot.position.z) * weight;
        highestGround = std::min(highestGround, GroundY(x, z, highestGround));
    }
    pilot.position.y = pilot.goal.y = std::max(pilot.ceiling, highestGround - 90.0f);
    if (Distance(pilot.position, pilot.goal) < 90.0f) return false;
    pilot.heading = pilot.modelYaw = pilot.cameraYaw = headingToGoal;

    // Font_PrintAsciiAt receives a PS2-style 32-bit string address.
    auto* overlayText = Game<char*>(rva::Port_LowAlloc, std::size_t{128});
    if (!overlayText) return false;

    // The story, its timer, and AI are not stepped during this separate flight loop.
    // Remove the opponent from the renderer and restore it before the battle restarts.
    int& rivalActive = Field<int>(rival, 8);
    const int savedActive = std::exchange(rivalActive, 0);
    int& avatarActive = Field<int>(avatar, 8);
    const int savedAvatarActive = std::exchange(avatarActive, 1);
    int& avatarNoAnim = Field<int>(avatar, 0xC70);
    const int savedNoAnim = std::exchange(avatarNoAnim, 0);
    float& avatarScale = Field<float>(avatar, 0xA20);
    const float savedScale = std::exchange(avatarScale, 1.0f);
    auto& avatarFlags = Field<std::uint32_t>(avatar, 0xA40);
    const auto savedAvatarFlags = avatarFlags;
    Game(rva::BtlScene_SetSingleView, 1);
    auto* pad = reinterpret_cast<void*>(ImageBase() + rva::gPad);
    const bool recorded = GetEnvironmentVariableW(L"BT3_PAD_PLAY", nullptr, 0) != 0;
    FlightLog("Flight started", recorded ? Game<int>(rva::Port_PadPlayPos) : 0, Distance(pilot.position, pilot.goal));
    if (recorded) {
        FlightLog("Capture frame", *reinterpret_cast<int*>(ImageBase() + rva::gGsFrame));
        FlightLog("Initial altitude", 0, pilot.position.y);
    }

    for (int frames = 1;; ++frames) {
        Game(rva::Job_Run);
        Game(rva::Gfx_BeginFrame);
        Game(rva::Pad_Update);
        Game(rva::Snd_Update);
        const float ground = Move(pilot, pad);
        SetPilotPose(pilot, fighter, avatar, ground);
        Game(rva::BtlStage_Update);
        Game(rva::BtlCam_SelectView, 0);
        Game(rva::BtlCam_UpdateView, 0);
        avatarFlags |= 2;  // BTL_OBJ_FLAG_VISIBLE, independently of the story script.
        avatarNoAnim = 0;  // Story pose changes can freeze model animation again.
        Game(rva::BtlObj_UpdateAll);
        Game(rva::BtlObj_UpdateVisibility, 0);
        Game(rva::BtlObj_FinishVisibility, 0);
        if (recorded && frames == 1) {
            auto* drawList = Game<void*>(rva::BtlObjVis_GetList, 0);
            FlightLog("Visible models", Field<int>(drawList, 0x30));
            FlightLog("Avatar bounds X", 0, Field<float>(avatar, 0xAF4));
            FlightLog("Avatar bounds Y", 0, Field<float>(avatar, 0xAF8));
            FlightLog("Avatar bounds Z", 0, Field<float>(avatar, 0xAFC));
            FlightLog("Pilot X", 0, pilot.position.x);
            FlightLog("Pilot Y", 0, pilot.position.y);
            FlightLog("Pilot Z", 0, pilot.position.z);
            FlightLog("Avatar root X", savedNoAnim, Field<Vec>(avatar, 0x950).x);
            FlightLog("Avatar transform X", avatarNoAnim, Field<Vec>(avatar, 0x9D0).x);
            FlightLog("Avatar scale before flight", 0, savedScale);
        }
        Draw(pilot, overlayText);
        Game(rva::Gfx_EndFrame, 2);
        Game(rva::Dma_Flush);

        const float remaining = Distance(pilot.position, pilot.goal);
        if (recorded && frames % 60 == 0 && frames <= 240) {
            FlightLog("Distance to C", frames, remaining);
            FlightLog("Altitude", frames, pilot.position.y);
        }
        if (remaining < 65.0f) {
            FlightLog("Point C reached", frames, remaining);
            break;
        }
        if (recorded && frames >= 900) {
            FlightLog("Recording did not reach C", frames, remaining);
            break;
        }
    }
    rivalActive = savedActive;
    avatarActive = savedAvatarActive;
    avatarNoAnim = savedNoAnim;
    avatarScale = savedScale;
    avatarFlags = savedAvatarFlags;
    Game(rva::Port_LowFree, overlayText);
    return true;
}
} // namespace

void ArmRaditzFlight() {
    flightPending = true;
}

extern "C" __declspec(dllexport) void RaditzInitStage() {
    if (flightPending) {
        earthStage = LoadEarthStage();
        if (earthStage) {
            // Keep the untouched arena resource. The stage loader relocates its pointers in place.
            originalStage = StageResource();
            StageResource() = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(earthStage));
            FlightLog("BT2 Earth stage loaded");
        } else {
            FlightLog("BT2 Earth stage unavailable, starting the original battle");
            flightPending = false;
        }
    }
    Game(rva::StgModel_InitStage);
}

extern "C" __declspec(dllexport) void RaditzBattleLoop() {
    if (std::exchange(flightPending, false) && earthStage) {
        if (!Explore()) FlightLog("BT2 Earth flight unavailable");
        Game(rva::BtlLoad_BeginStageSwap);
        StageResource() = std::exchange(originalStage, 0);
        Game(rva::BtlLoad_EndStageSwap);
        Game(rva::Port_LowFree, std::exchange(earthStage, nullptr));
        Game(rva::BtlCam_Reset);
        Game(rva::Battle_Restart);
        FlightLog("Original Raditz battle started");
    }
    Game(rva::Battle_Loop);
}
