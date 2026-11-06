#pragma once
#include <stdint.h>
#include <string.h>

struct Sighting {
  uint32_t id;
  char utc[21];   // "YYYY-MM-DDTHH:MM:SSZ"
  double lat;
  double lon;
  float alt_m;
  float hdop;
  uint32_t sats;
  char type[12];  // slug from kSightingTypes, or empty if untagged
};

struct SightingType {
  const char* slug;   // stored in the log; no commas, under 12 characters
  const char* label;  // shown on screen
};

// Quick tags: the number key after logging picks the entry at that position.
static const SightingType kSightingTypes[] = {
    {"flock", "Flock"},
    {"alpr", "Other ALPR"},
    {"unknown", "Unknown"},
};
static const int kNumSightingTypes =
    sizeof(kSightingTypes) / sizeof(kSightingTypes[0]);

inline const char* sightingTypeLabel(const char* slug) {
  for (int i = 0; i < kNumSightingTypes; i++) {
    if (strcmp(kSightingTypes[i].slug, slug) == 0) return kSightingTypes[i].label;
  }
  return slug;
}
