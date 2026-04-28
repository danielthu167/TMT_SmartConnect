/*
 * TMT SmartConnect — MultiChannel example
 *
 * Điều khiển 2 relay và gửi trạng thái nút vật lý lên dashboard.
 * V0 → relay 1 (nhận lệnh từ dashboard)
 * V1 → relay 2 (nhận lệnh từ dashboard)
 * V2 → trạng thái nút vật lý (ESP gửi lên dashboard)
 *
 * Cài đặt:
 *   1. Điền WIFI_SSID, WIFI_PASSWORD, API_KEY bên dưới
 *   2. Chọn board: ESP8266 hoặc ESP32
 *   3. Upload → vào trang cấu hình thiết bị, thêm các kênh V0, V1, V2
 */

#include <TMT_SmartConnect.h>

// ── Cấu hình ─────────────────────────────────────────────────────────────────

#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"
#define API_KEY "TMTK_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define BUTTON_PIN 0 // BOOT button trên NodeMCU/DevKit

// ── Khởi tạo ─────────────────────────────────────────────────────────────────

TmtSmartConnect tmt;

bool lastButtonState = false;

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== TMT SmartConnect — MultiChannel ===");

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Kết nối WiFi
  WiFi.mode(WIFI_STA);
  Serial.print("[WiFi] Đang kết nối tới ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" OK — IP: ");
  Serial.println(WiFi.localIP());

  // Đăng ký callback cho từng kênh — server gửi lệnh xuống
  tmt.onWrite(V0, [](bool val) {
    digitalWrite(RELAY1_PIN, val ? HIGH : LOW);
    Serial.printf("[V0] Relay 1 = %s\n", val ? "ON" : "OFF");
  });

  tmt.onWrite(V1, [](bool val) {
    digitalWrite(RELAY2_PIN, val ? HIGH : LOW);
    Serial.printf("[V1] Relay 2 = %s\n", val ? "ON" : "OFF");
  });

  // Tuỳ chọn: callback khi kết nối / mất kết nối server
  tmt.onConnect([]() { Serial.println("[TMT] Đã kết nối!"); });
  tmt.onDisconnect(
      []() { Serial.println("[TMT] Mất kết nối, đang thử lại..."); });

  // Khởi động TMT SmartConnect — WiFi phải kết nối trước
  tmt.begin(API_KEY);
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  tmt.loop();

  // Đọc nút vật lý và gửi trạng thái lên V2
  bool btn = (digitalRead(BUTTON_PIN) == LOW); // active LOW
  if (btn != lastButtonState) {
    lastButtonState = btn;
    tmt.virtualWrite(V2, btn);
    Serial.printf("[V2] Nút = %s\n", btn ? "NHẤN" : "THẢ");
  }

  delay(10); // yield for WiFi/TCP stack; helps prevent ESP8266 WDT reset
}
