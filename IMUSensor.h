#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "config.h"

class IMUSensor {
public:
  bool begin(TwoWire* bus = &Wire) {
    if (!mpu_.begin(MPU6050_ADDR, bus)) {
      return false;
    }
    mpu_.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu_.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_.setFilterBandwidth(MPU6050_BAND_21_HZ);
    return true;
  }

  void update() {
    sensors_event_t a, g, temp;
    mpu_.getEvent(&a, &g, &temp);

    // Complementary filter đơn giản → pitch/roll (độ)
    const float ax = a.acceleration.x;
    const float ay = a.acceleration.y;
    const float az = a.acceleration.z;

    float accPitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2957795f;
    float accRoll  = atan2f(ay, az) * 57.2957795f;

    const float dt = 0.02f;
    const float alpha = 0.96f;
    pitch_ = alpha * (pitch_ + g.gyro.y * dt * 57.2957795f) + (1 - alpha) * accPitch;
    roll_  = alpha * (roll_  + g.gyro.x * dt * 57.2957795f) + (1 - alpha) * accRoll;

    const bool tipped = (fabsf(roll_) > FALL_ROLL_DEG) || (fabsf(pitch_) > FALL_PITCH_DEG);
    if (tipped) {
      if (fallSince_ == 0) fallSince_ = millis();
    } else {
      fallSince_ = 0;
    }
  }

  // Đọc thô: gia tốc m/s², con quay rad/s, nhiệt độ °C
  void readRaw(float& ax, float& ay, float& az,
               float& gx, float& gy, float& gz, float& tempC) {
    sensors_event_t a, g, temp;
    mpu_.getEvent(&a, &g, &temp);
    ax = a.acceleration.x; ay = a.acceleration.y; az = a.acceleration.z;
    gx = g.gyro.x; gy = g.gyro.y; gz = g.gyro.z;
    tempC = temp.temperature;
  }

  float pitch() const { return pitch_; }
  float roll() const { return roll_; }

  bool isFallen() const {
    return fallSince_ != 0 && (millis() - fallSince_ >= FALL_CONFIRM_MS);
  }

private:
  Adafruit_MPU6050 mpu_;
  float pitch_ = 0;
  float roll_ = 0;
  uint32_t fallSince_ = 0;
};
