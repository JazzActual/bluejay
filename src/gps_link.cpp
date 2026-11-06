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

#ifdef BJ_TEST_SERIAL
bool bjEchoNmea = false;  // test builds only: copies raw GPS output to serial
#endif

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
    const char c = gpsSerial.read();
#ifdef BJ_TEST_SERIAL
    if (bjEchoNmea) Serial.write(c);
#endif
    _gps.encode(c);
    if (c == '\n') {
      _line[_lineLen] = '\0';
      parseStatusLine();
      _lineLen = 0;
    } else if (_lineLen < sizeof(_line) - 1) {
      _line[_lineLen++] = c;
    }
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
  out.type[0] = '\0';
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

uint32_t GpsLink::satellitesInView() const {
  uint32_t total = 0;
  for (int i = 0; i < kNumTalkers; i++) total += _inView[i];
  return total;
}

// TinyGPS++ doesn't expose these: satellites in view and signal strength from
// GSV, and antenna status from TXT. Sentences look like
// $GPGSV,msgs,msg,inView,id,elevation,azimuth,snr,...*checksum
void GpsLink::parseStatusLine() {
  if (_lineLen < 7 || _line[0] != '$') return;
  const char* type = _line + 3;

  if (strncmp(type, "RMC", 3) == 0) {  // first sentence of each update
    _bestSignalDb = _signalThisCycleDb;
    _signalThisCycleDb = 0;
    return;
  }
  if (strncmp(type, "TXT", 3) == 0) {
    if (strstr(_line, "ANTENNA OPEN")) _antennaOpen = true;
    else if (strstr(_line, "ANTENNA OK")) _antennaOpen = false;
    return;
  }
  if (strncmp(type, "GSV", 3) != 0) return;

  static const char kTalkers[] = "PLABQN";  // GP, GL, GA, BD/GB, GQ, GN
  char talker = _line[2];
  if (_line[1] == 'B') talker = 'B';
  const char* found = strchr(kTalkers, talker);
  if (!found) return;
  const int slot = found - kTalkers;

  // Walks the comma-separated fields, which can be empty.
  int field = 0;
  const char* start = _line;
  for (const char* p = _line;; p++) {
    if (*p == ',' || *p == '*' || *p == '\0') {
      if (p > start) {
        const int value = atoi(start);
        if (field == 3) _inView[slot] = value;
        // SNR is the last of each four-field satellite block, from field 7.
        else if (field >= 7 && (field - 7) % 4 == 0 && value > _signalThisCycleDb) {
          _signalThisCycleDb = value;
        }
      }
      if (*p != ',') break;
      field++;
      start = p + 1;
    }
  }
}

int GpsLink::rxPin() const { return kProbes[_probe].rx; }
uint32_t GpsLink::baud() const { return kProbes[_probe].baud; }
