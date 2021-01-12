#include "ble_api.h"
#include <BLEDevice.h>

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
bool BLEApi::_scanMustStop = false;
BLEScan *BLEApi::bleScan = nullptr;

class myAdvertisedDeviceCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    BLEApi::onDeviceFound(advertisedDevice);
  }
};
BLEAdvertisedDeviceCallbacks *BLEApi::_advertisedDeviceCallback = new myAdvertisedDeviceCallback();

void BLEApi::init()
{
  BLEDevice::init("ESP32BLEGW");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(BLEApi::_advertisedDeviceCallback);
  bleScan->setInterval(1349);
  bleScan->setWindow(449);
  bleScan->setActiveScan(true);
  _isReady = true;
}

bool BLEApi::isReady() {
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
    _isScanning = false;
    Serial.println("BLE Scan stopped");
    return true;
  }
  return false;
}

void BLEApi::onDeviceFound(BLEAdvertisedDevice advertisedDevice)
{
  Serial.print("BLE found: ");
  Serial.println(advertisedDevice.toString().c_str());
  // std::string name = "";
  // std::string address = advertisedDevice.getAddress().toString();
  // std::string manufacturerData = "";
  // if (advertisedDevice.haveManufacturerData()) {
  //   manufacturerData = advertisedDevice.getManufacturerData();
  // }


}

void BLEApi::onScanFinished(BLEScanResults results)
{
  if (_scanMustStop)
  {
    _isScanning = false;
    Serial.println("BLE Scan stopped");
  }
  else
  {
    Serial.println("BLE Scan restarted");
    // Because of stupid callback
    bleScan->start(DEFAULT_SCAN_DURATION, BLEApi::onScanFinished, false);
  }
}