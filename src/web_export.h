#pragma once
#include <Arduino.h>
#include "log_store.h"

// Soft-AP + tiny web server for downloading the log from a phone/laptop.
namespace WebExport {
void start(LogStore& store);
void stop();
void poll();
bool active();
const char* ssid();
String ip();
}  // namespace WebExport
