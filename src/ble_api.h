#ifndef ESP_GW_BLE_API_H
#define ESP_GW_BLE_API_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <functional>

#define DEFAULT_SCAN_DURATION 10

class BLEApi
{
public:
  static void init();
  static bool isReady();
  static bool startScan(uint32_t duration = 0);
  static bool stopScan();

private:
  static bool _isReady;
  static bool _isScanning;
  static bool _scanMustStop;
  static BLEScan *bleScan;
  static void onResult(BLEAdvertisedDevice advertisedDevice);
  static void onScanFinished(BLEScanResults results);
};

#endif