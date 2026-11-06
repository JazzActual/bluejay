#!/bin/sh
# Builds the firmware and copies it to docs/firmware for the web flasher.
# Run this from a clean commit once the firmware has been tested on a device.
set -eu

cd "$(dirname "$0")/.."

if [ -n "$(git status --porcelain -- src platformio.ini data)" ]; then
  echo "Commit your firmware changes first, so the published version is traceable." >&2
  exit 1
fi

version=$(git rev-parse --short HEAD)
core_dir=$(pio system info --json-output | python3 -c 'import json,sys; print(json.load(sys.stdin)["core_dir"]["value"])')
boot_app0="$core_dir/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

# Separate parts, not a merged image: a merged image would overwrite the gap at
# 0x9000 holding NVS, which stores each device's Wi-Fi password.
for env in m5stack-sticks3 m5stack-atoms3r; do
  pio run -e "$env"
  out="docs/firmware/$env"
  mkdir -p "$out"
  cp ".pio/build/$env/bootloader.bin" ".pio/build/$env/partitions.bin" ".pio/build/$env/firmware.bin" "$boot_app0" "$out/"

  for term in "$HOME" "$(whoami)" "$(hostname -s)"; do
    if LC_ALL=C grep -a -q -i -F -- "$term" "$out"/*.bin; then
      echo "Refusing to publish: a binary in $out contains a local path or name." >&2
      rm -rf "$out"
      exit 1
    fi
  done

  cat > "$out/manifest.json" <<EOF
{
  "name": "BlueJay",
  "version": "$version",
  "new_install_prompt_erase": true,
  "new_install_improv_wait_time": 0,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
EOF
done

echo "Updated docs/firmware to $version"
