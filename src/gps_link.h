#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "sighting.h"

// Owns the UART to the GPS unit. Probes pin orientation and baud rate until
// it sees NMEA sentences with valid checksums, then locks on.
class GpsLink {
 public:
  void begin();
  void poll();

  bool linkUp() const { return _locked; }
  bool hasFix();
  uint32_t satellites();
  double hdop();
  double lat();
  double lon();

  // Fill a sighting from the current fix (id left as 0). False if no fix.
  bool snapshot(Sighting& out);

  int rxPin() const;
  uint32_t baud() const;

 private:
  void applyProbe(uint8_t idx);

  TinyGPSPlus _gps;
  uint8_t _probe = 0;
  uint32_t _probeStartMs = 0;
  uint32_t _lastGoodMs = 0;
  uint32_t _lastPassed = 0;
  bool _locked = false;
};
