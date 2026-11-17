# Robot Dog ESP32-S3 — Hướng dẫn lắp ráp & nạp code

## 1. Sơ đồ nguồn (quan trọng)

```
  [18650 #1]──series──[18650 #2]     ≈ 7.4 V (đầy ~8.4 V)
              │
         XL4015 VIN+ / VIN−
              │
         XL4015 OUT = 5.0 V  (chỉnh trimmer, đo bằng đồng hồ)
              │
     ┌────────┼────────────────────────┐
     │        │                        │
  PCA9685   ESP32-S3                 MAX98357A
  V+ (servo) VIN/5V                  VIN
     │        │                        │
    GND ──── GND ──────────────────── GND   ← chung tất cả module
```

| Nguồn | Module |
|--------|--------|
| **5 V** (từ XL4015) | PCA9685 **V+**, ESP32 **5V**, MAX98357A **VIN**, SRF04 **VCC** |
| **3.3 V** (từ ESP32) | PCA9685 **VCC**, MPU6050 **VCC**, INMP441 **VDD**, kéo I2C |
| **GND chung** | Mọi module |

**Cảnh báo**
- 2 pin Samsung 25R nối **tiếp** trên đế → đúng với XL4015 (không nối song song).
- Servo MG90S kéo dòng lớn khi đứng/đi → tụ 470–1000 µF trên rail 5 V gần PCA9685 V+ rất nên có.
- **Không** cấp 7.4 V thẳng vào ESP32 hay servo 5 V.

---

## 2. Sơ đồ tín hiệu (khớp `config.h`)

### I2C (chung bus)

| Tín hiệu | ESP32-S3 | Module |
|----------|----------|--------|
| SDA | GPIO **8** | PCA9685 SDA, MPU6050 SDA |
| SCL | GPIO **9** | PCA9685 SCL, MPU6050 SCL |
| 3V3 | 3V3 | PCA9685 VCC, MPU6050 VCC |
| GND | GND | GND |

PCA9685 địa chỉ mặc định `0x40`, MPU6050 `0x68`.

### Servo MG90S → PCA9685

| Kênh PCA9685 | Chân | Vị trí cơ thể |
|--------------|------|----------------|
| 0 | FL_HIP | Trước trái — hông |
| 1 | FL_KNEE | Trước trái — gối |
| 2 | FR_HIP | Trước phải — hông |
| 3 | FR_KNEE | Trước phải — gối |
| 4 | BL_HIP | Sau trái — hông |
| 5 | BL_KNEE | Sau trái — gối |
| 6 | BR_HIP | Sau phải — hông |
| 7 | BR_KNEE | Sau phải — gối |

Mỗi servo: **nâu/đen → GND**, **đỏ → PCA9685 V+ (5V)**, **cam/vàng → PWMx**.

Dùng dây đực–cái / cái–cái 40P theo chân header.

### Siêu âm SRF04 / HC-SR04

| Chân cảm biến | ESP32-S3 |
|---------------|----------|
| TRIG | GPIO **10** |
| ECHO | GPIO **11** (qua **bộ chia áp**) |
| VCC | 5 V |
| GND | GND |

ESP32-S3 **không chịu 5 V** trên GPIO. ECHO 5 V → chia áp:

```
ECHO ──[1kΩ]──●── GPIO11
               │
             [2kΩ]
               │
              GND
```

(Hoặc 2k2 / 3k3 tương đương tỷ lệ ≈ 2:1.)

### Micro INMP441 (I2S)

| INMP441 | ESP32-S3 |
|---------|----------|
| VDD | 3.3 V |
| GND | GND |
| WS / LRCL | GPIO **4** |
| SCK / BCLK | GPIO **5** |
| SD / DOUT | GPIO **6** |
| L/R | **GND** (kênh trái) |

### Loa MAX98357A + loa 3W

| MAX98357A | ESP32-S3 / loa |
|-----------|----------------|
| VIN | 5 V |
| GND | GND |
| BCLK | GPIO **16** |
| LRC | GPIO **17** |
| DIN | GPIO **18** |
| GAIN | để trống hoặc theo datasheet |
| SD | kéo 3.3 V nếu module có chân shutdown |
| + / − loa | Loa mini 3W |

---

## 3. Lắp khung chó 8DOF (gợi ý)

1. Thân chữ nhật (mica / in 3D): gắn PCA9685, ESP32, pin, XL4015 phía bụng.
2. Mỗi chân 2 khớp: **hông** (xoay trước–sau), **gối** (gập).
3. MPU6050 cố định giữa thân, mũi chữ Z hướng lên, trục X về phía đầu chó.
4. SRF04 gắn mũi / ngực, hướng đi tới.
5. Mic hướng ra ngoài; loa hướng xuống hoặc ra hai bên (tránh feedback mic).

Sau khi lắp, chỉnh trong `config.h`:

- `NEUTRAL` — góc đứng thẳng
- `SERVO_DIR[]` — đảo servo lắp ngược (`+1` / `-1`)
- `SERVO_OFFSET[]` — bù lệch cơ khí từng khớp

---

## 4. Cài Arduino IDE & nạp code

1. Cài [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. Thêm ESP32 board package (Espressif) → chọn **ESP32S3 Dev Module**.
3. Tools gợi ý:
   - USB CDC On Boot: **Enabled**
   - Flash Size: theo board (thường 8MB / 16MB)
   - PSRAM: OPI / QSPI nếu board có
4. Library Manager cài:
   - **Adafruit PWM Servo Driver Library**
   - **Adafruit MPU6050**
   - **Adafruit Unified Sensor**
5. Mở thư mục `robotdog` (file `robotdog.ino`).
6. Cắm USB-A → C, chọn COM port, Upload.

---

## 5. Điều khiển

Mở Serial Monitor **115200 baud**.

| Lệnh | Việc làm |
|------|----------|
| `walk` / `di` | Đi bộ (trot) |
| `run` / `chay` | Chạy |
| `stop` / `dung` | Dừng, về tư thế đứng |
| `sit` / `ngoi` | Ngồi |
| `stand` | Đứng |
| `getup` | Chuỗi đứng dậy |
| `chat Ban khoe khong?` | Trò chuyện (offline hoặc cloud) |
| `status` | Kiểm tra |

**Tự động**
- MPU6050 nghiêng > 45° một lúc → `GetUp`
- SRF04 < 18 cm khi đang walk/run → `Stop`
- Mic phát hiện năng lượng cao → beep (offline) hoặc STT (cloud)

---

## 6. Bật AI đám mây (trò chuyện + nhận lệnh giọng nói)

Trong `config.h`:

```cpp
#define ENABLE_CLOUD_AI  true
#define WIFI_SSID        "ten_wifi"
#define WIFI_PASSWORD    "mat_khau"
#define AI_API_KEY       "sk-..."
#define AI_BASE_URL      "https://api.openai.com"
```

Cần WiFi và API key (OpenAI hoặc proxy tương thích). Offline (`ENABLE_CLOUD_AI false`) vẫn điều khiển đủ bằng Serial + câu trả lời có sẵn.

---

## 7. Checklist an toàn trước khi cho chạy

- [ ] XL4015 đã chỉnh đúng **5.0 V** (đo trước khi cắm ESP32/servo)
- [ ] GND chung toàn hệ
- [ ] ECHO siêu âm đã chia áp
- [ ] Pin 18650 đúng chiều, không chạm ngắn
- [ ] Thử `stand` / từng chân qua Serial trước khi `walk`
- [ ] Không để tay gần khớp khi `run`

---

## 8. Cấu trúc code

| File | Vai trò |
|------|---------|
| `config.h` | Pin, góc trung lập, WiFi/AI |
| `ServoDriver.h` | PCA9685 + MG90S |
| `GaitController.h` | walk / run / sit / getup |
| `IMUSensor.h` | MPU6050 + phát hiện ngã |
| `Ultrasonic.h` | SRF04 |
| `AudioIO.h` | INMP441 + MAX98357A |
| `VoiceAI.h` | Lệnh + chat |
| `robotdog.ino` | `setup` / `loop` |

Chúc bạn lắp ráp thành công — tinh chỉnh `NEUTRAL` / `SERVO_DIR` là bước quan trọng nhất để đi được thẳng.
