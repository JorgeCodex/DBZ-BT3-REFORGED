#pragma once
#include <cstdint>
namespace adventure_loader {
bool Install(bool goku);
bool Succeeded();
void Finish();
void Unload(void *work, int unusedSide);
} // namespace adventure_loader
