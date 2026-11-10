#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfplay.h>

#include <atomic>
#include <cstdio>
#include <filesystem>

extern "C" __declspec(dllexport) void PlayRaditzIntro();
#ifdef RADITZ_FLIGHT
void ArmRaditzFlight();
void ArmRaditzAdventure();
#endif

namespace {
std::filesystem::path ModuleFolder() {
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&PlayRaditzIntro), &module);
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    if (length == 0 || length == ARRAYSIZE(path)) return {};
    return std::filesystem::path(path).parent_path();
}

void Log(const std::filesystem::path& folder, const char* message, HRESULT result = S_OK) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, (folder / L"raditz-intro.log").c_str(), L"a") == 0) {
        std::fprintf(file, "%s (0x%08lX)\n", message, static_cast<unsigned long>(result));
        std::fclose(file);
    }
}

HWND GameWindow() {
    HWND window = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND candidate, LPARAM output) -> BOOL {
        if (!IsWindowVisible(candidate) || GetWindow(candidate, GW_OWNER)) return TRUE;
        *reinterpret_cast<HWND*>(output) = candidate;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&window));
    return window;
}

constexpr bool IsSkipPress(UINT message, WPARAM key, LPARAM flags) {
    // Bit 30 is set on auto-repeat while the confirmation key is still held.
    return message == WM_KEYDOWN && key == VK_RETURN && (flags & (1L << 30)) == 0;
}

class PlaybackEvents final : public IMFPMediaPlayerCallback {
public:
    PlaybackEvents() : done(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    STDMETHODIMP QueryInterface(REFIID id, void** out) override {
        if (id != __uuidof(IUnknown) && id != __uuidof(IMFPMediaPlayerCallback)) {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        *out = static_cast<IMFPMediaPlayerCallback*>(this);
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ++references; }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG count = --references;
        if (count == 0) delete this;
        return count;
    }
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* event) override {
        if (FAILED(event->hrEvent) || event->eEventType == MFP_EVENT_TYPE_ERROR) {
            result = FAILED(event->hrEvent) ? event->hrEvent : E_FAIL;
            SetEvent(done);
        } else if (event->eEventType == MFP_EVENT_TYPE_PLAY) {
            started = true;
        } else if (event->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED) {
            SetEvent(done);
        }
    }

    const HANDLE done;
    std::atomic<bool> started{false};
    std::atomic<HRESULT> result{S_OK};

private:
    ~PlaybackEvents() { if (done) CloseHandle(done); }
    std::atomic<ULONG> references{1};
};

void Play(const std::filesystem::path& folder, HWND parent) {
    const auto clip = folder / L"radditz.mp4";
    RECT client{};
    GetClientRect(parent, &client);
    HWND overlay = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                                   0, 0, client.right, client.bottom, parent, nullptr, nullptr, nullptr);
    if (!overlay) {
        Log(folder, "Could not create the video surface", HRESULT_FROM_WIN32(GetLastError()));
        return;
    }

    auto* events = new PlaybackEvents;
    IMFPMediaPlayer* player = nullptr;
    HRESULT result = events->done
        ? MFPCreateMediaPlayer(clip.c_str(), TRUE, MFP_OPTION_FREE_THREADED_CALLBACK, events, overlay, &player)
        : HRESULT_FROM_WIN32(GetLastError());
    bool reportedStart = false;
    bool interrupted = false;
    bool skipped = false;
    const ULONGLONG startDeadline = GetTickCount64() + 10000;
    while (SUCCEEDED(result)) {
        const DWORD wait = MsgWaitForMultipleObjects(1, &events->done, FALSE, 50, QS_ALLINPUT);
        if (events->started && !reportedStart) {
            Log(folder, "MP4 playback started");
            reportedStart = true;
        }
        if (wait == WAIT_OBJECT_0) {
            result = events->result;
            break;
        }
        if (wait == WAIT_FAILED || (!reportedStart && GetTickCount64() >= startDeadline)) {
            result = wait == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            break;
        }

        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (IsSkipPress(message.message, message.wParam, message.lParam) &&
                (message.hwnd == parent || IsChild(parent, message.hwnd))) {
                skipped = true;
                break;
            }
            if (message.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(message.wParam));
                interrupted = true;
                break;
            }
            if (message.message == WM_CLOSE && message.hwnd == parent) interrupted = true;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (skipped) break;
        if (interrupted || !IsWindow(parent)) {
            interrupted = true;
            break;
        }
        RECT resized{};
        GetClientRect(parent, &resized);
        if (!EqualRect(&client, &resized)) {
            client = resized;
            SetWindowPos(overlay, nullptr, 0, 0, client.right, client.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
            player->UpdateVideo();
        }
    }
    if (player) {
        player->Shutdown();
        player->Release();
    }
    events->Release();
    DestroyWindow(overlay);
    if (skipped) Log(folder, "MP4 skipped with Enter, returning to the original battle");
    else if (interrupted) Log(folder, "MP4 playback interrupted");
    else if (FAILED(result)) Log(folder, "MP4 playback failed, returning to the original battle", result);
    else Log(folder, "MP4 playback ended, returning to the original battle");
}
} // namespace

extern "C" __declspec(dllexport) void PlayRaditzIntro() {
    const HWND parent = GameWindow();
#ifdef RADITZ_FLIGHT
    ArmRaditzFlight();
#endif
    if (!parent) return;
    const auto folder = ModuleFolder();
    if (folder.empty()) return;
    Log(folder, "Raditz mission confirmed");
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
        Log(folder, "Could not initialize MP4 playback", com);
        return;
    }
    Play(folder, parent);
    if (SUCCEEDED(com)) CoUninitialize();
}
