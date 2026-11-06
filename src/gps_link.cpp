#include "gps_link.h"
#include "config.h"

namespace {
struct Probe {
  int rx;
  int tx;
  uint32_t baud;
};
const Probe kProbes[] = {
#ifdef BJ_GPS_BASE_RX_PIN
    {BJ_GPS_BASE_RX_PIN, BJ_GPS_BASE_TX_PIN, 115200},
#endif
    {BJ_GPS_RX_PIN, BJ_GPS_TX_PIN, BJ_GPS_BAUD},
    {BJ_GPS_TX_PIN, BJ_GPS_RX_PIN, BJ_GPS_BAUD},
    {BJ_GPS_RX_PIN, BJ_GPS_TX_PIN, 9600},
    {BJ_GPS_TX_PIN, BJ_GPS_RX_PIN, 9600},
};
constexpr uint8_t kNumProbes = sizeof(kProbes) / sizeof(kProbes[0]);
HardwareSerial& gpsSerial = Serial1;
}  // namespace

void GpsLink::begin() { applyProbe(0); }

void GpsLink::applyProbe(uint8_t idx) {
  _probe = idx % kNumProbes;
  const Probe& p = kProbes[_probe];
  gpsSerial.end();
  gpsSerial.setRxBufferSize(4096);  // survive short loop stalls (web export)
  gpsSerial.begin(p.baud, SERIAL_8N1, p.rx, p.tx);
  _probeStartMs = millis();
  _lastPassed = _gps.passedChecksum();
  log_i("GPS probe: rx=G%d tx=G%d baud=%lu", p.rx, p.tx, (unsigned long)p.baud);
}

void GpsLink::poll() {
  while (gpsSerial.available()) {
    _gps.encode(gpsSerial.read());
  }

  uint32_t now = millis();
  if (_gps.passedChecksum() != _lastPassed) {
    _lastPassed = _gps.passedChecksum();
    _lastGoodMs = now;
    if (!_locked) {
      _locked = true;
      log_i("GPS link up: rx=G%d baud=%lu", rxPin(), (unsigned long)baud());
    }
    return;
  }

  if (_locked) {
    if (now - _lastGoodMs > BJ_GPS_LINK_TIMEOUT_MS) {
      log_w("GPS link lost, re-probing");
      _locked = false;
      applyProbe(_probe);  // retry current settings first
    }
  } else if (now - _probeStartMs > BJ_GPS_PROBE_MS) {
    applyProbe(_probe + 1);
  }
}

bool GpsLink::hasFix() {
  return _locked && _gps.location.isValid() &&
         _gps.location.age() < BJ_MAX_FIX_AGE_MS && _gps.date.isValid() &&
         _gps.time.isValid() && _gps.date.year() >= 2020;
}

uint32_t GpsLink::satellites() {
  return _gps.satellites.isValid() ? _gps.satellites.value() : 0;
}
double GpsLink::hdop() { return _gps.hdop.isValid() ? _gps.hdop.hdop() : 99.9; }
double GpsLink::lat() { return _gps.location.lat(); }
double GpsLink::lon() { return _gps.location.lng(); }

bool GpsLink::snapshot(Sighting& out) {
  if (!hasFix()) return false;
  out.id = 0;
  snprintf(out.utc, sizeof(out.utc), "%04u-%02u-%02uT%02u:%02u:%02uZ",
           _gps.date.year(), _gps.date.month(), _gps.date.day(),
           _gps.time.hour(), _gps.time.minute(), _gps.time.second());
  out.lat = _gps.location.lat();
  out.lon = _gps.location.lng();
  out.alt_m = _gps.altitude.isValid() ? _gps.altitude.meters() : NAN;
  out.hdop = hdop();
  out.sats = satellites();
  return true;
}

int GpsLink::rxPin() const { return kProbes[_probe].rx; }
uint32_t GpsLink::baud() const { return kProbes[_probe].baud; }
