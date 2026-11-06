#pragma once
#include <stdint.h>

// User settings kept in NVS, so they survive restarts and firmware updates.
namespace Settings {
void begin();
uint8_t brightness();
uint8_t volume();
void setBrightness(uint8_t value);
void setVolume(uint8_t value);
}  // namespace Settings
