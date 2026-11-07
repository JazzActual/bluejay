#include "serial_console.h"
#include <Arduino.h>
#include <M5Unified.h>

#ifdef BJ_TEST_SERIAL
void bjTestCommand(const String& command);  // main.cpp
#endif

namespace {
String line;

void printHelp() {
  Serial.println(
      "bluejay commands:\n"
      "  status        GPS, log, and battery status\n"
      "  dump          print log as CSV\n"
      "  geojson       print log as GeoJSON\n"
      "  clear DELETE  erase all sightings");
}

void run(const String& cmd, LogStore& store, GpsLink& gps) {
  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "status") {
    Serial.printf("gps_link=%s rx=G%d baud=%lu fix=%s sats=%lu hdop=%.2f\n",
                  gps.linkUp() ? "up" : "probing", gps.rxPin(),
                  (unsigned long)gps.baud(), gps.hasFix() ? "yes" : "no",
                  (unsigned long)gps.satellites(), gps.hdop());
    Serial.printf("sightings=%lu\n", (unsigned long)store.count());
    const auto charging = M5.Power.isCharging();
    Serial.printf("battery=%ld%% %dmV charging=%s\n", (long)M5.Power.getBatteryLevel(),
                  (int)M5.Power.getBatteryVoltage(),
                  charging == m5::Power_Class::is_charging ? "yes"
                  : charging == m5::Power_Class::is_discharging ? "no" : "unknown");
  } else if (cmd == "dump") {
    Serial.println("-----BEGIN CSV-----");
    store.streamCsv(Serial);
    Serial.println("-----END CSV-----");
  } else if (cmd == "geojson") {
    Serial.println("-----BEGIN GEOJSON-----");
    store.streamGeoJson(Serial);
    Serial.println("-----END GEOJSON-----");
  } else if (cmd == "clear DELETE") {
    Serial.println(store.clear() ? "cleared" : "clear FAILED");
  } else if (cmd == "clear") {
    Serial.println("type 'clear DELETE' to erase all sightings");
#ifdef BJ_TEST_SERIAL
  } else if (cmd.startsWith("t ")) {
    bjTestCommand(cmd.substring(2));
#endif
  } else if (cmd.length()) {
    Serial.printf("unknown command: %s (try 'help')\n", cmd.c_str());
  }
}
}  // namespace

namespace SerialConsole {
void poll(LogStore& store, GpsLink& gps) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line.trim();
      run(line, store, gps);
      line = "";
    } else if (line.length() < 64) {
      line += c;
    }
  }
}
}  // namespace SerialConsole
