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
  char type[12];  // slug from kSightingTypes, or kDefaultSightingType
};

struct SightingType {
  char key;           // lowercase letter that picks it after logging
  const char* slug;   // stored in the log; no commas, under 12 characters
  const char* label;  // shown on screen
};

// Quick tags, also labeled in docs/app.js. Changing a key doesn't affect logged
// sightings, since they store the slug.
static const SightingType kSightingTypes[] = {
    {'f', "flock", "Flock"},
    {'v', "vigilant", "Vigilant"},
    {'g', "genetec", "Genetec"},
    {'a', "axon", "Axon"},
};
// Used when no tag is chosen. Older logs may have this field empty.
static const char kDefaultSightingType[] = "other";
static const int kNumSightingTypes =
    sizeof(kSightingTypes) / sizeof(kSightingTypes[0]);

inline const char* sightingTypeLabel(const char* slug) {
  if (slug[0] == '\0' || strcmp(slug, kDefaultSightingType) == 0) return "Other";
  for (int i = 0; i < kNumSightingTypes; i++) {
    if (strcmp(kSightingTypes[i].slug, slug) == 0) return kSightingTypes[i].label;
  }
  return slug;
}
