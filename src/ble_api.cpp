#include "ble_api.h"

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
bool BLEApi::_scanMustStop = false;
BLEScan *BLEApi::bleScan = nullptr;

void BLEApi::init()
{
  BLEDevice::init("ESP32BLEGW");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks((BLEAdvertisedDeviceCallbacks *)BLEApi::onResult);
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
    return true;
  }
  return false;
}

void BLEApi::onResult(BLEAdvertisedDevice advertisedDevice)
{
  Serial.print("BLE Advertised Device found: ");
  Serial.println(advertisedDevice.toString().c_str());
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