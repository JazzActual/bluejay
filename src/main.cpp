// bluejay — log ALPR / Flock camera sightings with GPS on an M5StickS3.
//
// Button A (front): log a sighting at the current GPS fix.
// Button B (side):  toggle the Wi-Fi export access point.
// USB serial:       type "help" for dump/export commands.

#include <M5Unified.h>
#include "config.h"
#include "gps_link.h"
#include "log_store.h"
#include "serial_console.h"
#include "web_export.h"

namespace {
GpsLink gps;
LogStore store;
M5Canvas canvas(&M5.Display);
bool storageOk = false;

enum class Flash { None, Logged, NoFix, Error };
Flash flash = Flash::None;
uint32_t flashUntilMs = 0;
uint32_t lastDrawMs = 0;

void beep(uint16_t freq, uint32_t ms) { M5.Speaker.tone(freq, ms); }

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
  beep(2400, 60);
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
  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.drawString(title, canvas.width() / 2, canvas.height() / 2 - 14);
  canvas.setFont(&fonts::FreeSans12pt7b);
  canvas.drawString(sub, canvas.width() / 2, canvas.height() / 2 + 24);
  canvas.setTextDatum(top_left);
}

void drawStatus() {
  const int w = canvas.width();
  canvas.fillScreen(TFT_BLACK);
  canvas.setFont(&fonts::Font2);

  // Header
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.drawString("BLUEJAY", 4, 2);
  canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  int batt = M5.Power.getBatteryLevel();
  canvas.setTextDatum(top_right);
  canvas.drawString(String(batt) + "%" + (M5.Power.isCharging() ? "+" : ""), w - 4, 2);
  canvas.setTextDatum(top_left);
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

  // Log state
  y = 66;
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
  canvas.drawFastHLine(0, 88, w, TFT_DARKGREY);
  y = 92;
  if (WebExport::active()) {
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.drawString(String("AP ") + WebExport::ssid(), 4, y);
    canvas.drawString(String("pw ") + BJ_AP_PASSWORD + "  " + WebExport::ip(), 4, y + 18);
  } else {
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    canvas.drawString("[A] log sighting", 4, y);
    canvas.drawString("[B] wifi export", 4, y + 18);
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

  // Grove 5V is an input by default on StickS3; switch it to output so the
  // GPS unit gets power.
  M5.Power.setExtOutput(true);

  M5.Display.setRotation(1);  // landscape 240x135
  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  M5.Speaker.setVolume(BJ_SPEAKER_VOLUME);

  storageOk = store.begin();
  gps.begin();
  Serial.println("bluejay ready — type 'help'");
}

void loop() {
  M5.update();
  gps.poll();
  WebExport::poll();
  SerialConsole::poll(store, gps);

  if (M5.BtnA.wasClicked()) logSighting();
  if (M5.BtnB.wasClicked()) {
    if (WebExport::active()) WebExport::stop(); else WebExport::start(store);
    lastDrawMs = 0;
  }

  if (millis() - lastDrawMs > 200) {
    lastDrawMs = millis();
    draw();
  }
}
