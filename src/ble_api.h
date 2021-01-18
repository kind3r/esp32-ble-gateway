#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#include <Arduino.h>
#include <BLEScan.h>
#include <functional>
#include "util.h"

#define DEFAULT_SCAN_DURATION 10

typedef std::function<void(BLEAdvertisedDevice advertisedDevice)> BLEDeviceFound;

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0);
  static bool stopScan();
  static void connect(const char *peripheralUuid);
  static void onDeviceFound(BLEDeviceFound cb);

  static void _onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice);
private:
  static bool _isReady;
  static bool _isScanning;
  static bool _scanMustStop;
  static BLEAdvertisedDeviceCallbacks *_advertisedDeviceCallback;
  static BLEScan *bleScan;
  static void onScanFinished(BLEScanResults results);
  static BLEDeviceFound _cbOnDeviceFound;
};

#endif