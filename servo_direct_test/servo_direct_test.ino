/*
 * Test 1 servo MG90S thang vao ESP32-S3 (khong PCA, khong pin).
 *
 * Day servo:
 *   Nau/den -> ESP32 GND  (sat USB, hang duoi)
 *   Do      -> ESP32 5V   (canh GND)
 *   Vang    -> ESP32 chan 4  (sau RST, hang 3V3)
 *
 * USB cam may tinh. Chi 1 servo.
 */

#include "esp_rom_sys.h"

static const int SERVO_PIN = 4;
static const int LEDC_RES = 14;
static const int LEDC_MAX = (1 << LEDC_RES) - 1;

static void writeUs(int us) {
  if (us < 500) us = 500;
  if (us > 2500) us = 2500;
  uint32_t duty = (uint32_t)us * LEDC_MAX / 20000;
  ledcWrite(SERVO_PIN, duty);
}

void setup() {
  ledcAttach(SERVO_PIN, 50, LEDC_RES);
#ifdef RGB_BUILTIN
  neopixelWrite(RGB_BUILTIN, 0, 40, 0);
#endif
  esp_rom_printf("\r\nSERVO DIRECT TEST GPIO4\r\n");
  writeUs(1500);
  delay(400);
}

void loop() {
  esp_rom_printf("-> 0 deg\r\n");
  writeUs(500);
  delay(800);
  esp_rom_printf("-> 90 deg\r\n");
  writeUs(1500);
  delay(800);
  esp_rom_printf("-> 180 deg\r\n");
  writeUs(2500);
  delay(800);
}
