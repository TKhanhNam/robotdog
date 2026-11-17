# Robot Dog — a voice-controlled quadruped on ESP32-S3

A four-legged robot controlled from an Android phone. The user speaks into the phone microphone; the dog answers through its own speaker and carries out commands such as walk, run, sit, and shake a paw. The app also finds music and plays it through the dog's speaker.

This repository has two parts:

| Path | Role |
|---|---|
| Repository root (`robotdog.ino`, `config.h`, `*.h`) | Arduino firmware for the ESP32-S3 |
| [`android/`](android) | Android app (Java) |

## How it works

```
Phone (app)                              Dog (ESP32-S3)
  Mic ── recording ──► Groq Whisper           │
  Text ── chat ──► Gemini / Groq / OpenRouter │
  Reply ── speech (TTS) ──┐                   │
  Walk / sit / shake ─────┼─ Wi-Fi ──► PCA9685 ── 8× MG90S servos
  Music (iTunes, Deezer, ─┘                   │
         internet radio)                  MAX98357A speaker
                                          INMP441 microphone
                                          HC-SR04 ultrasonic sensor (head sweep)
                                          MPU6050
```

The phone shares a 2.4 GHz hotspot. The dog runs its own access point and also joins that hotspot, then broadcasts a UDP beacon so the app can find its address. The phone therefore keeps its internet connection and can call the AI services.

## Hardware

- ESP32-S3 (USB CDC, 16 MB flash, OPI PSRAM)
- PCA9685 driving 8 MG90S servos for the legs, plus channel 8 sweeping the head that carries the HC-SR04
- MPU6050, INMP441, MAX98357A, HC-SR04
- Power: two 18650 cells in series, stepped down to 5 V by an XL4015. Never feed 7.4 V straight into the ESP32 or the servos.

Step-by-step wiring photos and videos are in [`huong-dan/`](huong-dan).

### Servo channels

| Channel | Joint |
|---|---|
| 0, 1 | Rear-left calf, rear-left thigh |
| 2, 3 | Rear-right calf, rear-right thigh |
| 4, 5 | Front-left calf, front-left thigh |
| 6, 7 | Front-right calf, front-right thigh |
| 8 | Head (ultrasonic sensor) |

## Flashing the firmware

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and the **esp32 by Espressif** boards package (board: ESP32S3 Dev Module).
2. Open this folder. The main sketch is `robotdog.ino`.
3. Board settings: **ESP32S3 Dev Module**, USB CDC On Boot, 16 MB flash, OPI PSRAM, partition scheme `16M (3MB APP / 9.9MB FATFS)`.
4. Select the board's COM port and upload.
5. The Serial Monitor at 115200 baud prints `FW gait v…`.

To let the dog join the phone's network, fill in the hotspot in `config.h` before uploading:

```cpp
#define HOTSPOT_SSID     "hotspot-name"
#define HOTSPOT_PASS     "password"
```

### Useful serial commands

| Command | Effect |
|---|---|
| `stand` `walk` `run` `sit4` `sit2` `stop` | Stand, walk, run, sit on all fours, sit on the hind legs, stop |
| `shakel` `shaker` `shake` | Offer the left paw, the right paw, or a random paw |
| `move <channel> <deg>` | Move one servo without saving |
| `set <channel> <deg>` | Save the standing angle of one servo |
| `signs` `show` `status` | Show gait directions, standing angles, status |
| `radar mid` / `radar go` | Hold the head centered to refit the horn, then resume the sweep |
| `relax` | Release all servos |

## Running the Android app

1. Open the `android/` folder in Android Studio.
2. Copy `android/app/src/main/java/com/example/robotdog/ApiKeys.example.java` to `ApiKeys.java` in the same folder and fill in your own keys:
   - **Gemini** (Google AI Studio): conversation and speech
   - **Groq**: speech recognition (Whisper) and a fallback for conversation
   - **OpenRouter**: a fallback for conversation
3. Connect the phone with USB debugging enabled and press Run, or run `gradlew assembleDebug` and install the APK.

The app requires Android 7 or newer. Forwarding Spotify or YouTube audio to the dog's speaker requires Android 10.

### Using the app

1. Turn on the phone's 2.4 GHz hotspot, using the same name and password as in `config.h`.
2. Open the app and wait until the status line reports a link.
3. Tap the walk, run, and sit buttons, or speak into the microphone: "xin chào", "đi nào", "bắt tay", "bật nhạc …".
4. The stop button cuts the current reply or the music immediately.

## Wiring

### Power

```
[18650]─series─[18650] ≈ 7.4 V
        │
     XL4015 OUT = 5.0 V
        │
   ┌────┼───────────────┐
PCA9685 V+   ESP32 5V   MAX98357A VIN   HC-SR04 VCC
   GND ──── GND ──── GND ──── GND
```

The ESP32's 3.3 V pin supplies the PCA9685 VCC, the MPU6050, and the INMP441. The HC-SR04 echo line must pass through a voltage divider before it reaches a GPIO, because the ESP32 pins are not 5 V tolerant.

### I2C

| Signal | ESP32-S3 | Modules |
|---|---|---|
| SDA | GPIO 8 | PCA9685, MPU6050 |
| SCL | GPIO 9 | PCA9685, MPU6050 |

The PCA9685 is at address `0x40` and the MPU6050 at `0x68`.

Photos and videos of each wire are in [`huong-dan/`](huong-dan).
