#pragma once
#include "config.h"

#if BJ_HAS_MENU
#include <M5Unified.h>
#include "gps_link.h"
#include "log_store.h"

struct Key {
  bool enter = false;
  bool esc = false;
  bool del = false;
  bool next = false;  // Button B: step forward, wrapping around
  char ch = 0;  // printable character, or 0
};

// Opened with Esc on the Cardputer and Button B on the StickS3.
namespace Menu {
struct Host {
  LogStore* store;
  GpsLink* gps;
  bool* storageOk;
  void (*toggleWifi)();
  void (*beep)(uint16_t freq, uint32_t ms);
};

void begin(const Host& host);
bool active();
void open();
void close();  // cancels anything unfinished
void handleKey(const Key& key);
void draw(M5Canvas& canvas);
const char* screenName();
}  // namespace Menu
#endif
