/*
 * May do dien ap XL4015 — chi ESP32, USB cap nguon.
 * Khong noi IN+/OUT+ thang vao GPIO. Bat buoc chia ap 20k + 10k.
 *
 *   XL4015 IN+  --[20k]--*-- GPIO1
 *                        |
 *                      [10k]
 *                        |
 *                       GND (chung ESP32)
 *
 *   XL4015 OUT+ --[20k]--*-- GPIO2
 *                        |
 *                      [10k]
 *                        |
 *                       GND
 *
 * Nap sketch nay (khong nap robotdog.ino). Serial 115200.
 */

#include <Arduino.h>
#include "esp_rom_sys.h"

static const int PIN_VIN = 1;
static const int PIN_VOUT = 2;
static const float DIV_VIN = 3.0f;   // (20k+10k)/10k
static const float DIV_VOUT = 3.0f;
static const float CAL_VIN = 1.00f;  // chinh neu lech dong ho
static const float CAL_VOUT = 1.00f;
static const float BATT_EMPTY = 6.40f;
static const float BATT_FULL = 8.40f;
static const int SAMPLES = 24;

static void say(const char* msg) {
  Serial.println(msg);
  Serial0.println(msg);
  Serial.flush();
  Serial0.flush();
  esp_rom_printf("%s\r\n", msg);
}

static void led(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
  neopixelWrite(RGB_BUILTIN, r, g, b);
#else
  neopixelWrite(48, r, g, b);
#endif
}

static float readAdcVolts(int pin, float divider, float calib) {
  uint32_t acc = 0;
  for (int i = 0; i < SAMPLES; i++) {
    acc += analogReadMilliVolts(pin);
    delayMicroseconds(250);
  }
  const float vAdc = (acc / (float)SAMPLES) / 1000.0f;
  return vAdc * divider * calib;
}

static int battPct(float vin) {
  float p = (vin - BATT_EMPTY) / (BATT_FULL - BATT_EMPTY) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)(p + 0.5f);
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(250);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VIN, ADC_11db);
  analogSetPinAttenuation(PIN_VOUT, ADC_11db);

  led(0, 40, 0);
  say("VOLT_METER v1");
  say("USB cap ESP32. GND chung voi XL4015.");
  say("GPIO1=IN (pin)  GPIO2=OUT (5V)  chia ap 20k/10k");
}

void loop() {
  const float vin = readAdcVolts(PIN_VIN, DIV_VIN, CAL_VIN);
  const float vout = readAdcVolts(PIN_VOUT, DIV_VOUT, CAL_VOUT);
  const int pct = battPct(vin);

  char line[160];
  snprintf(line, sizeof(line),
           "VOLT {\"vin\":%.3f,\"vout\":%.3f,\"pct\":%d}",
           vin, vout, pct);
  say(line);

  const bool noInput = vin < 0.4f && vout < 0.4f;
  const bool voutOk = vout >= 4.70f && vout <= 5.40f;
  const bool battLow = vin >= 0.4f && vin < 6.80f;

  if (noInput) led(0, 0, 50);
  else if (!voutOk && vout >= 0.4f) led(80, 0, 0);
  else if (battLow) led(80, 50, 0);
  else led(0, 40, 0);

  delay(250);
}
