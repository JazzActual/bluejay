#pragma once
#include "gps_link.h"
#include "log_store.h"

// Line-based commands over USB serial (115200). Type "help".
namespace SerialConsole {
void poll(LogStore& store, GpsLink& gps);
}
