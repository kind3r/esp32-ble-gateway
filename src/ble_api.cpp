#include "ble_api.h"
#include <BLEDevice.h>

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
bool BLEApi::_scanMustStop = false;
BLEScan *BLEApi::bleScan = nullptr;
BLEDeviceFound BLEApi::_cbOnDeviceFound = nullptr;
BLEAdvertisedDeviceCallbacks *BLEApi::_advertisedDeviceCallback = nullptr;

class myAdvertisedDeviceCallback : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    BLEApi::_onDeviceFoundProxy(advertisedDevice);
  }
};

void BLEApi::init()
{
  if (!_isReady)
  {
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    BLEDevice::init("ESP32BLEGW");
    bleScan = BLEDevice::getScan();
    _advertisedDeviceCallback = new myAdvertisedDeviceCallback();
    bleScan->setAdvertisedDeviceCallbacks(BLEApi::_advertisedDeviceCallback);
    bleScan->setInterval(1250); // 1349
    bleScan->setWindow(650); // 449
    bleScan->setActiveScan(true);
    _isReady = true;
  }
}

bool BLEApi::isReady()
{
  return _isReady;
}

bool BLEApi::startScan(uint32_t duration)
{
  if (!_isReady || _isScanning)
  {
    return false;
  }
  _isScanning = true;
  if (duration == 0)
  {
    duration = DEFAULT_SCAN_DURATION;
    _scanMustStop = false;
  }
  else
  {
    _scanMustStop = true;
  }
  Serial.println("BLE Scan started");
  bleScan->start(duration, BLEApi::onScanFinished, true);
  return true;
}

bool BLEApi::stopScan()
{
  if (_isReady && _isScanning)
  {
    _scanMustStop = true;
    bleScan->stop(); // this does not call the callback onScanFinished
    bleScan->clearResults();
    _isScanning = false;
    Serial.println("BLE Scan stopped on demand");
    return true;
  }
  return false;
}

void BLEApi::connect(const char *peripheralUuid) {
  BLEApi::stopScan();
  BLEClient *peripheral = BLEDevice::createClient();
  BLEAddress address = BLEAddress(peripheralUuid);
  bool connected = peripheral->connect(address);
  if (connected) {
    Serial.printf("Connected to [%s]\n", peripheralUuid);
  } else {
    Serial.printf("Could not connect to [%s]\n", peripheralUuid);
  }
}

void BLEApi::onDeviceFound(BLEDeviceFound cb)
{
  _cbOnDeviceFound = cb;
}

void BLEApi::_onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice)
{
  if (_cbOnDeviceFound)
  {
    _cbOnDeviceFound(advertisedDevice);
  }
}

void BLEApi::onScanFinished(BLEScanResults results)
{
  if (_scanMustStop)
  {
    _isScanning = false;
    bleScan->clearResults();
    Serial.println("BLE Scan stopped finished");
  }
  else
  {
    Serial.println("BLE Scan restarted");
    // Because of stupid callback
    bleScan->start(DEFAULT_SCAN_DURATION, BLEApi::onScanFinished, true);
  }
}