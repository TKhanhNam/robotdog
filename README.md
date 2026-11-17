# Robot Dog — chó robot ESP32-S3 nói chuyện bằng giọng nói

Chó robot bốn chân, điều khiển bằng app Android. Người dùng nói vào micro điện thoại, chó trả lời bằng loa gắn trên mình và thực hiện lệnh đi, chạy, ngồi, bắt tay. App tìm nhạc và phát qua loa chó.

Kho này gồm hai phần:

| Thư mục | Vai trò |
|---|---|
| Gốc repo (`robotdog.ino`, `config.h`, `*.h`) | Firmware Arduino cho ESP32-S3 |
| [`android/`](android) | App Android (Java) |

## Hệ thống hoạt động thế nào

```
Điện thoại (app)                         Chó (ESP32-S3)
  Micro ── ghi âm ──► Groq Whisper            │
  Chữ ── hỏi đáp ──► Gemini / Groq / OpenRouter
  Câu trả lời ── giọng nói (TTS) ──┐          │
  Lệnh đi / ngồi / bắt tay ────────┼─ WiFi ──► PCA9685 ── 8 servo MG90S
  Nhạc (iTunes, Deezer, radio) ────┘          │
                                          Loa MAX98357A
                                          Micro INMP441
                                          Cảm biến siêu âm HC-SR04 (quét đầu)
                                          MPU6050
```

Điện thoại bật hotspot 2.4 GHz. Chó vừa phát WiFi riêng, vừa nối vào hotspot đó và gửi beacon UDP để app tìm thấy địa chỉ của nó. Nhờ vậy điện thoại vẫn có mạng để gọi AI.

## Phần cứng

- ESP32-S3 (USB CDC, flash 16 MB, PSRAM OPI)
- PCA9685: 8 servo MG90S cho bốn chân, thêm kênh 8 quay đầu gắn HC-SR04
- MPU6050, INMP441, MAX98357A, HC-SR04
- Nguồn: 2 pin 18650 nối tiếp qua XL4015 hạ xuống 5 V. Không cấp 7.4 V thẳng vào ESP32 hoặc servo.

Sơ đồ nguồn, sơ đồ chân và ảnh lắp từng bước nằm ở [`README` cũ trong các mục bên dưới](#sơ-đồ-lắp-ráp) và thư mục [`huong-dan/`](huong-dan).

### Kênh servo

| Kênh | Khớp |
|---|---|
| 0, 1 | Cẳng, đùi trái sau |
| 2, 3 | Cẳng, đùi phải sau |
| 4, 5 | Cẳng, đùi trái trước |
| 6, 7 | Cẳng, đùi phải trước |
| 8 | Đầu (cảm biến siêu âm) |

## Nạp firmware

1. Cài [Arduino IDE](https://www.arduino.cc/en/software) và board **esp32 by Espressif** (board ESP32S3 Dev Module).
2. Mở thư mục này. File chính là `robotdog.ino`.
3. Board: **ESP32S3 Dev Module**, USB CDC On Boot, Flash 16 MB, PSRAM OPI, partition `16M (3MB APP / 9.9MB FATFS)`.
4. Chọn cổng COM của board rồi Upload.
5. Serial Monitor 115200 sẽ in `FW gait v…`.

Trong `config.h`, điền hotspot điện thoại trước khi nạp nếu muốn chó tự nối mạng:

```cpp
#define HOTSPOT_SSID     "ten-hotspot"
#define HOTSPOT_PASS     "mat-khau"
```

### Lệnh Serial thường dùng

| Lệnh | Tác dụng |
|---|---|
| `stand` `walk` `run` `sit4` `sit2` `stop` | Đứng, đi, chạy, ngồi bốn chân, ngồi hai chân sau, dừng |
| `shakel` `shaker` `shake` | Bắt tay trái, phải, hoặc chọn ngẫu nhiên |
| `move <kênh> <góc>` | Quay một servo, không lưu |
| `set <kênh> <góc>` | Lưu góc đứng của một servo |
| `signs` `show` `status` | Xem chiều đi, góc đứng, trạng thái |
| `radar mid` / `radar go` | Giữ đầu ở giữa để lắp lại càng, rồi quét tiếp |
| `relax` | Nhả hết servo |

## Chạy app Android

1. Mở thư mục `android/` bằng Android Studio.
2. Copy `android/app/src/main/java/com/example/robotdog/ApiKeys.example.java` thành `ApiKeys.java` cùng thư mục, rồi điền khóa của mình:
   - **Gemini** (Google AI Studio): hội thoại và giọng nói
   - **Groq**: nhận giọng nói (Whisper) và hội thoại dự phòng
   - **OpenRouter**: hội thoại dự phòng
3. Cắm điện thoại, bật USB debugging, bấm Run. Hoặc `gradlew assembleDebug` rồi cài APK.

App cần Android 7 trở lên. Thu tiếng Spotify / YouTube ra loa chó cần Android 10.

### Cách dùng app

1. Bật hotspot 2.4 GHz trên điện thoại (tên và mật khẩu trùng `config.h`).
2. Mở app, đợi dòng trạng thái báo đã liên kết.
3. Bấm các nút đi, chạy, ngồi, hoặc nói vào micro: "xin chào", "đi nào", "bắt tay", "bật nhạc …".
4. Nút dừng cắt ngay câu đang nói hoặc bài nhạc.

## Sơ đồ lắp ráp

### Nguồn

```
[18650]─tiếp─[18650] ≈ 7.4 V
        │
     XL4015 OUT = 5.0 V
        │
   ┌────┼───────────────┐
PCA9685 V+   ESP32 5V   MAX98357A VIN   HC-SR04 VCC
   GND ──── GND ──── GND ──── GND
```

3.3 V lấy từ ESP32 cấp cho PCA9685 VCC, MPU6050 và INMP441. Echo của HC-SR04 phải qua bộ chia áp trước khi vào GPIO, vì ESP32 không chịu 5 V trên chân.

### I2C

| Tín hiệu | ESP32-S3 | Module |
|---|---|---|
| SDA | GPIO 8 | PCA9685, MPU6050 |
| SCL | GPIO 9 | PCA9685, MPU6050 |

PCA9685 địa chỉ `0x40`, MPU6050 địa chỉ `0x68`.

Ảnh và video lắp từng dây nằm trong [`huong-dan/`](huong-dan).
