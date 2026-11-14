#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <initializer_list>

#include "bt3_runtime_addresses.h"
#include "earth_collision.h"
#include "adventure_loader.h"

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
enum class Encounter { None, Piccolo, KameHouse, RaditzFinal };
Encounter confirmedEncounter = Encounter::None;
bool piccoloWon = false;
bool kameHouseWon = false;
bool adminOpen = false;
bool pauseOpen = false;
int pauseFrames = 0;
int pausePulseFrame = 0;
int PauseRowX(int row) { return 56 - row * 5; }
int PauseRowY(int row) {
    // The lower entries enter first, followed by the rows above them.
    const int target = 125 + row * 39;
    const float t = std::clamp((pauseFrames - (3 - row) * 3) / 12.0f, 0.0f, 1.0f);
    const float eased = 1.0f - (1.0f - t) * (1.0f - t);
    return static_cast<int>(std::lround(-36.0f + (target + 36.0f) * eased));
}
int pauseChoice = 0;
int pauseDestination = -1;
int pauseNotice = -1;
int pauseNoticeFrames = 0;
bool adventureHudVisible = true;
bool adventureGoku = false;
bool characterMenuOpen = false;
bool characterMenuChoice = false;
// Completed encounters can be replayed only while controlling Raditz.
bool MissionAvailable(int index) {
    if (adventureGoku) return index == 2 && kameHouseWon;
    if (index == 0) return true;
    if (index == 1) return piccoloWon;
    return false;
}
bool adminReload = false;
int adminSelection = 0;
void* worldTargetObject = nullptr;
int worldTargetId = -1;
int worldTargetKind = 0;
bool resumeAtEncounter = false;
Vec resumePosition;
float resumeHeading = 0.0f;
std::uint8_t encounterSetup[0x5A8]{};

bool ProgressPath(wchar_t (&path)[MAX_PATH]) {
    const DWORD n = GetFullPathNameW(L"saves\\dbzbt3r-progress.ini", MAX_PATH, path, nullptr);
    return n > 0 && n < MAX_PATH;
}

bool LoadVictory(const wchar_t* key) {
    wchar_t path[MAX_PATH];
    return ProgressPath(path) && GetPrivateProfileIntW(L"story", key, 0, path) == 1;
}

bool SetVictory(const wchar_t* key, bool value) {
    if (!CreateDirectoryW(L"saves", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    wchar_t path[MAX_PATH];
    return ProgressPath(path) && WritePrivateProfileStringW(L"story", key, value ? L"1" : L"0", path) != 0;
}

bool SaveVictory(const wchar_t* key) {
    if (!CreateDirectoryW(L"saves", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    wchar_t path[MAX_PATH];
    return ProgressPath(path) && WritePrivateProfileStringW(L"story", key, L"1", path) != 0;
}
// BT2 story marker IDs: 14 = Piccolo/Raditz encounter, 9 = Kame House.
constexpr int MarkerCount = 3;
constexpr int ScouterColumns = 8;
constexpr int ScouterTileSize = 64;
constexpr int ScouterTiles = 56;
constexpr int MinimapTileCount = 16;
constexpr int MinimapArrowCount = 32;
constexpr int MinimapBase = MarkerCount + 2 + ScouterTiles;
constexpr int ColumnBase = MinimapBase + MinimapTileCount + MinimapArrowCount;
constexpr int FadeIndex = ColumnBase + MarkerCount;
constexpr int MissionDotBase = FadeIndex + 1;
constexpr int SelectorBase = MissionDotBase + MarkerCount;
constexpr int SelectorHighlight = SelectorBase + 32;
constexpr int ScouterReticleBase = SelectorHighlight + 1;
constexpr int PauseBase = ScouterReticleBase + 6;
constexpr int PauseBars = PauseBase + 56;
constexpr int PauseText = PauseBars + 8;
constexpr int PauseIcons = PauseText + 32;
constexpr int ResourceCount = PauseIcons + 20;
void* markerResources[ResourceCount]{};
struct MarkerSprite {
    int flags, x0, y0, x1, y1, u0, v0, u1, v1, texture, r, g, b, a;
};
MarkerSprite* markerSprites = nullptr;
MarkerSprite* pauseSprites = nullptr;
constexpr int ColumnSections = 12;
MarkerSprite* columnSprites = nullptr;

bool LoadMarkerResource(int index, const char* path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb")) return false;
    constexpr size_t entrySize = 64, packetSize = 16512, totalSize = 128 + packetSize;
    auto* data = Game<unsigned char*>(rva::Port_LowAlloc, totalSize);
    bool ok = data && std::fread(data + 64, 1, entrySize, f) == entrySize &&
              std::fread(data + 128, 1, packetSize, f) == packetSize && std::fgetc(f) == EOF;
    std::fclose(f);
    if (!ok) {
        if (data) Game(rva::Port_LowFree, data);
        return false;
    }
    std::memset(data, 0, 64);
    Field<std::uint32_t>(data, 0x10) = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data + 64));
    Field<std::uint32_t>(data, 0x40) = 1; // Pixels present.
    Field<std::uint32_t>(data, 0x44) = 0; // 32-bit colour, no palette.
    Field<std::uint32_t>(data, 0x48) = static_cast<std::uint32_t>(packetSize);
    Field<std::uint32_t>(data, 0x78) = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data + 128));
    Field<std::uint32_t>(data, 0x7c) = 0;
    markerResources[index] = data;
    return true;
}

void LoadMarkers() {
    markerSprites = Game<MarkerSprite*>(rva::Port_LowAlloc, ResourceCount * 2 * sizeof(MarkerSprite));
    pauseSprites = Game<MarkerSprite*>(rva::Port_LowAlloc, 72 * sizeof(MarkerSprite));
    columnSprites = Game<MarkerSprite*>(rva::Port_LowAlloc, MarkerCount * ColumnSections * 2 * sizeof(MarkerSprite));
    const char* paths[MarkerCount] = {"piccolo-marker.bin", "goku-marker.bin", "raditz-marker.bin"};
    for (int i = 0; i < MarkerCount; ++i) {
        if (!LoadMarkerResource(i, paths[i])) FlightLog("Story marker resource unavailable", i);
    }
    if (!LoadMarkerResource(MarkerCount, "scouter-triangles.bin")) FlightLog("Scouter marker resource unavailable");
    if (!LoadMarkerResource(MarkerCount + 1, "scouter-filter.bin")) FlightLog("Scouter filter resource unavailable");
    for (int tile = 0; tile < MinimapTileCount; ++tile) {
        char path[32]; std::snprintf(path, sizeof(path), "minimap-%02d.bin", tile);
        if (!LoadMarkerResource(MinimapBase + tile, path)) FlightLog("Minimap tile unavailable", tile);
    }
    for (int direction = 0; direction < MinimapArrowCount; ++direction) {
        char path[32]; std::snprintf(path, sizeof(path), "minimap-arrow-%d.bin", direction);
        if (!LoadMarkerResource(MinimapBase + MinimapTileCount + direction, path)) FlightLog("Minimap arrow unavailable", direction);
    }
    for (int i = 0; i < MarkerCount; ++i)
        if (!LoadMarkerResource(ColumnBase + i, "mission-model.bin")) FlightLog("Mission column resource unavailable", i);
    for (int i = 0; i < MarkerCount; ++i)
        if (!LoadMarkerResource(MissionDotBase + i, "minimap-mission-dot.bin")) FlightLog("Minimap mission dot unavailable", i);
    for (int i = 0; i < 32; ++i) {
        char path[48]; std::snprintf(path, sizeof(path), "selector-%s-%02d.bin", i < 16 ? "raditz" : "team", i % 16);
        if (!LoadMarkerResource(SelectorBase + i, path)) FlightLog("Selector texture unavailable", i);
    }
    for (int row = 0; row < 4; ++row) for (int frame = 0; frame < (row == 2 ? 4 : 5); ++frame) {
        char path[32]; std::snprintf(path, sizeof(path), "pause-icon-%d-%d.bin", row, frame);
        if (!LoadMarkerResource(PauseIcons + row * 5 + frame, path)) FlightLog("Pause icon unavailable", row * 5 + frame);
    }
    if (!LoadMarkerResource(SelectorHighlight, "selector-highlight.bin")) FlightLog("Selector highlight unavailable");
    const char* reticles[3] = {"scouter-reticle-right.bin", "scouter-reticle-left.bin", "scouter-reticle-up.bin"};
    for (int i = 0; i < 6; ++i)
        if (!LoadMarkerResource(ScouterReticleBase + i, reticles[i % 3])) FlightLog("Scouter triangle unavailable", i);

    for (int i = 0; i < 15; ++i) {
        char path[40]; std::snprintf(path, sizeof(path), "pause-goku-%02d.bin", i);
        if (!LoadMarkerResource(PauseBase + i, path)) FlightLog("Pause Goku unavailable", i);
    }
    for (int i = 0; i < 8; ++i) {
        char path[40]; std::snprintf(path, sizeof(path), "pause-%s-%02d.bin", i < 4 ? "normal" : "selected", i % 4);
        if (!LoadMarkerResource(PauseBars + i, path)) FlightLog("Pause bar unavailable", i);
    }
    for (int i = 0; i < 32; ++i) {
        char path[48];
        std::snprintf(path, sizeof(path), "pause-text-%s-%d-%d.bin", i < 16 ? "normal" : "selected", (i % 16) / 4, i % 4);
        if (!LoadMarkerResource(PauseText + i, path)) FlightLog("Pause text unavailable", i);
    }

    if (!LoadMarkerResource(FadeIndex, "mission-fade.bin")) FlightLog("Mission fade resource unavailable");
    for (int tile = 0; tile < ScouterTiles; ++tile) {
        char path[32]; std::snprintf(path, sizeof(path), "scouter-frame-%02d.bin", tile);
        if (!LoadMarkerResource(MarkerCount + 2 + tile, path)) FlightLog("Scouter frame tile unavailable", tile);
    }
}

void FreeMarkers() {
    if (pauseSprites) Game(rva::Port_LowFree, pauseSprites);
    pauseSprites = nullptr;
    if (columnSprites) Game(rva::Port_LowFree, columnSprites);
    columnSprites = nullptr;
    for (auto& resource : markerResources) {
        if (resource) Game(rva::Port_LowFree, resource);
        resource = nullptr;
    }
    if (markerSprites) Game(rva::Port_LowFree, markerSprites);
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
constexpr std::uint32_t Circle = 1 << 8;
constexpr std::uint32_t Cross = 1 << 9;
constexpr std::uint32_t Triangle = 1 << 10;
constexpr std::uint32_t Square = 1 << 11;
constexpr std::uint32_t Start = 1 << 12;
constexpr std::uint32_t L1 = 1 << 14;
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
    Vec markerGoals[MarkerCount];
    float markerX[MarkerCount]{};
    float markerY[MarkerCount]{};
    bool markerVisible[MarkerCount]{};
    bool scouterActive = false;
    Vec arrivalStart;
    Vec arrivalTarget;
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
constexpr float ArrivalRadius = 180.0f;
constexpr int ArrivalFrames = 45;

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

    if (pilot.arrived) {
        const float t = std::clamp(pilot.arriveFrames / 35.0f, 0.0f, 1.0f);
        const float ease = t * t * (3.0f - 2.0f * t);
        const Vec previous = pilot.position;
        pilot.position = {pilot.arrivalStart.x + (pilot.arrivalTarget.x - pilot.arrivalStart.x) * ease,
                          pilot.arrivalStart.y + (pilot.arrivalTarget.y - pilot.arrivalStart.y) * ease,
                          pilot.arrivalStart.z + (pilot.arrivalTarget.z - pilot.arrivalStart.z) * ease};
        pilot.velocity = {pilot.position.x - previous.x, pilot.position.y - previous.y, pilot.position.z - previous.z, 0};
        if (std::hypot(pilot.velocity.x, pilot.velocity.z) > 0.01f) {
            pilot.heading = std::atan2(pilot.arrivalTarget.x - pilot.position.x, pilot.arrivalTarget.z - pilot.position.z);
            pilot.modelYaw = TurnToward(pilot.modelYaw, pilot.heading, 0.3f, 0.0314f, Deg36);
        }
        pilot.fastFlight = t < 0.92f;
        return GroundY(pilot.position.x, pilot.position.z, pilot.floor);
    }

    const auto pressed = Field<std::uint32_t>(pad, 0x18C);
    if (!adventureGoku && (pressed & Triangle)) {
        pilot.scouterActive = !pilot.scouterActive;
        FlightLog(pilot.scouterActive ? "Scouter enabled" : "Scouter disabled");
    }
    const float piccoloDistance = std::hypot(pilot.position.x - pilot.markerGoals[0].x, pilot.position.z - pilot.markerGoals[0].z);
    const float raditzDistance = std::hypot(pilot.position.x - pilot.markerGoals[2].x, pilot.position.z - pilot.markerGoals[2].z);
    const float kameDistance = std::hypot(pilot.position.x - pilot.markerGoals[1].x, pilot.position.z - pilot.markerGoals[1].z);
    Encounter nearest = Encounter::None;
    float nearestDistance = ArrivalRadius;
    if (MissionAvailable(0) && piccoloDistance < nearestDistance) { nearest = Encounter::Piccolo; nearestDistance = piccoloDistance; }
    if (MissionAvailable(1) && kameDistance < nearestDistance) { nearest = Encounter::KameHouse; nearestDistance = kameDistance; }
    if (MissionAvailable(2) && raditzDistance < nearestDistance) { nearest = Encounter::RaditzFinal; nearestDistance = raditzDistance; }
    if ((pressed & Square) && nearest != Encounter::None) {
        confirmedEncounter = nearest;
        pilot.arrived = true;
        pilot.arrivalStart = pilot.position;
        pilot.arrivalTarget = pilot.markerGoals[confirmedEncounter == Encounter::Piccolo ? 0 : confirmedEncounter == Encounter::KameHouse ? 1 : 2];
        pilot.arrivalTarget.y = GroundY(pilot.arrivalTarget.x, pilot.arrivalTarget.z, pilot.floor) - 18.0f;
        pilot.boosting = false;
        pilot.fastFlight = false;
        pilot.velocity = {0, 0, 0, 0};
        pilot.pitch = pilot.bank = pilot.verticalInput = 0.0f;
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

    if (selPressed) adventureHudVisible = !adventureHudVisible;

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

// The port saves the F1 aspect selection in this file. Refresh periodically so
// changing the setting during Adventure also updates the frame layout.
float ScouterAspectScale() {
    static int ticks = 0;
    static float scale = 1.0f;
    if (ticks++ % 30 == 0) {
        FILE* file = nullptr;
        if (fopen_s(&file, "bt3_settings.txt", "r") == 0) {
            char line[256]; int aspect = 0;
            while (std::fgets(line, sizeof(line), file)) {
                if (sscanf_s(line, "aspect_milli=%d", &aspect) == 1) break;
            }
            std::fclose(file);
            scale = std::clamp(aspect / 1333.333f, 1.0f, 3.0f);
        }
    }
    return scale;
}

// Keyboard-only development panel. Edge detection survives a scene reload.
bool AdminPressed(int key, bool& previous) {
    DWORD owner = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &owner);
    const bool down = owner == GetCurrentProcessId() && (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !previous;
    previous = down;
    return pressed;
}

void UpdateAdminPanel() {
    static bool f8 = false, up = false, down = false, space = false;
    const bool toggle = AdminPressed(VK_F8, f8);
    const bool selectUp = AdminPressed(VK_UP, up);
    const bool selectDown = AdminPressed(VK_DOWN, down);
    const bool change = AdminPressed(VK_SPACE, space);
    if (toggle) adminOpen = !adminOpen;
    if (!adminOpen) return;
    if (selectUp || selectDown) adminSelection = 1 - adminSelection;
    if (!change) return;
    bool newPiccolo = piccoloWon, newKame = kameHouseWon;
    if (adminSelection == 0) {
        newPiccolo = !newPiccolo;
        if (!newPiccolo) newKame = false;
    } else {
        newKame = !newKame;
        if (newKame) newPiccolo = true;
    }
    // Restore the previous saved values on failure; never reload a half-written state.
    if (!SetVictory(L"piccolo_victory", newPiccolo) || !SetVictory(L"kame_house_victory", newKame)) {
        SetVictory(L"piccolo_victory", piccoloWon);
        SetVictory(L"kame_house_victory", kameHouseWon);
        FlightLog("Admin panel could not save progress");
        return;
    }
    piccoloWon = newPiccolo;
    kameHouseWon = newKame;
    adventureGoku = newKame;
    characterMenuOpen = false;
    adminReload = true;
    FlightLog("Admin panel updated progress", piccoloWon ? 1 : 0, kameHouseWon ? 1.0f : 0.0f);
}

struct alignas(16) MissionColumnVertex { Vec pos; Vec uv; Vec color; };
void DrawColumns3D(Pilot& pilot) {
    if (!(adventureGoku || pilot.scouterActive) || !markerResources[ColumnBase]) return;
    static const float positions[] = {-0.30000001f,0.00000000f,0.00000000f,0.30000001f,0.00000000f,-0.00000000f,0.30000001f,5.00000000f,-0.00000000f,-0.30000001f,5.00000000f,0.00000000f,-0.21213204f,0.00000000f,0.21213204f,0.21213204f,0.00000000f,-0.21213204f,0.21213204f,5.00000000f,-0.21213204f,-0.21213204f,5.00000000f,0.21213204f,-0.00000000f,0.00000000f,0.30000001f,0.00000000f,0.00000000f,-0.30000001f,0.00000000f,5.00000000f,-0.30000001f,-0.00000000f,5.00000000f,0.30000001f,0.21213204f,0.00000000f,0.21213204f,-0.21213204f,0.00000000f,-0.21213204f,-0.21213204f,5.00000000f,-0.21213204f,0.21213204f,5.00000000f,0.21213204f};
    static const float uvs[] = {0.00000000f,1.00000000f,1.00000000f,1.00000000f,1.00000000f,0.00000000f,0.00000000f,0.00000000f,0.00000000f,1.00000000f,1.00000000f,1.00000000f,1.00000000f,0.00000000f,0.00000000f,0.00000000f,0.00000000f,1.00000000f,1.00000000f,1.00000000f,1.00000000f,0.00000000f,0.00000000f,0.00000000f,0.00000000f,1.00000000f,1.00000000f,1.00000000f,1.00000000f,0.00000000f,0.00000000f,0.00000000f};
    static const unsigned short indices[] = {0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11,12,13,14,12,14,15};
    auto* res = markerResources[ColumnBase];
    const auto cbp = Field<std::int32_t>(res, 0x50) + 0x3040;
    const auto tex = Field<std::uint64_t>(res, 0x70) | (static_cast<std::uint64_t>(cbp) << 37) | (std::uint64_t{4} << 32) | 0x3000;
    Game(0x420c30, res, 0, 0x3000, cbp);
    Game(0x414cb0);
    const auto view = *reinterpret_cast<std::uint32_t*>(ImageBase() + rva::gBtlCamView);
    if (!view) return;
    Game(0x4063a0);
    Game(0x406740, reinterpret_cast<void*>(static_cast<std::uintptr_t>(view) + 0x140));
    for (int i=0;i<MarkerCount;++i) {
        if (!MissionAvailable(i)) continue;
        Vec base=pilot.markerGoals[i]; base.y=GroundY(base.x,base.z,pilot.floor);
        for (int t=0;t<8;++t) {
            MissionColumnVertex v[16]{};
            for (int k=0;k<3;++k) {
                const auto a=indices[t*3+k];
                v[k].pos={base.x+positions[a*3]*50.0f,base.y-positions[a*3+1]*280.0f,base.z+positions[a*3+2]*50.0f,1.0f};
                v[k].color={128.0f,128.0f,128.0f,128.0f};
                v[k].uv={uvs[a*2],uvs[a*2+1],1.0f,1.0f};
            }
            Game(0x8acd0,v,1,0,0,0,tex,0);
        }
    }
    Game(0x406460);
}


void DrawMarker(Pilot& pilot, char* label) {
    constexpr std::size_t labelSize = 112;
    const float proximity = std::hypot(pilot.position.x - pilot.markerGoals[0].x, pilot.position.z - pilot.markerGoals[0].z);
    const float raditzDistance = std::hypot(pilot.position.x - pilot.markerGoals[2].x, pilot.position.z - pilot.markerGoals[2].z);
    const float kameDistance = std::hypot(pilot.position.x - pilot.markerGoals[1].x, pilot.position.z - pilot.markerGoals[1].z);
    const bool nearPiccolo = !kameHouseWon && proximity < ArrivalRadius;
    const bool nearKame = piccoloWon && !kameHouseWon && kameDistance < ArrivalRadius;
    const bool nearRaditz = kameHouseWon && raditzDistance < ArrivalRadius;
    if (nearRaditz && (!nearKame || raditzDistance <= kameDistance)) std::snprintf(label, labelSize, "RADITZ: CUADRADO PARA PELEAR");
    else if (nearPiccolo && (!nearKame || proximity <= kameDistance)) std::snprintf(label, labelSize, piccoloWon ? "PICCOLO: CUADRADO PARA REPETIR" : "PICCOLO: CUADRADO PARA PELEAR");
    else if (nearKame) std::snprintf(label, labelSize, kameHouseWon ? "KAME HOUSE: CUADRADO PARA REPETIR" : "KAME HOUSE: CUADRADO PARA PELEAR");
    else if (kameHouseWon) std::snprintf(label, labelSize, "ENCUENTRA A RADITZ");
    else if (piccoloWon) std::snprintf(label, labelSize, "ENCUENTRA A GOKU");
    else std::snprintf(label, labelSize, "ENCUENTRA A PICCOLO");
    const auto color = Game<std::uint32_t>(rva::Font_GetColor);
    const auto scale = Game<std::uint32_t>(rva::Font_GetScale);
    const int enabled = Game<int>(rva::Font_IsEnabled);
    Game(rva::Font_SetEnabled, 1);
    Game(rva::Font_SetScale, AsWord(1.0f));
    Game(rva::Font_SetColor, 0x80FFFFFFu);
    if (!adventureGoku && pilot.scouterActive && markerResources[MarkerCount + 1] && markerSprites) {
        auto* filter = markerSprites + (MarkerCount + 1) * 2;
        // Match the opening of the frame; do not tint the area outside it.
        // The short strips approximate the clipped corners and the top-left notch.
        const float aspect = ScouterAspectScale();
        const auto lensX = [aspect](float px) {
            const float cropped = -6.0f + px * 518.0f / 512.0f;
            return static_cast<int>(std::lround(256.0f + (cropped - 256.0f) * aspect));
        };
        struct LensStrip { int x1, y1, x2, y2; };
        constexpr LensStrip strips[] = {
            {130, 50, 488, 63}, {47, 63, 497, 72},
            {32, 72, 497, 399}, {32, 399, 490, 409}
        };
        for (const auto& strip : strips) {
            filter[0] = {1, lensX(static_cast<float>(strip.x1)), strip.y1,
                         lensX(static_cast<float>(strip.x2)), strip.y2,
                         0, 0, 64, 64, 0, 128, 128, 128, 128};
            filter[1] = {}; filter[1].flags = 2;
            Game(rva::Sprite_DrawList, markerResources[MarkerCount + 1], 0, 0, filter);
        }
    }
    if (!adventureGoku && pilot.scouterActive && markerSprites) {
        // Detect every available Raditz encounter independently.
        for (int targetIndex = 0; targetIndex < 2; ++targetIndex) {
        if (!MissionAvailable(targetIndex)) continue;
        Vec goal = pilot.markerGoals[targetIndex];
        goal.y = GroundY(goal.x, goal.z, pilot.floor) - 10.0f;
        alignas(16) float screen[4][4];
        Game(rva::Vu0Screen_StoreMtx, &screen[0][0]);
        const auto dot = [&](int col) {
            return goal.x * screen[0][col] + goal.y * screen[1][col] + goal.z * screen[2][col] + screen[3][col];
        };
        const float depth = dot(3);
        if (std::isfinite(depth) && depth > 1.0f) {
            const float x = dot(0) / depth - 1792.0f;
            const float y = dot(1) / depth - 1824.0f;
            if (std::isfinite(x) && std::isfinite(y) && x >= 0 && x <= 512 && y >= 0 && y <= 448) {
                const int sx = static_cast<int>(std::lround(x)), sy = static_cast<int>(std::lround(y));
                const float aspect = ScouterAspectScale();
                const int halfWidth = std::max(1, static_cast<int>(std::lround(8.0f / aspect)));
                const int offsetX = static_cast<int>(std::lround(32.0f / aspect));
                const int xs[3] = {sx - offsetX, sx + offsetX, sx};
                const int ys[3] = {sy, sy, sy + 28};
                for (int i = 0; i < 3; ++i) {
                    const int id = ScouterReticleBase + targetIndex * 3 + i;
                    if (!markerResources[id]) continue;
                    auto* sprites = markerSprites + id * 2;
                    sprites[0] = {1, xs[i] - halfWidth, ys[i] - 8, xs[i] + halfWidth, ys[i] + 8, 0, 0, 64, 64, 0, 128, 128, 128, 128};
                    sprites[1] = {}; sprites[1].flags = 2;
                    Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprites);
                }
                std::snprintf(label + 304, 32, "????");
                Game(rva::Font_PrintAsciiAt, sx - 40, sy - 48, label + 304);
            }
        }
        }
        for (int tile = 0; tile < ScouterTiles; ++tile) {
            const int index = MarkerCount + 2 + tile;
            if (!markerResources[index]) continue;
            auto* sprite = markerSprites + index * 2;
            const int x = (tile % ScouterColumns) * ScouterTileSize;
            const int y = (tile / ScouterColumns) * ScouterTileSize;
            const float aspect = ScouterAspectScale();
            // Hide only the left outline of the grey housing, retaining its body.
            // Keep the right edge anchored to its original position.
            constexpr float leftCrop = 6.0f;
            const auto frameX = [aspect, leftCrop](float px) {
                const float cropped = -leftCrop + px * (512.0f + leftCrop) / 512.0f;
                return static_cast<int>(std::lround(256.0f + (cropped - 256.0f) * aspect));
            };
            const int left = frameX(static_cast<float>(x));
            const int right = frameX(static_cast<float>(x + ScouterTileSize));
            sprite[0] = {1, left, y, right, y + ScouterTileSize, 0, 0, 64, 64, 0, 128, 128, 128, 128};
            sprite[1] = {}; sprite[1].flags = 2;
            Game(rva::Sprite_DrawList, markerResources[index], 0, 0, sprite);
        }
    }

    // The map remains north-up. Its source is a 5200-unit orthographic view
    // of the unscaled BT2 map; position is scaled by the actual world scale.
    if (adventureHudVisible && markerSprites && earthScale > 0.0f) {
        constexpr int mapX = 16, mapY = 304, mapSize = 128, tileSize = mapSize / 4;
        for (int tile = 0; tile < MinimapTileCount; ++tile) {
            const int id = MinimapBase + tile;
            if (!markerResources[id]) continue;
            auto* sprite = markerSprites + id * 2;
            const int x = mapX + (tile % 4) * tileSize, y = mapY + (tile / 4) * tileSize;
            sprite[0] = {1, x, y, x + tileSize, y + tileSize, 0, 0, 64, 64, 0, 128, 128, 128, 128};
            sprite[1] = {}; sprite[1].flags = 2;
            Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
        }
        if (adventureGoku || pilot.scouterActive) {
            for (int i = 0; i < MarkerCount; ++i) {
                if (!MissionAvailable(i)) continue;
                const int id = MissionDotBase + i;
                if (!markerResources[id]) continue;
                const float u = 0.5f + pilot.markerGoals[i].x / (5200.0f * earthScale);
                const float v = 0.5f - pilot.markerGoals[i].z / (5200.0f * earthScale);
                if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) continue;
                const int x = mapX + static_cast<int>(std::lround(u * mapSize));
                const int y = mapY + static_cast<int>(std::lround(v * mapSize));
                auto* sprite = markerSprites + id * 2;
                sprite[0] = {1, x - 4, y - 4, x + 4, y + 4, 0, 0, 64, 64, 0, 128, 128, 128, 128};
                sprite[1] = {}; sprite[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
            }
        }
        const float u = 0.5f + pilot.position.x / (5200.0f * earthScale);
        const float v = 0.5f - pilot.position.z / (5200.0f * earthScale);
        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
            const int sx = mapX + static_cast<int>(std::lround(u * mapSize));
            const int sy = mapY + static_cast<int>(std::lround(v * mapSize));
            int dir = static_cast<int>(std::lround(pilot.modelYaw * (MinimapArrowCount / 2.0f) / 3.14159265f));
            dir = (dir % MinimapArrowCount + MinimapArrowCount) % MinimapArrowCount;
            const int id = MinimapBase + MinimapTileCount + dir;
            if (markerResources[id]) {
                auto* sprite = markerSprites + id * 2;
                sprite[0] = {1, sx - 7, sy - 7, sx + 7, sy + 7, 0, 0, 64, 64, 0, 128, 128, 128, 128};
                sprite[1] = {}; sprite[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
            }
        }
    }
    if (characterMenuOpen && markerSprites) {
        const int base = SelectorBase + (kameHouseWon ? 16 : 0);
        const int left = 352, top = 270, size = 144, tileSize = size / 4;
        for (int i = 0; i < 16; ++i) {
            const int id = base + i; if (!markerResources[id]) continue;
            const int x = left + (i % 4) * tileSize, y = top + (i / 4) * tileSize;
            auto* sprite = markerSprites + id * 2;
            sprite[0] = {1,x,y,x+tileSize,y+tileSize,0,0,64,64,0,128,128,128,128};
            sprite[1] = {}; sprite[1].flags = 2;
            Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
        }
        if (markerResources[SelectorHighlight]) {
            const int x = left + (characterMenuChoice ? 72 : 0) + 3, y = top + 39;
            auto* sprite = markerSprites + SelectorHighlight * 2;
            sprite[0] = {1,x,y,x+66,y+66,0,0,64,64,0,128,128,128,128};
            sprite[1] = {}; sprite[1].flags = 2;
            Game(rva::Sprite_DrawList, markerResources[SelectorHighlight], 0, 0, sprite);
        }
    }
    if (pauseOpen) {
        if (markerSprites) {
            if (markerResources[FadeIndex]) {
                auto* shade = markerSprites + FadeIndex * 2;
                const int alpha = std::min(pauseFrames * 6, 72);
                shade[0] = {1,0,0,512,448,0,0,64,64,0,128,128,128,alpha};
                shade[1] = {}; shade[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResources[FadeIndex], 0, 0, shade);
            }
            for (int i = 0; i < 15; ++i) {
                const int id = PauseBase + i;
                if (!markerResources[id]) continue;
                auto* sprite = markerSprites + id * 2;
                const float t = std::clamp(pauseFrames / 15.0f, 0.0f, 1.0f);
                const float eased = 1.0f - (1.0f - t) * (1.0f - t);
                // Portrait content occupies x=16..176 within its 192px canvas.
                // Start outside the left edge and finish flush with the right.
                const int origin = static_cast<int>(std::lround(-176.0f + 512.0f * eased));
                const int x = origin + (i % 3) * 64;
                const int y = 128 + (i / 3) * 64;
                sprite[0] = {1,x,y,x+64,y+64,0,0,64,64,0,128,128,128,128};
                sprite[1] = {}; sprite[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
            }
            if (pauseSprites) for (int row = 0; row < 4; ++row) for (int tile = 0; tile < 4; ++tile) {
                const int elapsed = pauseFrames - (3 - row) * 3;
                if (elapsed <= 0) continue;
                if (pauseFrames >= 25 && row == pauseChoice) continue;
                const int id = PauseBars + tile;
                if (!markerResources[id]) continue;
                auto* sprite = pauseSprites + (row * 4 + tile) * 2;
                // Match the slim unselected strips in the original menu.
                const int x = PauseRowX(row) + tile * 240 / 4, y = PauseRowY(row) + 6;
                const int right = PauseRowX(row) + (tile + 1) * 240 / 4;
                const int alpha = std::clamp(elapsed * 16, 0, 128);
                sprite[0] = {1,x,y,right,y+22,0,0,64,64,0,128,128,128,alpha};
                sprite[1] = {}; sprite[1].flags = 2;
                Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
            }
            // The selected bar begins at the left edge of the viewport.
            if (pauseSprites && pauseFrames >= 25) {
                const int width = 388; // Approximately 25% wider than 310.
                for (int tile = 0; tile < 4; ++tile) {
                    const int left = width * tile / 4, right = width * (tile + 1) / 4;
                    const int id = PauseBars + 4 + tile;
                    if (!markerResources[id]) continue;
                    auto* sprite = pauseSprites + 32 + tile * 2;
                    const int y = PauseRowY(pauseChoice);
                    sprite[0] = {1,left,y,right,y+34,0,0,64,64,0,128,128,128,128};
                    sprite[1] = {}; sprite[1].flags = 2;
                    Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
                }
            }
            if (pauseSprites) for (int row = 0; row < 4; ++row) {
                const int elapsed = pauseFrames - (3 - row) * 3;
                if (elapsed <= 0) continue;
                const bool selected = pauseFrames >= 25 && row == pauseChoice;
                const int iconFrame = (pausePulseFrame / 6) % (row == 2 ? 4 : 5);
                const int iconId = PauseIcons + row * 5 + iconFrame;
                if (markerResources[iconId]) {
                    auto* icon = markerSprites + iconId * 2;
                    const int size = selected ? 44 : 34;
                    const int x = PauseRowX(row) - 4 - (size - 34) / 2;
                    const int y = PauseRowY(row) - 1 - (size - 34) / 2;
                    icon[0] = {1,x,y,x+size,y+size,0,0,64,64,0,128,128,128,std::clamp(elapsed * 16, 0, 128)};
                    icon[1] = {}; icon[1].flags = 2;
                    Game(rva::Sprite_DrawList, markerResources[iconId], 0, 0, icon);
                }
                const int textWidth = selected ? 220 : 200;
                const int textHeight = selected
                    ? 56 + static_cast<int>(std::lround(5.0f * std::sin(pausePulseFrame * 0.22f)))
                    : 50;
                for (int tile = 0; tile < 4; ++tile) {
                    const int id = PauseText + (selected ? 16 : 0) + row * 4 + tile;
                    if (!markerResources[id]) continue;
                    auto* sprite = pauseSprites + 40 + (row * 4 + tile) * 2;
                    const int left = PauseRowX(row) + 32 + tile * textWidth / 4;
                    const int right = PauseRowX(row) + 32 + (tile + 1) * textWidth / 4;
                    const int y = PauseRowY(row) + 17 - textHeight / 2;
                    const int alpha = std::clamp(elapsed * 16, 0, 128);
                    sprite[0] = {1,left,y,right,y+textHeight,0,0,64,64,0,128,128,128,alpha};
                    sprite[1] = {}; sprite[1].flags = 2;
                    Game(rva::Sprite_DrawList, markerResources[id], 0, 0, sprite);
                }
            }
        }
        char* description = label + 368;
        char* title = label + 448;
        const char* descriptions[] = {"CONTINUA LA PARTIDA", "REGRESA A LA SELECCION DE MISION", "REGRESA A LA SELECCION DE SAGA", "REGRESA AL MENU PRINCIPAL"};
        std::snprintf(description, 80, "%s", descriptions[pauseChoice]);
        std::snprintf(title, 32, "PAUSA");
        Game(rva::Font_SetColor, 0x80FFFFFFu);
        Game(rva::Font_SetScale, AsWord(0.8f));
        Game(rva::Font_PrintAsciiAt, 30, 65, title);
        if (pauseFrames >= 25) {
            Game(rva::Font_SetScale, AsWord(0.6f));
            Game(rva::Font_PrintAsciiAt, 30, 350, description);
        }
    }
    if (adminOpen) {
        // Text is kept in low memory until the native font queue is flushed.
        char* header = label + 112;
        char* row1 = label + 176;
        char* row2 = label + 240;
        std::snprintf(header, 64, "ADMIN F8: CERRAR  ARRIBA/ABAJO: ELEGIR  ESPACIO: CAMBIAR");
        std::snprintf(row1, 64, "%s [%c] PICCOLO VS RADITZ", adminSelection == 0 ? ">" : " ", piccoloWon ? 'X' : ' ');
        std::snprintf(row2, 64, "%s [%c] RADITZ VS GOKU", adminSelection == 1 ? ">" : " ", kameHouseWon ? 'X' : ' ');
        Game(rva::Font_SetColor, 0x80FFFFFFu);
        Game(rva::Font_SetScale, AsWord(0.7f));
        Game(rva::Font_PrintAsciiAt, 20, 55, header);
        Game(rva::Font_PrintAsciiAt, 20, 80, row1);
        Game(rva::Font_PrintAsciiAt, 20, 104, row2);
    }
    if (pilot.arrived && pilot.arriveFrames >= 15 && markerSprites && markerResources[FadeIndex]) {
        const int alpha = std::clamp((pilot.arriveFrames - 15) * 128 / 25, 0, 128);
        auto* sprite = markerSprites + FadeIndex * 2;
        sprite[0] = {1, 0, 0, 512, 448, 0, 0, 64, 64, 0, 128, 128, 128, alpha};
        sprite[1] = {}; sprite[1].flags = 2;
        Game(rva::Sprite_DrawList, markerResources[FadeIndex], 0, 0, sprite);
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
    DrawColumns3D(pilot);
    Game(rva::Ot_Draw);
    Game(rva::Gfx_MarkPass, 3);
    Game(rva::StgFx_DrawPost);
    Game(rva::Gfx_MarkPass, 0);
    DrawMarker(pilot, overlayText);
    Game(rva::Gfx_MarkPass, 3);
    Game(rva::StgFx_DrawOverlay);
    Game(rva::Gfx_MarkPass, 0);
}

bool AdventureCycle(bool singleActor) {
    if (!Game<int>(rva::BtlStage_IsReady)) return false;
    auto* fighter = Game<void*>(rva::BtlChar_Get, 0);
    if (!fighter) return false;
    auto* avatar = Game<void*>(rva::BtlChar_GetObj, fighter);
    if (!avatar) return false;
    auto* opponent = singleActor ? nullptr : Game<void*>(rva::BtlChar_Get, 1);
    auto* rival = opponent ? Game<void*>(rva::BtlChar_GetObj, opponent) : nullptr;

    Pilot pilot;
    // Adventure limits are independent of the template arena's invisible bounds.
    pilot.radius = 2850.0f * earthScale;
    pilot.ceiling = -700.0f * earthScale;
    pilot.floor = 0.0f; // Sea-level fallback where the terrain has no triangles.

    // Coordenadas propias del mapa de aventura: punto de aparición y destino.
    // Crater identified from the BT2 Earth map; coordinates use original BT2 scale.
    pilot.position = { 1452.0f * earthScale, 0.0f, 650.0f * earthScale };
    pilot.goal = { 806.0f * earthScale, 0.0f, 278.0f * earthScale };
    pilot.markerGoals[0] = pilot.goal;
    pilot.markerGoals[1] = { 1452.0f * earthScale, 0.0f, -1218.0f * earthScale };
    // Position the final encounter in front of the pod, clear of its geometry.
    pilot.markerGoals[2] = { 1352.0f * earthScale, 0.0f, 650.0f * earthScale };
    for (auto& marker : pilot.markerGoals) {
        marker.y = std::max(pilot.ceiling, GroundY(marker.x, marker.z, pilot.floor) - 45.0f);
    }
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
    if (resumeAtEncounter) pilot.position = resumePosition;
    else if (kameHouseWon) pilot.position = pilot.markerGoals[1];
    const float headingToGoal = std::atan2(pilot.goal.x - pilot.position.x, pilot.goal.z - pilot.position.z);
    if (!std::isfinite(headingToGoal)) return false;
    pilot.heading = pilot.modelYaw = resumeAtEncounter ? resumeHeading : headingToGoal;

    auto* overlayText = Game<char*>(rva::Port_LowAlloc, std::size_t{480});
    if (!overlayText) return false;

    const bool hasNpc = !singleActor && adventure_loader::CompanionSlot() >= 0;
    const bool hasCompanion = adventureGoku && hasNpc;
    const bool showNpc = hasNpc && (adventureGoku || !piccoloWon);
    if (rival) Field<int>(rival, 8) = showNpc ? 1 : 0;
    Pilot follower = pilot; Pilot target;
    bool companionGlow = false;
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
        UpdateAdminPanel();
        const auto menuHeld = Field<std::uint32_t>(pad, 0x188);
        const auto pausePressed = Field<std::uint32_t>(pad, 0x18C);
        if (!adminOpen && !characterMenuOpen && !pilot.arrived && (pausePressed & Start)) {
            pauseOpen = !pauseOpen;
            if (pauseOpen) { pauseChoice = 0; pauseFrames = 0; pausePulseFrame = 0; }
            pauseNoticeFrames = 0;
        }
        const bool pauseWasOpen = pauseOpen;
        if (pauseOpen) {
            pauseFrames = std::min(pauseFrames + 1, 60);
            pausePulseFrame = (pausePulseFrame + 1) % 1800;
            if (pausePressed & Up) pauseChoice = (pauseChoice + 3) % 4;
            if (pausePressed & Down) pauseChoice = (pauseChoice + 1) % 4;
            if (pausePressed & Cross) {
                pauseDestination = pauseChoice - 1; // Reanudar has no destination.
                pauseNotice = pauseChoice;
                pauseNoticeFrames = 0;
                pauseOpen = false;
                FlightLog("Pause destination requested", pauseChoice);
            } else if (pausePressed & Circle) pauseOpen = false;
        } else if (pauseNoticeFrames > 0) --pauseNoticeFrames;
        if (!pauseOpen && !pauseWasOpen && !adminOpen && !pilot.arrived && (menuHeld & Circle)) {
            if (!characterMenuOpen) { characterMenuOpen = true; characterMenuChoice = adventureGoku; }
            const float horizontal = Field<float>(pad, 0x1A4);
            if ((menuHeld & Left) || horizontal < -0.5f) characterMenuChoice = false;
            if (kameHouseWon && ((menuHeld & Right) || horizontal > 0.5f)) characterMenuChoice = true;
        } else if (characterMenuOpen) {
            characterMenuOpen = false;
            if (characterMenuChoice != adventureGoku && kameHouseWon) {
                adventureGoku = characterMenuChoice;
                adminReload = true;
                FlightLog("Adventure character switched", adventureGoku ? 1 : 0);
            }
        }
        if (adminReload) {
            resumePosition = pilot.position;
            resumeHeading = pilot.heading;
            resumeAtEncounter = true;
            break;
        }
        const Vec previousPosition = pilot.position;
        const float ground = (adminOpen || characterMenuOpen || pauseOpen) ? GroundY(pilot.position.x, pilot.position.z, pilot.floor) : Move(pilot, pad);
        SetPilotPose(pilot, fighter, avatar, ground);
        if (opponent && rival && hasCompanion) {
            const int previousMotion = follower.motion;
            follower = pilot; follower.motion = previousMotion;
            // Half of the player's collision width is its body radius.
            const float bodyRadius = Field<float>(avatar, 0x1004);
            const float behind = std::isfinite(bodyRadius) && bodyRadius > 0.0f && bodyRadius < 50.0f ? bodyRadius : 6.0f;
            follower.position.x += 17.5f * std::cos(pilot.modelYaw) - behind * std::sin(pilot.modelYaw);
            follower.position.z -= 17.5f * std::sin(pilot.modelYaw) + behind * std::cos(pilot.modelYaw);
            SetPilotPose(follower, opponent, rival, ground);
            if (frames == 1) FlightLog("Companion follows player");
        }
        if (opponent && rival && showNpc && !hasCompanion) {
            target.position = piccoloWon ? pilot.markerGoals[1] : pilot.markerGoals[0];
            target.position.y = GroundY(target.position.x, target.position.z, 0.0f);
            target.heading = target.modelYaw = std::atan2(pilot.position.x - target.position.x, pilot.position.z - target.position.z);
            SetPilotPose(target, opponent, rival, target.position.y);
        }
        // Keep the target alive throughout the Adventure session. Destroying
        // and recreating replay Piccolo on distance crossings can corrupt his pose.
        if (piccoloWon && adventure_loader::ReplayPiccoloSlot() >= 0) {
            const int wanted = adventureGoku ? 2 : 1;
            if (wanted != worldTargetKind) {
                if (worldTargetId >= 0) Game<int>(0x69130, worldTargetId);
                worldTargetObject = nullptr; worldTargetId = -1; worldTargetKind = 0;
                const int slot = wanted == 1 ? adventure_loader::ReplayPiccoloSlot() : wanted == 2 ? adventure_loader::TargetSlot() : -1;
                if (slot >= 0) {
                    worldTargetId = Game<int>(0x691C0, slot);
                    if (worldTargetId >= 0) worldTargetObject = Game<void*>(0x68FC0, worldTargetId);
                    if (worldTargetObject) {
                        Game(0x8930, worldTargetObject, 0, 0, 1);
                        Field<int>(worldTargetObject, 0xC70) = 0;
                        Field<float>(worldTargetObject, 0xA20) = 1.0f;
                        worldTargetKind = wanted;
                    }
                }
            }
        }
        if (worldTargetObject) {
            Vec pos = worldTargetKind == 1 ? pilot.markerGoals[0] : pilot.markerGoals[2];
            // Place the mission character at the map's ground height.
            pos.y = GroundY(pos.x, pos.z, pilot.floor);
            Vec rot{0, std::atan2(pilot.position.x - pos.x, pilot.position.z - pos.z), 0};
            Field<Vec>(worldTargetObject, 0x950) = pos;
            Field<Vec>(worldTargetObject, 0x960) = rot;
            Field<std::uint32_t>(worldTargetObject, 0xA40) |= 2;
            Game(rva::BtlObjXf_Update, worldTargetObject);
        }
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
        if (rival && showNpc) { Field<std::uint32_t>(rival, 0xA40) |= 2; Field<int>(rival, 0xC70) = 0; Field<float>(rival, 0xA20) = 1.0f; }
        if (rival && !showNpc) { Field<std::uint32_t>(rival, 0xA40) &= ~std::uint32_t{2}; Field<int>(rival, 8) = 0; }
        avatarFlags |= 2;  // BTL_OBJ_FLAG_VISIBLE
        avatarNoAnim = 0;
        Game(rva::BtlObj_UpdateAll);
        Game(rva::BtlObj_UpdateVisibility, 0);
        Game(rva::BtlObj_FinishVisibility, 0);
        if (pilot.fastFlight && movedLength > 0.5f) {
            Game<int>(rva::EftSpdLine_SpawnBodyTrails, Field<int>(fighter, 0x0C));
        }
        if (opponent && rival && hasCompanion && follower.fastFlight && movedLength > 0.5f)
            Game<int>(rva::EftSpdLine_SpawnBodyTrails, Field<int>(opponent, 0x0C));
        if (opponent && rival && hasCompanion) {
            const bool dashLoop = follower.motion == DragonDashLoop;
            if (dashLoop != companionGlow) {
                companionGlow = dashLoop;
                Game<int>(rva::EftGlow_Request, Field<int>(opponent, 0x0C), dashLoop ? 0 : 2);
            }
        }
        UpdateDashEffects(pilot, fighter);
        Draw(pilot, overlayText);
        Game(rva::Gfx_EndFrame, 2);
        Game(rva::Dma_Flush);

        if (pauseDestination >= 0) break;
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

    if (pilot.arrived) { resumePosition = pilot.position; resumeHeading = pilot.heading; resumeAtEncounter = true; }
    Game(rva::Port_LowFree, overlayText);
    FlightLog("Adventure cycle ended");
    return pilot.arrived;
}
} // namespace

// Temporary changes are applied only around the original loader/unloader and
// restored in reverse order, including on a failed signature check.
class AdventurePatchScope {
    struct Entry { unsigned char* address; unsigned char original[5]; std::size_t size; DWORD protection; };
    Entry entries[6]{};
    int count = 0;
public:
    bool apply(std::uintptr_t rva, std::initializer_list<unsigned char> expected,
               std::initializer_list<unsigned char> replacement) {
        if (count == 6 || expected.size() != replacement.size() || expected.size() > 5) return false;
        auto* address = reinterpret_cast<unsigned char*>(ImageBase() + rva);
        if (std::memcmp(address, expected.begin(), expected.size()) != 0) return false;
        DWORD protection = 0;
        if (!VirtualProtect(address, expected.size(), PAGE_EXECUTE_READWRITE, &protection)) return false;
        auto& entry = entries[count++];
        entry.address = address; entry.size = expected.size(); entry.protection = protection;
        std::memcpy(entry.original, address, entry.size);
        std::memcpy(address, replacement.begin(), entry.size);
        FlushInstructionCache(GetCurrentProcess(), address, entry.size);
        return true;
    }
    void restore() {
        while (count) {
            auto& entry = entries[--count];
            std::memcpy(entry.address, entry.original, entry.size);
            FlushInstructionCache(GetCurrentProcess(), entry.address, entry.size);
            DWORD ignored = 0;
            VirtualProtect(entry.address, entry.size, entry.protection, &ignored);
        }
    }
    ~AdventurePatchScope() { restore(); }
};

// The native unloader assumes ten member buffers on each side. Skip just the
// unused side; if patching is unavailable, allocate temporary placeholders so
// the unmodified unloader can safely free them instead of freeing null pointers.
static void UnloadAdventure(void* work, bool skippedBuffers, int unusedSide) {
    AdventurePatchScope patch;
    bool skippedCleanup = false;
    if (skippedBuffers) {
        if (unusedSide == 0)
            skippedCleanup = patch.apply(0x455D, {0xE8,0xCE,0x26,0x00,0x00}, {0xE9,0xE1,0x00,0x00,0x00});
        else
            skippedCleanup = patch.apply(0x4643, {0xE8,0xE8,0x25,0x00,0x00}, {0xE9,0xE1,0x00,0x00,0x00});
        if (!skippedCleanup) {
            constexpr std::uintptr_t Heap_Alloc = 0x430310;
            auto* side = static_cast<std::uint8_t*>(work) + 0xC0 + unusedSide * 0x270;
            for (int i = 0; i < 5; ++i) {
                for (int n = 0; n < 2; ++n) {
                    const auto buffer = Game<std::uint32_t>(Heap_Alloc, 0x1000, 0x40, 0, 2);
                    if (!buffer) { FlightLog("Adventure cleanup could not allocate fallback buffers"); return; }
                    Field<std::uint32_t>(side, 0x60 + i * 0x64 + n * 4) = buffer;
                }
            }
            FlightLog("Adventure cleanup used temporary fallback buffers");
        }
    }
    Game<int>(rva::Battle_Unload);
    if (skippedCleanup) FlightLog("Adventure skipped unused-side buffer cleanup");
}


static bool RunAdventureOnce() {
    LoadMarkers();
    if (!earthCollision.load("earth-collision.bin")) {
        FlightLog("Earth collision data unavailable");
        FreeMarkers();
        return false;
    }
    FlightLog("Earth collision data loaded", static_cast<int>(earthCollision.triangles.size()));
    void* stage = LoadEarthStage();
    if (!stage) {
        FlightLog("BT2 Earth stage unavailable");
        FreeMarkers();
        return false;
    }

    // The mission setup is already selected. Preserve it across this temporary load.
    auto* work = Game<void*>(rva::Battle_GetWork);
    std::uint8_t savedWork[0x1A00];
    std::memcpy(savedWork, work, sizeof(savedWork));
    Field<std::uint64_t>(work, 0x19F0) = 0;

    FlightLog("Adventure loading before Battle_Init", 0, earthScale);
    // v0.1.39.6 Battle_Load creates two fighter objects after loading. Run its
    // resource setup separately, retaining the native story script and loader,
    // then create only the actor selected below. Keep the original load path
    // for an executable whose expected code is not present.
    constexpr unsigned char loadSignature[] = {0x48, 0x83, 0xEC, 0x28, 0xB9, 0x01, 0x00, 0x00, 0x00, 0xE8, 0xA2, 0x2E, 0x06, 0x00};
    const bool ownActorLoad = std::memcmp(reinterpret_cast<const void*>(ImageBase() + rva::Battle_Load),
                                          loadSignature, sizeof(loadSignature)) == 0;
    bool skippedOtherModel = false;
    bool skippedOtherBuffers = false;
    bool ownResourceJob = false;
    int unusedBufferSide = adventureGoku ? 1 : 0;
    if (ownActorLoad) {
        constexpr std::uintptr_t BtlObj_Init = 0x69B10;
        constexpr std::uintptr_t Gsc_InitDefault = 0x42E350;
        constexpr std::uintptr_t gBtlScriptCommands = 0x64A7A0;
        constexpr std::uintptr_t BtlScript_Init = 0x6DEC0;
        constexpr std::uintptr_t BtlLoad_PushInitialJob = 0x43F0;
        constexpr std::uintptr_t Load_RunBlocking = 0x4335D0;
        Game(BtlObj_Init, 1);
        const auto commands = *reinterpret_cast<std::uintptr_t*>(ImageBase() + gBtlScriptCommands);
        Game(Gsc_InitDefault, reinterpret_cast<void*>(commands));
        Game(BtlScript_Init);
        ownResourceJob = adventure_loader::Install(adventureGoku, piccoloWon);
        if (!ownResourceJob) {
            FlightLog("Adventure own job unavailable; using native resource job");
            Game(BtlLoad_PushInitialJob);
        }
        Game(Load_RunBlocking);
        if (ownResourceJob) {
            adventure_loader::Finish();
            if (!adventure_loader::Succeeded()) {
                FlightLog("Adventure own resource job failed");
                adventure_loader::Unload(work, unusedBufferSide);
                Game(rva::Port_LowFree, stage);
                std::memcpy(work, savedWork, sizeof(savedWork)); FreeMarkers();
                return false;
            }
            skippedOtherModel = true; skippedOtherBuffers = true;
        }
        FlightLog("Adventure resources loaded without battle fighter objects");
    } else {
        FlightLog("Adventure custom loader unavailable; using native loader");
        Game<int>(rva::Battle_Load);
    }
    std::memcpy(encounterSetup, work, sizeof(encounterSetup));
    // Swap temporary sides after the story script has populated and loaded them.
    // Keep the player's input assignment; the complete setup is restored on exit.
    auto* side0 = static_cast<std::uint8_t*>(work) + 0xC0;
    auto* side1 = side0 + 0x270;
    constexpr int RaditzCharacter = 77;
    if (Field<int>(side1, 4) != RaditzCharacter) {
        FlightLog("Raditz not found in mission setup; adventure cancelled");
        if (ownResourceJob) adventure_loader::Unload(work, unusedBufferSide);
        else UnloadAdventure(work, skippedOtherBuffers, unusedBufferSide);
        Game(rva::Port_LowFree, stage);
        std::memcpy(work, savedWork, sizeof(savedWork));
        FreeMarkers();
        return false;
    }
    std::uint8_t tempSide[0x270];
    const int playerPad = Field<int>(side0, 0x204);
    const int playerControl = Field<int>(side0, 0x208);
    if (!adventureGoku) {
        std::memcpy(tempSide, side0, sizeof(tempSide));
        std::memcpy(side0, side1, sizeof(tempSide));
        std::memcpy(side1, tempSide, sizeof(tempSide));
        unusedBufferSide = 1 - unusedBufferSide;
    }
    const bool companionEnabled = ownResourceJob &&
                                  adventure_loader::CompanionSlot() >= 0 && adventure_loader::CompanionParam();
    if (companionEnabled) {
        const int storyGokuCostume = Field<int>(side1, 0x254);
        std::memcpy(side1, side0, 0x270);
        const int npc = adventureGoku || !piccoloWon ? 0x16 : 0;
        const int costume = npc == 0x16 ? 1 : storyGokuCostume;
        Field<int>(side1, 4) = npc; Field<int>(side1, 8) = costume;
        Field<int>(side1, 0x250) = npc; Field<int>(side1, 0x254) = costume;
        Field<int>(side1, 0x25C) = npc; Field<int>(side1, 0x260) = costume;
        Field<std::uint32_t>(side1, 0x60) = adventure_loader::CompanionParam();
        Field<int>(side1, 0x26C) = adventure_loader::CompanionSlot();
        skippedOtherModel = false; FlightLog("Adventure second character setup");
    }
    Field<int>(side0, 0x204) = playerPad;
    Field<int>(side0, 0x208) = playerControl;
    FlightLog(adventureGoku ? "Goku selected for Adventure" : "Raditz selected for Adventure", Field<int>(side0, 4));
    const auto arena = std::exchange(StageResource(),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(stage)));

    // Set up the presentation resources needed for flight. Battle_Main starts
    // the story script, sequence, and AI after this hook returns.
    Game(rva::StgFx_Init);
    Game(rva::StgModel_InitStage);

    // BT3 v0.1.39.6 hardcodes two characters in BtlScene_Init. Temporarily
    // initialize its effects for one actor; restore the instruction before
    // entering any real battle. Fall back safely on an unrecognized build.
    auto* countInstruction = reinterpret_cast<unsigned char*>(ImageBase() + 0x6CA8C);
    constexpr unsigned char expectedCount[] = {0xC7, 0x00, 0x02, 0x00, 0x00, 0x00};
    DWORD oldProtection = 0;
    bool singleActor = !companionEnabled && std::memcmp(countInstruction, expectedCount, sizeof(expectedCount)) == 0 &&
                       VirtualProtect(countInstruction, sizeof(expectedCount), PAGE_EXECUTE_READWRITE, &oldProtection);
    if (singleActor && !ownActorLoad) {
        constexpr std::uintptr_t BtlObj_Destroy = 0x69130;
        if (Game<int>(BtlObj_Destroy, Field<int>(side1, 0x268)) == 0) {
            singleActor = false;
            DWORD ignored = 0;
            VirtualProtect(countInstruction, sizeof(expectedCount), oldProtection, &ignored);
            FlightLog("Adventure could not release opponent; using compatibility mode");
        }
    }
    if (singleActor) {
        countInstruction[2] = 1;
        FlushInstructionCache(GetCurrentProcess(), countInstruction, sizeof(expectedCount));
    } else FlightLog("Adventure using multiple actors or compatibility mode");

    if (ownActorLoad && skippedOtherModel && !singleActor) {
        FlightLog("Adventure cannot initialize one actor safely; stopping exploration");
        Game(rva::StgFx_Term);
        Game(rva::BtlStage_Term);
        StageResource() = arena;
        Game(rva::Port_LowFree, stage);
        if (ownResourceJob) adventure_loader::Unload(work, unusedBufferSide);
        else UnloadAdventure(work, skippedOtherBuffers, unusedBufferSide);
        std::memcpy(work, savedWork, sizeof(savedWork));
        FreeMarkers();
        return false;
    }
    if (ownActorLoad) {
        constexpr std::uintptr_t BtlObj_CreateChara = 0x691C0;
        Field<int>(side0, 0x268) = Game<int>(BtlObj_CreateChara, Field<int>(side0, 0x26C));
        if (!singleActor) Field<int>(side1, 0x268) = Game<int>(BtlObj_CreateChara, Field<int>(side1, 0x26C));
        FlightLog(singleActor ? "Adventure created only its playable actor" : "Adventure created two actors");
    }
    Game(rva::BtlChar_AllocAll, singleActor ? 1 : 2);
    Game(rva::BtlCam_Init);
    Game(rva::Ot_Init);
    Game(rva::BtlScene_Init, 0);
    if (companionEnabled && adventureGoku && adventure_loader::TargetSlot() >= 0) {
        const int id = Game<int>(0x691C0, adventure_loader::TargetSlot());
        worldTargetId = id;
        worldTargetKind = 2;
        worldTargetObject = Game<void*>(0x68FC0, id);
        if (worldTargetObject) {
            Game(0x8930, worldTargetObject, 0, 0, 1);
            Field<int>(worldTargetObject, 0xC70) = 0;
            Field<float>(worldTargetObject, 0xA20) = 1.0f;
            FlightLog("World Raditz target created", id);
        }
    }
    if (singleActor) {
        countInstruction[2] = 2;
        FlushInstructionCache(GetCurrentProcess(), countInstruction, sizeof(expectedCount));
        DWORD ignored = 0;
        VirtualProtect(countInstruction, sizeof(expectedCount), oldProtection, &ignored);
    }
    Game(rva::StgFx_Reset);
    Game(rva::Fade_ResetAll);

    const bool arrived = AdventureCycle(singleActor);
    FreeMarkers();

    worldTargetObject = nullptr; worldTargetId = -1; worldTargetKind = 0;
    Game(rva::BtlChar_FreeAll);
    Game(rva::BtlCam_Term);
    Game(rva::BtlScene_Term);
    Game(rva::Ot_Term);
    Game(rva::StgFx_Term);
    Game(rva::BtlStage_Term);
    StageResource() = arena; // Battle_Unload owns the arena buffer, not our map.
    Game(rva::Port_LowFree, stage);
    if (ownResourceJob) adventure_loader::Unload(work, unusedBufferSide);
    else UnloadAdventure(work, skippedOtherBuffers, unusedBufferSide);
    Game(rva::Fade_ResetAll);
    std::memcpy(work, savedWork, sizeof(savedWork));
    FlightLog(arrived ? (confirmedEncounter == Encounter::Piccolo ? "Adventure confirmed Piccolo encounter" : confirmedEncounter == Encounter::RaditzFinal ? "Adventure confirmed final Raditz encounter" : "Adventure confirmed Kame House encounter") : "Adventure ended before confirmation");
    return arrived;
}

bool RunRaditzAdventure() {
    piccoloWon = LoadVictory(L"piccolo_victory");
    kameHouseWon = piccoloWon && LoadVictory(L"kame_house_victory");
    adventureGoku = kameHouseWon;
    characterMenuOpen = false;
    pauseDestination = -1;
    pauseOpen = false;
    confirmedEncounter = Encounter::None;
    resumeAtEncounter = false;
    auto* work = Game<void*>(rva::Battle_GetWork);
    std::uint8_t original[0x1A00];
    std::memcpy(original, work, sizeof(original));
    bool startNativeBattle = false;
    for (;;) {
        adminReload = false;
        const bool arrived = RunAdventureOnce();
        if (pauseDestination >= 0) {
            // This executable's mission hook checks gProgress at RVA 0x64b490.
            // Dragon History modes: episode list 7, saga select 6, main menu 4.
            const auto progressSlot = *reinterpret_cast<std::uint32_t**>(ImageBase() + 0x64b490);
            const auto raw = progressSlot ? *progressSlot : 0;
            if (raw) {
                auto* progress = reinterpret_cast<void*>(static_cast<std::uintptr_t>(raw));
                const int modes[] = {7, 6, 4};
                Field<int>(progress, 0x18) = modes[pauseDestination];
                FlightLog("Pause requested menu mode", modes[pauseDestination]);
            } else FlightLog("Pause failed: progress pointer unavailable");
            break;
        }
        if (adminReload) {
            confirmedEncounter = Encounter::None;
            continue;
        }
        if (!arrived) break;
        if (confirmedEncounter == Encounter::RaditzFinal) {
            startNativeBattle = true;
            FlightLog("Adventure yielding to native Raditz story battle");
            break;
        }
        // Start a separate 1P-versus-CPU match with Piccolo (Early) and Raditz.
        std::memset(work, 0, sizeof(original));
        std::memcpy(work, encounterSetup, sizeof(encounterSetup));
        Field<int>(work, 0xBC) = 0;  // no story script replaces this setup
        Field<int>(work, 0x08) = 2;  // 1P battle mode: exits after the result without a versus menu
        Field<int>(work, 0x24) = 0;  // single screen
        auto* side0 = static_cast<std::uint8_t*>(work) + 0xC0;
        auto* side1 = side0 + 0x270;
        constexpr int PiccoloEarly = 0x16;
        if (confirmedEncounter == Encounter::KameHouse) {
            // Preserve the original mission character, costume and form on both sides.
            std::uint8_t tempSide[0x270];
            const int playerPad = Field<int>(side0, 0x204);
            std::memcpy(tempSide, side0, sizeof(tempSide));
            std::memcpy(side0, side1, sizeof(tempSide));
            std::memcpy(side1, tempSide, sizeof(tempSide));
            Field<int>(side0, 0) = Field<int>(side1, 0) = 1;
            Field<int>(side0, 0x1F8) = Field<int>(side1, 0x1F8) = 0;
            Field<int>(side0, 0x204) = playerPad;
            Field<int>(side0, 0x208) = 0;
            Field<int>(side1, 0x208) = 2;
            constexpr int KameHouseStage = 12;
            Field<int>(work, 0x1C) = KameHouseStage;
            Field<int>(work, 0x28) = KameHouseStage;
            FlightLog("Kame House Raditz vs Goku battle starting", Field<int>(side1, 4), static_cast<float>(Field<int>(side1, 8)));
        } else {
        Field<int>(side0, 0) = 1;
        Field<int>(side0, 4) = PiccoloEarly;
        Field<int>(side0, 8) = 1;
        Field<int>(side0, 12) = 0;
        Field<int>(side0, 0x1F8) = 0;
        Field<int>(side0, 0x208) = 0;
        Field<int>(side0, 0x250) = PiccoloEarly;
        Field<int>(side0, 0x254) = 1;
        Field<int>(side0, 0x258) = 0;
        Field<int>(side0, 0x25C) = PiccoloEarly;
        Field<int>(side0, 0x260) = 1;
        Field<int>(side0, 0x264) = 0;
        Field<int>(side1, 0x208) = 2;
        FlightLog("Piccolo vs Raditz battle starting", PiccoloEarly);
        }
        // v0.1.39.6: the native clock uses this table of preset seconds.
        // Temporarily replace the 45-second preset with 40 and restore it afterwards.
        auto* timeLimits = reinterpret_cast<std::int16_t*>(ImageBase() + 0x4765C0);
        const bool knownTimeTable = timeLimits[0] == -1 && timeLimits[1] == 60 &&
                                    timeLimits[2] == 90 && timeLimits[3] == 180 &&
                                    timeLimits[4] == 240 && timeLimits[5] == 45;
        if (confirmedEncounter == Encounter::KameHouse) {
            Field<int>(work, 0x10) = 2; // native 90-second preset
        } else if (knownTimeTable) {
            timeLimits[5] = 40;
            Field<int>(work, 0x10) = 5;
        } else {
            Field<int>(work, 0x10) = 5;
            FlightLog("Unexpected time-limit table; using native 45-second preset");
        }
        Game<int>(rva::Battle_Main);
        if (confirmedEncounter == Encounter::Piccolo && knownTimeTable) timeLimits[5] = 45;
        const auto winner = Field<std::uint32_t>(work, 0x1938);
        const auto reason = Field<std::uint32_t>(work, 0x193C);
        FlightLog(confirmedEncounter == Encounter::KameHouse ? "Kame House battle result" : "Piccolo battle result", static_cast<int>(winner), static_cast<float>(reason));
        if ((winner & 0xFu) == 1u && !(reason & 0x18000u)) {
            if (confirmedEncounter == Encounter::KameHouse) {
                kameHouseWon = true;
                adventureGoku = true;
                if (!SaveVictory(L"kame_house_victory")) FlightLog("Unable to save Kame House victory");
                else FlightLog("Kame House victory saved");
            } else {
                piccoloWon = true;
                if (!SaveVictory(L"piccolo_victory")) FlightLog("Unable to save Piccolo victory");
                else FlightLog("Piccolo victory saved");
            }
        }
        confirmedEncounter = Encounter::None;
        std::memcpy(work, original, sizeof(original));
    }
    std::memcpy(work, original, sizeof(original));
    FlightLog(startNativeBattle ? "Adventure dispatcher requested native battle" : "Adventure dispatcher returned to menu");
    return startNativeBattle;
}

// Compatibility with the existing executable's two battle call-site hooks.
extern "C" __declspec(dllexport) void RaditzInitStage() {
    Game(rva::StgModel_InitStage);
}

extern "C" __declspec(dllexport) void RaditzBattleLoop() {
    Game(rva::Battle_Loop);
}


