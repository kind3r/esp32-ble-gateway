#ifndef ESP_GW_NOBLE_API_H
#define ESP_GW_NOBLE_API_H

#define LOG_LOCAL_LEVEL 5

#ifndef ESP_GW_WEBSOCKET_PORT
#define ESP_GW_WEBSOCKET_PORT 80
#endif

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
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
  static std::map<uint32_t, std::string> challenges;
  static std::map<std::string, uint8_t> peripheralConnections;

  static void clientDisconnectCleanup(uint8_t client);
  static bool clientCanConnect(uint8_t client, std::string peripheral);
  static bool clientConnected(uint8_t client, std::string peripheral);

  static void initClient(uint8_t client);
  static void checkAuth(uint8_t client, const char *response);
  static void sendJsonMessage(JsonDocument &command, const uint8_t client);
  static void sendJsonMessage(JsonDocument &command);
  static void sendAuthMessage(const uint8_t client);
  static void sendState(const uint8_t client);
  static void sendConnected(const uint8_t client, std::string id);
  static void sendDisconnected(const uint8_t client, std::string id);
  static void sendDisconnected(const uint8_t client, std::string id, std::string reason);
  static void sendServices(const uint8_t client, std::string id, std::map<std::string, BLERemoteService *> *services);
  static void sendCharacteristics(const uint8_t client, std::string id, std::string service, std::map<std::string, BLERemoteCharacteristic*> *characteristics);
  static void sendCharacteristicValue(const uint8_t client, std::string id, std::string service, std::string characteristic, std::string value);
  static void sendCharacteristicValue(const uint8_t client, std::string id, std::string service, std::string characteristic, std::string value, bool isNotification);
  static void onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length);
  static void onBLEDeviceFound(BLEAdvertisedDevice advertisedDevice, std::string id);
};

#endif