// bluejay — log ALPR / Flock camera sightings with GPS.
// Runs on M5StickS3, AtomS3R, or Cardputer ADV (see platformio.ini / config.h).
//
// StickS3: Button A (front) logs a sighting. Button B (side) turns the screen
// on, and holding it for 3 s toggles Wi-Fi.
// AtomS3R: tap the screen button to log, hold it to toggle Wi-Fi.
// Cardputer ADV: Enter logs and a letter key tags it. Esc opens the menu.
// USB serial: type "help" for dump/export commands.

#include <M5Unified.h>
#include "config.h"
#if BJ_HAS_KEYBOARD
#include <M5Cardputer.h>
#include "menu.h"
#endif
#include "ap_password.h"
#include "bluejay.h"
#include "config.h"
#include "gps_link.h"
#include "log_store.h"
#include "serial_console.h"
#include "settings.h"
#include "web_export.h"

#if BJ_HAS_SPEAKER
// Embedded by board_build.embed_files in platformio.ini.
extern const uint8_t kBluejayWavStart[] asm("_binary_data_bluejaycall_wav_start");
extern const uint8_t kBluejayWavEnd[] asm("_binary_data_bluejaycall_wav_end");
#endif

namespace {
GpsLink gps;
LogStore store;
M5Canvas canvas(&M5.Display);
bool storageOk = false;

enum class Flash { None, Logged, Tagged, NoFix, Error };
Flash flash = Flash::None;
uint32_t flashUntilMs = 0;
uint32_t lastDrawMs = 0;
bool screenOn = true;
uint32_t lastActivityMs = 0;
#ifdef BJ_TEST_SERIAL
int testGpsState = -1;  // -1 real, 0 searching, 1 no fix, 2 fix
bool testP0 = false;

// Test builds only: drives P0 of the Cap LoRa-1262's PI4IOE expander, which
// M5Stack documents as the LoRa RF switch.
void setTestP0(bool on) {
  const uint8_t addr = 0x43;
  const uint32_t freq = 400000;
  uint8_t direction = M5.In_I2C.readRegister8(addr, 0x03, freq) | 0x01;
  uint8_t highZ = M5.In_I2C.readRegister8(addr, 0x07, freq) & ~0x01;
  uint8_t output = M5.In_I2C.readRegister8(addr, 0x05, freq);
  output = on ? (output | 0x01) : (output & ~0x01);
  M5.In_I2C.writeRegister8(addr, 0x05, output, freq);
  M5.In_I2C.writeRegister8(addr, 0x03, direction, freq);
  M5.In_I2C.writeRegister8(addr, 0x07, highZ, freq);
  testP0 = on;
  log_i("P0 %d, readback dir=%02x out=%02x", on, M5.In_I2C.readRegister8(addr, 0x03, freq),
        M5.In_I2C.readRegister8(addr, 0x05, freq));
}
#endif

// Counts as activity, so it also restarts the screen timeout.
void wakeScreen() {
  lastActivityMs = millis();
  if (screenOn) return;
  M5.Display.wakeup();
  M5.Display.setBrightness(Settings::brightness());
  screenOn = true;
  lastDrawMs = 0;  // force redraw
  log_i("Screen on");
}

// Wi-Fi keeps the screen on: it shows the password and is a reminder that the
// radio is draining the battery.
void updateScreenTimeout() {
  const bool idle = millis() - lastActivityMs >= BJ_SCREEN_TIMEOUT_MS;
#if BJ_HAS_KEYBOARD
  // Closes even if Wi-Fi keeps the screen on, so an idle device can't act on a
  // stray Enter inside the menu.
  if (idle && Menu::active()) {
    Menu::close();
    lastDrawMs = 0;
  }
#endif
  if (!idle || !screenOn || WebExport::active()) return;
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  screenOn = false;
  log_i("Screen off");
}

void beep(uint16_t freq, uint32_t ms) {
#if BJ_HAS_SPEAKER
  M5.Speaker.tone(freq, ms);
#else
  (void)freq;
  (void)ms;
#endif
}

// Played on a successful log. Restarts if a call is already playing.
void playCall() {
#if BJ_HAS_SPEAKER
  M5.Speaker.playWav(kBluejayWavStart, kBluejayWavEnd - kBluejayWavStart, 1,
                     0, true);
#endif
}

// Small screens (AtomS3R 128x128) get a compact layout.
bool compact() { return canvas.width() < 200; }

void toggleWifi() {
  if (WebExport::active()) WebExport::stop(); else WebExport::start(store);
  lastDrawMs = 0;
}

void showFlash(Flash f, uint32_t durationMs = 900) {
  flash = f;
  flashUntilMs = millis() + durationMs;
  lastDrawMs = 0;  // force redraw
}

void saveSighting(Sighting& s);

void logSighting() {
  Sighting s;
  if (!storageOk) {
    showFlash(Flash::Error);
    beep(BJ_BEEP_HZ, BJ_ERROR_BEEP_MS);
    return;
  }
  if (!gps.snapshot(s)) {
    showFlash(Flash::NoFix);
    beep(BJ_BEEP_HZ, BJ_NO_FIX_BEEP_MS);
    return;
  }
  saveSighting(s);
}

void saveSighting(Sighting& s) {
  if (!s.type[0]) strlcpy(s.type, kDefaultSightingType, sizeof(s.type));
  if (!store.append(s)) {
    showFlash(Flash::Error);
    beep(BJ_BEEP_HZ, BJ_ERROR_BEEP_MS);
    return;
  }
  Serial.printf("logged #%lu %s %.7f,%.7f\n", (unsigned long)s.id, s.utc,
                s.lat, s.lon);
#if BJ_HAS_KEYBOARD
  showFlash(Flash::Logged, BJ_TAG_WINDOW_MS);
#else
  showFlash(Flash::Logged);
#endif
  playCall();
}

void drawFlash() {
  Sighting last;
  uint16_t bg = TFT_DARKGREEN;
  const char* title = "LOGGED";
  char sub[32] = "";
  switch (flash) {
    case Flash::Logged:
      if (store.last(last)) snprintf(sub, sizeof(sub), "#%lu", (unsigned long)last.id);
      break;
    case Flash::Tagged:
      title = "TAGGED";
      if (store.last(last)) snprintf(sub, sizeof(sub), "%s", sightingTypeLabel(last.type));
      break;
    case Flash::NoFix:
      bg = TFT_RED;
      title = "NO FIX";
      snprintf(sub, sizeof(sub), "not logged");
      break;
    default:
      bg = TFT_RED;
      title = "ERROR";
      snprintf(sub, sizeof(sub), "storage failed");
      break;
  }
  canvas.fillScreen(bg);
  canvas.setTextColor(TFT_WHITE, bg);
  canvas.setTextDatum(middle_center);
  const bool small = compact();
  canvas.setFont(small ? &fonts::FreeSansBold12pt7b : &fonts::FreeSansBold18pt7b);
  canvas.drawString(title, canvas.width() / 2, canvas.height() / 2 - (small ? 10 : 14));
  canvas.setFont(small ? &fonts::FreeSans9pt7b : &fonts::FreeSans12pt7b);
  canvas.drawString(sub, canvas.width() / 2, canvas.height() / 2 + (small ? 18 : 24));
#if BJ_HAS_KEYBOARD
  if (flash == Flash::Logged) {
    // Two per row, as they don't all fit on one line.
    String rows[2];
    for (int i = 0; i < kNumSightingTypes; i++) {
      String& row = rows[i < 2 ? 0 : 1];
      if (row.length()) row += "  ";
      row += String((char)toupper(kSightingTypes[i].key)) + " " + kSightingTypes[i].label;
    }
    canvas.setFont(&fonts::Font2);
    canvas.drawString(rows[0], canvas.width() / 2, canvas.height() - 26);
    canvas.drawString(rows[1], canvas.width() / 2, canvas.height() - 9);
  }
#endif
  canvas.setTextDatum(top_left);
}

// 128x128 layout: Font2 gives ~16 chars x 8 rows.
void drawStatusCompact() {
  const int w = canvas.width();
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);

  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString("BLUEJAY", 2, 1);
  canvas.drawFastHLine(0, 18, w, TFT_DARKGREY);

  if (WebExport::active()) {
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(WebExport::ssid(), 2, 22);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("password:", 2, 38);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    // Must stay fully readable: it's how a changed password gets recovered.
    if (canvas.textWidth(ApPassword::get()) > w - 4) canvas.setFont(&fonts::Font0);
    canvas.drawString(ApPassword::get(), 2, 54);
    canvas.setFont(&fonts::Font2);
    canvas.drawString(WebExport::ip(), 2, 74);
    canvas.drawFastHLine(0, 94, w, TFT_DARKGREY);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("tap: log", 2, 97);
    canvas.drawString("hold: wifi off", 2, 112);
    return;
  }

  int y = 21;
  if (!gps.linkUp()) {
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString("GPS searching", 2, y);
    canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas.drawString("check GPS base", 2, y + 16);
  } else if (!gps.hasFix()) {
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawString("NO FIX", 2, y);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(String(gps.satellitesInView()) + " in view", 2, y + 16);
  } else {
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString("FIX " + String(gps.satellites()) + "sat H" +
                          String(gps.hdop(), 1),
                      2, y);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.6f", gps.lat());
    canvas.drawString(buf, 2, y + 16);
    snprintf(buf, sizeof(buf), "%.6f", gps.lon());
    canvas.drawString(buf, 2, y + 32);
  }

  y = 74;
  if (!storageOk) {
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawString("STORAGE ERROR", 2, y);
  } else {
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString("Logged: " + String(store.count()), 2, y);
  }

  canvas.drawFastHLine(0, 94, w, TFT_DARKGREY);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  canvas.drawString("tap: log", 2, 97);
  canvas.drawString("hold: wifi", 2, 112);
}

// Colors from the bird sprite, so the bars and branch match it.
constexpr uint16_t kBarColor = 0x18E5;
constexpr uint16_t kBranchColor = 0x8AC5;
constexpr uint16_t kBranchShadeColor = 0x59C3;
constexpr int kTopBarHeight = 20;
constexpr int kBottomBarHeight = 19;

void drawTopBar(int w) {
  canvas.fillRect(0, 0, w, kTopBarHeight, kBarColor);

  int rightEdge = w - 4;
#if BJ_HAS_BATTERY
  canvas.setTextColor(TFT_LIGHTGREY, kBarColor);
  const String batt = String(M5.Power.getBatteryLevel()) + "%" + (M5.Power.isCharging() ? "+" : "");
  canvas.setTextDatum(top_right);
  canvas.drawString(batt, rightEdge, 2);
  canvas.setTextDatum(top_left);
  rightEdge -= canvas.textWidth(batt) + 6;
#endif

  bool linkUp = gps.linkUp();
  bool hasFix = gps.hasFix();
  uint32_t sats = gps.satellites();
  uint32_t inView = gps.satellitesInView();
  int bestDb = gps.bestSignalDb();
  double lat = gps.lat();
  double lon = gps.lon();
#ifdef BJ_TEST_SERIAL
  // Lets a script see every state without showing a real location.
  if (testGpsState >= 0) {
    linkUp = testGpsState > 0;
    hasFix = testGpsState == 2;
    sats = 18;
    inView = 25;
    bestDb = 42;
    lat = 39.7392;
    lon = -104.9903;
  }
#endif

  if (!linkUp) {
    canvas.setTextColor(TFT_YELLOW, kBarColor);
    canvas.drawString("GPS searching...", 4, 2);
  } else if (!hasFix) {
    canvas.setTextColor(TFT_RED, kBarColor);
    canvas.drawString("NO FIX", 4, 2);
    const int x = 4 + canvas.textWidth("NO FIX") + 6;
    canvas.setTextColor(TFT_LIGHTGREY, kBarColor);
    canvas.drawString(String(inView) + " in view  " + String(bestDb) + " dB", x, 2);
  } else {
    const String fix = String(sats) + " sat";
    canvas.setTextColor(TFT_GREEN, kBarColor);
    canvas.drawString(fix, 4, 2);
    const int x = 4 + canvas.textWidth(fix) + 6;
    // Fewer decimals if the coordinates won't fit beside the battery level.
    char coords[32];
    snprintf(coords, sizeof(coords), "%.6f,%.6f", lat, lon);
    if (x + canvas.textWidth(coords) > rightEdge) {
      snprintf(coords, sizeof(coords), "%.4f,%.4f", lat, lon);
    }
    canvas.setTextColor(TFT_WHITE, kBarColor);
    canvas.drawString(coords, x, 2);
  }
}

void drawBottomBar(int w, int h) {
  const int y = h - kBottomBarHeight;
  canvas.fillRect(0, y, w, kBottomBarHeight, kBarColor);
  if (!storageOk) {
    canvas.setTextColor(TFT_RED, kBarColor);
    canvas.drawString("STORAGE ERROR", 4, y + 2);
    return;
  }
  canvas.setTextColor(TFT_WHITE, kBarColor);
  canvas.drawString("Logged: " + String(store.count()), 4, y + 2);
  Sighting last;
  if (store.last(last)) {
    canvas.setTextColor(TFT_LIGHTGREY, kBarColor);
    canvas.setTextDatum(top_right);
    String detail = last.utc + 11;
#if BJ_HAS_KEYBOARD
    detail = String(sightingTypeLabel(last.type)) + "  " + detail;
#endif
    canvas.drawString(detail, w - 4, y + 2);
    canvas.setTextDatum(top_left);
  }
}

// Extends the branch in the 96 px sprite to both screen edges.
void drawBranch(int w, int birdX, int birdY) {
  canvas.fillRect(0, birdY + 82, birdX, 6, kBranchColor);
  canvas.fillRect(0, birdY + 88, birdX, 2, kBranchShadeColor);
  const int rightX = birdX + BLUEJAY_96_W;
  canvas.fillRect(rightX, birdY + 84, w - rightX, 6, kBranchColor);
  canvas.fillRect(rightX, birdY + 90, w - rightX, 2, kBranchShadeColor);
}

void drawHome(int w) {
  const int birdX = (w - BLUEJAY_96_W) / 2;
  const int birdY = kTopBarHeight;
  drawBranch(w, birdX, birdY);
  canvas.pushImage(birdX, birdY, BLUEJAY_96_W, BLUEJAY_96_H, bluejay_96);

  const int rightX = birdX + BLUEJAY_96_W + 4;
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
#if BJ_HAS_KEYBOARD
  canvas.drawString("[Enter]", 4, 34);
  canvas.drawString("log", 4, 52);
  canvas.drawString("[Esc]", rightX, 34);
  canvas.drawString("menu", rightX, 52);
#elif BJ_HAS_BTN_B
  canvas.drawString("[A]", 4, 34);
  canvas.drawString("log", 4, 52);
  canvas.drawString("[B] hold", rightX, 34);
  canvas.drawString("3s: wifi", rightX, 52);
#else
  canvas.drawString("tap", 4, 34);
  canvas.drawString("log", 4, 52);
  canvas.drawString("hold", rightX, 34);
  canvas.drawString("wifi", rightX, 52);
#endif
}

// The small bird makes room for what's needed to connect.
void drawWifiHome(int w, int h) {
  canvas.pushImage(w - BLUEJAY_48_W, h - kBottomBarHeight - BLUEJAY_48_H, BLUEJAY_48_W,
                   BLUEJAY_48_H, bluejay_48);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.drawString(WebExport::ssid(), 4, 24);
  canvas.drawString(WebExport::ip(), 4, 42);
  canvas.drawString(String("pw ") + ApPassword::get(), 4, 60);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
#if BJ_HAS_KEYBOARD
  canvas.drawString("[Esc] menu:", 4, 78);
  canvas.drawString("wifi off", 4, 96);
#elif BJ_HAS_BTN_B
  canvas.drawString("[B] hold 3s:", 4, 78);
  canvas.drawString("wifi off", 4, 96);
#else
  canvas.drawString("hold: wifi off", 4, 78);
#endif
}

void drawStatus() {
  if (compact()) {
    drawStatusCompact();
    return;
  }
  const int w = canvas.width();
  const int h = canvas.height();
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);

  drawTopBar(w);
  if (WebExport::active()) drawWifiHome(w, h);
  else drawHome(w);
  drawBottomBar(w, h);
}

void draw() {
  if (flash != Flash::None && millis() > flashUntilMs) flash = Flash::None;
#if BJ_HAS_KEYBOARD
  if (Menu::active()) {
    Menu::draw(canvas);
    canvas.pushSprite(0, 0);
    return;
  }
#endif
  if (flash != Flash::None) drawFlash(); else drawStatus();
  canvas.pushSprite(0, 0);
}

#if BJ_HAS_KEYBOARD
// False if this key isn't a tag.
bool tagLast(char keyChar) {
  const SightingType* match = nullptr;
  for (int i = 0; i < kNumSightingTypes; i++) {
    if (kSightingTypes[i].key == tolower(keyChar)) match = &kSightingTypes[i];
  }
  if (!match) return false;
  const char* slug = match->slug;
  if (store.setLastType(slug)) {
    Serial.printf("tagged %s\n", slug);
    showFlash(Flash::Tagged);
  } else {
    showFlash(Flash::Error);
    beep(BJ_BEEP_HZ, BJ_ERROR_BEEP_MS);
  }
  return true;
}

void handleKey(const Key& key) {
  const bool wasOn = screenOn;
  wakeScreen();
  lastDrawMs = 0;  // force redraw

  // With the screen off, Enter logs and every other key only wakes it.
  if (!wasOn) {
    if (key.enter) logSighting();
    return;
  }
  if (Menu::active()) {
    Menu::handleKey(key);
    return;
  }
#ifdef BJ_TEST_SERIAL
  if (key.ch == 'p') {
    setTestP0(!testP0);
    return;
  }
#endif
  if (flash == Flash::Logged && key.ch && tagLast(key.ch)) return;
  if (key.enter) {
    logSighting();
  } else if (key.esc) {
    flash = Flash::None;
    Menu::open();
  }
}

void pollKeyboard() {
  auto& keyboard = M5Cardputer.Keyboard;
  if (!keyboard.isChange() || !keyboard.isPressed()) return;
  const auto& state = keyboard.keysState();
  Key key;
  key.enter = state.enter;
  key.del = state.del;
  for (char c : state.word) {
    if (c == '`') key.esc = true;  // the key labeled Esc
    else key.ch = c;
  }
  handleKey(key);
}
#endif
}  // namespace

#ifdef BJ_TEST_SERIAL
#include <LittleFS.h>
// Test builds only (-DBJ_TEST_SERIAL): lets a script drive the keys and add
// sightings without a GPS fix. Published firmware doesn't include this.
void bjTestCommand(const String& command) {
  if (command == "fake") {
    static int n = 0;
    n++;
    Sighting s{};
    snprintf(s.utc, sizeof(s.utc), "2026-01-01T00:00:%02dZ", n % 60);
    s.lat = 10 + n * 0.001;
    s.lon = -20 - n * 0.001;
    s.alt_m = (n % 2) ? 100.5f : NAN;
    s.hdop = 1.2f;
    s.sats = 9;
    saveSighting(s);
  } else if (command.startsWith("key ")) {
#if BJ_HAS_KEYBOARD
    const String name = command.substring(4);
    Key key;
    if (name == "enter") key.enter = true;
    else if (name == "esc") key.esc = true;
    else if (name == "del") key.del = true;
    else key.ch = name[0];
    handleKey(key);
#endif
  } else if (command == "idle") {
    lastActivityMs = millis() - BJ_SCREEN_TIMEOUT_MS;
  } else if (command == "pw") {
    Serial.printf("pw=%s\n", ApPassword::get());
  } else if (command == "set") {
    Serial.printf("bright=%u volume=%u\n", Settings::brightness(), Settings::volume());
  } else if (command == "oldlog") {
    File f = LittleFS.open(BJ_LOG_PATH, "w");
    f.print("id,utc,lat,lon,alt_m,hdop,sats\r\n1,2026-01-01T00:00:01Z,1.5000000,2.5000000,nan,0.90,8\n");
    f.close();
  } else if (command.startsWith("gpsstate ")) {
    testGpsState = command.substring(9).toInt();
    lastDrawMs = 0;
  } else if (command.startsWith("wifi")) {
    toggleWifi();
  } else if (command == "screen") {
    // Current screen as run-length encoded RGB565, one line per row.
    draw();
    Serial.println("-----BEGIN SCREEN-----");
    Serial.flush();
    for (int y = 0; y < canvas.height(); y++) {
      String row;
      int run = 0;
      uint16_t color = 0;
      for (int x = 0; x < canvas.width(); x++) {
        const uint16_t c = canvas.readPixel(x, y);
        if (run && c == color) {
          run++;
          continue;
        }
        if (run) row += String(run) + ":" + String(color, HEX) + " ";
        color = c;
        run = 1;
      }
      row += String(run) + ":" + String(color, HEX);
      Serial.println(String(y) + "|" + row);
      Serial.flush();
      delay(3);
    }
    Serial.println("-----END SCREEN-----");
  } else if (command == "p0") {
    setTestP0(!testP0);
  } else if (command == "gpslog") {
    File f = LittleFS.open("/gpstest.csv", "r");
    Serial.println("-----BEGIN GPSLOG-----");
    if (f) {
      while (f.available()) Serial.write(f.read());
      f.close();
    }
    Serial.println("-----END GPSLOG-----");
  } else if (command == "gpslogclear") {
    LittleFS.remove("/gpstest.csv");
  } else if (command == "nmea") {
    extern bool bjEchoNmea;
    bjEchoNmea = !bjEchoNmea;
  } else if (command == "reboot") {
    ESP.restart();
  }
#if BJ_HAS_KEYBOARD
  Serial.printf("state menu=%s flash=%d screen=%d wifi=%d count=%lu\n",
                Menu::screenName(), (int)flash, screenOn, WebExport::active(),
                (unsigned long)store.count());
#endif
}
#endif

void setup() {
  auto cfg = M5.config();
#if BJ_HAS_KEYBOARD
  M5Cardputer.begin(cfg, true);
#else
  M5.begin(cfg);
#endif
  Serial.begin(115200);
  setCpuFrequencyMhz(BJ_CPU_MHZ);
  log_i("CPU %lu MHz", (unsigned long)getCpuFrequencyMhz());
  Settings::begin();
  M5.Display.setBrightness(Settings::brightness());

#if BJ_GROVE_5V_SWITCHED
  // Grove 5V is an input by default on StickS3; switch it to output so the
  // GPS unit gets power.
  M5.Power.setExtOutput(true);
#endif

#if defined(BJ_BOARD_STICKS3) || defined(BJ_BOARD_CARDPUTER_ADV)
  M5.Display.setRotation(1);  // landscape 240x135
#endif
  canvas.setColorDepth(16);
  canvas.setSwapBytes(true);  // the bird sprites are plain RGB565
  canvas.createSprite(M5.Display.width(), M5.Display.height());
#if BJ_HAS_SPEAKER
  M5.Speaker.setVolume(Settings::volume());
#endif
#if BJ_HAS_BTN_B
  M5.BtnB.setHoldThresh(BJ_BTN_B_HOLD_WIFI_MS);
#else
  M5.BtnA.setHoldThresh(BJ_HOLD_WIFI_MS);
#endif

  storageOk = store.begin();
  ApPassword::begin();
  gps.begin();
#if BJ_HAS_KEYBOARD
  Menu::begin({&store, &gps, &storageOk, toggleWifi, beep});
#endif
  lastActivityMs = millis();
  Serial.println("bluejay ready on " BJ_BOARD_NAME " — type 'help'");
}

void loop() {
#if BJ_HAS_KEYBOARD
  M5Cardputer.update();
#else
  M5.update();
#endif
  gps.poll();
  WebExport::poll();
  SerialConsole::poll(store, gps);

#if BJ_HAS_KEYBOARD
  pollKeyboard();
#else
  // wasClicked() only fires on a release before the hold threshold, so a
  // long hold toggles Wi-Fi without also logging a sighting. Logging works with
  // the screen off, so a sighting is never lost to a press that only wakes it.
  if (M5.BtnA.wasClicked()) {
    wakeScreen();
    logSighting();
  }
#if BJ_HAS_BTN_B
  if (M5.BtnB.wasPressed()) wakeScreen();
  if (M5.BtnB.wasHold()) {
    wakeScreen();
    toggleWifi();
  }
#else
  if (M5.BtnA.wasHold()) {
    wakeScreen();
    toggleWifi();
  }
#endif
#endif

  updateScreenTimeout();

#ifdef BJ_TEST_SERIAL
  // No location in this log: just satellite counts, signal, and uptime.
  static uint32_t lastGpsLogMs = 0;
  if (millis() - lastGpsLogMs >= 30000) {
    lastGpsLogMs = millis();
    File f = LittleFS.open("/gpstest.csv", "a");
    if (f) {
      f.printf("%lu,link=%d,fix=%d,used=%lu,view=%lu,best=%d,ant_open=%d,p0=%d,batt=%d\n",
               (unsigned long)(millis() / 1000), gps.linkUp(), gps.hasFix(),
               (unsigned long)gps.satellites(), (unsigned long)gps.satellitesInView(),
               gps.bestSignalDb(), gps.antennaOpen(), testP0, (int)M5.Power.getBatteryLevel());
      f.close();
    }
  }
#endif

  if (screenOn && millis() - lastDrawMs > 200) {
    lastDrawMs = millis();
    draw();
  }

  delay(BJ_LOOP_DELAY_MS);
}
