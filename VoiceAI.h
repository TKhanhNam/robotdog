#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "AudioIO.h"
#include "GaitController.h"

// Lệnh điều khiển robot
enum class VoiceCmd : uint8_t {
  None,
  Walk,
  Run,
  Stop,
  Sit,
  Stand,
  GetUp,
  Chat,   // trò chuyện (cloud hoặc câu trả lời sẵn)
  Status,
  Test
};

class VoiceAI {
public:
  void begin(AudioIO* audio, GaitController* gait) {
    audio_ = audio;
    gait_ = gait;
#if ENABLE_CLOUD_AI
    connectWifi();
#endif
  }

  // Gọi định kỳ: Serial + phát hiện giọng nói đơn giản
  void update() {
    handleSerial();
#if ENABLE_CLOUD_AI
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 800) {
      lastCheck = millis();
      uint32_t e = audio_->measureEnergy(150);
      if (e > VOICE_ENERGY_THRESH && !busy_) {
        listenAndHandle();
      }
    }
#endif
  }

  void execute(VoiceCmd cmd, const String& chatText = "") {
    switch (cmd) {
      case VoiceCmd::Walk:
        Serial.println("[CMD] WALK");
        gait_->setState(DogState::Walk);
        break;
      case VoiceCmd::Run:
        Serial.println("[CMD] RUN");
        gait_->setState(DogState::Run);
        break;
      case VoiceCmd::Stop:
        Serial.println("[CMD] STOP");
        gait_->setState(DogState::Stop);
        break;
      case VoiceCmd::Sit:
        Serial.println("[CMD] SIT");
        gait_->setState(DogState::Sit);
        break;
      case VoiceCmd::Stand:
        Serial.println("[CMD] STAND");
        gait_->setState(DogState::Stand);
        break;
      case VoiceCmd::GetUp:
        Serial.println("[CMD] GETUP");
        gait_->setState(DogState::GetUp);
        break;
      case VoiceCmd::Chat:  replyChat(chatText); break;
      case VoiceCmd::Status:
        Serial.println("[Status] OK — lenh: walk run stop sit stand getup test chat");
        break;
      case VoiceCmd::Test:
        Serial.println("[Test] Quay 8 servo 60->120->90");
        gait_->testSweep();
        break;
      default: break;
    }
  }

  static VoiceCmd parseCommand(String s) {
    s.toLowerCase();
    s.trim();
    // Ưu tiên cụm dài trước để tránh nhầm (dung day vs dung)
    if (s == "walk" || s == "di" || s == "đi" || s == "di bo" || s.startsWith("di ")) return VoiceCmd::Walk;
    if (s.indexOf("walk") >= 0 || s.indexOf("đi bộ") >= 0) return VoiceCmd::Walk;
    if (s == "run" || s == "chay" || s == "chạy" || s.indexOf("chay ") >= 0 || s.indexOf("run") >= 0) return VoiceCmd::Run;
    if (s.indexOf("getup") >= 0 || s.indexOf("dung day") >= 0 || s.indexOf("đứng dậy") >= 0 ||
        s.indexOf("tu day") >= 0 || s.indexOf("tự dậy") >= 0) return VoiceCmd::GetUp;
    if (s == "stand" || s.indexOf("stand") >= 0 || s == "dung yen" || s.indexOf("đứng yên") >= 0) return VoiceCmd::Stand;
    if (s == "stop" || s == "dung" || s == "dừng" || s.indexOf("stop") >= 0 ||
        s.indexOf("dung lai") >= 0 || s.indexOf("dừng lại") >= 0) return VoiceCmd::Stop;
    if (s == "sit" || s == "ngoi" || s == "ngồi" || s.indexOf("sit") >= 0) return VoiceCmd::Sit;
    if (s.indexOf("chat") >= 0 || s == "noi" || s.indexOf("nói chuyện") >= 0 ||
        s.indexOf("tro chuyen") >= 0 || s.indexOf("trò chuyện") >= 0) return VoiceCmd::Chat;
    if (s.indexOf("status") >= 0 || s.indexOf("trang thai") >= 0) return VoiceCmd::Status;
    if (s == "test" || s.indexOf("test") >= 0) return VoiceCmd::Test;
    return VoiceCmd::None;
  }

private:
  AudioIO* audio_ = nullptr;
  GaitController* gait_ = nullptr;
  bool busy_ = false;
  bool wifiOk_ = false;

  void handleSerial() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    Serial.printf("[Serial] nhan: \"%s\"\n", line.c_str());

    if (line.startsWith("chat ") || line.startsWith("CHAT ")) {
      execute(VoiceCmd::Chat, line.substring(5));
      return;
    }
    VoiceCmd c = parseCommand(line);
    if (c == VoiceCmd::Chat) {
      execute(VoiceCmd::Chat, "Xin chao! Ban khoe khong?");
    } else if (c != VoiceCmd::None) {
      execute(c);
    } else {
      // Câu tự do → coi như chat
      execute(VoiceCmd::Chat, line);
    }
  }

  void replyChat(const String& userText) {
#if ENABLE_CLOUD_AI
    if (wifiOk_) {
      String answer = cloudChat(userText.length() ? userText : "Xin chào!");
      Serial.printf("[AI] %s\n", answer.c_str());
      // TTS cloud → phát (đơn giản hóa: beep pattern + in Serial)
      // Full TTS binary cần buffer lớn; phát beep báo đã trả lời
      audio_->beep(880, 100);
      delay(50);
      audio_->beep(1100, 100);
      return;
    }
#endif
    // Offline canned responses (tiếng Việt)
    String q = userText;
    q.toLowerCase();
    const char* ans;
    if (q.indexOf("ten") >= 0 || q.indexOf("tên") >= 0 || q.indexOf("name") >= 0)
      ans = "Toi la Robot Dog, san sang chay nhay ben ban!";
    else if (q.indexOf("khoe") >= 0 || q.indexOf("hello") >= 0 || q.indexOf("xin chao") >= 0)
      ans = "Xin chao! Ban muon toi di, chay, hay ngoi?";
    else if (q.indexOf("may tuoi") >= 0 || q.indexOf("bao nhieu") >= 0)
      ans = "Toi vua duoc lap rap, con rat tre!";
    else
      ans = "Toi nghe thay ban roi. Cu lenh: di, chay, dung, ngoi, dung day.";

    Serial.printf("[Chat] %s\n", ans);
    // Melody thay TTS offline
    audio_->beep(523, 100);
    delay(40);
    audio_->beep(659, 100);
    delay(40);
    audio_->beep(784, 140);
  }

#if ENABLE_CLOUD_AI
  void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WiFi] Connecting");
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
    wifiOk_ = WiFi.status() == WL_CONNECTED;
    if (wifiOk_) Serial.printf("[WiFi] OK %s\n", WiFi.localIP().toString().c_str());
    else Serial.println("[WiFi] FAIL — dung che do local");
  }

  void listenAndHandle() {
    busy_ = true;
    audio_->beep(1500, 60);
    Serial.println("[Voice] Listening...");

    const size_t total = (MIC_SAMPLE_RATE * VOICE_LISTEN_MS) / 1000;
    int16_t* pcm = (int16_t*)malloc(total * sizeof(int16_t));
    if (!pcm) { busy_ = false; return; }

    size_t got = audio_->record(pcm, total);
    String text = cloudSTT(pcm, got);
    free(pcm);

    Serial.printf("[STT] \"%s\"\n", text.c_str());
    VoiceCmd c = parseCommand(text);
    if (c == VoiceCmd::None || c == VoiceCmd::Chat) {
      execute(VoiceCmd::Chat, text);
    } else {
      execute(c);
    }
    busy_ = false;
  }

  String cloudSTT(const int16_t* pcm, size_t samples) {
    // Gửi WAV thô qua multipart — nhiều proxy hỗ trợ; đơn giản hóa trả ""
    // Production: dùng endpoint /v1/audio/transcriptions
    if (!wifiOk_ || samples < 1000) return "";

    // Xây header WAV 44 byte + PCM
    size_t dataBytes = samples * sizeof(int16_t);
    size_t wavSize = 44 + dataBytes;
    uint8_t* wav = (uint8_t*)malloc(wavSize);
    if (!wav) return "";
    writeWavHeader(wav, dataBytes, MIC_SAMPLE_RATE, 1, 16);
    memcpy(wav + 44, pcm, dataBytes);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = String(AI_BASE_URL) + "/v1/audio/transcriptions";
    http.begin(client, url);
    http.addHeader("Authorization", String("Bearer ") + AI_API_KEY);

    // Multipart tối giản
    String boundary = "----RobotDogBoundary";
    String head = "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"model\"\r\n\r\n" + String(AI_STT_MODEL) + "\r\n"
      "--" + boundary + "\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"a.wav\"\r\n"
      "Content-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    size_t bodyLen = head.length() + wavSize + tail.length();
    uint8_t* body = (uint8_t*)malloc(bodyLen);
    if (!body) { free(wav); return ""; }
    memcpy(body, head.c_str(), head.length());
    memcpy(body + head.length(), wav, wavSize);
    memcpy(body + head.length() + wavSize, tail.c_str(), tail.length());
    free(wav);

    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    int code = http.POST(body, bodyLen);
    free(body);

    String resp = http.getString();
    http.end();
    if (code != 200) {
      Serial.printf("[STT] HTTP %d %s\n", code, resp.c_str());
      return "";
    }
    // {"text":"..."}
    int i = resp.indexOf("\"text\"");
    if (i < 0) return "";
    int a = resp.indexOf('"', i + 6);
    int b = resp.indexOf('"', a + 1);
    if (a < 0 || b < 0) return "";
    return resp.substring(a + 1, b);
  }

  String cloudChat(const String& user) {
    if (!wifiOk_) return "Chua co WiFi.";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = String(AI_BASE_URL) + "/v1/chat/completions";
    http.begin(client, url);
    http.addHeader("Authorization", String("Bearer ") + AI_API_KEY);
    http.addHeader("Content-Type", "application/json");

    String payload = String("{")
      + "\"model\":\"" + AI_CHAT_MODEL + "\","
      + "\"messages\":["
      + "{\"role\":\"system\",\"content\":\"Ban la chu cho robot vui ve, tra loi ngan bang tieng Viet.\"},"
      + "{\"role\":\"user\",\"content\":\"" + escapeJson(user) + "\"}"
      + "],\"max_tokens\":120}";

    int code = http.POST(payload);
    String resp = http.getString();
    http.end();
    if (code != 200) {
      Serial.printf("[Chat] HTTP %d\n", code);
      return "Xin loi, mang/API loi.";
    }
    int i = resp.indexOf("\"content\"");
    if (i < 0) return resp;
    int a = resp.indexOf('"', i + 9);
    // content sau ": "
    a = resp.indexOf('"', i + 10);
    int b = resp.indexOf('"', a + 1);
    // Có thể có escape — lấy đoạn đơn giản
    if (a < 0) return resp;
    // Tìm content value đúng hơn
    int cpos = resp.indexOf(':', i);
    int q1 = resp.indexOf('"', cpos);
    int q2 = q1 + 1;
    while (q2 < (int)resp.length()) {
      if (resp[q2] == '"' && resp[q2 - 1] != '\\') break;
      q2++;
    }
    return resp.substring(q1 + 1, q2);
  }

  static String escapeJson(const String& s) {
    String o;
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '"' || c == '\\') o += '\\';
      if (c == '\n') { o += "\\n"; continue; }
      o += c;
    }
    return o;
  }

  static void writeWavHeader(uint8_t* h, uint32_t dataBytes, uint32_t rate, uint16_t ch, uint16_t bits) {
    auto w32 = [&](int off, uint32_t v) {
      h[off] = v & 0xff; h[off+1] = (v >> 8) & 0xff;
      h[off+2] = (v >> 16) & 0xff; h[off+3] = (v >> 24) & 0xff;
    };
    auto w16 = [&](int off, uint16_t v) {
      h[off] = v & 0xff; h[off+1] = (v >> 8) & 0xff;
    };
    memcpy(h, "RIFF", 4);
    w32(4, 36 + dataBytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    w32(16, 16);
    w16(20, 1);
    w16(22, ch);
    w32(24, rate);
    w32(28, rate * ch * bits / 8);
    w16(32, ch * bits / 8);
    w16(34, bits);
    memcpy(h + 36, "data", 4);
    w32(40, dataBytes);
  }
#endif
};
