/*
 * TMT SmartConnect — BasicLED example
 *
 * Điều khiển đèn LED qua dashboard TMT SmartConnect.
 * V0 → bật/tắt LED
 *
 * Cài đặt:
 *   1. Điền WIFI_SSID, WIFI_PASSWORD, API_KEY bên dưới
 *   2. Chọn board: ESP8266 hoặc ESP32
 *   3. Upload → vào trang cấu hình thiết bị, thêm nút điều khiển V0
 */

#include <TMT_SmartConnect.h>

// ── Cấu hình ─────────────────────────────────────────────────────────────────

#define WIFI_SSID "TMT Electronic Guest"
#define WIFI_PASSWORD ""
#define API_KEY "TMTK_2FB8D5CEBEBFC2512695F49AB5E9D641F5039517"

#ifdef ESP8266
#define LED_PIN LED_BUILTIN // GPIO2 trên NodeMCU (active LOW)
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_PIN 2 // GPIO2 trên ESP32 DevKit
#define LED_ON HIGH
#define LED_OFF LOW
#endif

// ── Khởi tạo ─────────────────────────────────────────────────────────────────

TmtSmartConnect tmt;

// ── Callback: server gửi lệnh xuống V0 ──────────────────────────────────────

void onLEDCommand(bool value) {
  digitalWrite(LED_PIN, value ? LED_ON : LED_OFF);
  Serial.print("[V0] LED = ");
  Serial.println(value ? "BẬT" : "TẮT");
}

// ── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== TMT SmartConnect — BasicLED ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

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

  // Đăng ký callback cho kênh V0
  tmt.onWrite(V0, onLEDCommand);

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
  delay(10); // yield for WiFi/TCP stack; helps prevent ESP8266 WDT reset
}
