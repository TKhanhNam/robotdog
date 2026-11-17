#pragma once

// ============================================================
// Robot Dog — cấu hình phần cứng & hành vi
// ESP32-S3 + PCA9685 + MPU6050 + SRF04 + INMP441 + MAX98357A
// ============================================================

// ---------- WiFi điều khiển (không cần cắm USB sau khi nạp) ----------
// Robot tự phát WiFi. Điện thoại/laptop nối vào mạng này rồi mở http://192.168.4.1
#define WIFI_AP_SSID     "RobotDog"
#define WIFI_AP_PASS     "robotdog123"   // tối thiểu 8 ký tự
#define WIFI_AP_ENABLE   true
// Chạy pin: phát yếu hơn để đỡ sụt áp lúc phát sóng. 8.5 dBm vẫn đủ trong một phòng.
#define WIFI_AP_TX_POWER WIFI_POWER_8_5dBm

// Hotspot điện thoại: chó tự nối vào để điện thoại vẫn có 4G bình thường. Để trống = không nối.
// Hotspot phải phát băng tần 2.4 GHz (ESP32 không bắt được 5 GHz).
#define HOTSPOT_SSID     ""
#define HOTSPOT_PASS     ""
// Chó báo địa chỉ của mình trên mạng hotspot qua UDP broadcast cổng này.
#define BEACON_PORT      4210

// Chỉ dùng khi ENABLE_CLOUD_AI (đang tắt)
#define WIFI_SSID        "YOUR_WIFI_SSID"
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"

// OpenAI-compatible API (OpenAI / Groq / local proxy...)
#define AI_API_KEY       "YOUR_API_KEY"
#define AI_BASE_URL      "https://api.openai.com"
#define AI_CHAT_MODEL    "gpt-4o-mini"
#define AI_STT_MODEL     "whisper-1"
#define AI_TTS_MODEL     "tts-1"
#define AI_TTS_VOICE     "alloy"

// true = bật STT/Chat/TTS qua WiFi; false = chỉ Serial + lệnh từ khóa đơn giản
#define ENABLE_CLOUD_AI  false

// true = chi USB (khong pin): quay tung servo, dam USB khong sap
// Van PHAI jumper ESP32 chan 5V -> oc xanh V+ PCA, OE -> GND
#define POWER_FROM_USB   false

// ---------- I2C: PCA9685 (0x40) + MPU6050 (0x68) ----------
#define PIN_I2C_SDA      8
#define PIN_I2C_SCL      9
#define PCA9685_ADDR     0x40
#define MPU6050_ADDR     0x68

// ---------- Servo PCA9685 — mỗi chân 1 cặp: cẳng số chẵn, đùi số lẻ ----------
// 0/1 trái sau | 2/3 phải sau | 4/5 trái trước | 6/7 phải trước
enum ServoCh : uint8_t {
  BL_KNEE = 0,  // cẳng trái sau
  BL_HIP  = 1,  // đùi trái sau
  BR_KNEE = 2,  // cẳng phải sau
  BR_HIP  = 3,  // đùi phải sau
  FL_KNEE = 4,  // cẳng trái trước
  FL_HIP  = 5,  // đùi trái trước
  FR_KNEE = 6,  // cẳng phải trước
  FR_HIP  = 7,  // đùi phải trước
  SERVO_COUNT = 8
};

// Xung PWM MG90S (microseconds) — chỉnh nếu góc thực tế lệch
#define SERVO_MIN_US     500
#define SERVO_MAX_US     2500
#define SERVO_FREQ_HZ    50

// Góc STAND theo kênh (đã chỉnh tay trên robot)
static const int16_t STAND_DEG_DEFAULT[SERVO_COUNT] = {
  155, // 0 cẳng trái sau
  90,  // 1 đùi trái sau   — 155 cong trước, 25 cong sau
  90,  // 2 cẳng phải sau
  90,  // 3 đùi phải sau
  25,  // 4 cẳng trái trước
  90,  // 5 đùi trái trước — dưới ~85 bị kẹt
  150, // 6 cẳng phải trước — 76..110 còn co về trước
  76   // 7 đùi phải trước — 90 ngả sau
};

// Tư thế ngồi: lệch so với STAND (độ, cùng chiều GAIT_SIGN).
// Đùi âm = gập về sau, cẳng dương = gập về trước. Cẳng ≈ 1,5–2 × đùi thì bàn chân nằm dưới hông.
// Cẳng phải sau (CH2, stand 90) chỉ gập thêm được tới 175 → tối đa 85°.
// sit2 = 2 chân sau quỳ, 2 chân trước đứng.
static const int8_t SIT2_OFF[SERVO_COUNT] = {
  85,  // 0 cẳng trái sau
  -55, // 1 đùi trái sau
  85,  // 2 cẳng phải sau
  -55, // 3 đùi phải sau
  0,   // 4 cẳng trái trước
  0,   // 5 đùi trái trước
  0,   // 6 cẳng phải trước
  0    // 7 đùi phải trước
};
// sit4 = cả 4 chân quỳ.
static const int8_t SIT4_OFF[SERVO_COUNT] = {
  85,  // 0 cẳng trái sau
  -55, // 1 đùi trái sau
  85,  // 2 cẳng phải sau
  -55, // 3 đùi phải sau
  85,  // 4 cẳng trái trước
  -55, // 5 đùi trái trước
  85,  // 6 cẳng phải trước
  -55  // 7 đùi phải trước
};
// Thời gian hạ người / đứng dậy (ms)
#define SIT_MOVE_MS            900

// Bắt tay: đang đứng, giơ chân trước phải (CH6, CH7) ra trước và lắc, xong đứng lại.
// Đùi phải trước (CH7, stand 76) vung trước tối đa ~70° (xuống 5).
#define PAW_THIGH_FWD          55
#define PAW_CALF_FWD           0
#define PAW_SHAKE_AMP          12
#define PAW_SHAKES             3
#define PAW_SHAKE_MS           420
#define PAW_MOVE_MS            600
// Bắt tay trái (CH4 cẳng, CH5 đùi). Đùi trái trước kẹt dưới ~85 nên không vung trước được:
// giơ bàn chân bằng cách gập cẳng về trước, và lắc bằng cẳng.
#define PAW_L_THIGH_FWD        0
#define PAW_L_CALF_FWD         70
#define PAW_L_SHAKE_AMP        12

// ---------- Dáng đi: trot chéo (trái sau + phải trước) ↔ (phải sau + trái trước) ----------
// GAIT_SIGN: chiều góc làm đùi vung VỀ TRƯỚC / cẳng chĩa VỀ TRƯỚC.
// Sai chiều kênh nào → gửi Serial "flip <ch>" (lưu vào flash, không cần nạp lại).
static const int8_t GAIT_SIGN_DEFAULT[SERVO_COUNT] = {
  -1, // 0 cẳng trái sau
  +1, // 1 đùi trái sau
  +1, // 2 cẳng phải sau
  -1, // 3 đùi phải sau
  +1, // 4 cẳng trái trước
  -1, // 5 đùi trái trước
  -1, // 6 cẳng phải trước
  -1  // 7 đùi phải trước
};

// Giới hạn góc khi đi/chạy
static const int16_t GAIT_MIN_DEG[SERVO_COUNT] = {5, 5, 5, 5, 5, 85, 5, 5};
static const int16_t GAIT_MAX_DEG[SERVO_COUNT] = {175, 175, 175, 175, 175, 175, 175, 175};

// ---------- Dáng đi theo SovGVD/esp32-robot-dog-code: trot + IK 2 khớp mỗi chân ----------
// STAND coi như chân duỗi thẳng đứng. Đo từ tâm trục servo đùi tới tâm trục gối, và từ gối tới mũi bàn chân.
#define LEG_THIGH_MM           55
#define LEG_CALF_MM            60
// Độ cao hông khi đi (tỉ lệ chiều dài chân duỗi thẳng). Thấp hơn = gối gập nhiều, servo gối chịu tải nặng hơn.
#define WALK_HEIGHT            0.92f
#define RUN_HEIGHT             0.92f
// Một bước bàn chân dịch trước–sau bao nhiêu mm, và nhấc khỏi đất bao nhiêu mm.
// Cẳng phải sau (CH2) chạm 175 khi nhấc quá ~20 mm ở độ cao 0.92.
#define WALK_STRIDE_MM         48
#define RUN_STRIDE_MM          62
#define WALK_LIFT_MM           20
#define RUN_LIFT_MM            20
// Mỗi cặp chân chéo vung trong phần này của nửa nhịp, phần còn lại cả 4 chân chạm đất (SovGVD: 100/200 ms)
#define GAIT_SWING_FRAC        0.6f
// Chỉnh riêng từng kênh khi đi/chạy (độ, cùng chiều GAIT_SIGN; âm = kéo về sau, cẳng thẳng hơn).
// Cẳng trái CH0, CH4 rướn ra trước hơn cẳng phải CH2, CH6.
static const int8_t GAIT_TRIM_DEG[SERVO_COUNT] = {
  -20, // 0 cẳng trái sau
  0,   // 1 đùi trái sau
  0,   // 2 cẳng phải sau
  0,   // 3 đùi phải sau
  -20, // 4 cẳng trái trước
  0,   // 5 đùi trái trước
  0,   // 6 cẳng phải trước
  0    // 7 đùi phải trước
};
// Chu kỳ một bước đủ 2 cặp chân (SovGVD: 400 ms; MG90S cập nhật 50 Hz nên chậm hơn chút)
#define GAIT_CYCLE_WALK_MS     800
#define GAIT_CYCLE_RUN_MS      520
// Thời gian hạ người từ stand xuống tư thế đi
#define GAIT_RAMP_MS           450
// Động cơ 8: xung thấp = phải hơn. Quét đều 2 bên quanh giữa 1500 µs:
// lệnh "radar mid" giữ đầu ở giữa để lắp lại càng servo cho cảm biến nhìn thẳng.
// Chỉnh biên độ không cần nạp lại: "radar <phai_us> <trai_us>" (lưu flash).
#define RADAR_CH               8
#define RADAR_RIGHT_US         1050
#define RADAR_LEFT_US          1950
#define RADAR_PERIOD_MS        2800
#define STAND_PREP_MS          700
#define STAND_HOLD_MS          400
#define GAIT_TICK_MS           20

// DIR theo kênh (đã chỉnh cho stand)
static const int8_t SERVO_DIR[SERVO_COUNT] = {
  -1, // 0 cẳng trái sau
  -1, // 1 đùi trái sau
  -1, // 2 cẳng phải sau
  -1, // 3 đùi phải sau
  +1, // 4 cẳng trái trước
  +1, // 5 đùi trái trước
  +1, // 6 cẳng phải trước
  -1  // 7 đùi phải trước
};

// Offset góc (độ) sau lắp ráp — tinh chỉnh từng khớp
static const int8_t SERVO_OFFSET[SERVO_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0
};

// Pin map khớp hướng dẫn ChatGPT (share 6ab28a70) — bộ bạn vừa lắp.
// PCA9685 vẫn GPIO 8/9. MPU ưu tiên Wire 8/9 (chung PCA); nếu không thấy thì Wire1 11/12.
#define PIN_I2C2_SDA     11
#define PIN_I2C2_SCL     12

// ---------- Siêu âm HC-SR04 ----------
// Trắng=Vcc 5V, Nâu=Trig, Đỏ=Echo (chia áp), Đen=GND
#define PIN_US_TRIG      18
#define PIN_US_ECHO      10
#define OBSTACLE_CM      18

// ---------- Loa MAX98357A (I2S TX) ----------
#define PIN_SPK_BCLK     4
#define PIN_SPK_LRC      5
#define PIN_SPK_DIN      6
#define SPK_SAMPLE_RATE  16000

// ---------- Micro INMP441 (I2S RX) ----------
#define PIN_MIC_SCK      15   // BCLK
#define PIN_MIC_WS       16   // WS / LRCL
#define PIN_MIC_SD       17   // SD / DOUT
#define MIC_SAMPLE_RATE  16000
// L/R micro → GND (kênh trái)

// ---------- Hành vi ----------
#define FALL_ROLL_DEG        45.0f
#define FALL_PITCH_DEG       45.0f
#define FALL_CONFIRM_MS      400
#define VOICE_LISTEN_MS      3000
#define VOICE_ENERGY_THRESH  1800  // ngưỡng phát hiện nói (tinh chỉnh)

// ---------- Nghe "hello" (so mẫu giọng đã ghi bằng lệnh train) ----------
#define HELLO_TEMPLATES      3       // số lần nói mẫu khi train
#define HELLO_MAX_FRAMES     100     // 100 × 16 ms = 1,6 s
#define HELLO_MIN_MS         160     // ngắn hơn: tiếng gõ / ù servo, bỏ qua
#define HELLO_VAD_ON         3.2f    // to hơn nền bao nhiêu lần thì coi là bắt đầu nói
#define HELLO_VAD_OFF        1.8f    // nhỏ hơn mức này đủ lâu thì coi là nói xong
#define HELLO_VAD_MIN        0.0005f // mức tối thiểu (tỉ lệ toàn thang)
#define HELLO_END_PEAK       0.22f   // nhỏ hơn 22% đỉnh của câu → coi là im
#define HELLO_PEAK_MIN       4.0f    // đỉnh câu phải to hơn nền
#define HELLO_THR_FACTOR     1.35f   // ngưỡng = khoảng cách giữa các mẫu × hệ số
#define HELLO_COOLDOWN_MS    1200
