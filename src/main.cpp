// bluejay — log ALPR / Flock camera sightings with GPS.
// Runs on M5StickS3 or AtomS3R (see platformio.ini / config.h).
//
// StickS3: Button A (front) logs a sighting. Button B (side) turns the screen
// on, and holding it for 3 s toggles Wi-Fi.
// AtomS3R: tap the screen button to log, hold it to toggle Wi-Fi.
// Cardputer ADV: press G0 to log, hold it to toggle Wi-Fi.
// USB serial: type "help" for dump/export commands.

#include <M5Unified.h>
#include "ap_password.h"
#include "config.h"
#include "gps_link.h"
#include "log_store.h"
#include "serial_console.h"
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

enum class Flash { None, Logged, NoFix, Error };
Flash flash = Flash::None;
uint32_t flashUntilMs = 0;
uint32_t lastDrawMs = 0;
bool screenOn = true;
uint32_t lastActivityMs = 0;

// Counts as activity, so it also restarts the screen timeout.
void wakeScreen() {
  lastActivityMs = millis();
  if (screenOn) return;
  M5.Display.wakeup();
  M5.Display.setBrightness(BJ_SCREEN_BRIGHTNESS);
  screenOn = true;
  lastDrawMs = 0;  // force redraw
  log_i("Screen on");
}

// Wi-Fi keeps the screen on: it shows the password and is a reminder that the
// radio is draining the battery.
void updateScreenTimeout() {
  if (!screenOn || WebExport::active()) return;
  if (millis() - lastActivityMs < BJ_SCREEN_TIMEOUT_MS) return;
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

void showFlash(Flash f) {
  flash = f;
  flashUntilMs = millis() + 900;
  lastDrawMs = 0;  // force redraw
}

void logSighting() {
  Sighting s;
  if (!storageOk) {
    showFlash(Flash::Error);
    beep(300, 400);
    return;
  }
  if (!gps.snapshot(s)) {
    showFlash(Flash::NoFix);
    beep(400, 250);
    return;
  }
  if (!store.append(s)) {
    showFlash(Flash::Error);
    beep(300, 400);
    return;
  }
  Serial.printf("logged #%lu %s %.7f,%.7f\n", (unsigned long)s.id, s.utc,
                s.lat, s.lon);
  showFlash(Flash::Logged);
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
    canvas.drawString(String(gps.satellites()) + " sats", 2, y + 16);
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

void drawStatus() {
  if (compact()) {
    drawStatusCompact();
    return;
  }
  const int w = canvas.width();
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(&fonts::Font2);

  // Header
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString("BLUEJAY", 4, 2);
#if BJ_HAS_BATTERY
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  int batt = M5.Power.getBatteryLevel();
  canvas.setTextDatum(top_right);
  canvas.drawString(String(batt) + "%" + (M5.Power.isCharging() ? "+" : ""), w - 4, 2);
  canvas.setTextDatum(top_left);
#endif
  canvas.drawFastHLine(0, 20, w, TFT_DARKGREY);

  // GPS state
  int y = 24;
  if (!gps.linkUp()) {
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
    canvas.drawString("GPS unit: searching...", 4, y);
    canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas.drawString("check Grove cable", 4, y + 18);
  } else if (!gps.hasFix()) {
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawString("NO FIX", 4, y);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString(String(gps.satellites()) + " sats  (need sky view)", 4, y + 18);
  } else {
    canvas.setTextColor(TFT_GREEN, TFT_BLACK);
    canvas.drawString("FIX  " + String(gps.satellites()) + " sats  HDOP " +
                          String(gps.hdop(), 1),
                      4, y);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[40];
    snprintf(buf, sizeof(buf), "%.6f, %.6f", gps.lat(), gps.lon());
    canvas.drawString(buf, 4, y + 18);
  }

  // Log state. The Wi-Fi footer needs a third line, so everything moves up.
  const bool wifiOn = WebExport::active();
  y = wifiOn ? 62 : 66;
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  if (!storageOk) {
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.drawString("STORAGE ERROR", 4, y);
  } else {
    canvas.drawString("Logged: " + String(store.count()), 4, y);
    Sighting last;
    if (store.last(last)) {
      canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
      canvas.drawString(String("last ") + (last.utc + 11), 110, y);
    }
  }

  // Footer
  canvas.drawFastHLine(0, wifiOn ? 80 : 88, w, TFT_DARKGREY);
  y = wifiOn ? 83 : 92;
  if (wifiOn) {
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(String("AP ") + WebExport::ssid() + "  " + WebExport::ip(), 4, y);
    canvas.drawString(String("pw ") + ApPassword::get(), 4, y + 17);
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
#if BJ_HAS_BTN_B
    canvas.drawString("[B] hold 3s: wifi off", 4, y + 34);
#else
    canvas.drawString("[G0] hold: wifi off", 4, y + 34);
#endif
  } else {
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
#if BJ_HAS_BTN_B
    canvas.drawString("[A] log sighting", 4, y);
    canvas.drawString("[B] hold 3s: wifi export", 4, y + 18);
#else
    canvas.drawString("[G0] log sighting", 4, y);
    canvas.drawString("[G0] hold: wifi export", 4, y + 18);
#endif
  }
}

void draw() {
  if (flash != Flash::None && millis() > flashUntilMs) flash = Flash::None;
  if (flash != Flash::None) drawFlash(); else drawStatus();
  canvas.pushSprite(0, 0);
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  setCpuFrequencyMhz(BJ_CPU_MHZ);
  log_i("CPU %lu MHz", (unsigned long)getCpuFrequencyMhz());
  M5.Display.setBrightness(BJ_SCREEN_BRIGHTNESS);

#if BJ_GROVE_5V_SWITCHED
  // Grove 5V is an input by default on StickS3; switch it to output so the
  // GPS unit gets power.
  M5.Power.setExtOutput(true);
#endif

#if defined(BJ_BOARD_STICKS3) || defined(BJ_BOARD_CARDPUTER_ADV)
  M5.Display.setRotation(1);  // landscape 240x135
#endif
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
#if BJ_HAS_SPEAKER
  M5.Speaker.setVolume(BJ_SPEAKER_VOLUME);
#endif
#if BJ_HAS_BTN_B
  M5.BtnB.setHoldThresh(BJ_BTN_B_HOLD_WIFI_MS);
#else
  M5.BtnA.setHoldThresh(BJ_HOLD_WIFI_MS);
#endif

  storageOk = store.begin();
  ApPassword::begin();
  gps.begin();
  lastActivityMs = millis();
  Serial.println("bluejay ready on " BJ_BOARD_NAME " — type 'help'");
}

void loop() {
  M5.update();
  gps.poll();
  WebExport::poll();
  SerialConsole::poll(store, gps);

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

  updateScreenTimeout();

  if (screenOn && millis() - lastDrawMs > 200) {
    lastDrawMs = millis();
    draw();
  }

  delay(BJ_LOOP_DELAY_MS);
}
