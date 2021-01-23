#ifndef ESP_GW_NOBLE_API_H
#define ESP_GW_NOBLE_API_H

#ifndef ESP_GW_WEBSOCKET_PORT
#define ESP_GW_WEBSOCKET_PORT 80
#endif

#define INVALID_CLIENT 255

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include "config.h"
#include "security.h"
#include "ble_api.h"

// typedef std::map<BLEPeripheralID, uint8_t> PeripheralClient;
struct PeripheralClient {
  BLEPeripheralID id;
  uint8_t client;
};

class NobleApi
{
public:
  static void init();
  static void loop();

private:
  static Security *sec;
  static WebSocketsServer *ws;
  static std::map<uint32_t, std::string> challenges;

  static void clientDisconnectCleanup(uint8_t client);
  static bool clientCanConnect(uint8_t client, BLEPeripheralID id);
  static bool clientConnected(uint8_t client, BLEPeripheralID id);

  static void initClient(uint8_t client);
  static void checkAuth(uint8_t client, const char *response);
  static void sendJsonMessage(JsonDocument &command, const uint8_t client);
  static void sendJsonMessage(JsonDocument &command);
  static void sendAuthMessage(const uint8_t client);
  static void sendState(const uint8_t client);
  static void sendConnected(const uint8_t client, BLEPeripheralID id);
  static void sendDisconnected(const uint8_t client, BLEPeripheralID id);
  static void sendDisconnected(const uint8_t client, BLEPeripheralID id, std::string reason);
  static void sendServices(const uint8_t client, BLEPeripheralID id, std::map<std::string, BLERemoteService *> *services);
  static void sendCharacteristics(const uint8_t client, BLEPeripheralID id, std::string service, std::map<std::string, BLERemoteCharacteristic *> *characteristics);
  static void sendCharacteristicValue(const uint8_t client, BLEPeripheralID id, std::string service, std::string characteristic, std::string value, bool isNotification = false);
  static void sendCharacteristicNotification(const uint8_t client, BLEPeripheralID id, std::string service, std::string characteristic, bool state);
  static void sendCharacteristicWrite(const uint8_t client, BLEPeripheralID id, std::string service, std::string characteristic);
  static void onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length);
  static void onBLEDeviceFound(BLEAdvertisedDevice advertisedDevice, BLEPeripheralID id);
  static void onBLEDeviceDisconnected(BLEPeripheralID id);
  static void onCharacteristicNotification(BLEPeripheralID id, std::string service, std::string characteristic, std::string data, bool isNotify);

  static PeripheralClient peripheralConnections[MAX_CLIENT_CONNECTIONS];
  static uint8_t activeConnections;
  static bool addClient(BLEPeripheralID id, uint8_t client);
  static uint8_t getClient(BLEPeripheralID id);
  static void delClient(BLEPeripheralID id);
};

#endif