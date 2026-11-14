#pragma once
#include <cstdint>
namespace adventure_loader {
int TargetSlot();
int ReplayPiccoloSlot();
int ReplayGokuSlot();
int CompanionSlot();
std::uint32_t CompanionParam();
bool Install(bool goku, bool piccoloWon);
bool Succeeded();
void Finish();
void Unload(void *work, int unusedSide);
} // namespace adventure_loader
