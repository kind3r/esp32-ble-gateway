#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#include <Arduino.h>
#include <BLEScan.h>
#include <functional>
#include "util.h"

#define DEFAULT_SCAN_DURATION 10

class myAdvertisedDeviceCallbacks;
class myClientCallbacks;

typedef std::array<uint8_t, ESP_BD_ADDR_LEN> BLEPeripheralID;
typedef std::function<void(BLEAdvertisedDevice advertisedDevice, BLEPeripheralID id)> BLEDeviceFound;
typedef std::function<void(BLEPeripheralID id)> BLEDeviceEvent;
typedef std::function<void(BLEPeripheralID id, std::string service, std::string characteristic, std::string data, bool isNotify)> BLECharacteristicNotification;

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0);
  static bool stopScan();
  static void onDeviceFound(BLEDeviceFound cb);
  static void onDeviceConnected(BLEDeviceEvent cb);
  static void onDeviceDisconnected(BLEDeviceEvent cb);
  static void onCharacteristicNotification(BLECharacteristicNotification cb);
  static bool connect(BLEPeripheralID);
  static bool disconnect(BLEPeripheralID);
  static std::map<std::string, BLERemoteService *> *discoverServices(BLEPeripheralID id);
  static std::map<std::string, BLERemoteCharacteristic *> *discoverCharacteristics(BLEPeripheralID id, std::string service);
  static std::string readCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic);
  static bool notifyCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, bool notify = true);
  static bool writeCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, uint8_t *data, size_t length, bool withoutResponse = true);
  static BLEPeripheralID idFromAddress(BLEAddress address);
  static BLEAddress addressFromId(BLEPeripheralID id);
  static std::string idToString(BLEPeripheralID id);

private:
  friend class myAdvertisedDeviceCallbacks;
  friend class myClientCallbacks;
  static bool _isReady;
  static bool _isScanning;
  static bool _scanMustStop;
  static BLEAdvertisedDeviceCallbacks *_advertisedDeviceCallback;
  static BLEClientCallbacks *_clientCallback;
  static BLEScan *bleScan;
  static std::map<BLEPeripheralID, esp_ble_addr_type_t> addressTypes;
  static std::map<BLEPeripheralID, BLEClient *> connections;
  static void _onScanFinished(BLEScanResults results);
  static void _onCharacteristicNotification(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify);
  static void _onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice);
  static void _onDeviceInteractionProxy(BLEPeripheralID id, bool connected);
  static BLEDeviceFound _cbOnDeviceFound;
  static BLEDeviceEvent _cbOnDeviceConnected;
  static BLEDeviceEvent _cbOnDeviceDisconnected;
  static BLECharacteristicNotification _cbOnCharacteristicNotification;
};

#endif