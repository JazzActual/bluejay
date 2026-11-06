#pragma once

// ---- Board selection (set by the PlatformIO env) ----
// GPS pins: the Unit GPS v1.1 plugs into the board's Grove port. It transmits
// on one of the two signal lines; the firmware probes both orientations and
// both common baud rates at boot, so the pins below are just the first
// combination tried.
#if defined(BJ_BOARD_STICKS3)
  #define BJ_BOARD_NAME "StickS3"
  #define BJ_GPS_RX_PIN 10      // Grove white  (ESP32 RX <- GPS TX)
  #define BJ_GPS_TX_PIN 9       // Grove yellow (ESP32 TX -> GPS RX)
  #define BJ_HAS_BATTERY 1
  #define BJ_HAS_SPEAKER 1
  #define BJ_HAS_BTN_B 1        // side button toggles Wi-Fi
  #define BJ_GROVE_5V_SWITCHED 1  // Grove 5V must be switched to output
#elif defined(BJ_BOARD_ATOMS3R)
  #define BJ_BOARD_NAME "AtomS3R"
  #define BJ_GPS_RX_PIN 1       // Grove white  (ESP32 RX <- GPS TX)
  #define BJ_GPS_TX_PIN 2       // Grove yellow (ESP32 TX -> GPS RX)
  // Atomic GPS Base v2.0 (ATGM336H / AT6668) stacked under the Atom. Fixed
  // wiring, 115200 baud. Probed first; the Grove Unit GPS is the fallback.
  #define BJ_GPS_BASE_RX_PIN 5  // ESP32 RX <- base UART_TX
  #define BJ_GPS_BASE_TX_PIN 6  // ESP32 TX -> base UART_RX
  #define BJ_HAS_BATTERY 0      // USB-C powered only
  #define BJ_HAS_SPEAKER 0
  #define BJ_HAS_BTN_B 0        // single button: hold to toggle Wi-Fi
  #define BJ_GROVE_5V_SWITCHED 0  // Grove 5V comes straight from USB 5V
#else
  #error "No board selected: build with -e m5stack-sticks3 or -e m5stack-atoms3r"
#endif

// Single-button boards: hold this long to toggle the Wi-Fi export AP.
#define BJ_HOLD_WIFI_MS 1500

// ---- GPS unit (Unit GPS v1.1, AT6668) ----
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
// The password is generated per device on first boot (see ap_password.h) and can
// be changed from the web page. The maximum keeps it readable on small screens.
#define BJ_AP_PW_PREFIX "bluejay"
#define BJ_AP_PW_RANDOM_CHARS 8
#define BJ_AP_PW_MIN_LEN 8      // WPA2 minimum
#define BJ_AP_PW_MAX_LEN 16

// ---- Feedback ----
#define BJ_SPEAKER_VOLUME 64    // 0-255. Keep well under ~75% on battery.
