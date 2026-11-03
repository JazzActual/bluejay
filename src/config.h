#pragma once

// ---- GPS unit (Unit GPS v1.1, AT6668) on the StickS3 Grove port (PORT.A) ----
// Grove: Yellow = G9, White = G10. The GPS unit transmits on one of them; the
// firmware probes both orientations and both common baud rates at boot, so
// these are just the first combination tried.
#define BJ_GPS_RX_PIN 10        // ESP32 RX  <- GPS TX
#define BJ_GPS_TX_PIN 9         // ESP32 TX  -> GPS RX
#define BJ_GPS_BAUD 115200      // AT6668 factory default
#define BJ_GPS_PROBE_MS 3000    // time spent on each pin/baud combination
#define BJ_GPS_LINK_TIMEOUT_MS 10000  // re-probe if valid NMEA stops this long

// A fix older than this is treated as no fix and logging is refused.
#define BJ_MAX_FIX_AGE_MS 3000

// ---- Storage ----
#define BJ_LOG_PATH "/sightings.csv"

// ---- Wi-Fi export access point ----
// SSID becomes e.g. "bluejay-A1B2" (last bytes of the MAC).
#define BJ_AP_SSID_PREFIX "bluejay-"
#define BJ_AP_PASSWORD "bluejay123" // WPA2, must be >= 8 chars. Change this.

// ---- Feedback ----
#define BJ_SPEAKER_VOLUME 64    // 0-255. Keep well under ~75% on battery.
