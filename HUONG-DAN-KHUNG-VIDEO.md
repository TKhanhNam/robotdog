# Hướng dẫn lắp theo khung trong video 7570.mp4

Video tham khảo cho thấy khung **chó robot in 3D màu trắng, 8 khớp (2 servo/chân)**:
- Thân chữ nhật rỗng, mặt trên mở để đặt board
- Mỗi chân: **servo hông** (gắn góc thân, quay trước–sau) + **servo gối** (giữa đùi–ống)
- Chân nhọn chạm đất; tư thế hơi dang ra hai bên

Bạn **chưa lắp xong** — dưới đây là thứ tự lắp đúng khung này với linh kiện bạn có.

## Thứ tự lắp (bắt buộc làm lần lượt)

### Bước A — Ráp khung nhựa (chưa gắn điện)

1. Lắp 2 tấm bên thân + tấm đáy / thanh ngang theo kit in 3D.
2. Xác định **đầu** (tấm trước) và **đuôi**.
3. Đánh dấu 4 góc: FL · FR · BL · BR (nhìn từ trên, đầu phía trước).

### Bước B — Gắn 8× MG90S vào khung

Với **mỗi chân**:

1. **Servo hông**: bắt ốc vào ổ ở **góc thân**. Trục servo nằm ngang, cánh tay / bánh răng gắn với đùi.
2. **Servo gối**: bắt vào đoạn đùi; cánh tay gắn ống chân.
3. **Trước khi siết chặt cánh tay (horn):**
   - Cấp nguồn tạm / nạp code → lệnh `stand`
   - Hoặc xoay tay tới vị trí giữa (~90°)
   - Rồi mới bắt horn sao cho khi đứng: thân ngang, gối hơi gập

| Chân | Hông → PCA9685 | Gối → PCA9685 |
|------|----------------|---------------|
| Trước trái FL | CH0 | CH1 |
| Trước phải FR | CH2 | CH3 |
| Sau trái BL | CH4 | CH5 |
| Sau phải BR | CH6 | CH7 |

Dây servo: nâu/đen=GND, đỏ=V+, cam/vàng=PWM. Luồn dây **vào trong thân**, kéo lên mặt trên chỗ PCA9685 (tránh kẹt khớp).

### Bước C — Nguồn trong lòng thân

Nhét vào **khoang giữa thân** (trọng tâm thấp như trong video):

1. Đế 2×18650 nối tiếp  
2. XL4015 — chỉnh **5.00V** bằng đồng hồ **trước** khi cắm board  
3. Tụ ~1000µF (nên có) sát V+/GND PCA9685  

```
18650 series → XL4015 → 5V
  → PCA9685 V+ (servo)
  → ESP32 5V/VIN
  → MAX98357A VIN
  → SRF04 VCC
GND chung tất cả
3V3 ESP32 → PCA9685 VCC, MPU6050, INMP441
```

### Bước D — Board trên mặt thân (như video)

Video để board **trên nóc**. Bạn lắp:

1. **PCA9685** cố định giữa/nóc — cắm 8 dây servo CH0–CH7  
2. **ESP32-S3** cạnh hoặc chồng phía trên PCA9685 (USB ra ngoài để nạp)  
3. Nối I2C: SDA=GPIO8, SCL=GPIO9  

### Bước E — Cảm biến & âm thanh trên khung này

| Linh kiện | Chỗ gắn trên khung trắng |
|-----------|--------------------------|
| MPU6050 | Giữa thân, phẳng; trục X về phía đầu |
| SRF04 | Tấm trước (mũi/mắt), hướng đi tới |
| INMP441 | Gần tấm trước / cổ, hướng ra ngoài |
| MAX98357A + loa 3W | Dưới bụng hoặc gần đuôi; loa không hướng thẳng vào mic |

ECHO SRF04 → chia áp 1kΩ + 2kΩ vào GPIO11.

### Bước F — Gọn dây (quan trọng với khung này)

Video mẫu bị rối dây trên nóc. Bạn nên:

1. Buộc dây từng chân bằng zip / chỉ  
2. Chừa vòng dư gần khớp (để chân quay không giật dây)  
3. Cố định bó dây vào thân, không để chạm đất  

### Bước G — Nạp code & chỉnh đứng

1. USB A–C → nạp `robotdog.ino`  
2. Serial 115200: `stand` → chỉnh `NEUTRAL` / `SERVO_DIR` / `SERVO_OFFSET` trong `config.h`  
3. `sit` → `stand` → `walk` → (sau) `run`  
4. Nghiêng thân thử `getup`; tay trước SRF04 khi walk phải `stop`  

## Ảnh đã tạo từ video của bạn

- `huong-dan/khung-cua-ban-vi-tri.png` — vị trí từng linh kiện trên khung  
- `huong-dan/khung-cua-ban-chi-tiet-chan.png` — chi tiết hông/gối  
- `khung_video/frame_01.jpg` … `frame_11.jpg` — khung hình trích từ `7570.mp4`  

## Lưu ý khớp code

Khung video = **8DOF, không có khớp dang ngang** → đúng với code trot hiện tại (hip + knee).  
Nếu sau khi `stand` chân trái/phải đảo chiều: đổi dấu `SERVO_DIR` của chân đó trong `config.h`.
