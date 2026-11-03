#pragma once

// ---- GPS unit (Unit GPS v1.1, AT6668) on the StickS3 Grove port (PORT.A) ----
// Grove: Yellow = G9, White = G10. The GPS unit transmits on one of them; the
// firmware probes both orientations and both common baud rates at boot, so
// these are just the first combination tried.
#define BB_GPS_RX_PIN 10        // ESP32 RX  <- GPS TX
#define BB_GPS_TX_PIN 9         // ESP32 TX  -> GPS RX
#define BB_GPS_BAUD 115200      // AT6668 factory default
#define BB_GPS_PROBE_MS 3000    // time spent on each pin/baud combination
#define BB_GPS_LINK_TIMEOUT_MS 10000  // re-probe if valid NMEA stops this long

// A fix older than this is treated as no fix and logging is refused.
#define BB_MAX_FIX_AGE_MS 3000

// ---- Storage ----
#define BB_LOG_PATH "/sightings.csv"

// ---- Wi-Fi export access point ----
// SSID becomes e.g. "birdbanger-A1B2" (last bytes of the MAC).
#define BB_AP_SSID_PREFIX "birdbanger-"
#define BB_AP_PASSWORD "birdbanger"   // WPA2, must be >= 8 chars. Change this.

// ---- Feedback ----
#define BB_SPEAKER_VOLUME 64    // 0-255. Keep well under ~75% on battery.
