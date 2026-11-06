#pragma once

// Wi-Fi export password. Each device generates its own on first boot and keeps
// it in NVS, so it survives restarts and firmware updates. The user can replace
// it from the web page. It is shown on screen while Wi-Fi is on, and is never
// written to the serial log.
namespace ApPassword {
void begin();  // call before Wi-Fi starts: generation uses the hardware RNG
const char* get();
bool isValid(const char* candidate);
bool set(const char* candidate);  // false if invalid or not saved
bool regenerate();  // replaces it with a new random one; false if not saved
}  // namespace ApPassword
