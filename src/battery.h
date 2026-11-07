#pragma once
#include <stdint.h>

// Battery level from voltage, using separate curves for charging and not, since
// a charger lifts the voltage well above the battery's real charge.
namespace Battery {
void begin();         // takes a few quick readings to start the average
void poll();          // call every loop; samples every 2 s
uint8_t percent();    // 0-100
float volts();        // averaged
bool powered();       // charging, or a rising voltage where that can't be read
}  // namespace Battery
