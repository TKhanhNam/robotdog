#pragma once
#include <Arduino.h>
#include "config.h"

class Ultrasonic {
public:
  void begin() {
    pinMode(PIN_US_TRIG, OUTPUT);
    pinMode(PIN_US_ECHO, INPUT);
    digitalWrite(PIN_US_TRIG, LOW);
  }

  // cm; trả về -1 nếu timeout / lỗi
  float readCm() {
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    unsigned long us = pulseIn(PIN_US_ECHO, HIGH, 25000UL);
    if (us == 0) return -1.0f;
    return (us * 0.0343f) / 2.0f;
  }

  bool obstacleNear() {
    float d = readCm();
    lastCm_ = d;
    return (d > 0 && d < OBSTACLE_CM);
  }

  float lastCm() const { return lastCm_; }

private:
  float lastCm_ = -1;
};
