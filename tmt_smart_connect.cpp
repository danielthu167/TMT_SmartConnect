#include "tmt_smart_connect.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

TmtSmartConnect::TmtSmartConnect()
    : _port(TMT_SMART_CONNECT_DEFAULT_PORT), _tcpConnected(false),
      _wsUpgraded(false), _socketConnected(false), _hardwareSent(false),
      _lastHeartbeatRx(0), _lastHeartbeatTx(0), _reconnectAt(0),
      _reconnectInterval(RECONNECT_MIN), _reconnectCount(0),
      _onConnect(nullptr), _onDisconnect(nullptr) {
  memset(_apiKey, 0, sizeof(_apiKey));
  memset(_host, 0, sizeof(_host));
  memset(_channelStates, 0, sizeof(_channelStates));
  memset(_callbacks, 0, sizeof(_callbacks));
  memset(_rxBuf, 0, sizeof(_rxBuf));
}

// ─── Public API ──────────────────────────────────────────────────────────────

void TmtSmartConnect::begin(const char *apiKey, const char *host,
                            uint16_t port) {
  strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);
  strncpy(_host, host, sizeof(_host) - 1);
  _port = port;

  if (WiFi.status() != WL_CONNECTED)
    Serial.println(
        "[TMT] WiFi not connected — call WiFi.begin() before begin()");
}

void TmtSmartConnect::onWrite(uint8_t ch, VirtualWriteCallback cb) {
  if (ch < TMT_MAX_CHANNELS)
    _callbacks[ch] = cb;
}

bool TmtSmartConnect::virtualRead(uint8_t ch) const {
  return (ch < TMT_MAX_CHANNELS) ? _channelStates[ch] : false;
}

void TmtSmartConnect::virtualWrite(uint8_t ch, bool value) {
  if (ch >= TMT_MAX_CHANNELS)
    return;
  _channelStates[ch] = value;
  if (_socketConnected)
    _sendDeviceData();
}

// ─── loop() ──────────────────────────────────────────────────────────────────

void TmtSmartConnect::loop() {
  // 1. WiFi check — WiFi is managed by the app, not this library
  if (WiFi.status() != WL_CONNECTED) {
    if (_tcpConnected)
      _disconnect(false);
    return;
  }

  // 2. Not connected — attempt reconnect with backoff
  if (!_tcpConnected) {
    if (millis() < _reconnectAt)
      return;

    // RSSI check before reconnect
    int rssi = WiFi.RSSI();
    if (rssi < -90) {
      _reconnectAt = millis() + _reconnectInterval;
      return;
    }

    if (!_connectTcp() || !_doWsHandshake()) {
      _disconnect(true);
      return;
    }
    return;
  }

  // 3. TCP dropped unexpectedly
  if (!_client.connected()) {
    _disconnect(true);
    return;
  }

  // 4. Heartbeat timeout — no server message for 35s
  if (_socketConnected && !_isConnectionHealthy()) {
    _disconnect(true);
    return;
  }

  // 5. Read incoming frames
  while (_client.available()) {
    int len = _wsReadFrame();
    if (len < 0) {
      _disconnect(true);
      return;
    }
    if (len > 0)
      _handleRawMessage(_rxBuf, (size_t)len);
  }

  // 6. Periodic heartbeat-pong
  if (_socketConnected && millis() - _lastHeartbeatTx > HEARTBEAT_TX_INTERVAL) {
    _lastHeartbeatTx = millis();
    _wsSendText("42[\"heartbeat-pong\",{}]");
  }
}

// ─── Connection helpers ──────────────────────────────────────────────────────

bool TmtSmartConnect::_connectTcp() {
  _client.setInsecure(); // TLS without CA bundle (embedded constraint)
  return _client.connect(_host, _port);
}

bool TmtSmartConnect::_doWsHandshake() {
  // Random 16-byte key → base64
  uint8_t keyBytes[16];
  for (int i = 0; i < 16; i++)
    keyBytes[i] = random(256);
  String wsKey = _base64(keyBytes, 16);

  // Build "host" authority — include port when non-standard (RFC 7230)
  bool isStdPort = (_port == 80 || _port == 443);
  String hostHeader = _host;
  if (!isStdPort) {
    hostHeader += ":";
    hostHeader += _port;
  }

  String req;
  req += "GET /socket.io/?EIO=4&transport=websocket HTTP/1.1\r\n";
  req += "Host: ";
  req += hostHeader;
  req += "\r\n";
  req += "Upgrade: websocket\r\n";
  req += "Connection: Upgrade\r\n";
  req += "Sec-WebSocket-Key: ";
  req += wsKey;
  req += "\r\n";
  req += "Sec-WebSocket-Version: 13\r\n";
  req += "x-tmt-smart-connect-key: ";
  req += _apiKey;
  req += "\r\n";
  req += "x-tmt-smart-connect-device-id: ";
  req += _apiKey;
  req += "\r\n";
  req += "x-tmt-smart-connect-protocol: 1\r\n";
  req += "\r\n";
  _client.print(req);

  // Wait for HTTP 101
  unsigned long dl = millis() + 5000;
  while (!_client.available() && millis() < dl)
    delay(10);
  String status = _client.readStringUntil('\n');
  if (status.indexOf("101") < 0)
    return false;

  // Drain remaining headers
  dl = millis() + 3000;
  while (millis() < dl) {
    String line = _client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      break;
  }

  _tcpConnected = true;
  _wsUpgraded = true;
  return true;
}

void TmtSmartConnect::_disconnect(bool scheduleReconnect) {
  bool wasConnected = _socketConnected;

  _client.stop();
  _tcpConnected = false;
  _wsUpgraded = false;
  _socketConnected = false;
  _hardwareSent = false;

  if (wasConnected && _onDisconnect)
    _onDisconnect();

  if (scheduleReconnect) {
    _reconnectCount++;

    // Exponential backoff — every RECONNECT_BACKOFF_STEP attempts add 2s
    if (_reconnectCount % RECONNECT_BACKOFF_STEP == 0) {
      _reconnectInterval += 2000;
      if (_reconnectInterval > RECONNECT_MAX)
        _reconnectInterval = RECONNECT_MAX;
    }
    _reconnectAt = millis() + _reconnectInterval;
  }
}

bool TmtSmartConnect::_isConnectionHealthy() {
  // 35s without any message = dead connection
  return _lastHeartbeatRx == 0 ||
         (millis() - _lastHeartbeatRx < HEARTBEAT_RX_TIMEOUT);
}

// ─── WebSocket frame I/O ─────────────────────────────────────────────────────

bool TmtSmartConnect::_wsSendText(const char *text, size_t payloadLen) {
  if (!_tcpConnected || !_wsUpgraded)
    return false;

  uint8_t header[10];
  size_t hLen = 0;
  uint8_t mask[4];
  for (int i = 0; i < 4; i++)
    mask[i] = random(256);

  header[hLen++] = 0x81; // FIN + text

  if (payloadLen < 126) {
    header[hLen++] = 0x80 | (uint8_t)payloadLen;
  } else if (payloadLen < 65536) {
    header[hLen++] = 0x80 | 126;
    header[hLen++] = (payloadLen >> 8) & 0xFF;
    header[hLen++] = payloadLen & 0xFF;
  } else {
    return false;
  }
  memcpy(header + hLen, mask, 4);
  hLen += 4;

  _client.write(header, hLen);

  uint8_t chunk[64];
  for (size_t i = 0; i < payloadLen;) {
    size_t n = min((size_t)64, payloadLen - i);
    for (size_t j = 0; j < n; j++)
      chunk[j] = (uint8_t)text[i + j] ^ mask[(i + j) & 3];
    _client.write(chunk, n);
    i += n;
  }
  return true;
}

int TmtSmartConnect::_wsReadFrame() {
  if (_client.available() < 2)
    return 0;

  uint8_t b0 = _client.read();
  uint8_t b1 = _client.read();

  uint8_t opcode = b0 & 0x0F;
  bool masked = (b1 & 0x80) != 0;
  size_t payLen = b1 & 0x7F;

  // Extended length — wait for bytes before consuming
  int extraLen = (payLen == 126) ? 2 : (payLen == 127 ? -1 : 0);
  if (extraLen < 0)
    return -1; // 8-byte length unsupported
  int maskLen = masked ? 4 : 0;

  unsigned long dl = millis() + 2000;
  while (_client.available() < extraLen + maskLen && millis() < dl)
    delay(1);
  if (_client.available() < extraLen + maskLen)
    return -1;

  if (extraLen == 2) {
    payLen = ((size_t)_client.read() << 8);
    payLen |= (size_t)_client.read();
  }

  uint8_t mask[4] = {0};
  if (masked)
    for (int i = 0; i < 4; i++)
      mask[i] = _client.read();

  if (opcode == 0x08)
    return -1;          // close
  if (opcode == 0x09) { // ping → echo payload in masked pong (RFC 6455 §5.5.3)
    if (payLen > 125)
      return -1; // control frame >125 bytes violates spec — stream corrupt

    uint8_t pingData[125];
    unsigned long dl2 = millis() + 1000;
    size_t collected = 0;
    while (collected < payLen && millis() < dl2) {
      if (_client.available())
        pingData[collected++] = _client.read();
    }
    if (collected < payLen)
      return -1; // drain timeout — stream corrupt, force reconnect

    // Send pong: FIN + opcode=0x8A, MASK bit set, 4-byte random mask, masked
    // payload
    uint8_t pongMask[4];
    for (int i = 0; i < 4; i++)
      pongMask[i] = random(256);
    uint8_t pongHdr[6] = {0x8A,        (uint8_t)(0x80 | payLen),
                          pongMask[0], pongMask[1],
                          pongMask[2], pongMask[3]};
    _client.write(pongHdr, sizeof(pongHdr));
    for (size_t i = 0; i < payLen; i++)
      pingData[i] ^= pongMask[i & 3];
    if (payLen > 0)
      _client.write(pingData, payLen);
    return 0;
  }
  // Helper: drain exactly payLen bytes, waiting for arrival
  auto drainPayload = [&]() -> bool {
    unsigned long d = millis() + 3000;
    size_t remaining = payLen;
    while (remaining > 0 && millis() < d) {
      if (_client.available()) {
        _client.read();
        remaining--;
      }
    }
    return remaining == 0; // false = timed out, stream corrupt
  };

  if (opcode != 0x01 && opcode != 0x00) { // non-text frame — drain and skip
    if (!drainPayload())
      return -1; // timeout → disconnect
    return 0;
  }
  if (payLen >= TMT_RX_BUF_SIZE) { // oversized — can't buffer, disconnect
    return -1;
  }

  dl = millis() + 3000;
  while (_client.available() < (int)payLen && millis() < dl)
    delay(1);
  if (_client.available() < (int)payLen)
    return -1;

  for (size_t i = 0; i < payLen; i++)
    _rxBuf[i] = _client.read() ^ mask[i & 3];
  _rxBuf[payLen] = '\0';

  _lastHeartbeatRx = millis(); // any frame = connection alive
  return (int)payLen;
}

// ─── Socket.IO / Engine.IO protocol ──────────────────────────────────────────

void TmtSmartConnect::_handleRawMessage(const char *msg, size_t len) {
  if (len == 0)
    return;
  char eio = msg[0];

  if (eio == '0') { // Engine.IO OPEN → send Socket.IO namespace connect
    _wsSendText("40");
    return;
  }
  if (len == 1 && eio == '2') { // Engine.IO PING → PONG
    _wsSendText("3");
    return;
  }
  if (len >= 2 && eio == '4') {
    char sio = msg[1];
    if (sio == '0') { // Socket.IO CONNECT confirmed
      _socketConnected = true;
      _reconnectInterval = RECONNECT_MIN; // reset backoff on success
      _reconnectCount = 0;
      if (_onConnect)
        _onConnect();
      _sendDeviceData();
      return;
    }
    if (sio == '1') { // Socket.IO DISCONNECT
      _socketConnected = false;
      return;
    }
    if (sio == '2' && len > 2) {
      _handleSioEvent(msg + 2, len - 2);
      return;
    }
  }
}

void TmtSmartConnect::_handleSioEvent(const char *json, size_t len) {
  if (len < 4 || json[0] != '[')
    return;

  const char *q1 = strchr(json, '"');
  if (!q1)
    return;
  const char *q2 = strchr(q1 + 1, '"');
  if (!q2)
    return;

  size_t evLen = q2 - q1 - 1;
  char event[64];
  if (evLen >= sizeof(event))
    return;
  memcpy(event, q1 + 1, evLen);
  event[evLen] = '\0';

  if (strcmp(event, "heartbeat-ping") == 0) {
    // Echo the "t" timestamp field back so server can measure round-trip
    // latency
    const char *tStart = strstr(json, "\"t\":");
    if (tStart) {
      tStart += 4;
      const char *tEnd = strpbrk(tStart, ",}");
      if (tEnd) {
        char pong[64];
        size_t tLen = tEnd - tStart;
        if (tLen < 20) {
          snprintf(pong, sizeof(pong), "42[\"heartbeat-pong\",{\"t\":%.*s}]",
                   (int)tLen, tStart);
          _wsSendText(pong);
          return;
        }
      }
    }
    _wsSendText("42[\"heartbeat-pong\",{}]");
    return;
  }
  if (strcmp(event, "client-status") == 0) {
    const char *obj = strchr(q2 + 1, '{');
    if (obj)
      _handleClientStatus(obj, len - (obj - json));
    return;
  }
  if (strcmp(event, "request-config") == 0) {
    _sendDeviceData();
    return;
  }
}

void TmtSmartConnect::_handleClientStatus(const char *json, size_t len) {
  // Payload: { "status": { "channel": "V0", "value": true }, "timestamp": ... }
  // Parse the inner "status" object for channel and value.
  const char *statusObj = nullptr;
  const char *p = strstr(json, "\"status\"");
  if (p) {
    p = strchr(p, '{');
    if (p)
      statusObj = p;
  }

  // Fallback: try top-level if no "status" wrapper (future-proof)
  const char *src = statusObj ? statusObj : json;
  size_t srcLen = statusObj ? (size_t)(json + len - src) : len;

  String channel = _extractStringField(src, srcLen, "channel");
  if (channel.length() < 2 || channel[0] != 'V')
    return;

  uint8_t idx = (uint8_t)channel.substring(1).toInt();
  if (idx >= TMT_MAX_CHANNELS)
    return;

  bool val = _extractBoolField(src, srcLen, "value");
  _channelStates[idx] = val;

  if (_callbacks[idx])
    _callbacks[idx](val);

  // Confirm updated state back to server
  _sendDeviceData();
}

// ─── Send helpers ────────────────────────────────────────────────────────────

void TmtSmartConnect::_sendDeviceData() {
  if (!_socketConnected)
    return;
  bool ok = _wsSendText("42[\"device-data\"," + _buildDeviceDataJson() + "]");
  if (ok)
    _hardwareSent = true; // only mark sent when TCP write succeeded
}

String TmtSmartConnect::_buildDeviceDataJson() {
  String j = "{";
  j += "\"deviceId\":\"";
  j += _apiKey;
  j += "\",";
  j += "\"deviceType\":\"tmt-custom\",";
  j += "\"message\":\"device-data\",";
  j += "\"data\":{";

  if (!_hardwareSent) {
    j += "\"deviceHardware\":{\"channelCount\":";
    j += TMT_MAX_CHANNELS;
    j += "},";
  }

  j += "\"deviceStatus\":{";
  for (uint8_t i = 0; i < TMT_MAX_CHANNELS; i++) {
    if (i > 0)
      j += ",";
    j += "\"V";
    j += i;
    j += "\":";
    j += _channelStates[i] ? "true" : "false";
  }
  j += "}}}";
  return j;
}

// ─── JSON field extractors
// ────────────────────────────────────────────────────

String TmtSmartConnect::_extractStringField(const char *json, size_t len,
                                            const char *key) {
  String search = "\"";
  search += key;
  search += "\":\"";
  const char *p = strstr(json, search.c_str());
  if (!p)
    return "";
  p += search.length();
  const char *end = strchr(p, '"');
  if (!end || end >= json + len)
    return "";
  return String(p).substring(0, end - p);
}

bool TmtSmartConnect::_extractBoolField(const char *json, size_t len,
                                        const char *key) {
  String search = "\"";
  search += key;
  search += "\":";
  const char *p = strstr(json, search.c_str());
  if (!p)
    return false;
  p += search.length();
  while (*p == ' ')
    p++;
  return strncmp(p, "true", 4) == 0;
}

// ─── Base64 ──────────────────────────────────────────────────────────────────

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String TmtSmartConnect::_base64(const uint8_t *data, size_t len) {
  String out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = ((uint32_t)data[i] << 16) |
                 (i + 1 < len ? (uint32_t)data[i + 1] << 8 : 0) |
                 (i + 2 < len ? (uint32_t)data[i + 2] : 0);
    out += B64[(v >> 18) & 0x3F];
    out += B64[(v >> 12) & 0x3F];
    out += (i + 1 < len) ? B64[(v >> 6) & 0x3F] : '=';
    out += (i + 2 < len) ? B64[v & 0x3F] : '=';
  }
  return out;
}
