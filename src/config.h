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
  #define BJ_HAS_BTN_B 1        // side button opens and steps through the menu
  #define BJ_HAS_KEYBOARD 0
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
  #define BJ_HAS_KEYBOARD 0
  #define BJ_GROVE_5V_SWITCHED 0  // Grove 5V comes straight from USB 5V
#elif defined(BJ_BOARD_CARDPUTER_ADV)
  #define BJ_BOARD_NAME "Cardputer ADV"
  #define BJ_GPS_RX_PIN 1       // Grove white  (ESP32 RX <- GPS TX)
  #define BJ_GPS_TX_PIN 2       // Grove yellow (ESP32 TX -> GPS RX)
  // Cap LoRa-1262 (ATGM336H / AT6668) on the EXT header. Fixed wiring, 115200
  // baud. Probed first; the Grove Unit GPS is the fallback.
  #define BJ_GPS_BASE_RX_PIN 15 // ESP32 RX <- cap GPS TX
  #define BJ_GPS_BASE_TX_PIN 13 // ESP32 TX -> cap GPS RX
  #define BJ_HAS_BATTERY 1
  #define BJ_HAS_SPEAKER 1
  #define BJ_HAS_BTN_B 0
  #define BJ_HAS_KEYBOARD 1     // Enter logs, Esc opens the menu; G0 is unused
  #define BJ_GROVE_5V_SWITCHED 0
#else
  #error "No board selected: build with -e m5stack-sticks3, -e m5stack-atoms3r, or -e m5stack-cardputer-adv"
#endif

#define BJ_HAS_MENU (BJ_HAS_KEYBOARD || BJ_HAS_BTN_B)

#define BJ_HOLD_WIFI_MS 1500       // single-button boards: hold to toggle Wi-Fi
#define BJ_BTN_B_HOLD_BACK_MS 1000 // boards with Button B: hold to go back in the menu

// ---- GPS unit (Unit GPS v1.1, AT6668) ----
#define BJ_GPS_BAUD 115200      // AT6668 factory default
#define BJ_GPS_PROBE_MS 3000    // time spent on each pin/baud combination
#define BJ_GPS_LINK_TIMEOUT_MS 10000  // re-probe if valid NMEA stops this long

// A fix older than this is treated as no fix and logging is refused.
#define BJ_MAX_FIX_AGE_MS 3000

// ---- Storage ----
#define BJ_LOG_PATH "/sightings.csv"

// Keyboard boards: time after logging to press a number key and tag the sighting.
#define BJ_TAG_WINDOW_MS 5000

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
#define BJ_BEEP_HZ 2000          // 400 Hz is nearly inaudible on the StickS3 speaker
#define BJ_NO_FIX_BEEP_MS 300
#define BJ_ERROR_BEEP_MS 600

// ---- Power ----
#define BJ_CPU_MHZ 80           // lowest speed Wi-Fi supports
#define BJ_LOOP_DELAY_MS 10     // idle time per loop; the GPS UART buffers meanwhile
#define BJ_SCREEN_BRIGHTNESS 80 // 0-255
#define BJ_SCREEN_TIMEOUT_MS 30000  // stays on while Wi-Fi is on
