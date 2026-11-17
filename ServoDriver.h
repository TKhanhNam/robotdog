#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Preferences.h>
#include "esp_rom_sys.h"
#include "config.h"

class ServoDriver {
public:
  void begin() {
    pwm_ = Adafruit_PWMServoDriver(PCA9685_ADDR);
    pwm_.begin();
    // Không reset chip — tránh giật/nhả chân lúc boot
    delay(10);
    pwm_.setOscillatorFrequency(25000000);
    pwm_.setPWMFreq(SERVO_FREQ_HZ);
    delay(10);
    loadDirFlip();
  }

  // Servo thay mới có thể quay ngược servo cũ: đảo chiều riêng kênh đó, lưu flash
  void flipDir(uint8_t ch) {
    if (ch >= SERVO_COUNT) return;
    dirFlip_ ^= (uint8_t)(1u << ch);
    Preferences p;
    if (p.begin("dog", false)) {
      p.putUChar("dirflip", dirFlip_);
      p.end();
    }
  }

  void resetDir() {
    dirFlip_ = 0;
    Preferences p;
    if (p.begin("dog", false)) {
      p.remove("dirflip");
      p.end();
    }
  }

  bool dirFlipped(uint8_t ch) const {
    return ch < SERVO_COUNT && (dirFlip_ & (1u << ch));
  }

  // angle: 0..180 (logic), áp dụng DIR + OFFSET
  void setAngle(uint8_t ch, float angle) {
    if (ch >= SERVO_COUNT) return;
    float dir = (float)SERVO_DIR[ch];
    if (dirFlip_ & (1u << ch)) dir = -dir;
    float a = 90.0f + (angle - 90.0f) * dir + (float)SERVO_OFFSET[ch];
    a = constrain(a, 0.0f, 180.0f);
    uint16_t us = map((long)(a * 10), 0, 1800, SERVO_MIN_US, SERVO_MAX_US);
    uint16_t tick = (uint32_t)us * 4096UL / 20000UL;
    if (tick > 4095) tick = 4095;
    pwm_.setPWM(ch, 0, tick);
  }

  void setAll(const float angles[SERVO_COUNT]) {
    for (uint8_t i = 0; i < SERVO_COUNT; i++) setAngle(i, angles[i]);
  }

  // Kênh radar: xung µs trực tiếp (có thể thấp hơn 500 µs để quay thêm sang phải).
  void setExtraUs(uint8_t ch, uint16_t us) {
    if (ch > 15) return;
    if (us < 200) us = 200;
    if (us > 2500) us = 2500;
    uint16_t tick = (uint32_t)us * 4096UL / 20000UL;
    if (tick > 4095) tick = 4095;
    pwm_.setPWM(ch, 0, tick);
  }

  void detach(uint8_t ch) {
    if (ch < SERVO_COUNT) pwm_.setPWM(ch, 0, 0);
  }

  void detachExtra(uint8_t ch) {
    if (ch <= 15) pwm_.setPWM(ch, 0, 0);
  }

  // USB test: 16 kenh PCA (CH0..15), khong can DIR
  void usbTestAll16() {
    for (uint8_t ch = 0; ch < 16; ch++) {
      esp_rom_printf("Quay CH%d\r\n", ch);
      for (uint16_t us = 1200; us <= 1800; us += 100) {
        pwm_.writeMicroseconds(ch, us);
        delay(70);
      }
      for (uint16_t us = 1800; us >= 1200; us -= 100) {
        pwm_.writeMicroseconds(ch, us);
        delay(70);
      }
      pwm_.writeMicroseconds(ch, 1500);
      delay(100);
      pwm_.setPWM(ch, 0, 0);
      delay(150);
    }
  }

private:
  Adafruit_PWMServoDriver pwm_;
  uint8_t dirFlip_ = 0;

  void loadDirFlip() {
    Preferences p;
    if (!p.begin("dog", true)) return;
    dirFlip_ = p.getUChar("dirflip", 0);
    p.end();
  }
};
