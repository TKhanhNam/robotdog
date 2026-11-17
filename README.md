# Robot Dog

A four-legged robot that holds a spoken conversation. The user talks into an Android phone; the dog answers through a speaker on its body and, when asked, walks, runs, sits, or offers a paw. It can also play a requested song through that same speaker.

This is a student project. The robot does not run the language model itself. A small ESP32-S3 drives the motors and the speaker, and the phone does the listening, the reasoning, and the network search for music.

**Author:** TKhanh Nam · [github.com/TKhanhNam/robotdog](https://github.com/TKhanhNam/robotdog)

| Part | Where | Language |
|---|---|---|
| Firmware | repository root (`robotdog.ino`, `config.h`, `*.h`) | C++ / Arduino |
| Phone app | [`android/`](android) | Java |

## What the project shows

The interesting part is not a single algorithm. It is making a cheap microcontroller, eight hobby servos, a phone, and three cloud APIs behave as one animal that a person can talk to.

Three constraints shaped the design:

1. **The ESP32 cannot run a useful language model**, and it has almost no memory for audio. Speech recognition, conversation, and music search therefore run on the phone and in the cloud. The robot only receives motion commands and a stream of 16 kHz PCM.
2. **The robot's own Wi-Fi has no internet.** Joining that network used to cut the phone off from the AI services. The phone now shares a 2.4 GHz hotspot. The dog keeps its own access point for direct control and also joins the hotspot, then announces its address with a UDP beacon.
3. **The mechanics are imperfect.** Several joints have a shorter useful range than the datasheet suggests, and one servo later failed mechanically. The gait and the handshake had to be written around the angles the hardware can actually reach, and the firmware stores per-joint corrections in flash so a joint can be tuned without reflashing.

## What it can do

- Walk, run, stand, stop, sit on all fours, and sit on the hind legs, from on-screen buttons or from speech.
- Offer the left paw, the right paw, or a paw chosen at random. "Shake" with no side is enough. Naming a side still selects that side. The dog stays standing and returns to standing afterwards.
- Answer spoken questions out loud, in Vietnamese, through the dog's speaker.
- Play a named song (30-second previews from iTunes, then Deezer) or a live internet-radio stream. A second path forwards whatever the phone is playing, including Spotify or YouTube, to the dog's speaker.
- A stop button cuts speech or music immediately.
- Sweep the head, which carries an ultrasonic sensor, from side to side.

## How a sentence becomes motion and sound

```
Phone microphone
    │  record, trim silence
    ▼
Groq Whisper  ── text ──►  Gemini, then Groq, then OpenRouter
                                  │
                    ┌─────────────┴──────────────┐
                    ▼                            ▼
            spoken reply (TTS)            action, if the sentence
            16 kHz PCM                    is a command: walk, sit, shake…
                    │                            │
                    └──────── Wi-Fi ─────────────┘
                                  │
                                  ▼
                           ESP32-S3 on the dog
                           ├─ PCA9685 → 8 leg servos + head servo
                           ├─ MAX98357A speaker
                           ├─ INMP441 microphone (on-device keywords)
                           ├─ HC-SR04 on the head
                           └─ MPU6050
```

The app tries Gemini first and falls back to Groq and OpenRouter, so one provider being down or out of free quota does not end the conversation. Motion commands such as "shake" are also matched on the phone before the cloud call, so a handshake still works with no network.

## Problems worth knowing about

These are the faults that took the longest, and the fixes are in the code.

**The speech recognizer invented a YouTube channel.** Whisper, asked to transcribe Vietnamese, would append "subscribe to Ghiền Mì Gõ" when the recording contained silence. The dog then talked about a channel nobody had mentioned. The app now cuts leading and trailing silence, drops segments Whisper itself marks as non-speech, and strips a list of known video-outro phrases. Temperature is set to 0.

**Music stopped after a few seconds.** The files were a full 30 seconds. The phone was writing decoded audio faster than real time, filling the TCP buffer, and closing the socket. Writes are now paced to stay about 1.5 seconds ahead of playback, and the socket is drained before it is closed.

**The dog obeyed its own speaker.** The on-board microphone heard the reply and treated it as a new command. The firmware ignores keyword hits while the speaker has received audio in the last 1.5 seconds.

**The left paw cannot swing forward the way the right paw can.** The front-left thigh servo binds below about 85°. A left handshake therefore lifts the paw by folding the calf, and shakes with the calf. The right handshake swings the thigh.

**One calf servo failed, and it looked like a software bug.** `walk` left the rear-left calf still, while a large `move` on the same channel worked. The same angles worked on the opposite calf. With the signal removed, the shaft could not be turned by hand. That pattern is a stripped or jammed gear, not a gait error. The README records it so the behaviour is not mistaken for a firmware fault.

## Limitations

- Song requests play a 30-second preview, not the full track, unless the phone forwards audio from another app.
- Cloud speech and chat depend on third-party free tiers. When those are exhausted the app says so and falls back to a local Vietnamese voice, which requires Google's speech engine and a downloaded Vietnamese voice.
- Balance while shaking on three legs is limited. A large paw raise can tip the body.
- The head only sweeps evenly after the servo horn is refitted at the servo's midpoint (`radar mid`). The pulse range alone cannot correct a horn that was installed off centre.
- Obstacle stopping uses a single ultrasonic sensor, so it knows that something is ahead, not what it is.

## Repository layout

| Path | Contents |
|---|---|
| `robotdog.ino` | Setup, serial commands, Wi-Fi, audio streaming, main loop |
| `GaitController.h` | Stand, walk, run, sit, handshake |
| `config.h` | Pins, servo angles, gait tuning, hotspot name |
| `ServoDriver.h` | PCA9685, per-channel direction stored in flash |
| `WifiControl.h` | Access point, phone hotspot, UDP beacon, command page |
| `android/` | The phone app |
| `huong-dan/` | Wiring photos and short videos (captions in Vietnamese) |

API keys are not in the repository. `android/app/src/main/java/com/example/robotdog/ApiKeys.example.java` shows the four fields. Copy it to `ApiKeys.java` (that name is gitignored) and fill in your own Gemini, Groq, and OpenRouter keys.

## Hardware

- ESP32-S3, USB CDC, 16 MB flash, OPI PSRAM
- PCA9685 at I2C `0x40`, driving eight MG90S servos and the head
- MPU6050 at I2C `0x68`, INMP441 microphone, MAX98357A speaker, HC-SR04
- Power: two 18650 cells in series, stepped down to 5.0 V with an XL4015. The ESP32 and the servos must not be fed 7.4 V. The HC-SR04 echo pin is 5 V and goes through a divider before the ESP32.

| PCA9685 channel | Joint |
|---|---|
| 0, 1 | Rear-left calf, rear-left thigh |
| 2, 3 | Rear-right calf, rear-right thigh |
| 4, 5 | Front-left calf, front-left thigh |
| 6, 7 | Front-right calf, front-right thigh |
| 8 | Head, carrying the ultrasonic sensor |

| Signal | ESP32-S3 |
|---|---|
| I2C SDA, SCL | GPIO 8, GPIO 9 |
| HC-SR04 trigger, echo | GPIO 18, GPIO 10 |
| Speaker BCLK, LRC, DIN | GPIO 4, GPIO 5, GPIO 6 |
| Microphone BCLK, WS, SD | GPIO 15, GPIO 16, GPIO 17 |

## Build and run

**Firmware.** Install the Arduino IDE and the Espressif ESP32 boards. Open this folder, board **ESP32S3 Dev Module**, USB CDC On Boot, 16 MB flash, OPI PSRAM, partition scheme `16M (3MB APP / 9.9MB FATFS)`. Upload, then open the Serial Monitor at 115200. It prints `FW gait v…`.

Before uploading, set the phone hotspot in `config.h` if the dog should find the phone by itself:

```cpp
#define HOTSPOT_SSID     "hotspot-name"
#define HOTSPOT_PASS     "password"
```

**App.** Open `android/` in Android Studio (Android 7 or newer; forwarding phone audio needs Android 10). Add `ApiKeys.java` as described above, enable USB debugging, and run. Turn on the phone's 2.4 GHz hotspot with the same name and password, open the app, and wait for the status line to report a link. Then use the buttons, or speak: "xin chào", "đi nào", "bắt tay", "bắt tay trái", "bật nhạc …". The stop button cuts speech and music at once.

**Useful serial commands**

| Command | Effect |
|---|---|
| `stand` `walk` `run` `sit4` `sit2` `stop` | Stand, walk, run, sit on four legs, sit on the hind legs, stop |
| `shake` `shakel` `shaker` | Random paw, left paw, right paw |
| `move <ch> <deg>` | Move one servo, do not save |
| `set <ch> <deg>` | Save one joint's standing angle |
| `signs` `show` `status` | Gait signs, standing angles, status |
| `radar mid` / `radar go` | Hold the head centred, then resume the sweep |
| `relax` | Release every servo |
