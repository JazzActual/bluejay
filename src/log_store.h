#pragma once
#include <Arduino.h>
#include "sighting.h"

// CSV log on LittleFS. New sightings are appended; changing the last one
// rewrites the file through a temporary copy.
class LogStore {
 public:
  bool begin();
  bool append(Sighting& s);  // assigns s.id
  bool clear();
  uint32_t count() const { return _count; }
  bool last(Sighting& out) const;

  bool setLastType(const char* slug);
  bool removeLast();
  bool fromEnd(uint32_t index, Sighting& out) const;  // 0 is the newest

  void streamCsv(Print& out) const;
  void streamGeoJson(Print& out) const;

 private:
  static bool parseLine(const String& line, Sighting& s);
  static size_t printRow(Print& out, const Sighting& s);
  bool writeHeader();
  bool loadState();
  bool rewrite(bool dropLast, const Sighting* replaceLast);

  uint32_t _count = 0;
  uint32_t _nextId = 1;
  Sighting _last{};
  bool _hasLast = false;
};
