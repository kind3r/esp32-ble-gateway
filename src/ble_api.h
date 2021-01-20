#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#include <Arduino.h>
#include <BLEScan.h>
#include <functional>
#include "util.h"

#define DEFAULT_SCAN_DURATION 10

class myAdvertisedDeviceCallbacks;
class myClientCallbacks;

typedef std::function<void(BLEAdvertisedDevice advertisedDevice, std::string id)> BLEDeviceFound;
typedef std::function<void(std::string id)> BLEDeviceEvent;
typedef std::function<void(std::string id, std::string service, std::string characteristic, std::string data, bool isNotify)> BLECharacteristicNotification;

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
  static bool connect(std::string id);
  static bool disconnect(std::string id);
  static std::map<std::string, BLERemoteService *> *discoverServices(std::string id);
  static std::map<std::string, BLERemoteCharacteristic *> *discoverCharacteristics(std::string id, std::string service);
  static std::string readCharacteristic(std::string id, std::string service, std::string characteristic);
  static bool notifyCharacteristic(std::string id, std::string service, std::string characteristic, bool notify = true);
  static std::string idFromAddress(BLEAddress address);
  static BLEAddress addressFromId(std::string id);


private:
  friend class myAdvertisedDeviceCallbacks;
  friend class myClientCallbacks;
  static bool _isReady;
  static bool _isScanning;
  static bool _scanMustStop;
  static BLEAdvertisedDeviceCallbacks *_advertisedDeviceCallback;
  static BLEClientCallbacks *_clientCallback;
  static BLEScan *bleScan;
  static std::map<std::string, esp_ble_addr_type_t> addressTypes;
  static std::map<std::string, BLEClient *> connections;
  static void _onScanFinished(BLEScanResults results);
  static void _onCharacteristicNotification(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify);
  static void _onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice);
  static void _onDeviceInteractionProxy(std::string id, bool connected);
  static BLEDeviceFound _cbOnDeviceFound;
  static BLEDeviceEvent _cbOnDeviceConnected;
  static BLEDeviceEvent _cbOnDeviceDisconnected;
  static BLECharacteristicNotification _cbOnCharacteristicNotification;
};

#endif