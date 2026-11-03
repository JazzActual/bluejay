#pragma once
#include <Arduino.h>
#include "sighting.h"

// Append-only CSV log on LittleFS.
class LogStore {
 public:
  bool begin();
  bool append(Sighting& s);  // assigns s.id
  bool clear();
  uint32_t count() const { return _count; }
  bool last(Sighting& out) const;

  void streamCsv(Print& out) const;
  void streamGeoJson(Print& out) const;

 private:
  static bool parseLine(const String& line, Sighting& s);
  bool writeHeader();

  uint32_t _count = 0;
  uint32_t _nextId = 1;
  Sighting _last{};
  bool _hasLast = false;
};
