# TMT SmartConnect

[![Arduino Library](https://www.ardu-badge.com/badge/TMT_SmartConnect.svg)](https://www.ardu-badge.com/TMT_SmartConnect)

**Nền Tảng IoT Cloud Control — Hỗ Trợ ESP32/ESP8266 — Dashboard 8 Kênh**

Giải pháp điều khiển thiết bị điện qua Internet – ổn định, linh hoạt, dễ triển khai cho mọi nhu cầu.

🌐 Platform: [smartconnect.tmtelectronic.com](https://smartconnect.tmtelectronic.com)

---

## Tính Năng

- **8 kênh điều khiển độc lập** — bật/tắt từng thiết bị riêng biệt (đèn, máy bơm, quạt, relay...)
- **Điều khiển từ xa qua Internet** — dùng trên điện thoại & máy tính, không giới hạn khoảng cách
- **Dashboard trực quan** — đặt tên từng kênh theo nhu cầu, cấu hình số kênh từ 1 → 8
- **Device ID riêng** — kết nối an toàn, quản lý nhiều thiết bị dễ dàng
- **Hỗ trợ ESP8266 & ESP32** — plug & play, phù hợp DIY & thương mại

## Cài Đặt

### Arduino IDE

1. Tải thư viện tại [Releases](https://github.com/danielthu167/TMT_SmartConnect/releases)
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library**
3. Chọn file `.zip` vừa tải

### Arduino Library Manager

Tìm kiếm **TMT SmartConnect** trong Library Manager.

## Sử Dụng

```cpp
#include <TMT_SmartConnect.h>

TmtSmartConnect sc;

void setup() {
  // Kết nối WiFi trước
  sc.begin("TMTK_your_api_key");
}

void loop() {
  sc.loop();
}

void onChannelChange(uint8_t channel, bool state) {
  // channel: 0-7, state: true = bật, false = tắt
}
```

Lấy API Key (Device ID) tại: [smartconnect.tmtelectronic.com](https://smartconnect.tmtelectronic.com)

## Ứng Dụng Thực Tế

- 🏠 Nhà thông minh — đèn, quạt, bình nước nóng
- 🌾 Nông nghiệp — máy bơm, tưới tự động
- 🏭 Xưởng sản xuất nhỏ
- 🐟 Hệ thống nuôi trồng

## Chi Phí Sử Dụng

Xem chi tiết tại: [TMT Smart Connect — Nền Tảng IoT Cloud Control](https://smartconnect.tmtelectronic.com/products/detail/?slug=tmt-smart-connect-nen-tang-iot-cloud-control)

## Lợi Ích

- Không phụ thuộc nền tảng nước ngoài
- Tùy biến theo nhu cầu thực tế
- Dễ triển khai, dễ mở rộng
- Hỗ trợ kỹ thuật nhanh chóng

---

© TMT Electronic — [tmtelectronic.com](https://tmtelectronic.com)
