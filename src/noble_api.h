#ifndef ESP_GW_NOBLE_API_H
#define ESP_GW_NOBLE_API_H

#define LOG_LOCAL_LEVEL 5

#ifndef ESP_GW_WEBSOCKET_PORT
#define ESP_GW_WEBSOCKET_PORT 80
#endif

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <map>
#include "config.h"
#include "security.h"
#include "ble_api.h"

class NobleApi
{
public:
  static void init();
  static void loop();

private:
  static Security *sec;
  static WebSocketsServer *ws;
  static std::map<uint32_t, std::string> *challenges;

  static void initClient(uint8_t client);
  static void checkAuth(uint8_t client, const char *response);
  static void sendJsonMessage(uint8_t client, JsonDocument &command);
  static void sendJsonMessage(JsonDocument &command);
  static void sendAuthMessage(uint8_t client);
  static void sendState(uint8_t client);

  static void onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length);
  static void onBLEDeviceFound(BLEAdvertisedDevice advertisedDevice);
};

#endif