// Entry point retained for compatibility with the existing mission hook.
// Cinematics are not played; their design belongs to each mission document.
#ifdef RADITZ_FLIGHT
bool RunRaditzAdventure();
#endif
extern "C" __declspec(dllexport) int PlayRaditzIntro() {
#ifdef RADITZ_FLIGHT
    return RunRaditzAdventure() ? 1 : 0;
#else
    return 1;
#endif
}
