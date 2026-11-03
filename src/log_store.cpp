#include "log_store.h"
#include <LittleFS.h>
#include "config.h"

static const char kHeader[] = "id,utc,lat,lon,alt_m,hdop,sats";

bool LogStore::begin() {
  if (!LittleFS.begin(true)) {
    log_e("LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(BB_LOG_PATH)) return writeHeader();

  File f = LittleFS.open(BB_LOG_PATH, "r");
  if (!f) return false;
  f.readStringUntil('\n');  // header
  while (f.available()) {
    String line = f.readStringUntil('\n');
    Sighting s;
    if (!parseLine(line, s)) continue;
    _count++;
    if (s.id >= _nextId) _nextId = s.id + 1;
    _last = s;
    _hasLast = true;
  }
  f.close();
  log_i("Log: %lu sightings, next id %lu", (unsigned long)_count,
        (unsigned long)_nextId);
  return true;
}

bool LogStore::writeHeader() {
  File f = LittleFS.open(BB_LOG_PATH, "w");
  if (!f) return false;
  f.println(kHeader);
  f.close();
  return true;
}

bool LogStore::append(Sighting& s) {
  File f = LittleFS.open(BB_LOG_PATH, "a");
  if (!f) return false;
  s.id = _nextId;
  size_t n = f.printf("%lu,%s,%.7f,%.7f,%.1f,%.2f,%lu\n", (unsigned long)s.id,
                      s.utc, s.lat, s.lon, s.alt_m, s.hdop,
                      (unsigned long)s.sats);
  f.close();
  if (n == 0) return false;
  _nextId++;
  _count++;
  _last = s;
  _hasLast = true;
  return true;
}

bool LogStore::clear() {
  LittleFS.remove(BB_LOG_PATH);
  _count = 0;
  _nextId = 1;
  _hasLast = false;
  return writeHeader();
}

bool LogStore::last(Sighting& out) const {
  if (!_hasLast) return false;
  out = _last;
  return true;
}

bool LogStore::parseLine(const String& line, Sighting& s) {
  unsigned long id = 0, sats = 0;
  char utc[21] = {0};
  float alt = NAN, hdop = NAN;
  double lat = 0, lon = 0;
  // alt may be "nan"; sscanf %f accepts it.
  int n = sscanf(line.c_str(), "%lu,%20[^,],%lf,%lf,%f,%f,%lu", &id, utc, &lat,
                 &lon, &alt, &hdop, &sats);
  if (n != 7) return false;
  s.id = id;
  memcpy(s.utc, utc, sizeof(s.utc));
  s.lat = lat;
  s.lon = lon;
  s.alt_m = alt;
  s.hdop = hdop;
  s.sats = sats;
  return true;
}

void LogStore::streamCsv(Print& out) const {
  File f = LittleFS.open(BB_LOG_PATH, "r");
  if (!f) return;
  uint8_t buf[512];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) out.write(buf, n);
  f.close();
}

void LogStore::streamGeoJson(Print& out) const {
  out.print("{\"type\":\"FeatureCollection\",\"features\":[");
  File f = LittleFS.open(BB_LOG_PATH, "r");
  if (f) {
    f.readStringUntil('\n');
    bool first = true;
    while (f.available()) {
      Sighting s;
      if (!parseLine(f.readStringUntil('\n'), s)) continue;
      if (!first) out.print(',');
      first = false;
      out.printf(
          "\n{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\","
          "\"coordinates\":[%.7f,%.7f]},\"properties\":{\"id\":%lu,"
          "\"utc\":\"%s\",\"hdop\":%.2f,\"sats\":%lu",
          s.lon, s.lat, (unsigned long)s.id, s.utc, s.hdop,
          (unsigned long)s.sats);
      if (!isnan(s.alt_m)) out.printf(",\"alt_m\":%.1f", s.alt_m);
      out.print("}}");
    }
    f.close();
  }
  out.print("\n]}\n");
}
