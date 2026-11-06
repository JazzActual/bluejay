# data/

Source audio for sounds built into the firmware.

- `bluejaycall.mp3`: the call played on the StickS3 after each successful log.
- `bluejaycall.wav`: the copy that gets embedded in the firmware (`board_build.embed_files` in `platformio.ini`). To swap in a different call, replace the mp3 and regenerate the WAV:

  ```sh
  afconvert -f WAVE -d LEI16@22050 -c 1 --mix data/bluejaycall.mp3 data/bluejaycall.wav
  ```

  Keep the clip to 1–2 seconds and trimmed tight, and check the recording's license before you share builds.

Don't run `pio run -t uploadfs`. PlatformIO would upload this folder as the LittleFS image, which replaces the internal filesystem and erases `/sightings.csv`.
