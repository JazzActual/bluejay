# birdbanger
Named for the pyrotechnic device that avian control specialists use to prevent damage to crops, aircraft, and property. Bird banger is an ESP32 firmware that helps visually log flock cameras and other ALPRs for more precise reporting at a later time.

## Hardware

- [M5StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO-1-N8R8, 135x240 ST7789P3)
- [Unit GPS v1.1](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1) (AT6668, NMEA over UART, 115200 baud) plugged into the StickS3 Grove port (G9/G10)

At boot the firmware tries both Grove pin orientations at 115200 and then 9600 baud, and uses the first one that returns valid NMEA. It also switches the Grove 5V rail to output so the GPS unit gets power.

## Build & flash

```sh
brew install platformio
pio run -t upload        # build + flash over USB-C
pio device monitor       # serial console, 115200
```

## Usage

| Input | Action |
|---|---|
| Button A (front) | Log a sighting at the current fix. Refused with a red "NO FIX" screen if the fix is stale (>3 s) or missing. |
| Button B (side) | Turn the Wi-Fi export AP on or off |

Each sighting is appended to `/sightings.csv` on internal flash (LittleFS) with the columns `id,utc,lat,lon,alt_m,hdop,sats`.

### Export over Wi-Fi
Press B, join `birdbanger-XXXX` (the password is shown on screen and set in `src/config.h`), then open `http://192.168.4.1/` to download CSV or GeoJSON, or to clear the log.

### Export over USB serial
Open the serial monitor and type:
- `status`: GPS link, fix, and log count
- `dump`: the log as CSV
- `geojson`: the log as GeoJSON
- `clear DELETE`: erase the log
