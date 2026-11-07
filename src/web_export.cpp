#include "web_export.h"
#include <WebServer.h>
#include <WiFi.h>
#include "ap_password.h"
#include "config.h"

namespace WebExport {
void restartSoon();
}

namespace {
WebServer* server = nullptr;
LogStore* store = nullptr;
char apSsid[33] = {0};
uint32_t restartAtMs = 0;  // 0 means no restart is pending

// Buffers Print output into HTTP chunks.
class ChunkPrint : public Print {
 public:
  explicit ChunkPrint(WebServer& s) : _s(s) {}
  ~ChunkPrint() { flush(); }
  size_t write(uint8_t c) override {
    _buf[_n++] = c;
    if (_n == sizeof(_buf)) flush();
    return 1;
  }
  void flush() override {
    if (_n) _s.sendContent(_buf, _n);
    _n = 0;
  }

 private:
  WebServer& _s;
  char _buf[1024];
  size_t _n = 0;
};

void handleIndex() {
  String html =
      "<!doctype html><html><head><meta name=viewport "
      "content='width=device-width,initial-scale=1'><title>bluejay</title>"
      "<style>body{font-family:system-ui,sans-serif;max-width:32em;margin:2em "
      "auto;padding:0 1em}a.b{display:block;margin:.6em 0;padding:.8em;"
      "background:#1a73e8;color:#fff;text-align:center;border-radius:6px;"
      "text-decoration:none}form{margin-top:3em}</style></head><body>"
      "<h1>bluejay</h1><p><b>";
  html += store->count();
  html +=
      "</b> sightings logged.</p>"
      "<a class=b href='/sightings.csv' download>Download CSV</a>"
      "<a class=b href='/sightings.geojson' download>Download GeoJSON</a>"
      "<form method=post action='/password'><label for=pw><b>Change Wi-Fi "
      "password</b></label><p>";
  html += BJ_AP_PW_MIN_LEN;
  html += " to ";
  html += BJ_AP_PW_MAX_LEN;
  html +=
      " characters, no spaces. You'll need to reconnect afterward. The device "
      "screen always shows the current password.</p><input id=pw name=password "
      "required autocomplete=off autocapitalize=off spellcheck=false "
      "minlength=";
  html += BJ_AP_PW_MIN_LEN;
  html += " maxlength=";
  html += BJ_AP_PW_MAX_LEN;
  html +=
      "> <button>Change password</button></form>"
      "<form method=post action='/clear' onsubmit=\"return confirm('Delete "
      "ALL sightings from the device?')\"><input type=hidden name=confirm "
      "value=DELETE><button>Clear log</button></form></body></html>";
  server->send(200, "text/html", html);
}

void handlePassword() {
  const String password = server->arg("password");
  if (!ApPassword::isValid(password.c_str())) {
    String error = "The password must be ";
    error += BJ_AP_PW_MIN_LEN;
    error += " to ";
    error += BJ_AP_PW_MAX_LEN;
    error += " characters with no spaces. It has not been changed.";
    server->send(400, "text/plain", error);
    return;
  }
  if (!ApPassword::set(password.c_str())) {
    server->send(500, "text/plain",
                 "The password couldn't be saved. It has not been changed.");
    return;
  }
  server->send(200, "text/plain",
               String("Password changed. Reconnect to ") + apSsid +
                   " using the new password shown on the device screen.");
  // Wait so this response gets out before the network restarts.
  WebExport::restartSoon();
}

void handleCsv() {
  server->sendHeader("Content-Disposition",
                     "attachment; filename=sightings.csv");
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "text/csv", "");
  {
    ChunkPrint out(*server);
    store->streamCsv(out);
  }
  server->sendContent("");  // terminating chunk
}

void handleGeoJson() {
  server->sendHeader("Content-Disposition",
                     "attachment; filename=sightings.geojson");
  server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server->send(200, "application/geo+json", "");
  {
    ChunkPrint out(*server);
    store->streamGeoJson(out);
  }
  server->sendContent("");
}

void handleClear() {
  if (server->arg("confirm") != "DELETE") {
    server->send(400, "text/plain", "missing confirm=DELETE");
    return;
  }
  bool ok = store->clear();
  server->sendHeader("Location", "/");
  server->send(ok ? 303 : 500, "text/plain", ok ? "cleared" : "clear failed");
}
}  // namespace

namespace WebExport {

void start(LogStore& s) {
  if (server) return;
  store = &s;
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apSsid, sizeof(apSsid), "%s%02X%02X", BJ_AP_SSID_PREFIX, mac[4],
           mac[5]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, ApPassword::get());

  server = new WebServer(80);
  server->on("/", HTTP_GET, handleIndex);
  server->on("/sightings.csv", HTTP_GET, handleCsv);
  server->on("/sightings.geojson", HTTP_GET, handleGeoJson);
  server->on("/clear", HTTP_POST, handleClear);
  server->on("/password", HTTP_POST, handlePassword);
  server->onNotFound([] { server->send(404, "text/plain", "not found"); });
  server->begin();
  log_i("AP up: %s @ %s", apSsid, WiFi.softAPIP().toString().c_str());
}

void stop() {
  if (!server) return;
  restartAtMs = 0;
  server->stop();
  delete server;
  server = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void restartSoon() {
  restartAtMs = millis() + 1000;
  if (restartAtMs == 0) restartAtMs = 1;
}

void poll() {
  if (!server) return;
  server->handleClient();
  if (restartAtMs != 0 && (int32_t)(millis() - restartAtMs) >= 0) {
    LogStore& s = *store;
    stop();
    start(s);  // applies the new password
  }
}

bool active() { return server != nullptr; }
const char* ssid() { return apSsid; }
String ip() { return WiFi.softAPIP().toString(); }

}  // namespace WebExport
