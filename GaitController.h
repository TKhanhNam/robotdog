#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>
#include "ServoDriver.h"
#include "config.h"

enum class DogState : uint8_t {
  Idle,
  Stand,
  StandPrep,
  Sit4,
  SitRear,
  Shake,
  Walk,
  Run,
  Stop,
  Relax,
  Calib
};

class GaitController {
public:
  void begin(ServoDriver* servos) {
    servos_ = servos;
    pending_ = DogState::Idle;
    memcpy(stand_, STAND_DEG_DEFAULT, sizeof(stand_));
    for (uint8_t i = 0; i < SERVO_COUNT; i++) sitDeg_[i] = -1;
    loadStand();
    loadSit();
    loadGaitSigns();
    for (uint8_t i = 0; i < SERVO_COUNT; i++) cur_[i] = (float)stand_[i];
    applyStand();
    state_ = DogState::Stand;
  }

  DogState state() const { return state_; }

  // Hành động đang làm hoặc sắp làm (kể cả lúc đang về stand để chuyển)
  DogState active() const {
    if (state_ == DogState::StandPrep && pending_ != DogState::Idle && pending_ != DogState::Stop) {
      return pending_;
    }
    if (state_ == DogState::Stop || pending_ == DogState::Stop) return DogState::Stand;
    return state_;
  }

  void setStandDeg(uint8_t ch, int16_t deg) {
    if (ch >= SERVO_COUNT) return;
    stand_[ch] = constrain(deg, 0, 180);
    saveStand();
  }

  void resetStand() {
    memcpy(stand_, STAND_DEG_DEFAULT, sizeof(stand_));
    Preferences p;
    if (p.begin("dog", false)) {
      p.remove("stand");
      p.end();
    }
    applyStand();
    state_ = DogState::Stand;
  }

  // Góc ngồi riêng cho 1 kênh (thay cho STAND + SIT_OFF), dùng cho servo lắp khác các chân kia
  void setSitDeg(uint8_t ch, int16_t deg) {
    if (ch >= SERVO_COUNT) return;
    sitDeg_[ch] = constrain(deg, 0, 180);
    saveSit();
  }

  int16_t sitDeg(uint8_t ch) const {
    return (ch < SERVO_COUNT) ? sitDeg_[ch] : -1;
  }

  void clearSitDeg() {
    for (uint8_t i = 0; i < SERVO_COUNT; i++) sitDeg_[i] = -1;
    Preferences p;
    if (p.begin("dog", false)) {
      p.remove("sit");
      p.end();
    }
  }

  // Quay 1 kênh tới góc, không đổi góc stand
  void moveChannel(uint8_t ch, float deg) {
    if (!servos_ || ch >= SERVO_COUNT) return;
    if (state_ != DogState::Sit4 && state_ != DogState::SitRear) state_ = DogState::Calib;
    deg = constrain(deg, 0, 180);
    to_[ch] = deg;
    cur_[ch] = deg;
    servos_->setAngle(ch, deg);
  }

  void freeChannel(uint8_t ch) {
    if (!servos_ || ch >= SERVO_COUNT) return;
    state_ = DogState::Calib;
    pending_ = DogState::Idle;
    servos_->detach(ch);
  }

  int16_t standDeg(uint8_t ch) const {
    return (ch < SERVO_COUNT) ? stand_[ch] : 90;
  }

  int8_t gaitSign(uint8_t ch) const {
    return (ch < SERVO_COUNT) ? sign_[ch] : 1;
  }

  void flipGaitSign(uint8_t ch) {
    if (ch >= SERVO_COUNT) return;
    sign_[ch] = -sign_[ch];
    saveGaitSigns();
  }

  void resetGaitSigns() {
    memcpy(sign_, GAIT_SIGN_DEFAULT, sizeof(sign_));
    saveGaitSigns();
  }

  // Đẩy 1 kênh theo chiều "về trước" rồi trả về stand, để kiểm tra GAIT_SIGN
  void testChannel(uint8_t ch) {
    if (!servos_ || ch >= SERVO_COUNT) return;
    state_ = DogState::Calib;
    pending_ = DogState::Idle;
    applyStand();
    delay(300);
    const float base = (float)stand_[ch];
    for (int i = 0; i <= 25; i++) {
      servos_->setAngle(ch, clampGait(ch, base + sign_[ch] * i));
      delay(20);
    }
    delay(500);
    for (int i = 25; i >= 0; i--) {
      servos_->setAngle(ch, clampGait(ch, base + sign_[ch] * i));
      delay(20);
    }
    applyStand();
    state_ = DogState::Stand;
  }

  void flipKneeStand(uint8_t ch) {
    if (ch != BL_KNEE && ch != BR_KNEE && ch != FL_KNEE && ch != FR_KNEE) return;
    stand_[ch] = 180 - stand_[ch];
    applyStand();
    state_ = DogState::Stand;
  }

  void flipAllKnees() {
    flipKneeStand(BL_KNEE);
    flipKneeStand(BR_KNEE);
    flipKneeStand(FL_KNEE);
    flipKneeStand(FR_KNEE);
  }

  void setChannel(uint8_t ch, float deg) {
    if (!servos_ || ch >= SERVO_COUNT) return;
    state_ = DogState::Calib;
    pending_ = DogState::Idle;
    deg = constrain(deg, 0, 180);
    servos_->setAngle(ch, deg);
    cur_[ch] = deg;
  }

  void relax() {
    state_ = DogState::Relax;
    pending_ = DogState::Idle;
    if (!servos_) return;
    for (uint8_t i = 0; i < SERVO_COUNT; i++) servos_->detach(i);
  }

  // Bắt tay bằng chân trước trái (left = true) hoặc phải
  void shake(bool left) {
    pawLeft_ = left;
    setState(DogState::Shake);
  }

  bool pawLeft() const { return pawLeft_; }

  void setState(DogState s) {
    if (s == DogState::Relax) {
      relax();
      return;
    }
    if (s == DogState::Stop || s == DogState::Idle) s = DogState::Stand;

    // walk ↔ run: đổi nhịp ngay, không đứng lại
    if ((state_ == DogState::Walk || state_ == DogState::Run) &&
        (s == DogState::Walk || s == DogState::Run)) {
      pending_ = DogState::Idle;
      state_ = s;
      return;
    }
    if (state_ == DogState::StandPrep && pending_ == s) return;
    if (state_ == DogState::Stand && s == DogState::Stand) return;

    const bool moving = (state_ == DogState::Walk || state_ == DogState::Run ||
                         state_ == DogState::Shake || state_ == DogState::StandPrep);
    pending_ = s;
    float a[SERVO_COUNT];
    standPose(a);
    const uint32_t prep = moving ? 280 : STAND_PREP_MS;
    beginMove(a, prep);
    state_ = DogState::StandPrep;
    const bool needHold = !moving && s != DogState::Stand;
    prepUntil_ = millis() + prep + (needHold ? STAND_HOLD_MS : 0);
  }

  void update() {
    const uint32_t now = millis();
    if (state_ == DogState::Calib || state_ == DogState::Relax) return;
    if (now - lastStep_ < GAIT_TICK_MS) return;
    lastStep_ = now;

    if (state_ == DogState::Walk || state_ == DogState::Run) {
      stepGait(state_ == DogState::Run, now);
      return;
    }
    if (state_ == DogState::Shake) {
      stepShake(now);
      return;
    }
    stepMove(now);
    if (state_ == DogState::StandPrep && now >= prepUntil_) startPending();
  }

  void applyStand() {
    float a[SERVO_COUNT];
    standPose(a);
    writeAll(a);
    memcpy(to_, a, sizeof(to_));
    moveMs_ = 0;
  }

private:
  struct Leg {
    uint8_t knee;
    uint8_t hip;
    bool left;
    bool pairA; // cặp A: trái sau + phải trước; cặp B: phải sau + trái trước
  };

  static constexpr Leg LEGS[4] = {
    {BL_KNEE, BL_HIP, true,  true},
    {FR_KNEE, FR_HIP, false, true},
    {BR_KNEE, BR_HIP, false, false},
    {FL_KNEE, FL_HIP, true,  false},
  };

  ServoDriver* servos_ = nullptr;
  DogState state_ = DogState::Stand;
  DogState pending_ = DogState::Idle;
  uint32_t lastStep_ = 0;
  uint32_t lastHold_ = 0;
  uint32_t prepUntil_ = 0;
  uint32_t gaitStart_ = 0;
  uint32_t moveStart_ = 0;
  uint32_t moveMs_ = 0;
  uint32_t shakeStart_ = 0;
  uint8_t shakePhase_ = 0;
  bool pawLeft_ = false;
  float from_[SERVO_COUNT] = {};
  float to_[SERVO_COUNT] = {};
  float cur_[SERVO_COUNT] = {};
  int16_t stand_[SERVO_COUNT] = {};
  int16_t sitDeg_[SERVO_COUNT] = {};
  int8_t sign_[SERVO_COUNT] = {};

  void loadStand() {
    Preferences p;
    if (!p.begin("dog", true)) return;
    if (p.getBytesLength("stand") == sizeof(stand_)) p.getBytes("stand", stand_, sizeof(stand_));
    p.end();
  }

  void saveStand() {
    Preferences p;
    if (!p.begin("dog", false)) return;
    p.putBytes("stand", stand_, sizeof(stand_));
    p.end();
  }

  void loadSit() {
    Preferences p;
    if (!p.begin("dog", true)) return;
    if (p.getBytesLength("sit") == sizeof(sitDeg_)) p.getBytes("sit", sitDeg_, sizeof(sitDeg_));
    p.end();
  }

  void saveSit() {
    Preferences p;
    if (!p.begin("dog", false)) return;
    p.putBytes("sit", sitDeg_, sizeof(sitDeg_));
    p.end();
  }

  void loadGaitSigns() {
    memcpy(sign_, GAIT_SIGN_DEFAULT, sizeof(sign_));
    Preferences p;
    if (!p.begin("dog", true)) return;
    if (p.isKey("gflip")) {
      const uint8_t mask = p.getUChar("gflip", 0);
      for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        if (mask & (1u << i)) sign_[i] = -sign_[i];
      }
    }
    p.end();
  }

  void saveGaitSigns() {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
      if (sign_[i] != GAIT_SIGN_DEFAULT[i]) mask |= (1u << i);
    }
    Preferences p;
    if (!p.begin("dog", false)) return;
    p.putUChar("gflip", mask);
    p.end();
  }

  float clampGait(uint8_t ch, float deg) const {
    return constrain(deg, (float)GAIT_MIN_DEG[ch], (float)GAIT_MAX_DEG[ch]);
  }

  void startPending() {
    DogState s = pending_;
    pending_ = DogState::Idle;
    lastStep_ = millis();

    float a[SERVO_COUNT];
    state_ = s;
    if (s == DogState::Sit4) {
      fillPose(a, SIT4_OFF);
      beginMove(a, SIT_MOVE_MS);
    } else if (s == DogState::SitRear) {
      fillPose(a, SIT2_OFF);
      beginMove(a, SIT_MOVE_MS);
    } else if (s == DogState::Shake) {
      standPose(a);
      beginMove(a, 200);
      shakePhase_ = 0;
    } else if (s == DogState::Walk || s == DogState::Run) {
      gaitStart_ = millis();
    } else {
      state_ = DogState::Stand;
    }
  }

  void writeAll(const float a[SERVO_COUNT]) {
    memcpy(cur_, a, sizeof(cur_));
    if (servos_) servos_->setAll(a);
  }

  void standPose(float a[SERVO_COUNT]) const {
    for (uint8_t i = 0; i < SERVO_COUNT; i++) a[i] = (float)stand_[i];
  }

  void fillPose(float a[SERVO_COUNT], const int8_t off[SERVO_COUNT]) const {
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
      if (off[i] != 0 && sitDeg_[i] >= 0) {
        a[i] = (float)sitDeg_[i];
      } else {
        a[i] = clampGait(i, (float)stand_[i] + sign_[i] * (float)off[i]);
      }
    }
  }

  void beginMove(const float target[SERVO_COUNT], uint32_t ms) {
    memcpy(from_, cur_, sizeof(from_));
    memcpy(to_, target, sizeof(to_));
    moveStart_ = millis();
    moveMs_ = ms;
  }

  // Đi mượt từ from_ tới to_; tới nơi thì giữ to_. Trả về true khi đã tới.
  bool stepMove(uint32_t now) {
    const float t = moveMs_ ? (float)(now - moveStart_) / (float)moveMs_ : 1.0f;
    if (t >= 1.0f) {
      if (now - lastHold_ >= 80) {
        lastHold_ = now;
        writeAll(to_);
      }
      return true;
    }
    const float s = t * t * (3.0f - 2.0f * t);
    float a[SERVO_COUNT];
    for (uint8_t i = 0; i < SERVO_COUNT; i++) a[i] = from_[i] + (to_[i] - from_[i]) * s;
    writeAll(a);
    return false;
  }

  // Bắt tay khi đứng: 0 giữ stand → 1 giơ chân trước (trái/phải) → 2 lắc → 3 hạ chân → đứng.
  void stepShake(uint32_t now) {
    float a[SERVO_COUNT];
    const uint8_t hip = pawLeft_ ? FL_HIP : FR_HIP;
    const uint8_t knee = pawLeft_ ? FL_KNEE : FR_KNEE;
    const uint8_t wag = pawLeft_ ? FL_KNEE : FR_HIP;
    const float amp = pawLeft_ ? (float)PAW_L_SHAKE_AMP : (float)PAW_SHAKE_AMP;
    switch (shakePhase_) {
      case 0:
        if (stepMove(now)) {
          standPose(a);
          a[hip] = clampGait(hip, a[hip] + sign_[hip] * (float)(pawLeft_ ? PAW_L_THIGH_FWD : PAW_THIGH_FWD));
          a[knee] = clampGait(knee, a[knee] + sign_[knee] * (float)(pawLeft_ ? PAW_L_CALF_FWD : PAW_CALF_FWD));
          beginMove(a, PAW_MOVE_MS);
          shakePhase_ = 1;
        }
        break;
      case 1:
        if (stepMove(now)) {
          shakeStart_ = now;
          shakePhase_ = 2;
        }
        break;
      case 2: {
        const uint32_t e = now - shakeStart_;
        memcpy(a, to_, sizeof(a));
        const float w = sinf((float)e / (float)PAW_SHAKE_MS * 6.2831853f);
        a[wag] = clampGait(wag, to_[wag] + sign_[wag] * amp * w);
        writeAll(a);
        if (e >= (uint32_t)PAW_SHAKES * PAW_SHAKE_MS) {
          standPose(a);
          beginMove(a, PAW_MOVE_MS);
          shakePhase_ = 3;
        }
        break;
      }
      default:
        if (stepMove(now)) state_ = DogState::Stand;
        break;
    }
  }

  // IK 2 khớp trong mặt phẳng dọc. Bàn chân (x về trước, z xuống dưới, mm, gốc ở trục đùi)
  // → góc đùi (+ = vung về trước) và góc gối (+ = cẳng gập về trước), tính từ chân duỗi thẳng.
  // Gối gập về sau giống dáng trùng chân cũ: đùi lùi, cẳng chĩa về trước.
  static void legIK(float x, float z, float& hipDeg, float& kneeDeg) {
    const float l1 = LEG_THIGH_MM, l2 = LEG_CALF_MM;
    const float d = constrain(sqrtf(x * x + z * z), fabsf(l1 - l2) + 1.0f, l1 + l2 - 0.5f);
    const float alpha = acosf(constrain((l1 * l1 + d * d - l2 * l2) / (2.0f * l1 * d), -1.0f, 1.0f));
    const float gamma = acosf(constrain((l1 * l1 + l2 * l2 - d * d) / (2.0f * l1 * l2), -1.0f, 1.0f));
    hipDeg = (atan2f(x, z) - alpha) * RAD_TO_DEG;
    kneeDeg = (PI - gamma) * RAD_TO_DEG;
  }

  // Quỹ đạo một bàn chân theo pha p (0..1) của nhịp, như SovGVD:
  // vung: nhấc thẳng lên, đưa ra trước khi đang cao, rồi đặt thẳng xuống;
  // chống: bàn chân nằm yên trên đất, trôi đều về sau khi thân tiến lên.
  static void footPath(float p, float stride, float lift, float height, float& x, float& z) {
    const float sw = GAIT_SWING_FRAC * 0.5f;
    if (p < sw) {
      const float s = p / sw;
      const float e = s * s * (3.0f - 2.0f * s);
      float up = 1.0f;
      if (s < 0.25f) up = sinf(s / 0.25f * HALF_PI);
      else if (s > 0.75f) up = sinf((1.0f - s) / 0.25f * HALF_PI);
      x = -stride * 0.5f + stride * e;
      z = height - lift * up;
    } else {
      const float q = (p - sw) / (1.0f - sw);
      x = stride * 0.5f - stride * q;
      z = height;
    }
  }

  // Trot chéo: (trái sau + phải trước) và (phải sau + trái trước) lệch nửa nhịp.
  void stepGait(bool run, uint32_t now) {
    const uint32_t elapsed = now - gaitStart_;
    const uint32_t cycle = run ? GAIT_CYCLE_RUN_MS : GAIT_CYCLE_WALK_MS;
    const float legLen = LEG_THIGH_MM + LEG_CALF_MM;

    float ramp = (float)elapsed / (float)GAIT_RAMP_MS;
    if (ramp > 1.0f) ramp = 1.0f;
    const float height = legLen - legLen * (1.0f - (run ? RUN_HEIGHT : WALK_HEIGHT)) * ramp;
    const float stride = (run ? RUN_STRIDE_MM : WALK_STRIDE_MM) * ramp;
    const float lift = (run ? RUN_LIFT_MM : WALK_LIFT_MM) * ramp;
    const float t = (float)(elapsed % cycle) / (float)cycle;

    float a[SERVO_COUNT];
    standPose(a);
    for (const Leg& leg : LEGS) {
      float p = leg.pairA ? t : t + 0.5f;
      if (p >= 1.0f) p -= 1.0f;
      float x, z, hipDeg, kneeDeg;
      footPath(p, stride, lift, height, x, z);
      legIK(x, z, hipDeg, kneeDeg);
      hipDeg += GAIT_TRIM_DEG[leg.hip] * ramp;
      kneeDeg += GAIT_TRIM_DEG[leg.knee] * ramp;
      a[leg.hip] = clampGait(leg.hip, stand_[leg.hip] + sign_[leg.hip] * hipDeg);
      a[leg.knee] = clampGait(leg.knee, stand_[leg.knee] + sign_[leg.knee] * kneeDeg);
    }
    writeAll(a);
  }
};
