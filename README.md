```
 ▄▄▄▄    ██▓     █    ██ ▓█████ ▄▄▄██▀▀▀▄▄▄     ▓██   ██▓
▓█████▄ ▓██▒     ██  ▓██▒▓█   ▀   ▒██  ▒████▄    ▒██  ██▒
▒██▒ ▄██▒██░    ▓██  ▒██░▒███     ░██  ▒██  ▀█▄   ▒██ ██░
▒██░█▀  ▒██░    ▓▓█  ░██░▒▓█  ▄▓██▄██▓ ░██▄▄▄▄██  ░ ▐██▓░
░▓█  ▀█▓░██████▒▒▒█████▓ ░▒████▒▓███▒   ▓█   ▓██▒ ░ ██▒▓░
░▒▓███▀▒░ ▒░▓  ░░▒▓▒ ▒ ▒ ░░ ▒░ ░▒▓▒▒░   ▒▒   ▓▒█░  ██▒▒▒ 
▒░▒   ░ ░ ░ ▒  ░░░▒░ ░ ░  ░ ░  ░▒ ░▒░    ▒   ▒▒ ░▓██ ░▒░ 
 ░    ░   ░ ░    ░░░ ░ ░    ░   ░ ░ ░    ░   ▒   ▒ ▒ ░░  
 ░          ░  ░   ░        ░  ░░   ░        ░  ░░ ░     
      ░                                          ░ ░     
```

Named for the Blue Jay, a bird that is known for raiding the nests of other birds and eating the eggs: BlueJay is an ESP32 firmware that helps visually log Flock cameras and other ALPRs for more precise reporting at a later time.

## Hardware

Controller (pick one):
- [M5StickS3](https://docs.m5stack.com/en/core/StickS3) (ESP32-S3-PICO-1-N8R8, 135x240 ST7789P3, battery, speaker). The GPS goes on Grove G9/G10.
- [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R) (ESP32-S3-PICO-1-N8R8, 128x128, USB-C powered, one button). The GPS goes on Grove G2/G1.
- [Cardputer ADV](https://docs.m5stack.com/en/core/Cardputer-Adv) (ESP32-S3FN8, 240x135, battery, speaker). The GPS goes on Grove G2/G1.

GPS (both use the AT6668 chip and send NMEA at 115200 baud):
- [Unit GPS v1.1](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1). Plugs into the controller's Grove port. Works with any controller.
- [Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262) (ATGM336H). Plugs into the Cardputer ADV's EXT header and talks to it on G15 (RX) and G13 (TX). Keep its LoRa antenna attached.
- [Atomic GPS Base v2.0](https://docs.m5stack.com/en/atom/Atomic_GPS_Base_v2.0) (ATGM336H). The AtomS3R stacks on top and talks to it on G5 (RX) and G6 (TX). AtomS3R only. It has a built-in antenna, so it needs a view of the sky.

On the AtomS3R the firmware checks for the Atomic GPS Base first and then falls back to the Grove port, so the same firmware works with either GPS.

At boot the firmware tries both Grove pin orientations at 115200 and then 9600 baud, and uses the first one that returns valid NMEA. On the StickS3 it also switches the Grove 5V rail to output so the GPS unit gets power. On the AtomS3R the Grove 5V comes directly from USB.

## Build & flash

[Install from your browser](https://jazzactual.github.io/bluejay/flash.html) with Chrome or Edge, or build it yourself:

```sh
brew install platformio
pio run -e m5stack-sticks3 -t upload   # StickS3 (the default env)
pio run -e m5stack-atoms3r -t upload   # AtomS3R
pio run -e m5stack-cardputer-adv -t upload   # Cardputer ADV
pio device monitor       # serial console, 115200
```

## Usage

| StickS3 | AtomS3R | Action |
|---|---|---|
| Button A (front) | Tap the screen | Log a sighting at the current fix. Refused with a red "NO FIX" screen if the fix is stale (>3 s) or missing. |
| Button B (side) | | Turn the screen on, or open the menu if it's on |
| | Hold the screen for 1.5 s | Turn the Wi-Fi export AP on or off |

In the StickS3 menu, B moves to the next item, A selects, and holding B for 1 s goes back.

On the Cardputer ADV, Enter logs a sighting and Esc opens the menu. Press F (Flock), V (Vigilant), G (Genetec), or A (Axon) within 5 s of logging to tag it, or it's logged as other. Keep the power switch OFF when connecting it to a computer.

The AtomS3R has no speaker, so the green or red screen flash is the only confirmation.

The screen turns off after 30 s unless Wi-Fi is on. The GPS keeps its fix and logging still works while it's off.

Each sighting is appended to `/sightings.csv` on internal flash (LittleFS) with the columns `id,utc,lat,lon,alt_m,hdop,sats,type`.

### Export over Wi-Fi
Turn on Wi-Fi export in the menu (or hold the AtomS3R button), join `bluejay-XXXX` using the password shown on screen, then open `http://192.168.4.1/` to download CSV or GeoJSON, change the password, or clear the log.

The password is random per device.

### Export over USB serial
Open the serial monitor and type:
- `status`: GPS link, fix, and log count
- `dump`: the log as CSV
- `geojson`: the log as GeoJSON
- `clear DELETE`: erase the log

## Map viewer
[Open the map viewer](https://jazzactual.github.io/bluejay/) to map a CSV or GeoJSON export. Files stay in the browser.
