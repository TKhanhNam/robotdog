#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "AudioIO.h"
#include "config.h"

// Nhận lệnh nói theo giọng người dùng: đặc trưng MFCC mỗi 16 ms, so khớp DTW
// với các mẫu ghi bằng lệnh train. Chạy trong task riêng trên lõi 0.
// Từ 0 "hello" chó tự đáp lại; các từ khác được đưa ra loop() qua takeWord().
class VoiceHello {
public:
  using LogFn = void (*)(const char*);
  static constexpr int NWORD = 6;

  static const char* wordName(int w) {
    static const char* names[NWORD] = {"hello", "stand", "walk", "run", "stop", "sit"};
    return (w >= 0 && w < NWORD) ? names[w] : "?";
  }

  static int wordIndex(const char* s) {
    for (int w = 0; w < NWORD; w++) {
      if (strcmp(s, wordName(w)) == 0) return w;
    }
    return -1;
  }

  bool begin(AudioIO* audio, LogFn log) {
    audio_ = audio;
    log_ = log;
    if (!audio_ || !audio_->micOk()) return false;
    initDsp();
    for (int w = 0; w < NWORD; w++) loadWord(w);
    updateSpeechRatio();
    xTaskCreatePinnedToCore(taskEntry, "hello", 8192, this, 1, &task_, 0);
    return true;
  }

  void requestTrain(int w = 0) {
    if (w < 0 || w >= NWORD) return;
    trainWord_ = w;
    trainReq_ = true;
    training_ = true;
  }
  bool training() const { return training_; }
  void setEnabled(bool on) { enabled_ = on; }
  void setDebug(bool on) { debug_ = on; }
  void setNoisy(bool on) { noisy_ = on; }
  bool enabled() const { return enabled_; }

  // Lệnh vừa nghe được (1..NWORD-1), -1 nếu chưa có. Gọi từ loop().
  int takeWord() {
    const int w = heard_;
    heard_ = -1;
    return w;
  }

  void setFactor(float f) {
    factor_ = f;
    recomputeReq_ = true;
  }

  void forget() {
    for (int w = 0; w < NWORD; w++) forgetWord(w);
  }

  void forgetWord(int w) {
    if (w < 0 || w >= NWORD) return;
    nTpl_[w] = 0;
    thr_[w] = 0;
    spr_[w] = 0;
    Preferences p;
    if (p.begin(nsName(w), false)) {
      p.clear();
      p.end();
    }
    updateSpeechRatio();
  }

  void printStatus() {
    note("[VOICE] nghe %s | he so %.2f | giong to gap %.1f lan nen",
         enabled_ ? "BAT" : "TAT", factor_, speechRatio_);
    for (int w = 0; w < NWORD; w++) {
      if (nTpl_[w] >= HELLO_TEMPLATES) {
        int msAvg = 0;
        for (int t = 0; t < nTpl_[w]; t++) msAvg += tplLen_[w][t];
        msAvg = msAvg * HOP * 1000 / FS / nTpl_[w];
        note("  %-6s: DA CO mau ~%d ms, nguong %.2f", wordName(w), msAvg, thr_[w]);
      } else {
        note("  %-6s: chua co mau — go 'train %s'", wordName(w), wordName(w));
      }
    }
  }

private:
  static constexpr int FS = MIC_SAMPLE_RATE;
  static constexpr int NFFT = 512;          // cửa sổ 32 ms
  static constexpr int HOP = 256;           // bước 16 ms
  static constexpr int NBIN = NFFT / 2 + 1;
  static constexpr int NMEL = 26;
  static constexpr int NCEP = 12;
  static constexpr int PREROLL = 6;         // giữ ~96 ms trước lúc phát hiện tiếng
  static constexpr int TRAIN_MIN_MS = 180;
  static constexpr int TRAIN_MAX_MS = 1500;

  AudioIO* audio_ = nullptr;
  LogFn log_ = nullptr;
  TaskHandle_t task_ = nullptr;

  volatile bool enabled_ = true;
  volatile bool debug_ = false;
  volatile bool noisy_ = false;
  volatile bool trainReq_ = false;
  volatile bool training_ = false;
  volatile bool recomputeReq_ = false;
  volatile int trainWord_ = 0;
  volatile int heard_ = -1;
  int nTpl_[NWORD] = {};
  float thr_[NWORD] = {};
  float spr_[NWORD] = {};
  float factor_ = HELLO_THR_FACTOR;
  float speechRatio_ = 0;                 // đỉnh giọng lúc train / nền
  float trainPeak_[HELLO_TEMPLATES] = {};

  float hamming_[NFFT];
  float cos_[NFFT / 2];
  float sin_[NFFT / 2];
  uint16_t rev_[NFFT];
  int bin_[NMEL + 2];
  float dct_[NCEP][NMEL];

  float win_[NFFT] = {};
  float re_[NFFT];
  float im_[NFFT];
  float pow_[NBIN];
  float logm_[NMEL];

  float pre_[PREROLL][NCEP];
  int preHead_ = 0;
  int preCount_ = 0;

  float seg_[HELLO_MAX_FRAMES][NCEP];
  int nSeg_ = 0;

  // Mẫu lưu dạng int16 (×100) để đủ chỗ cho nhiều từ
  int16_t tpl_[NWORD][HELLO_TEMPLATES][HELLO_MAX_FRAMES * NCEP];
  uint8_t tplLen_[NWORD][HELLO_TEMPLATES] = {};
  float bufA_[HELLO_MAX_FRAMES][NCEP];
  float bufB_[HELLO_MAX_FRAMES][NCEP];

  float rowA_[HELLO_MAX_FRAMES + 1];
  float rowB_[HELLO_MAX_FRAMES + 1];

  void note(const char* fmt, ...) {
    char b[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    if (log_) log_(b);
  }

  // Từ 0 giữ namespace cũ "hello" để mẫu đã train vẫn dùng được
  static const char* nsName(int w) {
    static const char* ns[NWORD] = {"hello", "vw1", "vw2", "vw3", "vw4", "vw5"};
    return ns[w];
  }

  static float hzToMel(float hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); }
  static float melToHz(float m) { return 700.0f * (powf(10.0f, m / 2595.0f) - 1.0f); }

  void initDsp() {
    for (int i = 0; i < NFFT; i++) {
      hamming_[i] = 0.54f - 0.46f * cosf(6.2831853f * i / (NFFT - 1));
      int r = 0;
      for (int b = 0; b < 9; b++) {
        if (i & (1 << b)) r |= 1 << (8 - b);
      }
      rev_[i] = (uint16_t)r;
    }
    for (int i = 0; i < NFFT / 2; i++) {
      cos_[i] = cosf(6.2831853f * i / NFFT);
      sin_[i] = -sinf(6.2831853f * i / NFFT);
    }
    const float lo = hzToMel(80.0f);
    const float hi = hzToMel(7600.0f);
    for (int m = 0; m < NMEL + 2; m++) {
      const float hz = melToHz(lo + (hi - lo) * m / (NMEL + 1));
      bin_[m] = (int)floorf((NFFT + 1) * hz / FS);
    }
    for (int c = 0; c < NCEP; c++) {
      for (int m = 0; m < NMEL; m++) {
        dct_[c][m] = cosf(3.1415926f * (c + 1) * (m + 0.5f) / NMEL);
      }
    }
  }

  void fft() {
    for (int i = 0; i < NFFT; i++) {
      const int j = rev_[i];
      if (j > i) {
        float t = re_[i]; re_[i] = re_[j]; re_[j] = t;
        t = im_[i]; im_[i] = im_[j]; im_[j] = t;
      }
    }
    for (int len = 2; len <= NFFT; len <<= 1) {
      const int half = len / 2;
      const int step = NFFT / len;
      for (int i = 0; i < NFFT; i += len) {
        for (int k = 0; k < half; k++) {
          const float wr = cos_[k * step];
          const float wi = sin_[k * step];
          const int a = i + k;
          const int b = a + half;
          const float xr = re_[b] * wr - im_[b] * wi;
          const float xi = re_[b] * wi + im_[b] * wr;
          re_[b] = re_[a] - xr;
          im_[b] = im_[a] - xi;
          re_[a] += xr;
          im_[a] += xi;
        }
      }
    }
  }

  void mfcc(float* out) {
    for (int i = 0; i < NFFT; i++) {
      re_[i] = win_[i] * hamming_[i];
      im_[i] = 0;
    }
    fft();
    for (int k = 0; k < NBIN; k++) pow_[k] = re_[k] * re_[k] + im_[k] * im_[k];
    for (int m = 0; m < NMEL; m++) {
      const int lo = bin_[m], mid = bin_[m + 1], hi = bin_[m + 2];
      float e = 0;
      for (int k = lo; k < mid; k++) e += pow_[k] * (float)(k - lo) / (float)(mid - lo);
      for (int k = mid; k < hi; k++) e += pow_[k] * (float)(hi - k) / (float)(hi - mid);
      logm_[m] = logf(e + 1e-12f);
    }
    for (int c = 0; c < NCEP; c++) {
      float s = 0;
      for (int m = 0; m < NMEL; m++) s += logm_[m] * dct_[c][m];
      out[c] = s;
    }
  }

  // Bỏ trung bình từng hệ số: bớt phụ thuộc mic/khoảng cách
  static void cmn(float (*f)[NCEP], int n) {
    for (int c = 0; c < NCEP; c++) {
      float mean = 0;
      for (int i = 0; i < n; i++) mean += f[i][c];
      mean /= n;
      for (int i = 0; i < n; i++) f[i][c] -= mean;
    }
  }

  void storeTpl(int w, int t, const float (*src)[NCEP], int n) {
    for (int i = 0; i < n * NCEP; i++) {
      const long v = lroundf(src[i / NCEP][i % NCEP] * 100.0f);
      tpl_[w][t][i] = (int16_t)constrain(v, -32768L, 32767L);
    }
    tplLen_[w][t] = (uint8_t)n;
  }

  int loadTpl(int w, int t, float (*dst)[NCEP]) const {
    const int n = tplLen_[w][t];
    for (int i = 0; i < n * NCEP; i++) dst[i / NCEP][i % NCEP] = tpl_[w][t][i] / 100.0f;
    return n;
  }

  float dtw(const float (*a)[NCEP], int n, const float (*b)[NCEP], int m) {
    float* prev = rowA_;
    float* cur = rowB_;
    const float INF = 1e30f;
    prev[0] = 0;
    for (int j = 1; j <= m; j++) prev[j] = INF;
    for (int i = 1; i <= n; i++) {
      cur[0] = INF;
      for (int j = 1; j <= m; j++) {
        float d = 0;
        for (int c = 0; c < NCEP; c++) {
          const float t = a[i - 1][c] - b[j - 1][c];
          d += t * t;
        }
        d = sqrtf(d);
        float best = prev[j];
        if (cur[j - 1] < best) best = cur[j - 1];
        if (prev[j - 1] < best) best = prev[j - 1];
        cur[j] = d + best;
      }
      float* t = prev; prev = cur; cur = t;
    }
    return prev[m] / (float)(n + m);
  }

  void recomputeThreshold(int w) {
    if (nTpl_[w] < 2) { thr_[w] = 0; return; }
    float s = 0;
    int k = 0;
    for (int i = 0; i < nTpl_[w]; i++) {
      const int ni = loadTpl(w, i, bufA_);
      for (int j = i + 1; j < nTpl_[w]; j++) {
        const int nj = loadTpl(w, j, bufB_);
        s += dtw(bufA_, ni, bufB_, nj);
        k++;
      }
    }
    thr_[w] = (s / k) * factor_;
  }

  void updateSpeechRatio() {
    float s = 0;
    int k = 0;
    for (int w = 0; w < NWORD; w++) {
      if (nTpl_[w] > 0 && spr_[w] > 0) { s += spr_[w]; k++; }
    }
    speechRatio_ = k ? s / k : 0;
  }

  void saveWord(int w) {
    Preferences p;
    if (!p.begin(nsName(w), false)) return;
    p.putUChar("n", (uint8_t)nTpl_[w]);
    p.putFloat("thr", thr_[w]);
    p.putFloat("fac", factor_);
    p.putFloat("spr", spr_[w]);
    char key[3] = {0, 0, 0};
    for (int t = 0; t < nTpl_[w]; t++) {
      const int len = tplLen_[w][t];
      key[0] = 't'; key[1] = (char)('0' + t);
      p.putBytes(key, tpl_[w][t], (size_t)len * NCEP * sizeof(int16_t));
      key[0] = 'l';
      p.putUChar(key, (uint8_t)len);
    }
    p.end();
  }

  void loadWord(int w) {
    Preferences p;
    if (!p.begin(nsName(w), true)) return;
    if (w == 0) factor_ = p.getFloat("fac", HELLO_THR_FACTOR);
    const int n = p.getUChar("n", 0);
    int ok = 0;
    char key[3] = {0, 0, 0};
    for (int t = 0; t < n && t < HELLO_TEMPLATES; t++) {
      key[0] = 'l'; key[1] = (char)('0' + t);
      const int len = p.getUChar(key, 0);
      if (len <= 0 || len > HELLO_MAX_FRAMES) break;
      key[0] = 't';
      const size_t want = (size_t)len * NCEP * sizeof(int16_t);
      if (p.getBytes(key, tpl_[w][t], want) != want) break;
      tplLen_[w][t] = (uint8_t)len;
      ok++;
    }
    nTpl_[w] = (ok == n) ? n : 0;
    thr_[w] = nTpl_[w] ? p.getFloat("thr", 0) : 0;
    spr_[w] = nTpl_[w] ? p.getFloat("spr", 0) : 0;
    p.end();
  }

  static bool isStop(int w) { return w == 4; }

  // Lúc train: ngưỡng thấp để bắt được giọng nhỏ. Sau train: chặt để khỏi nhận ù servo.
  float onRatio(bool train) const {
    if (train) return 2.0f;
    float r = (speechRatio_ <= 0) ? HELLO_VAD_ON
                                  : constrain(speechRatio_ * 0.35f, 2.4f, HELLO_VAD_ON);
    if (noisy_) r = fmaxf(r, 4.2f);
    return r;
  }

  float peakMin(bool train) const {
    if (train) return 2.5f;
    float m = (speechRatio_ <= 0) ? HELLO_PEAK_MIN
                                  : constrain(speechRatio_ * 0.40f, 3.2f, HELLO_PEAK_MIN);
    if (noisy_) m = 2.3f;  // so với nền đã gồm ù servo, không phải phòng yên
    return m;
  }

  // Bỏ âm thanh trong mic (vd tiếng loa vừa phát) rồi làm lại từ đầu
  void flushMic(int ms) {
    int32_t raw[HOP];
    const int hops = ms * FS / 1000 / HOP;
    audio_->lockMic();
    for (int i = 0; i < hops; i++) audio_->readMic(raw, HOP, 100);
    audio_->unlockMic();
    memset(win_, 0, sizeof(win_));
    preCount_ = 0;
  }

  static void taskEntry(void* arg) { static_cast<VoiceHello*>(arg)->run(); }

  void promptTrain(int idx) {
    note("[TRAIN] Noi '%s' lan %d/%d ...", wordName(trainWord_), idx + 1, HELLO_TEMPLATES);
  }

  // Xử lý 1 đoạn tiếng vừa nói xong
  void handleSegment(int n, float peakRatio, int& trainIdx, uint32_t& cooldownUntil,
                     uint32_t& trainPromptAt) {
    const int ms = n * HOP * 1000 / FS;
    const bool train = trainIdx >= 0;
    const int minMs = train ? 140 : (noisy_ ? 140 : HELLO_MIN_MS);
    if (ms < minMs || peakRatio < peakMin(train)) {
      if (debug_) note("  [dbg] bo doan %d ms, dinh/nen %.1f", ms, peakRatio);
      return;
    }
    cmn(seg_, n);

    if (train) {
      const int w = trainWord_;
      if (ms < TRAIN_MIN_MS || ms > TRAIN_MAX_MS) {
        note("[TRAIN] bo qua doan %d ms (can %d..%d ms) — noi lai", ms, TRAIN_MIN_MS, TRAIN_MAX_MS);
        return;
      }
      storeTpl(w, trainIdx, seg_, n);
      trainPeak_[trainIdx] = peakRatio;
      note("[TRAIN] '%s' da ghi mau %d/%d (%d ms, to gap %.1f lan nen)",
           wordName(w), trainIdx + 1, HELLO_TEMPLATES, ms, peakRatio);
      trainIdx++;
      audio_->beep(1300, 80, false);
      flushMic(250);
      if (trainIdx >= HELLO_TEMPLATES) {
        nTpl_[w] = HELLO_TEMPLATES;
        float sr = 0;
        for (int t = 0; t < HELLO_TEMPLATES; t++) sr += trainPeak_[t];
        spr_[w] = sr / HELLO_TEMPLATES;
        updateSpeechRatio();
        recomputeThreshold(w);
        saveWord(w);
        trainIdx = -1;
        training_ = false;
        note("[TRAIN] XONG '%s', nguong %.2f. Gio noi '%s' de thu.", wordName(w), thr_[w], wordName(w));
        cooldownUntil = millis() + 600;
      } else {
        promptTrain(trainIdx);
        trainPromptAt = millis();
      }
      return;
    }

    // Luôn so mọi từ đã train, kể cả lúc đang đi — lệnh mới phải cắt được lệnh cũ.
    int bestW = -1, secondW = -1;
    float bestRatio = 1e30f, secondRatio = 1e30f, bestD = 0, bestThr = 0;
    for (int w = 0; w < NWORD; w++) {
      if (nTpl_[w] < HELLO_TEMPLATES || thr_[w] <= 0) continue;
      int meanLen = 0;
      for (int t = 0; t < nTpl_[w]; t++) meanLen += tplLen_[w][t];
      meanLen /= nTpl_[w];
      if (meanLen < 1) continue;
      float lenRatio = (float)n / (float)meanLen;
      if (lenRatio < 1.0f) lenRatio = 1.0f / lenRatio;
      if (lenRatio > 1.8f) continue;
      float d = 1e30f;
      for (int t = 0; t < nTpl_[w]; t++) {
        const int m = loadTpl(w, t, bufA_);
        const float x = dtw(seg_, n, bufA_, m);
        if (x < d) d = x;
      }
      const float t = thr_[w];
      const float r = (d / t) * (0.55f + 0.45f * lenRatio);
      if (debug_) note("  [dbg] %s: d=%.2f thr=%.2f r=%.2f len=%d/%d", wordName(w), d, t, r, n, meanLen);
      if (r < bestRatio) {
        secondRatio = bestRatio;
        secondW = bestW;
        bestRatio = r;
        bestW = w;
        bestD = d;
        bestThr = t;
      } else if (r < secondRatio) {
        secondRatio = r;
        secondW = w;
      }
    }
    if (bestW < 0) {
      if (debug_) note("  [dbg] bo doan %d ms: khong giong mau", ms);
      return;
    }
    const float accept = noisy_ ? 1.08f : 1.00f;
    const float sep = isStop(bestW) ? 0.82f : 0.88f;
    const bool hasOther = secondRatio < 1e20f;
    const bool clear = hasOther ? (bestRatio < secondRatio * sep) : true;
    if (bestRatio < accept && clear) {
      note("[VOICE] nghe thay '%s' (do lech %.2f < %.2f)", wordName(bestW), bestD, bestThr);
      if (bestW == 0) {
        static const char* names[] = {"gau gau", "nhac vui", "chip chip"};
        const int k = audio_->playReply();
        if (k >= 0) note("  loa phat: %s", names[k]);
      } else {
        audio_->beep(1500, 70, false);
        heard_ = bestW;
      }
      flushMic(250);
      cooldownUntil = millis() + 700;
    } else {
      cooldownUntil = millis() + 280;
      static uint32_t lastNear = 0;
      if (debug_ || (bestRatio < 1.35f && millis() - lastNear > 1500)) {
        lastNear = millis();
        if (secondW >= 0) {
          note("[VOICE] gan '%s' (%.2f) / '%s' (%.2f) — chua chac",
               wordName(bestW), bestRatio, wordName(secondW), secondRatio);
        } else {
          note("[VOICE] gan giong '%s' nhung chua chac (ti le %.2f, can < %.2f)",
               wordName(bestW), bestRatio, accept);
        }
      }
    }
  }

  void run() {
    int32_t raw[HOP];
    float prevX = 0;
    float floorE = 0;
    float busyE = 0;  // nền gồm cả ù servo lúc đang đi
    int warm = 0;
    bool speaking = false;
    int loud = 0, quiet = 0;
    float peak = 0;
    uint32_t cooldownUntil = 0;
    int trainIdx = -1;
    uint32_t trainPromptAt = 0;
    int dbgHops = 0;
    float dbgMax = 0;

    for (;;) {
      if (trainReq_) {
        trainReq_ = false;
        trainIdx = 0;
        speaking = false;
        loud = 0;
        promptTrain(0);
        trainPromptAt = millis();
      }
      if (recomputeReq_) {
        recomputeReq_ = false;
        for (int w = 0; w < NWORD; w++) {
          if (nTpl_[w] < HELLO_TEMPLATES) continue;
          recomputeThreshold(w);
          saveWord(w);
        }
        note("[VOICE] he so %.2f -> da tinh lai nguong", factor_);
      }
      if (trainIdx >= 0 && millis() - trainPromptAt > 10000) {
        promptTrain(trainIdx);
        trainPromptAt = millis();
      }
      if (!enabled_ && trainIdx < 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }

      audio_->lockMic();
      const size_t got = audio_->readMic(raw, HOP, 100);
      audio_->unlockMic();
      if (got < (size_t)HOP) {
        vTaskDelay(pdMS_TO_TICKS(5));
        continue;
      }

      // Nhấn cao tần (bớt tiếng ù servo), tính mức năng lượng
      memmove(win_, win_ + HOP, (NFFT - HOP) * sizeof(float));
      float e = 0;
      for (int i = 0; i < HOP; i++) {
        const float x = (float)(raw[i] >> 8) / 8388608.0f;
        const float y = x - 0.97f * prevX;
        prevX = x;
        win_[NFFT - HOP + i] = y;
        e += fabsf(y);
      }
      e /= HOP;

      float feat[NCEP];
      mfcc(feat);
      memcpy(pre_[preHead_], feat, sizeof(feat));
      preHead_ = (preHead_ + 1) % PREROLL;
      if (preCount_ < PREROLL) preCount_++;

      if (warm < 30) {
        floorE = warm ? floorE * 0.9f + e * 0.1f : e;
        busyE = floorE;
        warm++;
        continue;
      }
      if (!speaking) {
        busyE = (busyE <= 0) ? e : busyE * 0.90f + e * 0.10f;
      }
      if (debug_) {
        if (e > dbgMax) dbgMax = e;
        if (++dbgHops >= 31) {
          note("  [dbg] nen=%.5f  to nhat=%.5f (gap %.1f)  %s",
               floorE, dbgMax, dbgMax / floorE, speaking ? "DANG NOI" : "");
          dbgHops = 0;
          dbgMax = 0;
        }
      }
      if (millis() < cooldownUntil) continue;

      if (!speaking) {
        float onLevel = fmaxf(floorE * onRatio(trainIdx >= 0), HELLO_VAD_MIN);
        if (noisy_ && trainIdx < 0) {
          onLevel = fmaxf(onLevel, busyE * 1.65f);
        }
        const int needLoud = (noisy_ && trainIdx < 0) ? 4 : 3;
        if (e > onLevel) {
          if (++loud >= needLoud) {
            speaking = true;
            quiet = 0;
            peak = e;
            nSeg_ = 0;
            for (int k = 0; k < preCount_; k++) {
              const int idx = (preHead_ - preCount_ + k + PREROLL) % PREROLL;
              memcpy(seg_[nSeg_++], pre_[idx], sizeof(feat));
            }
          }
        } else {
          loud = 0;
          floorE = floorE * 0.97f + e * 0.03f;
        }
      } else {
        if (nSeg_ < HELLO_MAX_FRAMES) memcpy(seg_[nSeg_++], feat, sizeof(feat));
        if (e > peak) peak = e;
        const float offLevel = fmaxf(floorE * HELLO_VAD_OFF, peak * HELLO_END_PEAK);
        const float backToServo = fmaxf(busyE * 1.35f, offLevel);
        if (noisy_ && trainIdx < 0) {
          if (e < backToServo) quiet++;
          else quiet = 0;
        } else {
          if (e < offLevel) quiet++;
          else if (e < peak * 0.42f && nSeg_ >= 10) quiet++;
          else quiet = 0;
        }
        const int needQuiet = (noisy_ && trainIdx < 0) ? 10 : ((nSeg_ <= 45) ? 10 : 18);
        if (quiet >= needQuiet || nSeg_ >= HELLO_MAX_FRAMES) {
          int n = nSeg_ - quiet + 3;
          if (n > nSeg_) n = nSeg_;
          if (n < 8) n = nSeg_;
          speaking = false;
          loud = 0;
          const float base = fmaxf(floorE, busyE * 0.75f);
          handleSegment(n, peak / base, trainIdx, cooldownUntil, trainPromptAt);
        }
      }
    }
  }
};
