#ifndef TMT_SMART_CONNECT_H
#define TMT_SMART_CONNECT_H

#if !defined(ESP8266) && !defined(ESP32)
#error "TMT_SmartConnect requires ESP8266 or ESP32"
#endif

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#define TMT_SMART_CONNECT_DEFAULT_PORT 8443 // port for ESP8266 BearSSL TLS 1.2
#else
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define TMT_SMART_CONNECT_DEFAULT_PORT 443
#endif

// Virtual channel shortcuts
#define V0 0
#define V1 1
#define V2 2
#define V3 3
#define V4 4
#define V5 5
#define V6 6
#define V7 7

#define TMT_MAX_CHANNELS 8
#define TMT_RX_BUF_SIZE 1024
#define TMT_CHANNEL_STR_LEN 32

typedef void (*VirtualWriteCallback)(bool value);
typedef void (*ConnectCallback)();
typedef void (*DisconnectCallback)();

enum TmtChannelType { TMT_BUTTON = 0, TMT_STRING = 1, TMT_NUMBER = 2 };

union TmtChannelValue {
  bool b;
  char s[TMT_CHANNEL_STR_LEN];
  float n;
};

struct TmtChannel {
  TmtChannelType type;
  TmtChannelValue value;
};

class TmtSmartConnect {
public:
  TmtSmartConnect();

#define TMT_SMART_CONNECT_DEFAULT_HOST "smartconnect.tmtelectronic.com"

  // Start connection to server. Must be called after WiFi is connected.
  // apiKey: TMTK_xxx key from TMT SmartConnect admin panel
  void begin(const char *apiKey,
             const char *host = TMT_SMART_CONNECT_DEFAULT_HOST,
             uint16_t port = TMT_SMART_CONNECT_DEFAULT_PORT);

  // Register callback for incoming commands on a channel (button channels only)
  void onWrite(uint8_t channel, VirtualWriteCallback callback);

  // Read / write channel state — button (boolean)
  bool virtualRead(uint8_t channel) const;
  void virtualWrite(uint8_t channel, bool value);
  void virtualWriteButton(uint8_t channel,
                          bool value); // alias for virtualWrite

  // Read / write channel state — string (read-only sensor value)
  const char *virtualReadString(uint8_t channel) const;
  void virtualWriteString(uint8_t channel, const char *value);

  // Read / write channel state — number (read-only sensor value)
  float virtualReadNumber(uint8_t channel) const;
  void virtualWriteNumber(uint8_t channel, float value);

  // Optional connect / disconnect callbacks
  void onConnect(ConnectCallback cb) { _onConnect = cb; }
  void onDisconnect(DisconnectCallback cb) { _onDisconnect = cb; }

  // Must be called in loop()
  void loop();

  bool isConnected() const { return _socketConnected; }
  int reconnectCount() const { return _reconnectCount; }

private:
  // Credentials
  char _apiKey[64];
  char _host[128];
  uint16_t _port;

#ifdef ESP8266
  BearSSL::WiFiClientSecure _client;
#else
  WiFiClientSecure _client;
#endif

  // State
  bool _tcpConnected;
  bool _wsUpgraded;
  bool _socketConnected;
  bool _hardwareSent;

  // Timing
  unsigned long _lastHeartbeatRx; // last time we received any server message
  unsigned long _lastHeartbeatTx; // last time we sent heartbeat-pong
  unsigned long _reconnectAt;
  unsigned long _reconnectInterval; // exponential backoff, 5s→30s
  int _reconnectCount;

  static const unsigned long HEARTBEAT_TX_INTERVAL =
      20000; // send heartbeat-pong every 20s
  static const unsigned long HEARTBEAT_RX_TIMEOUT = 35000; // dead if silent 35s
  static const unsigned long RECONNECT_MIN = 5000;
  static const unsigned long RECONNECT_MAX = 30000;
  static const int RECONNECT_BACKOFF_STEP = 5; // every 5 attempts +2s

  // Channels
  TmtChannel _channels[TMT_MAX_CHANNELS];
  VirtualWriteCallback _callbacks[TMT_MAX_CHANNELS];

  // User callbacks
  ConnectCallback _onConnect;
  DisconnectCallback _onDisconnect;

  // RX buffer
  char _rxBuf[TMT_RX_BUF_SIZE];

  // Internals
  bool _connectTcp();
  bool _doWsHandshake();
  void _disconnect(bool scheduleReconnect = true);
  bool _isConnectionHealthy();

  bool _wsSendText(const char *text, size_t len);
  bool _wsSendText(const String &s) {
    return _wsSendText(s.c_str(), s.length());
  }
  int _wsReadFrame();

  void _handleRawMessage(const char *msg, size_t len);
  void _handleSioEvent(const char *json, size_t len);
  void _handleClientStatus(const char *json, size_t len);

  void _sendDeviceData();
  String _buildDeviceDataJson();
  String _extractStringField(const char *json, size_t len, const char *key);
  bool _extractBoolField(const char *json, size_t len, const char *key);
  String _base64(const uint8_t *data, size_t len);
};

#endif
