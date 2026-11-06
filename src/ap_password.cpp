#include "ap_password.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include "config.h"

namespace {
const char kNamespace[] = "bluejay";
const char kKey[] = "ap_pw";

// 32 characters, so 5 random bits pick one without bias. Leaves out i, l and o,
// which are easy to misread on the small screen.
const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz123456789";
static_assert(sizeof(kAlphabet) - 1 == 32, "alphabet must have 32 characters");

char current[BJ_AP_PW_MAX_LEN + 1] = {0};

void generate(char* out, size_t size) {
  // The RNG needs this entropy source unless the Wi-Fi radio is running, and
  // it must not be enabled while the radio is on.
  const bool radioOff = WiFi.getMode() == WIFI_OFF;
  if (radioOff) bootloader_random_enable();
  size_t n = strlcpy(out, BJ_AP_PW_PREFIX, size);
  for (int i = 0; i < BJ_AP_PW_RANDOM_CHARS; i++) {
    out[n++] = kAlphabet[esp_random() & 31];
  }
  out[n] = '\0';
  if (radioOff) bootloader_random_disable();
}
}  // namespace

namespace ApPassword {

void begin() {
  Preferences prefs;
  const bool open = prefs.begin(kNamespace, false);
  if (open && prefs.isKey(kKey)) {
    prefs.getString(kKey, current, sizeof(current));
    if (isValid(current)) {
      prefs.end();
      return;
    }
  }

  generate(current, sizeof(current));
  // If this fails the password still works, but changes on the next boot.
  if (!open || prefs.putString(kKey, current) == 0) {
    log_e("Could not save the Wi-Fi password");
  }
  if (open) prefs.end();
}

const char* get() { return current; }

bool isValid(const char* candidate) {
  const size_t len = strlen(candidate);
  if (len < BJ_AP_PW_MIN_LEN || len > BJ_AP_PW_MAX_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    // Printable ASCII with no spaces, so it reads unambiguously on screen.
    if (candidate[i] < 0x21 || candidate[i] > 0x7E) return false;
  }
  return true;
}

bool regenerate() {
  char fresh[BJ_AP_PW_MAX_LEN + 1];
  generate(fresh, sizeof(fresh));
  return set(fresh);
}

bool set(const char* candidate) {
  if (!isValid(candidate)) return false;
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool saved = prefs.putString(kKey, candidate) > 0;
  prefs.end();
  if (saved) strlcpy(current, candidate, sizeof(current));
  return saved;
}

}  // namespace ApPassword
