#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "bt3_runtime_addresses.h"
#include "adventure_loader.h"
namespace {
std::uintptr_t ImageBase() {
    static const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    return base;
}
template <class Return = void, class... Args> Return Game(std::uintptr_t rva, Args... args) {
    return reinterpret_cast<Return (*)(Args...)>(ImageBase() + rva)(args...);
}
template <class T> T &Field(void *p, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<char *>(p) + offset);
}
void FlightLog(const char *message, int frames = 0, float value = 0) {
    FILE *file = nullptr;
    if (fopen_s(&file, "adventure-flight.log", "a") == 0) {
        std::fprintf(file, "%s | frames=%d | value=%.2f\n", message, frames, value);
        std::fclose(file);
    }
}
// Experimental loader: exactly one actor; the original story script is resolved first.
static int adventureNativeSide = 0;
static bool adventureLoadSucceeded = false;
static DWORD adventureThunkProtection = 0;
static void *adventureJobThunk = nullptr;
static int AdventureInitialJob(void *job) {
    int &state = Field<int>(job, 8);
    auto *work = Game<void *>(rva::Battle_GetWork);
    auto *side = static_cast<std::uint8_t *>(work) + 0xC0 + 0x270 * adventureNativeSide;
    auto *res = reinterpret_cast<void *>(
        static_cast<std::uintptr_t>(*reinterpret_cast<std::uint32_t *>(ImageBase() + rva::gCommonRes)) + 0x20);
    auto &common = Field<std::uint32_t>(res, 0x18);
    auto &stageSnd = Field<std::uint32_t>(res, 0x1C);
    auto &voice = Field<std::uint32_t>(res, 0x20);
    auto request = [](int id, std::uint32_t buf, int size) {
        return Game<std::uint32_t>(0x4168E0, id, buf, size);
    };
    auto alloc = [](int size) { return Game<std::uint32_t>(0x430310, size, 0x40, 0, 2); };
    auto zero = [](std::uint32_t address, int size) {
        std::memset(reinterpret_cast<void *>(static_cast<std::uintptr_t>(address)), 0, size);
    };
    switch (state) {
    case 0x5A:
        Field<std::uint32_t>(res, 0x10) = request(Field<int>(job, 0x0C) + 0x1FF, 0, 0);
        ++state;
        return 0;
    case 0x5B: {
        if (!Game<int>(0x4169C0))
            return 0;
        void *events = Game<void *>(0x6BF0);
        int script = Game<int>(0x42EAF0, Field<std::uint32_t>(res, 0x10));
        Field<int>(events, 0) = script;
        Game(0x42EF60, script, 1000);
        Game(0x6DF60, script);
        Game(0x6DF50);
        Game(0x5FB0, 0);
        state = 0;
        FlightLog("Adventure own loader story setup completed");
        return 0;
    }
    case 0: {
        int chara = Field<int>(side, 0x250);
        int stage = Field<int>(work, 0x1C);
        auto saveRef = *reinterpret_cast<std::uintptr_t *>(ImageBase() + 0x64B4C0);
        auto save = *reinterpret_cast<std::uint32_t *>(saveRef);
        int voiceBase =
            (Field<unsigned char>(reinterpret_cast<void *>(static_cast<std::uintptr_t>(save)), 0x1608) & 1)
                ? 0xC7B
                : 0xBDA;
        common = request(0x14A, 0, 0);
        stageSnd = request(0x14E + (stage >= 0x24 ? 0 : stage), 0, 0);
        voice = request(voiceBase + chara, 0, 0);
        FlightLog("Adventure own loader audio requested", chara);
        state++;
        return 0;
    }
    case 1:
        if (!Game<int>(0x4169C0))
            return 0;
        ++state;
        [[fallthrough]];
    case 2:
        Game(0x445AB0, 4, common, 0);
        Game(0x445AB0, 8, stageSnd, 0);
        Game(0x445AB0, 0x10, voice, 0);
        ++state;
        return 0;
    case 3:
        if (!Game<int>(0x445FC0))
            return 0;
        Game(0x430340, common);
        common = 0;
        Game(0x430340, stageSnd);
        stageSnd = 0;
        Game(0x430340, voice);
        voice = 0;
        ++state;
        return 0;
    case 4: {
        int model = Game<int>(0x69690, adventureNativeSide, Field<int>(side, 0x250), Field<int>(side, 0x254),
                              Field<int>(side, 0x258));
        if (model < 0) {
            FlightLog("Adventure own loader model request failed");
            return 1;
        }
        Field<int>(side, 0x26C) = model;
        int stageSize = Game<int>(0x3F8530);
        Field<int>(res, 0x2C) = stageSize;
        std::uint32_t stageBuffer = stageSize > 0x6CB800 ? Game<std::uint32_t>(0x3F25E0, stageSize) : 0;
        if (!stageBuffer)
            stageBuffer = alloc(stageSize);
        if (!stageBuffer) {
            FlightLog("Adventure own loader stage alloc failed");
            return 1;
        }
        zero(stageBuffer, stageSize);
        Field<std::uint32_t>(res, 4) = stageBuffer;
        Field<int>(res, 0x34) = 0x90800;
        auto bank = alloc(0x90800);
        if (!bank) {
            FlightLog("Adventure own loader bank alloc failed");
            return 1;
        }
        zero(bank, 0x90800);
        Field<std::uint32_t>(res, 0x0C) = bank;
        for (int i = 0; i < 5; i++) {
            auto a = alloc(0x1000);
            Field<std::uint32_t>(side, 0x60 + i * 0x64) = a;
            if (!a) {
                FlightLog("Adventure own loader member alloc failed");
                return 1;
            }
            auto b = alloc(0x1000);
            Field<std::uint32_t>(side, 0x64 + i * 0x64) = b;
            if (!b) {
                FlightLog("Adventure own loader member alloc failed");
                return 1;
            }
            zero(a, 0x1000);
            zero(b, 0x1000);
            Field<std::uint32_t>(side, 0x5C + i * 0x64) = a;
        }
        int stage = Field<int>(work, 0x1C);
        int screen = Field<int>(work, 0x24);
        int stageId = stage >= 0x24 ? 0x11E94 + stage : stage + (screen == 1 ? 0x198 : 0x171);
        auto stageRequest = request(stageId, stageBuffer, stageSize);
        if (!stageRequest) {
            FlightLog("Adventure own loader stage request failed");
            return 1;
        }
        Field<std::uint32_t>(res, 4) = stageRequest;
        if (Field<int>(side, 0) < 0 || Field<int>(side, 0) > 5) {
            FlightLog("Adventure own loader invalid member count");
            return 1;
        }
        for (int i = 0; i < Field<int>(side, 0); i++) {
            auto &buf = Field<std::uint32_t>(side, 0x60 + i * 0x64);
            buf = request(Field<int>(side, 4 + i * 0x64) * 2 + adventureNativeSide + 8, buf, 0x1000);
        }
        auto progressRef = *reinterpret_cast<std::uintptr_t *>(ImageBase() + 0x64B490);
        auto progress = *reinterpret_cast<std::uint32_t *>(progressRef);
        int id = Field<int>(reinterpret_cast<void *>(static_cast<std::uintptr_t>(progress)), 0) + 6;
        Field<std::uint32_t>(res, 8) = request(id, 0, 0);
        FlightLog("Adventure own loader model requested", model);
        ++state;
        return 0;
    }
    case 5:
        if (!Game<int>(0x4169C0))
            return 0;
        for (int i = 0; i < Field<int>(side, 0); i++) {
            auto *ptr = side + 0x60 + i * 0x64;
            auto buf = Field<std::uint32_t>(side, 0x60 + i * 0x64);
            Game(0x420A20, ptr, buf, buf);
        }
        {
            auto pool = *reinterpret_cast<std::uintptr_t *>(ImageBase() + 0x64A728);
            Game(0x433490, reinterpret_cast<void *>(pool + 0x200), job);
        }
        adventureLoadSucceeded = true;
        FlightLog("Adventure own loader completed");
        return 1;
    default:
        FlightLog("Adventure own loader invalid state", state);
        return 1;
    }
}
static bool InstallAdventureJob(bool goku) {
    adventureJobThunk = Game<void *>(rva::Port_LowAlloc, std::size_t{64});
    if (!adventureJobThunk)
        return false;
    if (reinterpret_cast<std::uintptr_t>(adventureJobThunk) > 0xFFFFFFFFu ||
        !VirtualProtect(adventureJobThunk, 64, PAGE_EXECUTE_READWRITE, &adventureThunkProtection)) {
        Game(rva::Port_LowFree, adventureJobThunk);
        adventureJobThunk = nullptr;
        return false;
    }
    unsigned char trampoline[12] = {0x48, 0xB8};
    const auto callback = reinterpret_cast<std::uintptr_t>(&AdventureInitialJob);
    std::memcpy(trampoline + 2, &callback, 8);
    trampoline[10] = 0xFF;
    trampoline[11] = 0xE0;
    std::memcpy(adventureJobThunk, trampoline, 12);
    FlushInstructionCache(GetCurrentProcess(), adventureJobThunk, 12);

    const auto pool = *reinterpret_cast<std::uintptr_t *>(ImageBase() + 0x64A728);
    std::memset(reinterpret_cast<void *>(pool), 0, 0x20C);
    for (int i = 0; i < 8; ++i)
        Game(0x433490, reinterpret_cast<void *>(pool + 0x200), reinterpret_cast<void *>(pool + i * 0x40));
    auto *job = Game<void *>(0x433560, reinterpret_cast<void *>(pool + 0x200));
    if (!job) {
        DWORD ignored = 0;
        VirtualProtect(adventureJobThunk, 64, adventureThunkProtection, &ignored);
        Game(rva::Port_LowFree, adventureJobThunk);
        adventureJobThunk = nullptr;
        return false;
    }
    std::memset(job, 0, 0x40);
    Field<std::uint32_t>(job, 4) = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(adventureJobThunk));
    auto *work = Game<void *>(rva::Battle_GetWork);
    const int script = Field<int>(work, 0xBC) - 1;
    Field<int>(job, 0x0C) = script;
    Field<int>(job, 8) = script >= 0 ? 0x5A : 0;
    adventureLoadSucceeded = false;
    adventureNativeSide = goku ? 0 : 1;
    FlightLog("Adventure own loader job queued");
    Game<int>(0x4308C0, job);
    return true;
}
static void OwnAdventureUnload(void *work, int unusedSide) {
    Game(0x69F40); // BtlObj_Term
    auto *res = reinterpret_cast<void *>(
        static_cast<std::uintptr_t>(*reinterpret_cast<std::uint32_t *>(ImageBase() + rva::gCommonRes)) + 0x20);
    if (Field<int>(work, 8) == 1) {
        auto *ev = Game<void *>(0x6BF0);
        Game(0x42EC00, Field<int>(ev, 0));
        auto &ptr = Field<std::uint32_t>(res, 0x10);
        if (ptr)
            Game(0x430340, ptr);
        ptr = 0;
    }
    auto *side = static_cast<std::uint8_t *>(work) + 0xC0 + (1 - unusedSide) * 0x270;
    for (int i = 0; i < 5; i++)
        for (int n = 0; n < 2; n++) {
            auto &ptr = Field<std::uint32_t>(side, 0x60 + i * 0x64 + n * 4);
            if (ptr)
                Game(0x430340, ptr);
            ptr = 0;
        }
    for (int off : {0x0C, 0x08, 0x04}) {
        auto &ptr = Field<std::uint32_t>(res, off);
        if (ptr)
            Game(0x430340, ptr);
        ptr = 0;
    }
    Game(0x445800);
    Game(0x445680, 4);
    Game(0x445680, 8);
    Game(0x445680, 0x10);
    Game(0x6E5C0);
    Game(0x42E360);
    FlightLog("Adventure own loader unloaded");
}

static void FinishAdventureJob() {
    if (adventureJobThunk) {
        DWORD ignored = 0;
        VirtualProtect(adventureJobThunk, 64, adventureThunkProtection, &ignored);
        Game(rva::Port_LowFree, adventureJobThunk);
        adventureJobThunk = nullptr;
    }
}

} // namespace
namespace adventure_loader {
bool Install(bool goku) { return InstallAdventureJob(goku); }
bool Succeeded() { return adventureLoadSucceeded; }
void Finish() { FinishAdventureJob(); }
void Unload(void *work, int unusedSide) { OwnAdventureUnload(work, unusedSide); }
} // namespace adventure_loader
