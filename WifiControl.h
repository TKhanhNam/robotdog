#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_system.h>
#include "config.h"
#include "Telemetry.h"

static const char WIFI_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1"/>
<title>Robot Dog</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,sans-serif;background:#111;color:#eee;padding:16px}
h1{font-size:1.2rem;margin:0 0 8px}
p{color:#9aa;margin:0 0 16px;font-size:.9rem}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{border:0;border-radius:12px;padding:18px 8px;font-size:1.05rem;font-weight:700;color:#fff}
.walk{background:#1b7}
.run{background:#e67}
.stand{background:#27a}
.stop{background:#c33}
.sit{background:#36a}
.aux{background:#444}
#ok{min-height:1.2em;color:#8f8;margin-top:12px}
</style>
</head>
<body>
<h1>Robot Dog</h1>
<p>Cam pin 5V vao chan 5V ESP32. Rut USB van dieu khien duoc.</p>
<div class="grid">
<button class="walk" onclick="go('walk')">Di bo</button>
<button class="run" onclick="go('run')">Chay</button>
<button class="stand" onclick="go('stand')">Dung</button>
<button class="stop" onclick="go('stop')">Dung lai</button>
<button class="sit" onclick="go('sit4')">Ngoi 4 chan</button>
<button class="sit" onclick="go('sit2')">Ngoi 2 chan sau</button>
<button class="sit" onclick="go('shake')">Bat tay</button>
<button class="aux" onclick="go('getup')">Day</button>
<button class="aux" onclick="go('beep')">Bip</button>
<button class="aux" onclick="go('reply')">Sua / nhac</button>
<button class="aux" onclick="go('relax')">Tha servo</button>
</div>
<p id="ok"></p>
<p id="st"></p>
<script>
async function go(c){
  const el=document.getElementById('ok');
  el.textContent='...';
  try{
    const r=await fetch('/cmd?c='+encodeURIComponent(c));
    el.textContent=r.ok?('Da gui: '+c):'Loi';
  }catch(e){el.textContent='Mat ket noi WiFi';}
}
async function st(){
  const el=document.getElementById('st');
  try{
    const j=await (await fetch('/status')).json();
    el.textContent='Da chay '+j.up+' giay | khoi dong lan truoc do: '+j.rst;
  }catch(e){el.textContent='Mat ket noi WiFi';}
}
setInterval(st,2000);st();
</script>
</body>
</html>
)HTML";

class WifiControl {
public:
  using CmdFn = void (*)(char*);
  bool ok() const { return ok_; }
  const char* ssid() const { return ssid_; }

  void begin(CmdFn onCmd) {
    onCmd_ = onCmd;

    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK ||
        (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0) {
      strncpy(ssid_, WIFI_AP_SSID, sizeof(ssid_) - 1);
    } else {
      snprintf(ssid_, sizeof(ssid_), "RobotDog-%02X%02X", mac[4], mac[5]);
    }

    WiFi.persistent(false);
    startAp();

    server_.on("/", HTTP_GET, [this]() {
      server_.send_P(200, "text/html; charset=utf-8", WIFI_PAGE);
    });
    server_.on("/cmd", HTTP_GET, [this]() { serveCmd(); });
    server_.on("/status", HTTP_GET, [this]() { serveStatus(); });
    // Điện thoại kiểm tra Internet bằng các URL này. Trả 204 để không nhảy về WiFi nhà.
    server_.on("/generate_204", HTTP_ANY, [this]() { server_.send(204); });
    server_.on("/gen_204", HTTP_ANY, [this]() { server_.send(204); });
    server_.on("/generate204", HTTP_ANY, [this]() { server_.send(204); });
    server_.on("/hotspot-detect.html", HTTP_ANY, [this]() {
      server_.send(200, "text/html", "Success");
    });
    server_.on("/connecttest.txt", HTTP_ANY, [this]() {
      server_.send(200, "text/plain", "Microsoft Connect Test");
    });
    server_.onNotFound([this]() {
      if (server_.uri().indexOf("generate_204") >= 0 || server_.uri().indexOf("gen_204") >= 0) {
        server_.send(204);
        return;
      }
      server_.sendHeader("Location", "/");
      server_.send(302);
    });
    server_.begin();
    dns_.start(53, "*", AP_IP);
  }

  void loop() {
    dns_.processNextRequest();
    server_.handleClient();
    if (millis() - lastCheck_ >= 3000) {
      lastCheck_ = millis();
      if (!apUp()) startAp();
    }
    staLoop();
  }

  bool apUp() const {
    return ok_ && (WiFi.getMode() & WIFI_MODE_AP) && WiFi.softAPIP() == AP_IP;
  }

  IPAddress apIp() const { return WiFi.softAPIP(); }

  bool staUp() const { return hotspotWanted() && WiFi.status() == WL_CONNECTED; }

private:
  const IPAddress AP_IP{192, 168, 4, 1};
  WebServer server_{80};
  DNSServer dns_;
  CmdFn onCmd_ = nullptr;
  bool ok_ = false;
  uint32_t lastCheck_ = 0;
  uint32_t lastJoin_ = 0;
  uint32_t lastBeacon_ = 0;
  bool staWas_ = false;
  WiFiUDP udp_;
  char ssid_[24] = "RobotDog";

  static bool hotspotWanted() { return HOTSPOT_SSID[0] != 0; }

  // Nối hotspot điện thoại; mất thì thử lại mỗi 15 s. Khi đã nối, báo IP cho app mỗi 2 s.
  void staLoop() {
    if (!hotspotWanted()) return;
    const uint32_t now = millis();
    const bool up = WiFi.status() == WL_CONNECTED;
    if (up != staWas_) {
      staWas_ = up;
      if (up) Serial.printf("[WiFi] da noi hotspot %s, IP %s\n", HOTSPOT_SSID, WiFi.localIP().toString().c_str());
      else Serial.println("[WiFi] mat hotspot");
    }
    if (!up) {
      if (now - lastJoin_ >= 15000) {
        lastJoin_ = now;
        WiFi.disconnect(false, false);
        WiFi.begin(HOTSPOT_SSID, HOTSPOT_PASS);
      }
      return;
    }
    if (now - lastBeacon_ >= 2000) {
      lastBeacon_ = now;
      const IPAddress ip = WiFi.localIP();
      const IPAddress mask = WiFi.subnetMask();
      IPAddress bcast;
      for (int i = 0; i < 4; i++) bcast[i] = ip[i] | (uint8_t)~mask[i];
      char msg[48];
      snprintf(msg, sizeof(msg), "ROBOTDOG %s %s", ssid_, ip.toString().c_str());
      if (udp_.beginPacket(bcast, BEACON_PORT)) {
        udp_.write((const uint8_t*)msg, strlen(msg));
        udp_.endPacket();
      }
    }
  }

  void startAp() {
    WiFi.mode(hotspotWanted() ? WIFI_AP_STA : WIFI_AP);
    delay(150);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_AP_TX_POWER);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    // WPA2 — Windows 11 dễ thấy hơn mạng mở
    ok_ = WiFi.softAP(ssid_, WIFI_AP_PASS, 6, 0, 4);
    delay(250);
    WiFi.setTxPower(WIFI_AP_TX_POWER);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    if (hotspotWanted()) {
      WiFi.setAutoReconnect(true);
      WiFi.begin(HOTSPOT_SSID, HOTSPOT_PASS);
      lastJoin_ = millis();
    }
  }

  void serveCmd() {
    String c = server_.arg("c");
    c.trim();
    c.toLowerCase();
    if (c.length() == 0 || c.length() > 46) {
      server_.send(400, "text/plain", "lenh rong");
      return;
    }
    char buf[48];
    c.toCharArray(buf, sizeof(buf));
    if (onCmd_) onCmd_(buf);
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.send(200, "text/plain", "OK");
  }

  static const char* resetName() {
    switch (esp_reset_reason()) {
      case ESP_RST_BROWNOUT: return "SUT AP (pin yeu / day long)";
      case ESP_RST_POWERON:  return "bat nguon";
      case ESP_RST_EXT:      return "nut RST";
      case ESP_RST_SW:       return "phan mem";
      case ESP_RST_PANIC:    return "loi chuong trinh";
      case ESP_RST_INT_WDT:
      case ESP_RST_TASK_WDT:
      case ESP_RST_WDT:      return "treo";
      default:               return "khac";
    }
  }

  void serveStatus() {
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"ap\":\"%s\",\"ip\":\"%s\",\"sta\":\"%s\",\"ok\":%s,\"up\":%lu,\"rst\":\"%s\","
             "\"cm\":%.0f,\"pitch\":%.1f,\"roll\":%.1f,\"fallen\":%s,\"imu\":%s,\"heard\":%d}",
             ssid_, WiFi.softAPIP().toString().c_str(),
             staUp() ? WiFi.localIP().toString().c_str() : "", ok_ ? "true" : "false",
             (unsigned long)(millis() / 1000), resetName(),
             dogTel.cm, dogTel.pitch, dogTel.roll,
             dogTel.fallen ? "true" : "false",
             dogTel.imu ? "true" : "false",
             dogTel.heard);
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.send(200, "application/json", buf);
  }
};
