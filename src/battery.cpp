// The voltage curves and power detection follow M5PORKCHOP's charging mode:
// https://github.com/0ct0sec/M5PORKCHOP
//
// MIT License. Copyright (c) 2025 0ct0
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "battery.h"
#include <M5Unified.h>

namespace {
const int kPoints = 11;
const float kDischargeV[kPoints] = {3.00f, 3.30f, 3.50f, 3.60f, 3.70f, 3.75f,
                                    3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
const uint8_t kDischargePct[kPoints] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 85, 100};
// Flat at the top: the charger holds the voltage near 4.2 V while it finishes.
const float kChargeV[kPoints] = {3.50f, 3.70f, 3.85f, 3.95f, 4.05f, 4.10f,
                                 4.13f, 4.16f, 4.18f, 4.19f, 4.20f};
const uint8_t kChargePct[kPoints] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

const uint32_t kSampleMs = 2000;
const int kHistory = 10;
const float kTrendRiseV = 0.010f;  // a charger has been plugged in
const float kTrendDropV = 0.030f;  // and unplugged again

float history[kHistory];
int historyCount = 0;
int historyNext = 0;
uint32_t lastSampleMs = 0;
float average = 0;
float entryV = 0;
float peakV = 0;
bool trendPowered = false;
bool powerNow = false;

uint8_t toPercent(float v, bool charging) {
  const float* volts = charging ? kChargeV : kDischargeV;
  const uint8_t* pct = charging ? kChargePct : kDischargePct;
  if (v <= volts[0]) return pct[0];
  if (v >= volts[kPoints - 1]) return pct[kPoints - 1];
  for (int i = 0; i < kPoints - 1; i++) {
    if (v <= volts[i + 1]) {
      const float ratio = (v - volts[i]) / (volts[i + 1] - volts[i]);
      return pct[i] + (uint8_t)(ratio * (pct[i + 1] - pct[i]));
    }
  }
  return 100;
}

void sample() {
  const int16_t mv = M5.Power.getBatteryVoltage();
  if (mv <= 0) return;
  history[historyNext] = mv / 1000.0f;
  historyNext = (historyNext + 1) % kHistory;
  if (historyCount < kHistory) historyCount++;
  float sum = 0;
  for (int i = 0; i < historyCount; i++) sum += history[i];
  average = sum / historyCount;

  if (entryV == 0) entryV = peakV = average;
  if (average > peakV) peakV = average;

  const auto state = M5.Power.isCharging();
  if (state == m5::Power_Class::charge_unknown) {
    // No charge signal (Cardputer): watch the voltage trend instead.
    if (!trendPowered && average - entryV >= kTrendRiseV) trendPowered = true;
    if (trendPowered && peakV - average >= kTrendDropV) {
      trendPowered = false;
      entryV = peakV = average;  // a new rise means plugged in again
    }
#if ARDUINO_USB_MODE
    powerNow = trendPowered || (bool)Serial;  // Serial: connected to a computer
#else
    powerNow = trendPowered;
#endif
  } else {
    powerNow = state == m5::Power_Class::is_charging;
  }
}
}  // namespace

namespace Battery {
void begin() {
  // Several quick readings, so the first one shown isn't a single noisy sample.
  for (int i = 0; i < kHistory; i++) {
    sample();
    delay(20);
  }
  lastSampleMs = millis();
}

void poll() {
  if (lastSampleMs != 0 && millis() - lastSampleMs < kSampleMs) return;
  lastSampleMs = millis();
  sample();
}
uint8_t percent() { return toPercent(average, powered()); }
float volts() { return average; }
bool powered() { return powerNow; }
}  // namespace Battery
