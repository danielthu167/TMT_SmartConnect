/*
 * TMT SmartConnect — MultiChannel example
 *
 * Kênh button (nhận lệnh từ dashboard):
 *   V0 → relay 1
 *   V1 → relay 2
 *
 * Kênh button (ESP gửi lên dashboard):
 *   V2 → trạng thái nút vật lý
 *
 * Kênh string (ESP gửi chuỗi cảm biến lên dashboard, chỉ đọc):
 *   V3 → trạng thái dạng text (ví dụ: "OK", "ERROR")
 *
 * Kênh number (ESP gửi số đo lên dashboard, chỉ đọc):
 *   V4 → nhiệt độ (°C)
 *   V5 → độ ẩm (%)
 *
 * Cài đặt dashboard:
 *   V0, V1, V2 → chọn type "Nút bấm"
 *   V3         → chọn type "Văn bản"
 *   V4         → chọn type "Số đo", đơn vị "°C"
 *   V5         → chọn type "Số đo", đơn vị "%"
 *
 * Cài đặt:
 *   1. Điền WIFI_SSID, WIFI_PASSWORD, API_KEY bên dưới
 *   2. Chọn board: ESP8266 hoặc ESP32
 *   3. Upload → vào trang cấu hình thiết bị, cấu hình kênh như trên
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

// Giả lập đọc cảm biến nhiệt độ và độ ẩm
float readTemperature() { return 25.0f + random(-30, 50) / 10.0f; }
float readHumidity() { return 60.0f + random(-100, 100) / 10.0f; }

unsigned long lastSensorUpdate = 0;

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

  // Gửi trạng thái ban đầu cho kênh string/number
  tmt.virtualWriteString(V3, "OK");
  tmt.virtualWriteNumber(V4, readTemperature());
  tmt.virtualWriteNumber(V5, readHumidity());
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  tmt.loop();

  // Đọc nút vật lý và gửi trạng thái lên V2 (button channel)
  bool btn = (digitalRead(BUTTON_PIN) == LOW); // active LOW
  if (btn != lastButtonState) {
    lastButtonState = btn;
    tmt.virtualWrite(V2, btn); // or: tmt.virtualWriteButton(V2, btn)
    Serial.printf("[V2] Nút = %s\n", btn ? "NHẤN" : "THẢ");
  }

  // Gửi dữ liệu cảm biến lên dashboard mỗi 5 giây
  if (millis() - lastSensorUpdate > 5000) {
    lastSensorUpdate = millis();

    // String channel: trạng thái dạng text
    tmt.virtualWriteString(V3, "OK");
    Serial.println("[V3] Trạng thái = OK");

    // Number channels: giá trị số đo
    float temp = readTemperature();
    float hum = readHumidity();
    tmt.virtualWriteNumber(V4, temp);
    tmt.virtualWriteNumber(V5, hum);
    Serial.printf("[V4] Nhiệt độ = %.1f°C\n", temp);
    Serial.printf("[V5] Độ ẩm = %.1f%%\n", hum);
  }

  delay(10); // yield for WiFi/TCP stack; helps prevent ESP8266 WDT reset
}
