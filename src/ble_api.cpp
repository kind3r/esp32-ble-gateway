#include "ble_api.h"
#include <BLEDevice.h>
#include <freertos/FreeRTOS.h>

namespace std
{
  template <>
  struct less<BLEApiAddress>
  {
    bool operator()(const BLEApiAddress &lhs, const BLEApiAddress &rhs) const
    {
      return lhs.address < rhs.address;
    }
  };
} // namespace std

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
bool BLEApi::_scanMustStop = false;
BLEScan *BLEApi::bleScan = nullptr;
BLEDeviceFound BLEApi::_cbOnDeviceFound = nullptr;
BLEAdvertisedDeviceCallbacks *BLEApi::_advertisedDeviceCallback = nullptr;
std::map<BLEApiAddress, esp_ble_addr_type_t> BLEApi::addressTypes;
std::map<std::string, BLEClient *> BLEApi::connections;

class myAdvertisedDeviceCallback : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    BLEApi::_onDeviceFoundProxy(advertisedDevice);
  }
};

/**
 * Initialize BLE API
 */
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
    bleScan->setWindow(650);    // 449
    bleScan->setActiveScan(true);
    _isReady = true;
  }
}

/**
 * If the API is ready
 */
bool BLEApi::isReady()
{
  return _isReady;
}

/**
 * Start scanning for BLE devices
 * @param duration of the scan in seconds. If 0, it will keep scanning until stopped 
 */
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

/**
 * Stop scanning for BLE devices
 */
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

/**
 * Set a callback for when a device is found
 */
void BLEApi::onDeviceFound(BLEDeviceFound cb)
{
  _cbOnDeviceFound = cb;
}

/**
 * Connect to a device
 * @param address device address
 */
bool BLEApi::connect(std::string id)
{
  BLEApi::stopScan();

  // get MAC address from id
  BLEAddress address = addressFromId(id);
  // get MAC address type
  BLEApiAddress a;
  memcpy(a.address, address.getNative(), ESP_BD_ADDR_LEN);
  esp_ble_addr_type_t addressType = addressTypes[a];

  BLEClient *peripheral = BLEDevice::createClient();
  bool connected = false;
  int8_t retry = 5;
  do
  {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // Serial.println("Connect attempt start");
    connected = peripheral->connect(address, addressType);
    // Serial.println("Connect attempt ended");
    retry--;
  } while (!connected && retry > 0);
  if (connected)
  {
    Serial.printf("Connected to [%s][%d]\n", address.toString().c_str(), retry);
    connections[id] = peripheral;
  }
  else
  {
    Serial.printf("Could not connect to [%s][%d]\n", address.toString().c_str(), retry);
  }
  return connected;
}

bool BLEApi::disconnect(std::string id)
{
  BLEClient *peripheral = connections[id];
  if (peripheral)
  {
    if (peripheral->isConnected())
    {
      peripheral->disconnect();
      connections.erase(id);
      free(peripheral);
    }
  }
  return true;
}

std::map<std::string, BLERemoteService *> *BLEApi::discoverServices(std::string id)
{
  BLEClient *peripheral = connections[id];
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return nullptr;
    }
    std::map<std::string, BLERemoteService *> *services;
    services = peripheral->getServices();
    return services;
  }
  return nullptr;
}

std::map<std::string, BLERemoteCharacteristic *> *BLEApi::discoverCharacteristics(std::string id, std::string service)
{
  BLEClient *peripheral = connections[id];
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return nullptr;
    }
    BLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
    if (remoteService != nullptr)
    {
      std::map<std::string, BLERemoteCharacteristic *> *characteristics;
      characteristics = remoteService->getCharacteristics();
      return characteristics;
    }
  }
  return nullptr;
}

std::string BLEApi::readCharacteristic(std::string id, std::string service, std::string characteristic)
{
  BLEClient *peripheral = connections[id];
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return "";
    }
    BLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
    if (remoteService != nullptr)
    {
      BLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
      if (remoteCharacteristic->canRead())
      {
        return remoteCharacteristic->readValue();
      }
    }
  }
  return "";
}

/**
 * DO NOT USE: Proxy method for setting up the ESP32 BLEDevice callback
 */
void BLEApi::_onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice)
{
  BLEApiAddress a;
  memcpy(a.address, advertisedDevice.getAddress().getNative(), 6);
  addressTypes[a] = advertisedDevice.getAddressType();
  if (_cbOnDeviceFound)
  {
    _cbOnDeviceFound(advertisedDevice, idFromAddress(advertisedDevice.getAddress()));
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
    meminfo();
    Serial.println("BLE Scan restarted");
    // Because of stupid callback
    bleScan->start(DEFAULT_SCAN_DURATION, BLEApi::onScanFinished, true);
  }
}

std::string BLEApi::idFromAddress(BLEAddress address)
{
  std::string peripheralUuid = address.toString();
  // remove ':' from address
  peripheralUuid.erase(14, 1);
  peripheralUuid.erase(11, 1);
  peripheralUuid.erase(8, 1);
  peripheralUuid.erase(5, 1);
  peripheralUuid.erase(2, 1);
  return peripheralUuid;
}

BLEAddress BLEApi::addressFromId(std::string id)
{
  std::string addressStr = id;
  addressStr.insert(2, ":");
  addressStr.insert(5, ":");
  addressStr.insert(8, ":");
  addressStr.insert(11, ":");
  addressStr.insert(14, ":");
  return BLEAddress(addressStr);
}