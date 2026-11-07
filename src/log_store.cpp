#include "log_store.h"
#include <LittleFS.h>
#include "config.h"

static const char kHeader[] = "id,utc,lat,lon,alt_m,hdop,sats,type";
static const char kTempPath[] = BJ_LOG_PATH ".tmp";

bool LogStore::begin() {
  if (!LittleFS.begin(true)) {
    log_e("LittleFS mount failed");
    return false;
  }

  // A leftover temp file means a rewrite was interrupted. Rename replaces the
  // log in one step, so the log itself is intact unless it's missing.
  if (LittleFS.exists(kTempPath)) {
    if (LittleFS.exists(BJ_LOG_PATH)) LittleFS.remove(kTempPath);
    else LittleFS.rename(kTempPath, BJ_LOG_PATH);
  }

  if (!LittleFS.exists(BJ_LOG_PATH)) return writeHeader();

  File f = LittleFS.open(BJ_LOG_PATH, "r");
  if (!f) return false;
  String header = f.readStringUntil('\n');
  f.close();
  header.trim();
  // Logs from older firmware have no type column.
  if (header != kHeader && !rewrite(false, nullptr)) return false;

  return loadState();
}

bool LogStore::loadState() {
  _count = 0;
  _nextId = 1;
  _hasLast = false;

  File f = LittleFS.open(BJ_LOG_PATH, "r");
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
  File f = LittleFS.open(BJ_LOG_PATH, "w");
  if (!f) return false;
  f.print(kHeader);
  f.print('\n');
  f.close();
  return true;
}

size_t LogStore::printRow(Print& out, const Sighting& s) {
  return out.printf("%lu,%s,%.7f,%.7f,%.1f,%.2f,%lu,%s\n", (unsigned long)s.id, s.utc,
             s.lat, s.lon, s.alt_m, s.hdop, (unsigned long)s.sats, s.type);
}

bool LogStore::append(Sighting& s) {
  File f = LittleFS.open(BJ_LOG_PATH, "a");
  if (!f) return false;
  s.id = _nextId;
  const size_t written = printRow(f, s);
  f.close();
  if (written == 0) return false;
  _nextId++;
  _count++;
  _last = s;
  _hasLast = true;
  return true;
}

bool LogStore::clear() {
  LittleFS.remove(BJ_LOG_PATH);
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

// Copies the log to a temp file, changing or dropping the last sighting, then
// swaps it in. The newest row is held back so the last one is known when the
// end of the file is reached. Rows that don't parse are kept as they are.
bool LogStore::rewrite(bool dropLast, const Sighting* replaceLast) {
  File in = LittleFS.open(BJ_LOG_PATH, "r");
  if (!in) return false;
  File out = LittleFS.open(kTempPath, "w");
  if (!out) {
    in.close();
    return false;
  }

  out.print(kHeader);
  out.print('\n');
  in.readStringUntil('\n');  // old header

  Sighting held;
  bool holding = false;
  while (in.available()) {
    String line = in.readStringUntil('\n');
    Sighting s;
    if (parseLine(line, s)) {
      if (holding) printRow(out, held);
      held = s;
      holding = true;
    } else {
      line.trim();
      if (line.length()) {
        out.print(line);
        out.print('\n');
      }
    }
  }
  if (holding && !dropLast) printRow(out, replaceLast ? *replaceLast : held);
  in.close();
  const bool written = out.getWriteError() == 0;
  out.close();

  if (!written || !LittleFS.rename(kTempPath, BJ_LOG_PATH)) {
    LittleFS.remove(kTempPath);
    return false;
  }
  return true;
}

bool LogStore::setLastType(const char* slug) {
  if (!_hasLast) return false;
  Sighting updated = _last;
  strlcpy(updated.type, slug, sizeof(updated.type));
  if (!rewrite(false, &updated)) return false;
  _last = updated;
  return true;
}

bool LogStore::removeLast() {
  if (!_hasLast) return false;
  if (!rewrite(true, nullptr)) return false;
  return loadState();
}

bool LogStore::fromEnd(uint32_t index, Sighting& out) const {
  if (index >= _count) return false;
  if (index == 0) return last(out);
  const uint32_t target = _count - 1 - index;

  File f = LittleFS.open(BJ_LOG_PATH, "r");
  if (!f) return false;
  f.readStringUntil('\n');  // header
  uint32_t i = 0;
  bool found = false;
  while (f.available()) {
    if (!parseLine(f.readStringUntil('\n'), out)) continue;
    if (i++ == target) {
      found = true;
      break;
    }
  }
  f.close();
  return found;
}

bool LogStore::parseLine(const String& line, Sighting& s) {
  unsigned long id = 0, sats = 0;
  char utc[21] = {0};
  char type[12] = {0};
  float alt = NAN, hdop = NAN;
  double lat = 0, lon = 0;
  // alt may be "nan"; sscanf %f accepts it. Type is empty when untagged, and
  // missing from rows written by older firmware.
  int n = sscanf(line.c_str(), "%lu,%20[^,],%lf,%lf,%f,%f,%lu,%11[^,\r\n]", &id,
                 utc, &lat, &lon, &alt, &hdop, &sats, type);
  if (n < 7) return false;
  s.id = id;
  memcpy(s.utc, utc, sizeof(s.utc));
  s.lat = lat;
  s.lon = lon;
  s.alt_m = alt;
  s.hdop = hdop;
  s.sats = sats;
  memcpy(s.type, type, sizeof(s.type));
  return true;
}

void LogStore::streamCsv(Print& out) const {
  File f = LittleFS.open(BJ_LOG_PATH, "r");
  if (!f) return;
  uint8_t buf[512];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) out.write(buf, n);
  f.close();
}

void LogStore::streamGeoJson(Print& out) const {
  out.print("{\"type\":\"FeatureCollection\",\"features\":[");
  File f = LittleFS.open(BJ_LOG_PATH, "r");
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
      if (s.type[0]) out.printf(",\"type\":\"%s\"", s.type);
      out.print("}}");
    }
    f.close();
  }
  out.print("\n]}\n");
}
