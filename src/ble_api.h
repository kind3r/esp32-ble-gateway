#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#include <Arduino.h>
#include <BLEScan.h>
#include <functional>
#include "util.h"

#define DEFAULT_SCAN_DURATION 10

typedef std::function<void(BLEAdvertisedDevice advertisedDevice, std::string id)> BLEDeviceFound;

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0);
  static bool stopScan();
  static void onDeviceFound(BLEDeviceFound cb);
  static bool connect(std::string id);
  static bool disconnect(std::string id);
  static std::map<std::string, BLERemoteService*> *discoverServices(std::string id);
  static std::map<std::string, BLERemoteCharacteristic*> *discoverCharacteristics(std::string id, std::string service);
  static std::string readCharacteristic(std::string id, std::string service, std::string characteristic);
  static std::string idFromAddress(BLEAddress address);
  static BLEAddress addressFromId(std::string id);


  static void _onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice);
private:
  static bool _isReady;
  static bool _isScanning;
  static bool _scanMustStop;
  static BLEAdvertisedDeviceCallbacks *_advertisedDeviceCallback;
  static BLEScan *bleScan;
  static std::map<std::string, esp_ble_addr_type_t> addressTypes;
  static std::map<std::string, BLEClient*> connections;
  static void onScanFinished(BLEScanResults results);
  static BLEDeviceFound _cbOnDeviceFound;
};

#endif