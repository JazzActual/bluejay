#pragma once
#include <stdint.h>

struct Sighting {
  uint32_t id;
  char utc[21];   // "YYYY-MM-DDTHH:MM:SSZ"
  double lat;
  double lon;
  float alt_m;
  float hdop;
  uint32_t sats;
};
