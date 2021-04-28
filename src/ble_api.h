#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED

#ifndef MAX_CLIENT_CONNECTIONS
#define MAX_CLIENT_CONNECTIONS 5
#endif

#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS MAX_CLIENT_CONNECTIONS

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_bt_defs.h>
#include <functional>
#include "util.h"

class myAdvertisedDeviceCallbacks;
class myClientCallbacks;

typedef std::array<uint8_t, ESP_BD_ADDR_LEN> BLEPeripheralID;
typedef std::function<void(NimBLEAdvertisedDevice *advertisedDevice, BLEPeripheralID id)> BLEDeviceFound;
typedef std::function<void(BLEPeripheralID id)> BLEDeviceEvent;
typedef std::function<void(BLEPeripheralID id, std::string service, std::string characteristic, std::string data, bool isNotify)> BLECharacteristicNotification;
struct BLEConnection
{
  BLEPeripheralID id;
  NimBLEClient *device;
};

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0, bool active = true);
  static bool stopScan();
  static void onDeviceFound(BLEDeviceFound cb);
  static void onDeviceConnected(BLEDeviceEvent cb);
  static void onDeviceDisconnected(BLEDeviceEvent cb);
  static void onCharacteristicNotification(BLECharacteristicNotification cb);
  static bool connect(BLEPeripheralID);
  static bool disconnect(BLEPeripheralID);
  static std::vector<NimBLERemoteService *> *discoverServices(BLEPeripheralID id);
  static std::vector<NimBLERemoteCharacteristic *> *discoverCharacteristics(BLEPeripheralID id, std::string service);
  static std::string readCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic);
  static bool notifyCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, bool notify = true);
  static bool writeCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, uint8_t *data, size_t length, bool withoutResponse = true);
  static BLEPeripheralID idFromAddress(NimBLEAddress address);
  static NimBLEAddress addressFromId(BLEPeripheralID id);
  static std::string idToString(BLEPeripheralID id);
  static BLEPeripheralID idFromString(const char *idStr);

private:
  friend class myAdvertisedDeviceCallbacks;
  friend class myClientCallbacks;
  static bool _isReady;
  static bool _isScanning;
  static NimBLEAdvertisedDeviceCallbacks *_advertisedDeviceCallback;
  static NimBLEClientCallbacks *_clientCallback;
  static NimBLEScan *bleScan;
  static std::map<BLEPeripheralID, uint8_t> addressTypes;
  static void _onScanFinished(NimBLEScanResults results);
  static void _onCharacteristicNotification(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify);
  static void _onDeviceFoundProxy(NimBLEAdvertisedDevice *advertisedDevice);
  static void _onDeviceInteractionProxy(BLEPeripheralID id, bool connected);
  static BLEDeviceFound _cbOnDeviceFound;
  static BLEDeviceEvent _cbOnDeviceConnected;
  static BLEDeviceEvent _cbOnDeviceDisconnected;
  static BLECharacteristicNotification _cbOnCharacteristicNotification;
  static BLEConnection connections[MAX_CLIENT_CONNECTIONS];
  static uint8_t activeConnections;
  static bool addConnection(BLEPeripheralID id, NimBLEClient *device);
  static NimBLEClient *getConnection(BLEPeripheralID id);
  static void delConnection(BLEPeripheralID id);
};

#endif