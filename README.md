# BlueJay
BlueJay is an ESP32 firmware that helps visually log Flock cameras and other ALPRs for more precise reporting at a later time.

## Hardware

Controller (pick one):
- [M5StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO-1-N8R8, 135x240 ST7789P3, battery, speaker). The GPS goes on Grove G9/G10.
- [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R) (ESP32-S3-PICO-1-N8R8, 128x128, USB-C powered, one button). The GPS goes on Grove G2/G1.

GPS (both use the AT6668 chip and send NMEA at 115200 baud):
- [Unit GPS v1.1](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1). Plugs into the controller's Grove port. Works with either controller.
- [Atomic GPS Base v2.0](https://docs.m5stack.com/en/atom/Atomic_GPS_Base_v2.0) (ATGM336H). The AtomS3R stacks on top and talks to it on G5 (RX) and G6 (TX). AtomS3R only. It has a built-in antenna, so it needs a view of the sky.

On the AtomS3R the firmware checks for the Atomic GPS Base first and then falls back to the Grove port, so the same firmware works with either GPS.

At boot the firmware tries both Grove pin orientations at 115200 and then 9600 baud, and uses the first one that returns valid NMEA. On the StickS3 it also switches the Grove 5V rail to output so the GPS unit gets power. On the AtomS3R the Grove 5V comes directly from USB.

## Build & flash

```sh
brew install platformio
pio run -e m5stack-sticks3 -t upload   # StickS3 (the default env)
pio run -e m5stack-atoms3r -t upload   # AtomS3R
pio device monitor       # serial console, 115200
```

## Usage

| StickS3 | AtomS3R | Action |
|---|---|---|
| Button A (front) | Tap the screen | Log a sighting at the current fix. Refused with a red "NO FIX" screen if the fix is stale (>3 s) or missing. |
| Button B (side) | Hold the screen for 1.5 s | Turn the Wi-Fi export AP on or off |

The AtomS3R has no speaker, so the green or red screen flash is the only confirmation.

Each sighting is appended to `/sightings.csv` on internal flash (LittleFS) with the columns `id,utc,lat,lon,alt_m,hdop,sats`.

### Export over Wi-Fi
Press B (or hold the AtomS3R button), join `bluejay-XXXX` using the password shown on screen, then open `http://192.168.4.1/` to download CSV or GeoJSON, change the password, or clear the log.

The password is random per device.

### Export over USB serial
Open the serial monitor and type:
- `status`: GPS link, fix, and log count
- `dump`: the log as CSV
- `geojson`: the log as GeoJSON
- `clear DELETE`: erase the log

## Map viewer
`docs/` is a GitHub Pages site that maps a CSV or GeoJSON export. Files stay in the browser.
