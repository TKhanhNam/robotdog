#pragma once
#include <Arduino.h>
#include <math.h>
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_random.h"
#include "freertos/semphr.h"
#include "config.h"

class AudioIO {
public:
  bool begin() {
    if (!micMutex_) micMutex_ = xSemaphoreCreateMutex();
    bclk_ = PIN_SPK_BCLK;
    ws_ = PIN_SPK_LRC;
    dout_ = PIN_SPK_DIN;
    bool spk = beginSpk();
    bool mic = beginMic();
    ok_ = spk;
    Serial0.printf("[Audio] loa=%s mic=%s  BCLK=%d LRC=%d DIN=%d\r\n",
                   spk ? "OK" : "FAIL", mic ? "OK" : "FAIL", bclk_, ws_, dout_);
    Serial.printf("[Audio] loa=%s mic=%s  BCLK=%d LRC=%d DIN=%d\r\n",
                  spk ? "OK" : "FAIL", mic ? "OK" : "FAIL", bclk_, ws_, dout_);
    return ok_;
  }

  bool ok() const { return ok_; }

  // PCM 16-bit mono, little-endian, 16000 Hz. Phục vụ loa khi điện thoại gửi tiếng nói / nhạc.
  void writeMonoPcm(const uint8_t* data, int len, uint8_t* carry, bool* hasCarry) {
    if (!tx_ || !data || len <= 0) return;
    uint8_t tmp[640];
    int n = 0;
    if (hasCarry && *hasCarry) {
      tmp[n++] = *carry;
      *hasCarry = false;
    }
    if (len > (int)sizeof(tmp) - n) len = (int)sizeof(tmp) - n;
    memcpy(tmp + n, data, (size_t)len);
    n += len;
    const int even = n - (n % 2);
    if ((n % 2) && carry && hasCarry) {
      *carry = tmp[even];
      *hasCarry = true;
    }
    const int samples = even / 2;
    for (int off = 0; off < samples;) {
      int chunk = samples - off;
      if (chunk > 256) chunk = 256;
      int16_t stereo[512];
      const uint8_t* p = tmp + off * 2;
      for (int i = 0; i < chunk; i++) {
        int16_t s = (int16_t)(p[i * 2] | (p[i * 2 + 1] << 8));
        stereo[i * 2] = s;
        stereo[i * 2 + 1] = s;
      }
      size_t wrote = 0;
      i2s_channel_write(tx_, stereo, (size_t)chunk * 4, &wrote, pdMS_TO_TICKS(50));
      off += chunk;
    }
  }

  bool usePins(int bclk, int ws, int dout) {
    stopSpk();
    bclk_ = bclk;
    ws_ = ws;
    dout_ = dout;
    bool spk = beginSpk();
    ok_ = spk;
    Serial0.printf("[Audio] doi chan BCLK=%d LRC=%d DIN=%d -> %s\r\n",
                   bclk_, ws_, dout_, spk ? "OK" : "FAIL");
    Serial.printf("[Audio] doi chan BCLK=%d LRC=%d DIN=%d -> %s\r\n",
                  bclk_, ws_, dout_, spk ? "OK" : "FAIL");
    return spk;
  }

  bool beep(uint16_t hz = 880, uint16_t ms = 500, bool log = true) {
    if (!tx_) {
      Serial0.println("[Audio] beep bo qua — I2S loa chua init");
      Serial.println("[Audio] beep bo qua — I2S loa chua init");
      return false;
    }
    const int n = (SPK_SAMPLE_RATE * (int)ms) / 1000;
    if (n <= 8) return false;
    // Data 16-bit: bộ đệm DMA là int16 xen kẽ trái/phải (slot 32-bit chỉ là khoảng trên dây)
    int16_t* buf = (int16_t*)malloc((size_t)n * 2 * sizeof(int16_t));
    if (!buf) return false;
    const float step = 6.2831853f * (float)hz / (float)SPK_SAMPLE_RATE;
    float ph = 0;
    for (int i = 0; i < n; i++) {
      const int16_t s = (int16_t)(sinf(ph) * 28000.0f);
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
      ph += step;
    }
    size_t w = 0;
    esp_err_t e = i2s_channel_write(tx_, buf, (size_t)n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(ms + 250));
    free(buf);
    if (log) {
      Serial0.printf("[Audio] beep %uHz pin %d/%d/%d write=%u %s\r\n",
                     (unsigned)hz, bclk_, ws_, dout_, (unsigned)w, esp_err_to_name(e));
      Serial.printf("[Audio] beep %uHz pin %d/%d/%d write=%u %s\r\n",
                    (unsigned)hz, bclk_, ws_, dout_, (unsigned)w, esp_err_to_name(e));
    }
    return e == ESP_OK && w > 0;
  }

  uint32_t measureEnergy(uint16_t ms = 200) {
    if (!rx_) return 0;
    const int frames = (MIC_SAMPLE_RATE * (int)ms) / 1000;
    const int bytes = frames * (int)sizeof(int32_t);
    int32_t* buf = (int32_t*)malloc((size_t)bytes);
    if (!buf) return 0;
    size_t got = 0;
    i2s_channel_read(rx_, buf, (size_t)bytes, &got, pdMS_TO_TICKS(ms + 80));
    const int n = (int)(got / sizeof(int32_t));
    uint64_t acc = 0;
    int used = 0;
    for (int i = 0; i < n; i++) {
      int32_t s = buf[i] >> 14;
      acc += (uint32_t)(s < 0 ? -s : s);
      used++;
    }
    free(buf);
    return used ? (uint32_t)(acc / (unsigned)used) : 0;
  }

  struct MicStats {
    int32_t minV;
    int32_t maxV;
    uint32_t avgAbs;
    int samples;
    int nonZero;
  };

  bool micOk() const { return rx_ != nullptr; }
  bool micLeft() const { return micLeft_; }

  // Task nghe giọng đọc mic liên tục; lệnh test phải giữ khóa này khi dùng mic
  void lockMic() { if (micMutex_) xSemaphoreTake(micMutex_, portMAX_DELAY); }
  void unlockMic() { if (micMutex_) xSemaphoreGive(micMutex_); }

  // Gọi khi đang giữ lockMic(). Trả về số mẫu đọc được.
  size_t readMic(int32_t* buf, size_t frames, uint32_t timeoutMs) {
    if (!rx_) return 0;
    size_t got = 0;
    i2s_channel_read(rx_, buf, frames * sizeof(int32_t), &got, pdMS_TO_TICKS(timeoutMs));
    return got / sizeof(int32_t);
  }

  // Âm thanh đáp lại: 0 tiếng sủa, 1 đoạn nhạc vui, 2 tiếng chíp; -1 = ngẫu nhiên
  int playReply(int kind = -1) {
    if (!tx_) return -1;
    if (kind < 0) kind = (int)(esp_random() % 3);
    const int fs = SPK_SAMPLE_RATE;
    const int maxFrames = fs * 9 / 10;
    int16_t* buf = (int16_t*)calloc((size_t)maxFrames * 2, sizeof(int16_t));
    if (!buf) return -1;
    int n = 0;

    auto put = [&](float v) {
      if (n >= maxFrames) return;
      if (v > 1.0f) v = 1.0f;
      if (v < -1.0f) v = -1.0f;
      const int16_t s = (int16_t)(v * 26000.0f);
      buf[n * 2] = s;
      buf[n * 2 + 1] = s;
      n++;
    };
    auto silence = [&](int ms) { for (int i = 0; i < fs * ms / 1000; i++) put(0); };

    if (kind == 0) {
      // Gâu gâu: giọng hạ dần, nhiều hài âm + chút tiếng xì, tắt nhanh
      const float startHz[2] = {720.0f, 820.0f};
      for (int b = 0; b < 2; b++) {
        const int len = fs * 200 / 1000;
        float ph = 0;
        for (int i = 0; i < len; i++) {
          const float t = (float)i / fs;
          const float hz = startHz[b] * (1.0f - 0.45f * (float)i / len);
          ph += 6.2831853f * hz / fs;
          const float env = (t < 0.008f ? t / 0.008f : 1.0f) * expf(-t / 0.07f);
          const float noise = ((float)(esp_random() & 0xFFFF) / 32768.0f - 1.0f) * 0.25f;
          put(env * (0.6f * sinf(ph) + 0.3f * sinf(2 * ph) + 0.15f * sinf(3 * ph) + noise));
        }
        silence(110);
      }
    } else if (kind == 1) {
      // Đô-mi-son-đô vui
      const float notes[] = {523.0f, 659.0f, 784.0f, 1047.0f, 784.0f, 1047.0f};
      const int lens[] = {110, 110, 110, 160, 90, 220};
      for (int k = 0; k < 6; k++) {
        const int len = fs * lens[k] / 1000;
        float ph = 0;
        for (int i = 0; i < len; i++) {
          ph += 6.2831853f * notes[k] / fs;
          const float t = (float)i / fs;
          const float rel = (float)(len - i) / fs;
          const float env = fminf(1.0f, t / 0.005f) * fminf(1.0f, rel / 0.03f);
          put(env * 0.8f * (sinf(ph) + 0.25f * sinf(2 * ph)));
        }
      }
    } else {
      // Chíp chíp: 2 lần vút lên rồi 1 lần xuống
      const float from[3] = {500.0f, 500.0f, 1600.0f};
      const float to[3] = {1600.0f, 1800.0f, 900.0f};
      const int lens[3] = {170, 170, 140};
      for (int k = 0; k < 3; k++) {
        const int len = fs * lens[k] / 1000;
        float ph = 0;
        for (int i = 0; i < len; i++) {
          const float hz = from[k] + (to[k] - from[k]) * (float)i / len;
          ph += 6.2831853f * hz / fs;
          const float t = (float)i / fs;
          const float rel = (float)(len - i) / fs;
          const float env = fminf(1.0f, t / 0.006f) * fminf(1.0f, rel / 0.02f);
          put(env * 0.8f * sinf(ph));
        }
        silence(60);
      }
    }

    size_t w = 0;
    const uint32_t ms = (uint32_t)n * 1000 / fs;
    i2s_channel_write(tx_, buf, (size_t)n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(ms + 300));
    free(buf);
    return kind;
  }

  // INMP441 L/R nối GND → kênh trái; nối 3V3 → kênh phải
  bool setMicSlot(bool left) {
    stopMic();
    micLeft_ = left;
    return beginMic();
  }

  // discardFirst: bỏ khối đầu vì mic cần ~80 ms ổn định sau khi có clock
  MicStats micStats(uint16_t ms = 250, bool discardFirst = true) {
    MicStats st = {0, 0, 0, 0, 0};
    if (!rx_) return st;
    const int frames = (MIC_SAMPLE_RATE * (int)ms) / 1000;
    const int bytes = frames * (int)sizeof(int32_t);
    int32_t* buf = (int32_t*)malloc((size_t)bytes);
    if (!buf) return st;
    size_t got = 0;
    if (discardFirst) i2s_channel_read(rx_, buf, (size_t)bytes, &got, pdMS_TO_TICKS(ms + 80));
    i2s_channel_read(rx_, buf, (size_t)bytes, &got, pdMS_TO_TICKS(ms + 80));
    const int n = (int)(got / sizeof(int32_t));
    uint64_t acc = 0;
    st.minV = INT32_MAX;
    st.maxV = INT32_MIN;
    for (int i = 0; i < n; i++) {
      const int32_t s = buf[i] >> 8; // 24-bit
      if (s != 0) st.nonZero++;
      if (s < st.minV) st.minV = s;
      if (s > st.maxV) st.maxV = s;
      acc += (uint32_t)(s < 0 ? -s : s);
    }
    free(buf);
    st.samples = n;
    if (n == 0) { st.minV = 0; st.maxV = 0; }
    st.avgAbs = n ? (uint32_t)(acc / (unsigned)n) : 0;
    return st;
  }

  // Loa phát tone (hoặc im lặng) trong khi mic thu cùng lúc.
  // Trả về công suất Goertzel trung bình tại hz trên tín hiệu mic.
  float toneLoopback(uint16_t hz, uint16_t ms, bool play) {
    if (!tx_ || !rx_) return -1.0f;
    const int block = MIC_SAMPLE_RATE / 50; // 20 ms; 1000/1500 Hz rơi đúng bin
    const int blocks = ms / 20;
    const size_t txBytes = (size_t)block * 2 * sizeof(int16_t);
    int16_t* tx = (int16_t*)malloc(txBytes);
    int32_t* rx = (int32_t*)malloc((size_t)block * sizeof(int32_t));
    if (!tx || !rx) { free(tx); free(rx); return -1.0f; }

    const float step = 6.2831853f * (float)hz / (float)SPK_SAMPLE_RATE;
    const float coeff = 2.0f * cosf(6.2831853f * (float)hz / (float)MIC_SAMPLE_RATE);
    float ph = 0;
    double acc = 0;
    int used = 0;

    for (int b = 0; b < blocks; b++) {
      for (int i = 0; i < block; i++) {
        const int16_t s = play ? (int16_t)(sinf(ph) * 28000.0f) : 0;
        tx[i * 2] = s;
        tx[i * 2 + 1] = s;
        ph += step;
        if (ph > 6.2831853f) ph -= 6.2831853f;
      }
      size_t w = 0, got = 0;
      i2s_channel_write(tx_, tx, txBytes, &w, pdMS_TO_TICKS(200));
      i2s_channel_read(rx_, rx, (size_t)block * 4, &got, pdMS_TO_TICKS(200));
      if (b < 4) continue; // bỏ đoạn đầu: DMA chưa ổn định
      const int n = (int)(got / sizeof(int32_t));
      float s1 = 0, s2 = 0;
      for (int i = 0; i < n; i++) {
        const float x = (float)(rx[i] >> 8) / 8388608.0f;
        const float s0 = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
      }
      if (n > 0) {
        acc += (s1 * s1 + s2 * s2 - coeff * s1 * s2) / ((float)n * (float)n);
        used++;
      }
    }

    // Đẩy im lặng để tone không còn trong DMA
    memset(tx, 0, txBytes);
    for (int b = 0; b < 6; b++) {
      size_t w = 0;
      i2s_channel_write(tx_, tx, txBytes, &w, pdMS_TO_TICKS(200));
    }
    free(tx);
    free(rx);
    return used ? (float)(acc / used) : -1.0f;
  }

private:
  i2s_chan_handle_t tx_ = nullptr;
  i2s_chan_handle_t rx_ = nullptr;
  SemaphoreHandle_t micMutex_ = nullptr;
  bool micLeft_ = true;
  bool ok_ = false;
  int bclk_ = PIN_SPK_BCLK;
  int ws_ = PIN_SPK_LRC;
  int dout_ = PIN_SPK_DIN;

  static void logErr(const char* step, esp_err_t e) {
    if (e == ESP_OK) return;
    Serial0.printf("[Audio] %s: %s\r\n", step, esp_err_to_name(e));
    Serial.printf("[Audio] %s: %s\r\n", step, esp_err_to_name(e));
  }

  void stopSpk() {
    if (!tx_) return;
    i2s_channel_disable(tx_);
    i2s_del_channel(tx_);
    tx_ = nullptr;
  }

  bool beginSpk() {
    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    // Không có dữ liệu mới thì phát 0; mặc định DMA phát lặp đoạn cuối → loa kêu mãi
    cfg.auto_clear = true;
    esp_err_t e = i2s_new_channel(&cfg, &tx_, NULL);
    logErr("spk new", e);
    if (e != ESP_OK) return false;

    i2s_std_slot_config_t slot =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    slot.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    i2s_std_config_t std = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SPK_SAMPLE_RATE),
      .slot_cfg = slot,
      .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = (gpio_num_t)bclk_,
        .ws = (gpio_num_t)ws_,
        .dout = (gpio_num_t)dout_,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
      },
    };
    e = i2s_channel_init_std_mode(tx_, &std);
    logErr("spk init", e);
    if (e != ESP_OK) return false;
    e = i2s_channel_enable(tx_);
    logErr("spk enable", e);
    return e == ESP_OK;
  }

  void stopMic() {
    if (!rx_) return;
    i2s_channel_disable(rx_);
    i2s_del_channel(rx_);
    rx_ = nullptr;
  }

  bool beginMic() {
    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t e = i2s_new_channel(&cfg, NULL, &rx_);
    if (e != ESP_OK) { logErr("mic new", e); rx_ = nullptr; return false; }
    i2s_std_slot_config_t slot =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
    slot.slot_mask = micLeft_ ? I2S_STD_SLOT_LEFT : I2S_STD_SLOT_RIGHT;
    i2s_std_config_t std = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
      .slot_cfg = slot,
      .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = (gpio_num_t)PIN_MIC_SCK,
        .ws = (gpio_num_t)PIN_MIC_WS,
        .dout = I2S_GPIO_UNUSED,
        .din = (gpio_num_t)PIN_MIC_SD,
        .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
      },
    };
    e = i2s_channel_init_std_mode(rx_, &std);
    if (e != ESP_OK) { logErr("mic init", e); i2s_del_channel(rx_); rx_ = nullptr; return false; }
    e = i2s_channel_enable(rx_);
    if (e != ESP_OK) { logErr("mic enable", e); i2s_del_channel(rx_); rx_ = nullptr; return false; }
    return true;
  }
};
