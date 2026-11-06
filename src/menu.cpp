#include "menu.h"

#if BJ_HAS_KEYBOARD
#include "ap_password.h"
#include "settings.h"
#include "web_export.h"

namespace {
enum class Screen {
  Closed,
  List,
  Wifi,
  Password,
  ConfirmRandom,
  Browse,
  ConfirmDelete,
  Clear,
  Gps,
  Brightness,
  Volume,
  Message,
};

const char* const kItems[] = {
    "Wi-Fi export",      "Change password", "New random password",
    "Sightings",         "Delete last sighting", "Clear log",
    "GPS status",        "Screen brightness",    "Speaker volume",
};
const Screen kItemScreens[] = {
    Screen::Wifi,       Screen::Password,      Screen::ConfirmRandom,
    Screen::Browse,     Screen::ConfirmDelete, Screen::Clear,
    Screen::Gps,        Screen::Brightness,    Screen::Volume,
};
const int kNumItems = sizeof(kItems) / sizeof(kItems[0]);
const int kVisibleRows = 6;

// Brightness stops above zero so the screen can't be set to invisible.
const uint8_t kBrightnessLevels[] = {20, 50, 80, 140, 255};
// Kept well under 75%, as high volume can brown out on battery.
const uint8_t kVolumeLevels[] = {0, 32, 64, 110, 160};
const int kNumLevels = 5;

Menu::Host host;
Screen screen = Screen::Closed;
int selected = 0;
int scrollTop = 0;
uint32_t browseIndex = 0;
String typed;
String message;

int nearestLevel(const uint8_t* levels, uint8_t value) {
  int best = 0;
  for (int i = 1; i < kNumLevels; i++) {
    if (abs(levels[i] - value) < abs(levels[best] - value)) best = i;
  }
  return best;
}

void showMessage(const String& text) {
  message = text;
  screen = Screen::Message;
}

void enterItem() {
  typed = "";
  const Screen next = kItemScreens[selected];
  const bool needsSightings = next == Screen::Browse || next == Screen::ConfirmDelete;
  if (needsSightings && host.store->count() == 0) {
    showMessage("No sightings yet");
    return;
  }
  if (next == Screen::Browse) browseIndex = 0;
  screen = next;
}

void moveSelection(int delta) {
  selected = (selected + delta + kNumItems) % kNumItems;
  if (selected < scrollTop) scrollTop = selected;
  if (selected >= scrollTop + kVisibleRows) scrollTop = selected - kVisibleRows + 1;
}

void adjustBrightness(int delta) {
  const int level = constrain(
      nearestLevel(kBrightnessLevels, Settings::brightness()) + delta, 0, kNumLevels - 1);
  Settings::setBrightness(kBrightnessLevels[level]);
  M5.Display.setBrightness(Settings::brightness());
}

void adjustVolume(int delta) {
  const int level = constrain(
      nearestLevel(kVolumeLevels, Settings::volume()) + delta, 0, kNumLevels - 1);
  Settings::setVolume(kVolumeLevels[level]);
  M5.Speaker.setVolume(Settings::volume());
  host.beep(880, 120);
}

void typeInto(const Key& key, size_t maxLength) {
  if (key.del) {
    if (typed.length()) typed.remove(typed.length() - 1);
  } else if (key.ch > ' ' && typed.length() < maxLength) {
    typed += key.ch;
  }
}

void submitPassword() {
  if (!ApPassword::set(typed.c_str())) {
    showMessage("Use 8 to 16 characters, no spaces. Not changed.");
    return;
  }
  if (WebExport::active()) WebExport::restartSoon();
  showMessage("Password changed");
}

void submitRandomPassword() {
  if (!ApPassword::regenerate()) {
    showMessage("Couldn't save. Not changed.");
    return;
  }
  if (WebExport::active()) WebExport::restartSoon();
  showMessage(String("New password: ") + ApPassword::get());
}

void submitDelete() {
  showMessage(host.store->removeLast() ? "Deleted" : "Couldn't delete. Log unchanged.");
}

void submitClear() {
  if (!typed.equalsIgnoreCase("DELETE")) {
    showMessage("Not cleared");
    return;
  }
  showMessage(host.store->clear() ? "Log cleared" : "Couldn't clear the log");
}

void header(M5Canvas& c, const char* title) {
  c.fillScreen(TFT_BLACK);
  c.setFont(&fonts::Font2);
  c.setTextDatum(top_left);
  c.setTextColor(TFT_ORANGE, TFT_BLACK);
  c.drawString(title, 4, 2);
  c.drawFastHLine(0, 20, c.width(), TFT_DARKGREY);
}

void footer(M5Canvas& c, const char* text) {
  c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  c.drawString(text, 4, c.height() - 18);
}

void drawList(M5Canvas& c) {
  header(c, "MENU");
  const int rowHeight = 18;
  for (int row = 0; row < kVisibleRows && scrollTop + row < kNumItems; row++) {
    const int item = scrollTop + row;
    const int y = 22 + row * rowHeight;
    const bool isSelected = item == selected;
    if (isSelected) c.fillRect(0, y, c.width(), rowHeight, TFT_NAVY);
    c.setTextColor(TFT_WHITE, isSelected ? TFT_NAVY : TFT_BLACK);
    c.drawString(kItems[item], 6, y + 1);
  }
}

void drawWifi(M5Canvas& c) {
  header(c, "WI-FI EXPORT");
  if (WebExport::active()) {
    c.setTextColor(TFT_GREEN, TFT_BLACK);
    c.drawString("On", 4, 24);
    c.setTextColor(TFT_CYAN, TFT_BLACK);
    c.drawString(WebExport::ssid(), 4, 42);
    c.drawString(String("pw ") + ApPassword::get(), 4, 60);
    c.drawString(WebExport::ip(), 4, 78);
    footer(c, "[Enter] turn off  [Esc] back");
  } else {
    c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    c.drawString("Off", 4, 24);
    footer(c, "[Enter] turn on  [Esc] back");
  }
}

void drawTyping(M5Canvas& c, const char* title, const char* prompt, const char* hint) {
  header(c, title);
  c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  c.drawString(prompt, 4, 24);
  c.fillRect(4, 46, c.width() - 8, 20, TFT_NAVY);
  c.setTextColor(TFT_WHITE, TFT_NAVY);
  c.drawString(typed + "_", 8, 48);
  footer(c, hint);
}

void drawConfirm(M5Canvas& c, const char* title, const String& line1, const String& line2,
                 const char* hint) {
  header(c, title);
  c.setTextColor(TFT_WHITE, TFT_BLACK);
  c.drawString(line1, 4, 28);
  c.drawString(line2, 4, 48);
  footer(c, hint);
}

void drawBrowse(M5Canvas& c) {
  header(c, "SIGHTINGS");
  Sighting s;
  if (!host.store->fromEnd(browseIndex, s)) {
    c.setTextColor(TFT_RED, TFT_BLACK);
    c.drawString("Couldn't read this sighting", 4, 28);
    footer(c, "[Esc] back");
    return;
  }
  c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  c.setTextDatum(top_right);
  c.drawString(String(browseIndex + 1) + "/" + String(host.store->count()), c.width() - 4, 2);
  c.setTextDatum(top_left);

  c.setTextColor(TFT_WHITE, TFT_BLACK);
  c.drawString("#" + String(s.id) + "  " + (s.type[0] ? sightingTypeLabel(s.type) : "No tag"), 4, 22);
  c.drawString(s.utc, 4, 39);
  char coords[40];
  snprintf(coords, sizeof(coords), "%.6f, %.6f", s.lat, s.lon);
  c.drawString(coords, 4, 56);
  c.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String detail = isnan(s.alt_m) ? String("alt n/a") : String(s.alt_m, 0) + " m";
  detail += "  HDOP " + String(s.hdop, 1) + "  " + String(s.sats) + " sats";
  c.drawString(detail, 4, 73);
  footer(c, "[;] newer  [.] older  [Esc] back");
}

void drawGps(M5Canvas& c) {
  header(c, "GPS STATUS");
  GpsLink& gps = *host.gps;
  c.setTextColor(TFT_WHITE, TFT_BLACK);
  c.drawString(String("Link: ") + (gps.linkUp() ? "up" : "searching"), 4, 22);
  c.drawString(String("Fix: ") + (gps.hasFix() ? "yes" : "no") + "  " +
                   String(gps.satellites()) + " sats  HDOP " + String(gps.hdop(), 1),
               4, 39);
  if (gps.hasFix()) {
    char coords[40];
    snprintf(coords, sizeof(coords), "%.6f, %.6f", gps.lat(), gps.lon());
    c.drawString(coords, 4, 56);
  }
  c.drawString("In view: " + String(gps.satellitesInView()) + "  best " +
                   String(gps.bestSignalDb()) + " dB",
               4, 73);
  c.drawString(String("Battery: ") + M5.Power.getBatteryLevel() + "%" +
                   (gps.antennaOpen() ? "  ant: open" : ""),
               4, 90);
  footer(c, "[Esc] back");
}

void drawLevel(M5Canvas& c, const char* title, const uint8_t* levels, uint8_t value, bool hasOff) {
  header(c, title);
  const int level = nearestLevel(levels, value);
  const int gap = 6;
  const int barWidth = (c.width() - 8 - gap * (kNumLevels - 1)) / kNumLevels;
  for (int i = 0; i < kNumLevels; i++) {
    const int x = 4 + i * (barWidth + gap);
    if (i <= level) c.fillRect(x, 40, barWidth, 30, TFT_CYAN);
    else c.drawRect(x, 40, barWidth, 30, TFT_DARKGREY);
  }
  if (hasOff && level == 0) {
    c.setTextColor(TFT_YELLOW, TFT_BLACK);
    c.drawString("Muted", 4, 76);
  }
  footer(c, "[,] less  [/] more  [Esc] back");
}

void drawMessage(M5Canvas& c) {
  header(c, "BLUEJAY");
  c.setTextColor(TFT_WHITE, TFT_BLACK);
  // Wrap on a space when the text is too wide for one line.
  int split = -1;
  if (c.textWidth(message) > c.width() - 8) {
    for (int i = message.length() - 1; i > 0; i--) {
      if (message[i] == ' ' && c.textWidth(message.substring(0, i)) <= c.width() - 8) {
        split = i;
        break;
      }
    }
  }
  if (split > 0) {
    c.drawString(message.substring(0, split), 4, 34);
    c.drawString(message.substring(split + 1), 4, 54);
  } else {
    c.drawString(message, 4, 44);
  }
  footer(c, "Press any key");
}
}  // namespace

namespace Menu {

void begin(const Host& h) { host = h; }
bool active() { return screen != Screen::Closed; }

void open() {
  screen = Screen::List;
  selected = 0;
  scrollTop = 0;
}

void close() {
  screen = Screen::Closed;
  typed = "";
  message = "";
}

void handleKey(const Key& key) {
  if (screen == Screen::List) {
    if (key.esc) close();
    else if (key.enter) enterItem();
    else if (key.ch == ';') moveSelection(-1);
    else if (key.ch == '.') moveSelection(1);
    return;
  }
  if (screen == Screen::Message) {
    screen = Screen::List;
    return;
  }
  if (key.esc) {
    typed = "";
    screen = Screen::List;
    return;
  }

  switch (screen) {
    case Screen::Wifi:
      if (key.enter) host.toggleWifi();
      break;
    case Screen::Password:
      if (key.enter) submitPassword();
      else typeInto(key, BJ_AP_PW_MAX_LEN);
      break;
    case Screen::ConfirmRandom:
      if (key.enter) submitRandomPassword();
      break;
    case Screen::Browse:
      if (key.ch == ';' && browseIndex > 0) browseIndex--;
      else if (key.ch == '.' && browseIndex + 1 < host.store->count()) browseIndex++;
      break;
    case Screen::ConfirmDelete:
      if (key.enter) submitDelete();
      break;
    case Screen::Clear:
      if (key.enter) submitClear();
      else typeInto(key, 6);
      break;
    case Screen::Brightness:
      if (key.ch == ',') adjustBrightness(-1);
      else if (key.ch == '/') adjustBrightness(1);
      break;
    case Screen::Volume:
      if (key.ch == ',') adjustVolume(-1);
      else if (key.ch == '/') adjustVolume(1);
      break;
    default:
      break;
  }
}

void draw(M5Canvas& c) {
  Sighting last;
  switch (screen) {
    case Screen::List: drawList(c); break;
    case Screen::Wifi: drawWifi(c); break;
    case Screen::Password:
      drawTyping(c, "CHANGE PASSWORD", "8 to 16 characters, no spaces",
                 "[Enter] save  [Del] erase  [Esc] back");
      break;
    case Screen::ConfirmRandom:
      drawConfirm(c, "NEW RANDOM PASSWORD", "Replace the Wi-Fi password?",
                  "You'll need to reconnect.", "[Enter] replace  [Esc] back");
      break;
    case Screen::Browse: drawBrowse(c); break;
    case Screen::ConfirmDelete:
      host.store->last(last);
      drawConfirm(c, "DELETE LAST SIGHTING", "Delete #" + String(last.id) + "?", last.utc,
                  "[Enter] delete  [Esc] keep");
      break;
    case Screen::Clear:
      drawTyping(c, "CLEAR LOG", ("Type DELETE to erase " + String(host.store->count()) +
                                  " sightings").c_str(),
                 "[Enter] confirm  [Esc] back");
      break;
    case Screen::Gps: drawGps(c); break;
    case Screen::Brightness:
      drawLevel(c, "SCREEN BRIGHTNESS", kBrightnessLevels, Settings::brightness(), false);
      break;
    case Screen::Volume:
      drawLevel(c, "SPEAKER VOLUME", kVolumeLevels, Settings::volume(), true);
      break;
    case Screen::Message: drawMessage(c); break;
    default: break;
  }
}

const char* screenName() {
  switch (screen) {
    case Screen::Closed: return "closed";
    case Screen::List: return "list";
    case Screen::Wifi: return "wifi";
    case Screen::Password: return "password";
    case Screen::ConfirmRandom: return "confirm-random";
    case Screen::Browse: return "browse";
    case Screen::ConfirmDelete: return "confirm-delete";
    case Screen::Clear: return "clear";
    case Screen::Gps: return "gps";
    case Screen::Brightness: return "brightness";
    case Screen::Volume: return "volume";
    case Screen::Message: return "message";
  }
  return "?";
}

}  // namespace Menu
#endif
