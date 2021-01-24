#include "ble_api.h"
#include <BLEDevice.h>
#include <freertos/FreeRTOS.h>

bool BLEApi::_isReady = false;
bool BLEApi::_isScanning = false;
bool BLEApi::_scanMustStop = false;
BLEScan *BLEApi::bleScan = nullptr;
BLEDeviceFound BLEApi::_cbOnDeviceFound = nullptr;
BLEDeviceEvent BLEApi::_cbOnDeviceConnected = nullptr;
BLEDeviceEvent BLEApi::_cbOnDeviceDisconnected = nullptr;
BLECharacteristicNotification BLEApi::_cbOnCharacteristicNotification = nullptr;
BLEAdvertisedDeviceCallbacks *BLEApi::_advertisedDeviceCallback = nullptr;
BLEClientCallbacks *BLEApi::_clientCallback = nullptr;
std::map<BLEPeripheralID, esp_ble_addr_type_t> BLEApi::addressTypes;
BLEConnection BLEApi::connections[MAX_CLIENT_CONNECTIONS];
uint8_t BLEApi::activeConnections = 0;

class myAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    BLEApi::_onDeviceFoundProxy(advertisedDevice);
  }
};

class myClientCallbacks : public BLEClientCallbacks
{
public:
  void onConnect(BLEClient *pClient)
  {
    BLEPeripheralID id = BLEApi::idFromAddress(pClient->getPeerAddress());
    Serial.printf("***** Connected to ==%s==\n", pClient->getPeerAddress().toString().c_str());
    BLEApi::_onDeviceInteractionProxy(id, true);
  }
  void onDisconnect(BLEClient *pClient)
  {
    BLEPeripheralID id = BLEApi::idFromAddress(pClient->getPeerAddress());
    Serial.printf("***** Disconnected from ==%s==\n", pClient->getPeerAddress().toString().c_str());
    BLEApi::_onDeviceInteractionProxy(id, false);
  }

private:
  std::string peripheralUuid;
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
    _advertisedDeviceCallback = new myAdvertisedDeviceCallbacks();
    bleScan->setAdvertisedDeviceCallbacks(_advertisedDeviceCallback);
    bleScan->setInterval(1250); // 1349
    bleScan->setWindow(650);    // 449
    bleScan->setActiveScan(true);
    _clientCallback = new myClientCallbacks();
    // TODO: maybe do some pre-descovery to get address types of devices around us
    // in case ESP was rebooted and clients try to connect before doing a scan
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
  bleScan->start(duration, _onScanFinished, true);
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
    vTaskDelay(100 / portTICK_PERIOD_MS);
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

void BLEApi::onDeviceConnected(BLEDeviceEvent cb)
{
  _cbOnDeviceConnected = cb;
}

void BLEApi::onDeviceDisconnected(BLEDeviceEvent cb)
{
  _cbOnDeviceDisconnected = cb;
}

void BLEApi::onCharacteristicNotification(BLECharacteristicNotification cb)
{
  _cbOnCharacteristicNotification = cb;
}

/**
 * Connect to a device
 * @param address device address
 */
bool BLEApi::connect(BLEPeripheralID id)
{
  BLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    return true;
  }
  BLEApi::stopScan();

  // get MAC address from id
  BLEAddress address = addressFromId(id);
  // get MAC address type
  esp_ble_addr_type_t addressType = addressTypes[id];

  bool connected = false;
  int8_t retry = 5;
  log_i("Connect attempt start");
  do
  {
    peripheral = BLEDevice::createClient();
    // TODO: sometimes the connect fails and remains hanging in the semaphore, patch BLE lib ?
    // ----------------------------
    // Connect attempt start
    // lld_pdu_get_tx_flush_nb HCI packet count mismatch (0, 1)
    // [E][BLEClient.cpp:214] gattClientEventHandler(): Failed to connect, status=Unknown ESP_ERR error
    // Retry connection in 1s
    // ----------------------------
    connected = peripheral->connect(address, addressType);
    // Serial.println("Connect attempt ended");
    retry--;
    if (connected)
    {
      peripheral->setClientCallbacks(_clientCallback);
    }
    else
    {
      log_d("Removing peripheral");
      delete peripheral;
      if (retry > 0)
      {
        log_i("Retry connection in 1s");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    }
  } while (!connected && retry > 0);
  if (connected)
  {
    log_i("Connected to [%s][%d]\n", address.toString().c_str(), retry);
    addConnection(id, peripheral);
  }
  else
  {
    log_e("Could not connect to [%s][%d]\n", address.toString().c_str(), retry);
  }
  return connected;
}

bool BLEApi::disconnect(BLEPeripheralID id)
{

  BLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      meminfo();
      peripheral->disconnect();
      // vTaskDelay(3000 / portTICK_PERIOD_MS);
      // delete peripheral;
      // meminfo();
    }
  }
  return true;
}

std::map<std::string, BLERemoteService *> *BLEApi::discoverServices(BLEPeripheralID id)
{
  BLEClient *peripheral = getConnection(id);
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

std::map<std::string, BLERemoteCharacteristic *> *BLEApi::discoverCharacteristics(BLEPeripheralID id, std::string service)
{
  BLEClient *peripheral = getConnection(id);
  if (peripheral)
  {
    if (!peripheral->isConnected())
    {
      return nullptr;
    }
    BLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
    if (remoteService != nullptr)
    {
      return remoteService->getCharacteristics();
      // std::map<std::string, BLERemoteCharacteristic *> *characteristics;
      // characteristics = remoteService->getCharacteristics();
      // return characteristics;
    }
  }
  return nullptr;
}

std::string BLEApi::readCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic)
{
  BLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
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

bool BLEApi::notifyCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, bool notify)
{
  BLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      BLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
      if (remoteService != nullptr)
      {
        BLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
        if (remoteCharacteristic->canNotify())
        {
          if (notify)
          {
            remoteCharacteristic->registerForNotify(_onCharacteristicNotification);
          }
          else
          {
            remoteCharacteristic->registerForNotify(nullptr);
          }
          return true;
        }
      }
    }
  }
  return false;
}

bool BLEApi::writeCharacteristic(BLEPeripheralID id, std::string service, std::string characteristic, uint8_t *data, size_t length, bool withoutResponse)
{
  BLEClient *peripheral = getConnection(id);
  if (peripheral != nullptr)
  {
    if (peripheral->isConnected())
    {
      BLERemoteService *remoteService = peripheral->getService(BLEUUID(service));
      if (remoteService != nullptr)
      {
        BLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(BLEUUID(characteristic));
        if (remoteCharacteristic->canWrite() || remoteCharacteristic->canWriteNoResponse())
        {
          remoteCharacteristic->writeValue(data, length, !withoutResponse);
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * DO NOT USE: Proxy method for setting up the ESP32 BLEDevice callback
 */
void BLEApi::_onDeviceFoundProxy(BLEAdvertisedDevice advertisedDevice)
{
  addressTypes[idFromAddress(advertisedDevice.getAddress())] = advertisedDevice.getAddressType();
  if (_cbOnDeviceFound)
  {
    _cbOnDeviceFound(advertisedDevice, idFromAddress(advertisedDevice.getAddress()));
  }
}

/**
 * DO NOT USE: Proxy method for setting up the ESP32 BLEDevice connect and disconnect events
 */
void BLEApi::_onDeviceInteractionProxy(BLEPeripheralID id, bool connected)
{
  if (connected)
  {
    Serial.println("***** Connect ACK");
    if (_cbOnDeviceConnected != nullptr)
    {
      _cbOnDeviceConnected(id);
    }
  }
  else
  {
    Serial.println("***** Disconnect ACK");
    BLEClient *peripheral = getConnection(id);
    if (peripheral != nullptr)
    {
      peripheral->setClientCallbacks(nullptr);
      delConnection(id);
      if (_cbOnDeviceDisconnected != nullptr)
      {
        _cbOnDeviceDisconnected(id);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      Serial.println("Dealocating memory");
      delete peripheral;
      meminfo();
    }
  }
}

void BLEApi::_onScanFinished(BLEScanResults results)
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
    bleScan->start(DEFAULT_SCAN_DURATION, _onScanFinished, true);
  }
}

void BLEApi::_onCharacteristicNotification(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
{
  if (_cbOnCharacteristicNotification != nullptr)
  {
    // patch required, see https://github.com/espressif/arduino-esp32/issues/3367
    BLERemoteService *service = characteristic->getRemoteService();
    BLEClient *client = service->getClient();
    std::string dataStr = std::string((char *)data, length);
    _cbOnCharacteristicNotification(
        idFromAddress(client->getPeerAddress()),
        service->getUUID().toString(),
        characteristic->getUUID().toString(),
        dataStr,
        isNotify);
  }
}

bool BLEApi::addConnection(BLEPeripheralID id, BLEClient *device)
{
  if (activeConnections < MAX_CLIENT_CONNECTIONS)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].device == nullptr)
      {
        connections[i].device = device;
        connections[i].id = id;
        activeConnections++;
        return true;
      }
    }
  }
  return false;
}

BLEClient *BLEApi::getConnection(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].id == id)
      {
        return connections[i].device;
      }
    }
  }
  return nullptr;
}

void BLEApi::delConnection(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (connections[i].id == id)
      {
        connections[i].device = nullptr;
        activeConnections--;
      }
    }
  }
}

BLEPeripheralID BLEApi::idFromAddress(BLEAddress address)
{
  BLEPeripheralID peripheralUuid;
  memcpy(peripheralUuid.data(), address.getNative(), ESP_BD_ADDR_LEN);
  return peripheralUuid;
}

BLEAddress BLEApi::addressFromId(BLEPeripheralID id)
{
  return BLEAddress(id.data());
}

std::string BLEApi::idToString(BLEPeripheralID id)
{
  auto size = ESP_BD_ADDR_LEN * 2 + 1;
  char *res = (char *)malloc(size);
  snprintf(res, size, "%02x%02x%02x%02x%02x%02x", id[0], id[1], id[2], id[3], id[4], id[5]);
  std::string ret(res);
  free(res);
  return ret;
}