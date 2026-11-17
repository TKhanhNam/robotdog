/*
 * Robot dog — STAND = dáng bạn chỉnh (home @ 90° / set từng kênh)
 * Hành động khác = offset từ STAND.
 *
 * Serial COM4 115200:
 *   relax   — nhả servo, chỉnh tay / vặn horn
 *   stand   — khóa dáng stand (home)
 *   walk | run | sit4 | sit2 | stop
 *   show | set <ch> <goc>
 */

#include <Wire.h>
#include <stdarg.h>
#include <stdio.h>
#include "esp_rom_sys.h"
#include "config.h"
#include "ServoDriver.h"
#include "GaitController.h"
#include "IMUSensor.h"
#include "Ultrasonic.h"
#include "AudioIO.h"
#include "VoiceHello.h"
#include "WifiControl.h"
#include "Telemetry.h"

DogTel dogTel;

ServoDriver servos;
GaitController gait;
IMUSensor imu;
Ultrasonic sonar;
AudioIO audio;
VoiceHello hello;
WifiControl wifi;
bool pcaOk = false;
bool imuOk = false;
bool audioOk = false;
bool fallHandled = false;

static void handleLine(char* line);

static void say(const char* msg) {
  // USB CDC: khong flush — rut cap khong duoc phep treo loop/WiFi
  if (Serial) Serial.println(msg);
  Serial0.println(msg);
}

static void sayf(const char* fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  say(buf);
}

static void led(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
  neopixelWrite(RGB_BUILTIN, r, g, b);
#else
  neopixelWrite(48, r, g, b);
#endif
}

static void printStand() {
  say("STAND home ch0..7:");
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    sayf("  ch%d = %d", i, (int)gait.standDeg(i));
  }
}

static void printSigns() {
  static const char* names[SERVO_COUNT] = {
    "cang trai sau", "dui trai sau", "cang phai sau", "dui phai sau",
    "cang trai truoc", "dui trai truoc", "cang phai truoc", "dui phai truoc"
  };
  say("Chieu 've truoc' khi di (flip <ch> de dao):");
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    sayf("  ch%d %-16s %+d", i, names[i], (int)gait.gaitSign(i));
  }
}

static uint16_t radarRightUs = RADAR_RIGHT_US;
static uint16_t radarLeftUs = RADAR_LEFT_US;
static bool radarHold = false;

static void loadRadar() {
  Preferences p;
  if (!p.begin("dog", true)) return;
  radarRightUs = p.getUShort("radR", RADAR_RIGHT_US);
  radarLeftUs = p.getUShort("radL", RADAR_LEFT_US);
  p.end();
}

static void saveRadar() {
  Preferences p;
  if (!p.begin("dog", false)) return;
  p.putUShort("radR", radarRightUs);
  p.putUShort("radL", radarLeftUs);
  p.end();
}

static void tryPca() {
  if (pcaOk) return;
  Wire.beginTransmission(PCA9685_ADDR);
  if (Wire.endTransmission() != 0) return;
  pcaOk = true;
  servos.begin();
  gait.begin(&servos);
  loadRadar();
  servos.setExtraUs(RADAR_CH, (radarRightUs + radarLeftUs) / 2);
  say("PCA9685 0x40: OK");
}

static void pcmBegin();
static void pcmHangup();

void setup() {
  setCpuFrequencyMhz(160);
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(400);
  led(0, 40, 0);
  say("");
  say("FW gait v43 — dau quet deu trai/phai (lenh radar)");

  wifi.begin(handleLine);
  pcmBegin();
  sayf("[WiFi] AP %s  %s", wifi.ssid(), wifi.ok() ? "OK" : "THAT BAI");
  sayf("[WiFi] Mat khau: %s", WIFI_AP_PASS);
  sayf("[WiFi] Trinh duyet: http://%s", wifi.apIp().toString().c_str());
  say("Jumper PCA V+ -> ESP32 5V, GND chung. Rut USB van giu WiFi.");
  delay(400);

  say("Map: 0=cangTS 1=duiTS 2=cangPS 3=duiPS 4=cangTT 5=duiTT 6=cangPT 7=duiPT 8=radar");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  tryPca();
  if (!pcaOk) say("PCA THAT BAI — chua co 5V (jumper PCA V+ vao ESP32 5V)");
  else {
    printStand();
    say("Radar ch8: quet cham trai-phai");
  }

  imuOk = imu.begin(&Wire);
  if (imuOk) {
    say("[IMU] OK tren I2C GPIO8/9 (chung PCA)");
  } else {
    Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL);
    Wire1.setClock(100000);
    delay(30);
    imuOk = imu.begin(&Wire1);
    say(imuOk ? "[IMU] OK tren I2C GPIO11/12" : "[IMU] KHONG THAY MPU6050");
  }

  sonar.begin();
  say("[SR04] TRIG=GPIO18 ECHO=GPIO10");

  audioOk = audio.begin();
  if (audioOk && hello.begin(&audio, say)) hello.printStatus();

  say("Lenh: stand walk run sit4 sit2 shakel shaker stop getup beep status");
  say("      relax | show | set <ch> <goc> | flipknee <ch>");
  say("      test <ch> | flip <ch> | signs | resetsigns");
  say("Test cam bien: selftest | scan | imu | sonar | mic | spk | audiotest | findmpu | sonardiag");
  say("Giong noi: train | voice | voice on/off | voice thr <he so> | forget | reply");
}

static bool wire1Started = false;

static void startWire1() {
  if (wire1Started) return;
  Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL);
  Wire1.setClock(100000);
  wire1Started = true;
  delay(30);
}

static int i2cScan(TwoWire& bus, const char* name) {
  char buf[128];
  int len = snprintf(buf, sizeof(buf), "[I2C %s]", name);
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() == 0) {
      found++;
      if (len < (int)sizeof(buf) - 6) len += snprintf(buf + len, sizeof(buf) - len, " 0x%02X", a);
    }
  }
  if (!found) snprintf(buf + len, sizeof(buf) - len, " khong thay thiet bi nao");
  say(buf);
  return found;
}

static bool ensureImu() {
  if (imuOk) return true;
  imuOk = imu.begin(&Wire);
  if (!imuOk) {
    startWire1();
    imuOk = imu.begin(&Wire1);
  }
  return imuOk;
}

static bool testImu() {
  say("--- MPU6050 (con quay + gia toc) ---");
  if (!ensureImu()) {
    say("[MPU6050] LOI: khong thay dia chi 0x68 tren GPIO8/9 lan GPIO11/12");
    say("  Kiem tra: VCC->3V3, GND, SDA->GPIO8, SCL->GPIO9, AD0->GND (hoac de trong)");
    return false;
  }
  float ax, ay, az, gx, gy, gz, t;
  float sumMag = 0, gyroMax = 0;
  for (int i = 0; i < 10; i++) {
    imu.readRaw(ax, ay, az, gx, gy, gz, t);
    sumMag += sqrtf(ax * ax + ay * ay + az * az);
    const float gm = fmaxf(fabsf(gx), fmaxf(fabsf(gy), fabsf(gz))) * 57.29578f;
    if (gm > gyroMax) gyroMax = gm;
    if (i % 3 == 0) {
      sayf("  gia toc %.2f %.2f %.2f m/s2 | quay %.1f %.1f %.1f do/s | %.1f C",
           ax, ay, az, gx * 57.29578f, gy * 57.29578f, gz * 57.29578f, t);
    }
    delay(100);
  }
  for (int i = 0; i < 60; i++) { imu.update(); delay(20); }
  const float mag = sumMag / 10.0f;
  sayf("  |g| = %.2f m/s2 (chuan ~9.8) | pitch=%.1f roll=%.1f | quay max %.1f do/s",
       mag, imu.pitch(), imu.roll(), gyroMax);
  const bool ok = mag > 7.0f && mag < 12.5f;
  if (ok) {
    say("[MPU6050] DAT");
    say("  Lenh 'imu' de xem pitch/roll lien tuc khi nghieng robot");
  } else {
    say("[MPU6050] LOI: gia toc khong hop ly — module hong hoac loi bus I2C");
  }
  return ok;
}

static void imuWatch() {
  if (!ensureImu()) { say("[MPU6050] khong thay module"); return; }
  say("[IMU] 6 giay: nghieng robot truoc/sau (pitch) va trai/phai (roll)");
  const uint32_t end = millis() + 6000;
  uint32_t lastPrint = 0;
  while (millis() < end) {
    imu.update();
    if (millis() - lastPrint >= 300) {
      lastPrint = millis();
      sayf("  pitch=%6.1f  roll=%6.1f  nga=%s", imu.pitch(), imu.roll(), imu.isFallen() ? "CO" : "khong");
    }
    delay(20);
  }
}

// Thử MPU6050 trên các cặp GPIO còn trống, phòng khi cắm nhầm chân SDA/SCL
static void findMpu() {
  say("[TIM MPU] thu cac cap chan GPIO, tim dia chi 0x68/0x69...");
  static const uint8_t pins[] = {1, 2, 7, 11, 12, 13, 14, 21, 38, 39, 40, 41, 42, 47};
  int hits = 0;

  Wire1.end();
  for (uint8_t sda : pins) {
    for (uint8_t scl : pins) {
      if (sda == scl) continue;
      Wire1.begin(sda, scl, 100000);
      Wire1.setTimeOut(10);
      for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
        Wire1.beginTransmission(addr);
        if (Wire1.endTransmission() == 0) {
          sayf("  THAY 0x%02X: SDA=GPIO%d SCL=GPIO%d", addr, sda, scl);
          hits++;
        }
      }
      Wire1.end();
    }
  }
  wire1Started = false;

  // SDA/SCL của MPU bị đảo trên bus PCA 8/9
  Wire.end();
  Wire.begin(PIN_I2C_SCL, PIN_I2C_SDA, 100000);
  Wire.setTimeOut(10);
  for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      sayf("  THAY 0x%02X khi DAO day: SDA=GPIO%d SCL=GPIO%d", addr, PIN_I2C_SCL, PIN_I2C_SDA);
      hits++;
    }
  }
  Wire.end();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

  if (!hits) {
    say("  Khong thay MPU6050 o cap chan nao.");
    say("  => Module chua co dien hoac day dut: do 3V3 giua VCC-GND cua MPU,");
    say("     kiem tra han chan header, thu module MPU khac neu co.");
  }
}

static void sonarDiag() {
  say("[SR04 CHAN DOAN]");
  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  delay(5);
  int highCount = 0;
  for (int i = 0; i < 100; i++) {
    highCount += digitalRead(PIN_US_ECHO);
    delay(5);
  }
  const int idle = highCount > 50 ? 1 : 0;
  sayf("  Echo GPIO%d luc nghi: muc cao %d/100 lan (dung phai ~0)", PIN_US_ECHO, highCount);

  // Kéo xuống yếu bên trong: chân hở sẽ về 0, chân bị nguồn kéo lên vẫn là 1
  pinMode(PIN_US_ECHO, INPUT_PULLDOWN);
  delay(5);
  int pdHigh = 0;
  for (int i = 0; i < 20; i++) {
    pdHigh += digitalRead(PIN_US_ECHO);
    delay(2);
  }
  pinMode(PIN_US_ECHO, INPUT);
  sayf("  Bat keo xuong: muc cao %d/20 lan", pdHigh);
  if (idle && pdHigh == 0) {
    say("  => Day Echo dang HO (khong noi toi cam bien) hoac thieu dien tro 2k xuong GND");
  } else if (idle && pdHigh > 0) {
    say("  => Echo bi KEO LEN NGUON lien tuc: day Echo cam nham vao 5V/3V3,");
    say("     hoac day VCC va Echo bi doi cho nhau tren cam bien");
  }

  int okNormal = 0;
  for (int i = 0; i < 5; i++) {
    if (sonar.readCm() > 0) okNormal++;
    delay(60);
  }
  sayf("  Trig=GPIO%d Echo=GPIO%d: %d/5 lan co echo", PIN_US_TRIG, PIN_US_ECHO, okNormal);

  // Thử đảo Trig/Echo
  int okSwap = 0;
  pinMode(PIN_US_ECHO, OUTPUT);
  pinMode(PIN_US_TRIG, INPUT);
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_US_ECHO, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_US_ECHO, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_US_ECHO, LOW);
    if (pulseIn(PIN_US_TRIG, HIGH, 25000UL) > 0) okSwap++;
    delay(60);
  }
  sayf("  Dao lai Trig=GPIO%d Echo=GPIO%d: %d/5 lan co echo", PIN_US_ECHO, PIN_US_TRIG, okSwap);
  sonar.begin();

  if (okNormal == 0 && okSwap > 0) {
    say("  => Day Trig va Echo dang bi DAO: doi cho 2 day nau/do (hoac bao de doi trong code)");
  } else if (okNormal == 0 && okSwap == 0 && idle == 0) {
    say("  => Cam bien khong phan hoi: kiem tra VCC phai la 5V, GND chung, day Trig toi GPIO18");
  }
}

static bool testSonar() {
  say("--- HC-SR04 (sieu am) ---");
  char buf[128];
  int len = snprintf(buf, sizeof(buf), "  cm:");
  int valid = 0;
  float mn = 9999, mx = 0;
  for (int i = 0; i < 8; i++) {
    const float d = sonar.readCm();
    if (d > 0) {
      valid++;
      mn = fminf(mn, d);
      mx = fmaxf(mx, d);
      len += snprintf(buf + len, sizeof(buf) - len, " %.0f", d);
    } else {
      len += snprintf(buf + len, sizeof(buf) - len, " --");
    }
    delay(70);
  }
  say(buf);
  if (valid >= 5) {
    sayf("[HC-SR04] DAT (%.0f..%.0f cm)", mn, mx);
    say("  Lenh 'sonar' roi dua tay lai gan: so cm phai giam");
    return true;
  }
  if (valid > 0) {
    say("[HC-SR04] CHAP CHON: luc doc duoc luc khong");
    say("  Kiem tra day Echo + GND; vat can trong khoang 3 cm..4 m");
    return false;
  }
  say("[HC-SR04] LOI: khong nhan duoc xung Echo");
  say("  Kiem tra: VCC->5V (3V3 khong du), GND chung, Trig->GPIO18, Echo->chia ap->GPIO10");
  return false;
}

static void sonarWatch() {
  say("[SR04] 6 giay: dua tay lai gan / ra xa cam bien");
  for (int i = 0; i < 24; i++) {
    const float d = sonar.readCm();
    if (d > 0) {
      sayf("  %5.1f cm%s", d, d < OBSTACLE_CM ? "  <- VAT CAN" : "");
    } else {
      say("     -- (khong co echo)");
    }
    delay(250);
  }
}

static void printMicStats(const AudioIO::MicStats& st) {
  sayf("  kenh=%s mau=%d khac0=%d min=%ld max=%ld muc=%lu",
       audio.micLeft() ? "trai" : "phai", st.samples, st.nonZero,
       (long)st.minV, (long)st.maxV, (unsigned long)st.avgAbs);
}

static bool testMic() {
  say("--- INMP441 (micro) ---");
  if (!audio.micOk()) audio.setMicSlot(audio.micLeft());
  if (!audio.micOk()) {
    say("[INMP441] LOI: khong khoi dong duoc I2S thu am");
    return false;
  }
  AudioIO::MicStats st = audio.micStats(250);
  if (st.samples > 0 && st.nonZero == 0) {
    say("  Kenh trai toan 0 — thu kenh phai (L/R noi 3V3?)");
    audio.setMicSlot(!audio.micLeft());
    st = audio.micStats(250);
    if (st.nonZero == 0) audio.setMicSlot(true);
  }
  printMicStats(st);
  if (st.samples == 0) {
    say("[INMP441] LOI: khong doc duoc mau nao (I2S het thoi gian cho)");
    return false;
  }
  if (st.nonZero == 0 || st.minV == st.maxV) {
    say("[INMP441] LOI: du lieu dung yen — mic khong gui am thanh");
    say("  Kiem tra: VDD->3V3, GND, SD->GPIO17, SCK->GPIO15, WS->GPIO16, L/R->GND");
    return false;
  }

  say("  Vo tay hoac noi to gan mic trong 3 giay...");
  const uint32_t base = st.avgAbs ? st.avgAbs : 1;
  uint32_t peak = 0;
  for (int i = 0; i < 15; i++) {
    const AudioIO::MicStats s = audio.micStats(200, false);
    if (s.avgAbs > peak) peak = s.avgAbs;
    int bars = (int)(s.avgAbs * 10 / (base * 4));
    if (bars > 30) bars = 30;
    char bar[32];
    memset(bar, '#', bars);
    bar[bars] = 0;
    sayf("  %7lu %s", (unsigned long)s.avgAbs, bar);
  }
  const float ratio = (float)peak / (float)base;
  sayf("  nen=%lu dinh=%lu (gap %.1f lan)", (unsigned long)base, (unsigned long)peak, ratio);
  say("[INMP441] DAT (co tin hieu am thanh)");
  if (ratio < 2.5f) say("  Chua thay tieng dong lon — thu lai lenh 'mic' va vo tay sat mic");
  return true;
}

static void micWatch() {
  if (!audio.micOk()) audio.setMicSlot(audio.micLeft());
  say("[MIC] 4 giay: noi hoac vo tay gan mic");
  for (int i = 0; i < 20; i++) {
    const AudioIO::MicStats s = audio.micStats(200, i == 0);
    int bars = (int)(s.avgAbs / 2000);
    if (bars > 30) bars = 30;
    char bar[32];
    memset(bar, '#', bars);
    bar[bars] = 0;
    sayf("  %7lu %s", (unsigned long)s.avgAbs, bar);
  }
}

static bool testSpeaker() {
  say("--- MAX98357A (loa) ---");
  if (!audioOk) {
    say("[MAX98357A] LOI: I2S phat am chua khoi dong");
    return false;
  }
  static const uint16_t notes[] = {523, 659, 784, 1047};
  bool ok = true;
  for (uint16_t hz : notes) {
    ok = audio.beep(hz, 300) && ok;
    delay(60);
  }
  if (ok) {
    say("[MAX98357A] Da phat 4 not do-mi-sol-do — NGHE loa de xac nhan");
    say("  Khong nghe: VIN->5V, GND, BCLK->GPIO4, LRC->GPIO5, DIN->GPIO6, 2 day loa + / -");
  } else {
    say("[MAX98357A] LOI: ghi I2S that bai");
  }
  return ok;
}

// Loa phát nốt, mic thu cùng lúc: mic bắt được đúng tần số thì cả hai đều chạy
// Kiểm mic không cần loa: động cơ đầu đứng yên rồi quay nhanh, mic phải nghe ồn hơn
static bool testMicByServo() {
  if (!pcaOk || !audio.micOk()) return false;
  say("--- Mic nghe dong co dau ---");
  servos.detachExtra(RADAR_CH);
  delay(300);
  uint64_t quiet = 0;
  for (int i = 0; i < 5; i++) quiet += audio.micStats(200, i == 0).avgAbs;
  quiet /= 5;

  uint64_t noisy = 0;
  for (int i = 0; i < 5; i++) {
    servos.setExtraUs(RADAR_CH, (i % 2) ? radarLeftUs : radarRightUs);
    noisy += audio.micStats(200, false).avgAbs;
  }
  noisy /= 5;
  servos.setExtraUs(RADAR_CH, (radarRightUs + radarLeftUs) / 2);

  const float ratio = (float)noisy / (float)(quiet ? quiet : 1);
  sayf("  dau dung yen=%lu  dau quay=%lu  (gap %.1f lan)",
       (unsigned long)quiet, (unsigned long)noisy, ratio);
  const bool ok = ratio >= 1.3f;
  say(ok ? "[MIC] DAT: mic nghe thay tieng dong co" : "[MIC] Chua thay khac biet ro");
  return ok;
}

static bool testLoopback() {
  say("--- Loa -> Mic (tu kiem) ---");
  if (!audioOk || !audio.micOk()) {
    say("[LOA+MIC] LOI: loa hoac mic chua khoi dong");
    return false;
  }
  static const uint16_t freqs[] = {1000, 1500};
  int pass = 0;
  for (uint16_t hz : freqs) {
    const float quiet = audio.toneLoopback(hz, 800, false);
    const float loud = audio.toneLoopback(hz, 800, true);
    const float db = 10.0f * log10f((loud + 1e-12f) / (quiet + 1e-12f));
    sayf("  %u Hz: im lang=%.2e  khi phat=%.2e  => %+.1f dB", hz, quiet, loud, db);
    if (db >= 10.0f) pass++;
  }
  if (pass == 2) {
    say("[LOA+MIC] DAT: mic thu duoc dung not loa phat ra");
  } else if (pass == 1) {
    say("[LOA+MIC] YEU: chi 1 not duoc mic bat — loa nho hoac dat xa mic");
  } else {
    say("[LOA+MIC] LOI: mic khong nghe thay loa");
    say("  Mic co tin hieu nen nhieu kha nang loa khong keu:");
    say("  VIN->5V, GND, BCLK->GPIO4, LRC->GPIO5, DIN->GPIO6, 2 day loa vao + / -");
  }
  return pass > 0;
}

static void selfTest() {
  say("");
  say("========== SELFTEST ==========");
  i2cScan(Wire, "GPIO8/9");
  say("  (0x40 = PCA9685, 0x68 = MPU6050, 0x70 = dia chi chung cua PCA9685)");
  const bool imuR = testImu();
  const bool sonR = testSonar();
  const bool micR = testMic();
  const bool spkR = testSpeaker();
  const bool loopR = testLoopback();
  say("========== KET QUA ==========");
  sayf("  PCA9685   (servo)    : %s", pcaOk ? "DAT" : "LOI");
  sayf("  MPU6050   (con quay) : %s", imuR ? "DAT" : "LOI");
  sayf("  HC-SR04   (sieu am)  : %s", sonR ? "DAT" : "LOI");
  sayf("  INMP441   (micro)    : %s", micR ? "DAT" : "LOI");
  sayf("  MAX98357A (loa)      : %s", !spkR ? "LOI" : (loopR ? "DAT (mic nghe thay)" : "DA PHAT nhung mic khong nghe thay"));
  say("==============================");
}

static bool applySpoken(char* line) {
  if (strstr(line, "ngoi 4") || strstr(line, "ngồi 4") || strstr(line, "bon chan") || strstr(line, "bốn chân")) {
    say("[GIONG] NGOI 4");
    if (pcaOk) gait.setState(DogState::Sit4);
    return true;
  }
  if (strstr(line, "ngoi 2") || strstr(line, "ngồi 2") || strstr(line, "hai chan") || strstr(line, "hai chân")) {
    say("[GIONG] NGOI 2");
    if (pcaOk) gait.setState(DogState::SitRear);
    return true;
  }
  if (strstr(line, "bat tay") || strstr(line, "bắt tay") || strstr(line, "shake")) {
    const bool sideR = strstr(line, "phai") || strstr(line, "phải");
    const bool left = strstr(line, "trai") || strstr(line, "trái") || (!sideR && random(2) == 0);
    say(left ? "[GIONG] BAT TAY TRAI" : "[GIONG] BAT TAY PHAI");
    if (pcaOk) gait.shake(left);
    return true;
  }
  if (strstr(line, "chay") || strstr(line, "chạy") || strstr(line, "run")) {
    say("[GIONG] CHAY");
    if (pcaOk) gait.setState(DogState::Run);
    return true;
  }
  if (strstr(line, "dung lai") || strstr(line, "dừng") || strstr(line, "stop")) {
    say("[GIONG] DUNG LAI");
    if (pcaOk) gait.setState(DogState::Stop);
    return true;
  }
  if (strstr(line, "day") || strstr(line, "dậy") || strstr(line, "getup")) {
    say("[GIONG] DAY");
    if (audioOk) audio.beep(660, 120);
    if (pcaOk) gait.setState(DogState::Stand);
    return true;
  }
  if (strstr(line, "dung") || strstr(line, "đứng") || strstr(line, "stand")) {
    say("[GIONG] DUNG");
    if (pcaOk) gait.setState(DogState::Stand);
    return true;
  }
  if (strstr(line, "tha") || strstr(line, "thả") || strstr(line, "relax")) {
    say("[GIONG] RELAX");
    if (pcaOk) gait.relax();
    return true;
  }
  if (strstr(line, "sua") || strstr(line, "sủa") || strstr(line, "bark")) {
    say("[GIONG] SUA");
    if (audioOk) audio.playReply(0);
    return true;
  }
  if (strstr(line, "di") || strstr(line, "đi") || strstr(line, "walk")) {
    say("[GIONG] DI");
    if (pcaOk) gait.setState(DogState::Walk);
    return true;
  }
  return false;
}

static void listenDogMic() {
  say("[MIC CHO] dang nghe 1.2 giay — noi sat mic tren cho");
  if (audioOk) audio.beep(1400, 80);
  audio.lockMic();
  const uint32_t e = audio.measureEnergy(1200);
  audio.unlockMic();
  dogTel.heard = (int)e;
  sayf("[MIC CHO] energy=%u", (unsigned)e);
  if (e > 1200) {
    say("[MIC CHO] co tieng -> sua");
    if (audioOk) audio.playReply(1);
  } else {
    say("[MIC CHO] khong nghe ro");
  }
}

static bool isSensorCmd(const char* line) {
  static const char* cmds[] = {
    "beep", "beep2", "mic", "status", "selftest", "kiemtra",
    "scan", "imu", "sonar", "spk", "loa", "findmpu", "sonardiag", "audiotest",
    "train", "forget", "reply", "wifi", "ear"
  };
  for (const char* c : cmds) {
    if (strcmp(line, c) == 0) return true;
  }
  return strncmp(line, "voice", 5) == 0 || strncmp(line, "reply ", 6) == 0 ||
         strncmp(line, "train ", 6) == 0 || strncmp(line, "forget ", 7) == 0;
}

// Giữ mic trong lúc chạy lệnh test, để task nghe "hello" không đọc chen vào
struct MicLock {
  MicLock() { audio.lockMic(); }
  ~MicLock() { audio.unlockMic(); }
};

static void handleLine(char* line) {
  for (char* p = line; *p; ++p) {
    if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
  }
  if (line[0] == 0) return;
  bool phrase = false;
  if (strncmp(line, "say ", 4) == 0) {
    line += 4;
    phrase = true;
  }
  for (const char* p = line; *p && !phrase; ++p) {
    if ((uint8_t)*p >= 128) phrase = true;
  }
  if (phrase && applySpoken(line)) return;

  if (!pcaOk && !isSensorCmd(line)) {
    say("PCA THAT BAI — chi dung: selftest scan imu sonar mic spk status");
    return;
  }

  if (strncmp(line, "radar", 5) == 0) {
    int r = 0, l = 0;
    if (strcmp(line, "radar mid") == 0) {
      radarHold = true;
      say("[RADAR] giu dau o giua (1500 us). Thao vit cang, xoay cam bien nhin thang roi lap lai. Xong go: radar go");
    } else if (strcmp(line, "radar go") == 0) {
      radarHold = false;
      sayf("[RADAR] quet lai %u..%u us", radarRightUs, radarLeftUs);
    } else if (sscanf(line + 5, "%d %d", &r, &l) == 2 && r >= 200 && l <= 2500 && r < l) {
      radarRightUs = (uint16_t)r;
      radarLeftUs = (uint16_t)l;
      radarHold = false;
      saveRadar();
      sayf("[RADAR] phai %d us, trai %d us (da luu)", r, l);
    } else {
      sayf("[RADAR] dang quet phai %u us .. trai %u us. Lenh: radar mid | radar go | radar <phai_us> <trai_us>",
           radarRightUs, radarLeftUs);
    }
  } else if (strcmp(line, "wifi") == 0) {
    sayf("[WiFi] AP %s  %s  http://%s", wifi.ssid(), wifi.ok() ? "OK" : "FAIL",
         wifi.apIp().toString().c_str());
    sayf("Mat khau: %s  (2.4 GHz)", WIFI_AP_PASS);
  } else if (strcmp(line, "selftest") == 0 || strcmp(line, "kiemtra") == 0) {
    MicLock lock;
    selfTest();
  } else if (strcmp(line, "train") == 0 || strncmp(line, "train ", 6) == 0) {
    const int w = line[5] == ' ' ? VoiceHello::wordIndex(line + 6) : 0;
    if (w < 0) {
      say("train <hello|stand|walk|run|stop|sit>");
      return;
    }
    sayf("[TRAIN] Dung cach mic ~30 cm, moi lan noi '%s' ro rang roi dung lai", VoiceHello::wordName(w));
    hello.requestTrain(w);
  } else if (strncmp(line, "forget ", 7) == 0) {
    const int w = VoiceHello::wordIndex(line + 7);
    if (w < 0) {
      say("forget <hello|stand|walk|run|stop|sit>");
      return;
    }
    hello.forgetWord(w);
    sayf("[VOICE] Da xoa mau '%s'", VoiceHello::wordName(w));
  } else if (strcmp(line, "voice") == 0) {
    hello.printStatus();
  } else if (strcmp(line, "voice on") == 0) {
    hello.setEnabled(true);
    hello.printStatus();
  } else if (strcmp(line, "voice off") == 0) {
    hello.setEnabled(false);
    hello.printStatus();
  } else if (strcmp(line, "voice debug") == 0) {
    hello.setDebug(true);
    say("[HELLO] debug BAT: in muc on moi 0,5 s (voice nodebug de tat)");
  } else if (strcmp(line, "voice nodebug") == 0) {
    hello.setDebug(false);
    say("[HELLO] debug TAT");
  } else if (strncmp(line, "voice thr ", 10) == 0) {
    const float f = atof(line + 10);
    if (f > 0.5f && f < 5.0f) {
      hello.setFactor(f);
    } else {
      say("voice thr <0.5..5>  (lon hon = de nhan hon, nho hon = kho nhan hon)");
    }
  } else if (strcmp(line, "forget") == 0) {
    hello.forget();
    say("[HELLO] Da xoa mau giong");
  } else if (strcmp(line, "ear") == 0) {
    listenDogMic();
  } else if (strcmp(line, "reply") == 0 || strncmp(line, "reply ", 6) == 0) {
    const int k = line[5] == ' ' ? atoi(line + 6) : -1;
    audio.playReply(k >= 0 && k <= 2 ? k : -1);
  } else if (strcmp(line, "scan") == 0) {
    i2cScan(Wire, "GPIO8/9");
    startWire1();
    i2cScan(Wire1, "GPIO11/12");
  } else if (strcmp(line, "audiotest") == 0) {
    MicLock lock;
    testMicByServo();
    testLoopback();
  } else if (strcmp(line, "findmpu") == 0) {
    findMpu();
  } else if (strcmp(line, "sonardiag") == 0) {
    sonarDiag();
  } else if (strcmp(line, "imu") == 0) {
    imuWatch();
  } else if (strcmp(line, "sonar") == 0) {
    sonarWatch();
  } else if (strcmp(line, "mic") == 0) {
    MicLock lock;
    micWatch();
  } else if (strcmp(line, "spk") == 0 || strcmp(line, "loa") == 0) {
    testSpeaker();
  } else if (strncmp(line, "flipknee ", 9) == 0) {
    int ch = atoi(line + 9);
    if (ch == 0 || ch == 2 || ch == 4 || ch == 6) {
      gait.flipKneeStand((uint8_t)ch);
      sayf("[CMD] flipknee ch%d -> %d", ch, (int)gait.standDeg((uint8_t)ch));
      printStand();
    } else {
      say("flipknee chi dung 0 2 4 6 (cang)");
    }
  } else if (strcmp(line, "flipstand") == 0) {
    say("[CMD] FLIP ALL KNEES");
    gait.flipAllKnees();
    printStand();
  } else if (strncmp(line, "test ", 5) == 0) {
    int ch = atoi(line + 5);
    if (ch < 0 || ch > 7) { say("test 0..7"); return; }
    sayf("[TEST] ch%d: dui phai vung VE TRUOC / cang phai chia VE TRUOC", ch);
    gait.testChannel((uint8_t)ch);
    say("Neu di nguoc chieu: flip <ch>");
  } else if (strncmp(line, "flip ", 5) == 0) {
    int ch = atoi(line + 5);
    if (ch < 0 || ch > 7) { say("flip 0..7"); return; }
    gait.flipGaitSign((uint8_t)ch);
    sayf("[CMD] flip ch%d -> %+d (da luu)", ch, (int)gait.gaitSign((uint8_t)ch));
  } else if (strcmp(line, "signs") == 0) {
    printSigns();
  } else if (strcmp(line, "resetsigns") == 0) {
    gait.resetGaitSigns();
    say("[CMD] chieu di ve mac dinh");
    printSigns();
  } else if (strncmp(line, "free ", 5) == 0) {
    int ch = atoi(line + 5);
    if (ch < 0 || ch > 7) { say("free 0..7"); return; }
    gait.freeChannel((uint8_t)ch);
    sayf("[CMD] nha servo ch%d — xoay chan bang tay duoc", ch);
  } else if (strncmp(line, "center ", 7) == 0) {
    int ch = atoi(line + 7);
    if (ch < 0 || ch > 7) { say("center 0..7"); return; }
    gait.setChannel((uint8_t)ch, 90.0f);
    sayf("[CMD] ch%d giu o 90 do — thao horn, lap lai cho chan thang roi go 'stand'", ch);
  } else if (strncmp(line, "dir ", 4) == 0) {
    int ch = atoi(line + 4);
    if (ch < 0 || ch > 7) { say("dir 0..7"); return; }
    servos.flipDir((uint8_t)ch);
    gait.applyStand();
    sayf("[DIR] ch%d dao chieu quay = %s (da luu). Lap lai horn khop nay o tu the stand.",
         ch, servos.dirFlipped((uint8_t)ch) ? "DAO" : "binh thuong");
  } else if (strcmp(line, "resetcal") == 0) {
    servos.resetDir();
    gait.clearSitDeg();
    gait.resetGaitSigns();
    gait.resetStand();
    say("[CAL] xoa het chinh sua da luu: dir, sitset, flip, set — dung lai config.h");
    printStand();
  } else if (strncmp(line, "move ", 5) == 0) {
    int ch = -1, deg = 90;
    if (sscanf(line + 5, "%d %d", &ch, &deg) == 2 && ch >= 0 && ch <= 7) {
      gait.moveChannel((uint8_t)ch, (float)deg);
      sayf("[MOVE] ch%d = %d (chua luu)", ch, deg);
    } else {
      say("Cu phap: move <ch> <goc>");
    }
  } else if (strncmp(line, "sitset ", 7) == 0) {
    int ch = -1, deg = 90;
    if (sscanf(line + 7, "%d %d", &ch, &deg) == 2 && ch >= 0 && ch <= 7) {
      gait.setSitDeg((uint8_t)ch, (int16_t)deg);
      gait.moveChannel((uint8_t)ch, (float)deg);
      sayf("[SIT] goc ngoi ch%d = %d (da luu, dung cho sit4 + sit2)", ch, deg);
    } else {
      say("Cu phap: sitset <ch> <goc>");
    }
  } else if (strcmp(line, "sitclear") == 0) {
    gait.clearSitDeg();
    say("[SIT] xoa goc ngoi rieng — dung lai config.h");
  } else if (strcmp(line, "resetstand") == 0) {
    gait.resetStand();
    say("[CMD] goc stand ve mac dinh trong config.h");
    printStand();
  } else if (strcmp(line, "hush") == 0) {
    pcmHangup();
    say("[PCM] dung phat ngay (nut dung tren app)");
  } else if (strcmp(line, "relax") == 0 || strcmp(line, "free") == 0) {
    say("[CMD] RELAX — chinh tay / van horn");
    gait.relax();
  } else if (strcmp(line, "show") == 0) {
    printStand();
  } else if (strncmp(line, "set ", 4) == 0) {
    int ch = -1, deg = 90;
    if (sscanf(line + 4, "%d %d", &ch, &deg) == 2 && ch >= 0 && ch <= 7) {
      gait.setStandDeg((uint8_t)ch, (int16_t)deg);
      gait.setChannel((uint8_t)ch, (float)deg);
      sayf("[CAL] stand ch%d = %d (da luu)", ch, deg);
    } else {
      say("Cu phap: set <ch> <goc>");
    }
  } else if (strstr(line, "walk") || strcmp(line, "di") == 0) {
    say("[CMD] WALK — ve STAND roi di");
    gait.setState(DogState::Walk);
  } else if (strstr(line, "run") || strcmp(line, "chay") == 0) {
    say("[CMD] RUN — ve STAND roi chay");
    gait.setState(DogState::Run);
  } else if (strstr(line, "sit4") || strstr(line, "ngoi4")) {
    say("[CMD] SIT4");
    gait.setState(DogState::Sit4);
  } else if (strstr(line, "sit2") || strstr(line, "ngoi2")) {
    say("[CMD] SIT2");
    gait.setState(DogState::SitRear);
  } else if (strstr(line, "shake") || strstr(line, "battay") || strncmp(line, "bat tay", 7) == 0) {
    const bool sideL = strcmp(line, "shakel") == 0 || strcmp(line, "shake l") == 0 || strstr(line, "trai");
    const bool sideR = strcmp(line, "shaker") == 0 || strcmp(line, "shake r") == 0 || strstr(line, "phai");
    const bool left = sideL || (!sideR && random(2) == 0);
    say(left ? "[CMD] BAT TAY — dung, gio chan truoc trai" : "[CMD] BAT TAY — dung, gio chan truoc phai");
    gait.shake(left);
  } else if (strstr(line, "stand") || strstr(line, "dung")) {
    say("[CMD] STAND (home)");
    gait.setState(DogState::Stand);
  } else if (strstr(line, "stop")) {
    say("[CMD] STOP");
    gait.setState(DogState::Stop);
  } else if (strstr(line, "getup") || strstr(line, "day")) {
    say("[CMD] GETUP -> STAND");
    if (audioOk) audio.beep(660, 120);
    gait.setState(DogState::Stand);
  } else if (strcmp(line, "beep") == 0) {
    say("[CMD] BEEP chan 4/5/6");
    audio.usePins(4, 5, 6);
    audio.beep(880, 600);
  } else if (strcmp(line, "beep2") == 0) {
    say("[CMD] BEEP2 chan 16/17/18 (so do cu)");
    audio.usePins(16, 17, 18);
    audio.beep(880, 600);
  } else if (strcmp(line, "status") == 0) {
    sayf("PCA=%s IMU=%s Audio=%s", pcaOk ? "OK" : "FAIL", imuOk ? "OK" : "FAIL", audioOk ? "OK" : "FAIL");
    if (imuOk) sayf("  pitch=%.1f roll=%.1f fallen=%s", imu.pitch(), imu.roll(), imu.isFallen() ? "YES" : "no");
    float d = sonar.readCm();
    sayf("  sonar=%.0f cm (nguong %d)", d, OBSTACLE_CM);
    sayf("  loa BCLK=%d LRC=%d DIN=%d", PIN_SPK_BCLK, PIN_SPK_LRC, PIN_SPK_DIN);
  } else {
    say("relax stand walk run sit4 sit2 shake stop getup | selftest scan imu sonar mic spk status");
  }
}

static void sweepRadar() {
  if (radarHold) {
    servos.setExtraUs(RADAR_CH, 1500);
    return;
  }
  const float phase = (millis() % RADAR_PERIOD_MS) / (float)RADAR_PERIOD_MS;
  const float s = sinf(phase * 6.2831853f); // -1 phải ... +1 trái
  const float mid = (radarRightUs + radarLeftUs) * 0.5f;
  const float amp = (radarLeftUs - radarRightUs) * 0.5f;
  servos.setExtraUs(RADAR_CH, (uint16_t)(mid + s * amp));
}

static DogState voiceState(int w) {
  switch (w) {
    case 1: return DogState::Stand;
    case 2: return DogState::Walk;
    case 3: return DogState::Run;
    case 4: return DogState::Stand;
    case 5: return DogState::Sit4;
    default: return DogState::Idle;
  }
}

static void runVoiceWord(int w) {
  const DogState next = voiceState(w);
  if (next == DogState::Idle) return;
  if (gait.active() == next) return;
  sayf("[VOICE] lenh '%s'", VoiceHello::wordName(w));
  if (!pcaOk) {
    say("PCA THAT BAI — khong cu dong duoc");
    return;
  }
  gait.setState(w == 4 ? DogState::Stop : next);
}

static WiFiServer pcmServer(81);
static WiFiClient pcmClient;
static uint8_t pcmCarry = 0;
static bool pcmHasCarry = false;
static int pcmHdrN = 0;
static bool pcmReady = false;

static void pcmBegin() { pcmServer.begin(); }

static void pcmHangup() {
  if (pcmClient) pcmClient.stop();
  pcmHdrN = 0;
  pcmReady = false;
  pcmHasCarry = false;
}

static uint32_t pcmLastData = 0;

// Kết nối mới luôn thay kết nối cũ: điện thoại đổi mạng giữa chừng để lại kết nối chết, không bao giờ báo đóng.
static void pcmLoop() {
  if (pcmServer.hasClient()) {
    pcmHangup();
    pcmClient = pcmServer.accept();
    pcmClient.setNoDelay(true);
    pcmLastData = millis();
    say("[PCM] loa san sang, cho dien thoai");
  }
  if (!pcmClient || !pcmClient.connected()) return;
  if (!pcmClient.available()) {
    if (millis() - pcmLastData > 10000) {
      say("[PCM] im qua 10 s, dong ket noi");
      pcmHangup();
    }
    return;
  }
  pcmLastData = millis();
  uint8_t buf[512];
  int spins = 0;
  while (pcmClient.available() && spins++ < 6) {
    if (!pcmReady) {
      int b = pcmClient.read();
      if (b < 0) return;
      static uint8_t sig[4];
      sig[pcmHdrN++] = (uint8_t)b;
      if (pcmHdrN == 4) {
        pcmReady = sig[0] == 'P' && sig[1] == 'C' && sig[2] == 'M' && sig[3] == '1';
        if (!pcmReady) {
          say("[PCM] sai header");
          pcmHangup();
          return;
        }
      }
      continue;
    }
    int n = pcmClient.read(buf, sizeof(buf));
    if (n <= 0) break;
    audio.writeMonoPcm(buf, n, &pcmCarry, &pcmHasCarry);
  }
}

void loop() {
  wifi.loop();
  pcmLoop();

  // Mic gắn trên chó nghe cả tiếng loa: đang phát (và 1,5 s sau đó) thì bỏ mọi lệnh giọng nói.
  const bool speakerBusy = pcmLastData != 0 && millis() - pcmLastData < 1500;
  const int heardWord = hello.takeWord();
  if (heardWord > 0) {
    if (speakerBusy) sayf("[VOICE] bo qua '%s' vi loa dang phat", VoiceHello::wordName(heardWord));
    else runVoiceWord(heardWord);
  }

  static uint32_t lastPcaTry = 0;
  if (!pcaOk && millis() - lastPcaTry >= 2000) {
    lastPcaTry = millis();
    tryPca();
  }

  if (pcaOk) {
    gait.update();
    hello.setNoisy(gait.state() == DogState::Walk || gait.state() == DogState::Run);
    static uint32_t lastRadar = 0;
    static bool radarQuiet = false;
    if (hello.training()) {
      // Dừng động cơ đầu lúc ghi mẫu giọng, tránh ghi nhầm tiếng ù
      if (!radarQuiet) {
        servos.detachExtra(RADAR_CH);
        radarQuiet = true;
      }
    } else if (millis() - lastRadar >= 40) {
      lastRadar = millis();
      radarQuiet = false;
      sweepRadar();
    }
  }

  if (imuOk) {
    static uint32_t lastImu = 0;
    if (millis() - lastImu >= 20) {
      lastImu = millis();
      imu.update();
      dogTel.imu = 1;
      dogTel.pitch = imu.pitch();
      dogTel.roll = imu.roll();
      dogTel.fallen = imu.isFallen() ? 1 : 0;
      if (imu.isFallen()) {
        if (!fallHandled) {
          fallHandled = true;
          sayf("[IMU] nga pitch=%.0f roll=%.0f -> STAND", imu.pitch(), imu.roll());
          if (audioOk) {
            audio.beep(520, 160);
            audio.beep(880, 160);
            audio.beep(520, 220);
          }
          if (pcaOk) gait.setState(DogState::Stand);
        }
      } else {
        fallHandled = false;
      }
    }
  }

  static uint32_t lastSonar = 0;
  if (millis() - lastSonar >= 220) {
    lastSonar = millis();
    const float d = sonar.readCm();
    dogTel.cm = d;
    const DogState st = gait.state();
    if (pcaOk && (st == DogState::Walk || st == DogState::Run) && d > 0 && d < OBSTACLE_CM) {
      sayf("[SR04] vat can %.0f cm -> STOP", d);
      if (audioOk) audio.beep(220, 200);
      gait.setState(DogState::Stop);
    }
  }

  static char line[64];
  static size_t n = 0;
  static uint32_t lastCharMs = 0;

  auto feed = [&](int ch) {
    if (ch < 0) return;
    lastCharMs = millis();
    char c = (char)ch;
    if (c == '\n' || c == '\r') {
      if (n) {
        line[n] = 0;
        n = 0;
        handleLine(line);
      }
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    }
  };

  while (Serial.available()) feed(Serial.read());
  while (Serial0.available()) feed(Serial0.read());
  if (n > 0 && millis() - lastCharMs >= 400) {
    line[n] = 0;
    n = 0;
    handleLine(line);
  }

  // Chớp mỗi 5 s: xanh dương = WiFi đang phát, tím = WiFi hỏng (đang bật lại),
  // vàng = WiFi phát nhưng lần khởi động gần nhất do sụt áp.
  static uint32_t beat = 0;
  static bool beatOn = false;
  if (millis() - beat >= 5000) {
    beat = millis();
    beatOn = true;
    if (!wifi.apUp()) led(30, 0, 30);
    else if (esp_reset_reason() == ESP_RST_BROWNOUT) led(30, 22, 0);
    else led(0, 0, 30);
  } else if (beatOn && millis() - beat >= 300) {
    beatOn = false;
    led(pcaOk ? 0 : 40, pcaOk ? 18 : 0, 0);
  }
}
