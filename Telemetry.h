#pragma once

struct DogTel {
  float cm = -1;
  float pitch = 0;
  float roll = 0;
  int fallen = 0;
  int imu = 0;
  int heard = 0;
};

extern DogTel dogTel;
