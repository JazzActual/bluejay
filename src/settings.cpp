#include "settings.h"
#include <Preferences.h>
#include "config.h"

namespace {
const char kNamespace[] = "bluejay";
uint8_t currentBrightness = BJ_SCREEN_BRIGHTNESS;
uint8_t currentVolume = BJ_SPEAKER_VOLUME;

void save(const char* key, uint8_t value) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return;
  prefs.putUChar(key, value);
  prefs.end();
}
}  // namespace

namespace Settings {

void begin() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return;  // no saved settings yet
  currentBrightness = prefs.getUChar("bright", BJ_SCREEN_BRIGHTNESS);
  currentVolume = prefs.getUChar("volume", BJ_SPEAKER_VOLUME);
  prefs.end();
}

uint8_t brightness() { return currentBrightness; }
uint8_t volume() { return currentVolume; }

void setBrightness(uint8_t value) {
  currentBrightness = value;
  save("bright", value);
}

void setVolume(uint8_t value) {
  currentVolume = value;
  save("volume", value);
}

}  // namespace Settings
