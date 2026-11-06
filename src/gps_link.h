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

  // From the receiver's status sentences, so these work without a fix.
  uint32_t satellitesInView() const;
  int bestSignalDb() const { return _bestSignalDb; }  // 0 if no signal
  bool antennaOpen() const { return _antennaOpen; }  // may just mean a passive antenna

  // Fill a sighting from the current fix (id left as 0). False if no fix.
  bool snapshot(Sighting& out);

  int rxPin() const;
  uint32_t baud() const;

 private:
  void applyProbe(uint8_t idx);
  void parseStatusLine();

  TinyGPSPlus _gps;
  uint8_t _probe = 0;
  uint32_t _probeStartMs = 0;
  uint32_t _lastGoodMs = 0;
  uint32_t _lastPassed = 0;
  bool _locked = false;

  static constexpr int kNumTalkers = 6;
  char _line[100] = {0};
  uint8_t _lineLen = 0;
  uint8_t _inView[kNumTalkers] = {0};
  int _signalThisCycleDb = 0;
  int _bestSignalDb = 0;
  bool _antennaOpen = false;
};
