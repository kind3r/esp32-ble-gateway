#include "noble_api.h"

bool NobleApi::ready = false;
Security *NobleApi::sec = nullptr;
WebSocketsServer *NobleApi::ws = nullptr;
Challenge NobleApi::challenges[WEBSOCKETS_SERVER_CLIENT_MAX];
PeripheralClient NobleApi::peripheralConnections[MAX_CLIENT_CONNECTIONS];
uint8_t NobleApi::activeConnections = 0;

bool isEmptyChallenge(Challenge challenge)
{
  for (auto i = 0; i < BLOCK_SIZE; i++)
  {
    if (challenge[i] != 0x00)
    {
      return false;
    }
  }
  return true;
}

void clearChallenge(Challenge challenge)
{
  for (auto i = 0; i < BLOCK_SIZE; i++)
  {
    challenge[i] = 0x00;
  }
}
/**
   * Initialize API
   */
bool NobleApi::init()
{
  // initialize clients and challenges
  for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
  {
    peripheralConnections[i].client = INVALID_CLIENT;
  }
  for (auto i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++)
  {
    clearChallenge(challenges[i]);
  }

  // instantiate security module
  sec = new Security(GwSettings::getAes());

  // initilalize BLE
  BLEApi::init();
  BLEApi::onDeviceFound(onBLEDeviceFound);
  BLEApi::onDeviceDisconnected(onBLEDeviceDisconnected);
  BLEApi::onCharacteristicNotification(onCharacteristicNotification);

  // initialize websocket
  ws = new WebSocketsServer(ESP_GW_WEBSOCKET_PORT);
  ws->enableHeartbeat(30000, 5000, 3);
  ws->begin();
  ws->onEvent(onWsEvent);
  ready = true;

  return ready;
}

/**
 * Loop to process websocket
 */
void NobleApi::loop()
{
  if (ready)
  {
    // Process websocket events
    ws->loop();
    // TODO: disconnect clients that did not authenticate in a resonable timeframe
  }
}

/**
 * Cleanup after a client disconnects:
 * - disconnect connected devices
 * - remove client connection mappings 
 * - remove challenges
 */
void NobleApi::clientDisconnectCleanup(uint8_t client)
{
  // disconnect all assigned peripheralUuid
  for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
  {
    if (peripheralConnections[i].client == client)
    {
      BLEApi::disconnect(peripheralConnections[i].id);
      delClient(peripheralConnections[i].id);
    }
  }

  clearChallenge(challenges[client]);
}

/**
 * Can the client connect to device ?
 */
bool NobleApi::clientCanConnect(uint8_t client, BLEPeripheralID id)
{
  uint8_t connectedClient = getClient(id);
  if (connectedClient != INVALID_CLIENT && connectedClient != client)
  {
    return false;
  }
  return true;
}

/**
 * Is client connected to the device ?
 */
bool NobleApi::clientConnected(uint8_t client, BLEPeripheralID id)
{
  uint8_t connectedClient = getClient(id);
  if (connectedClient == client)
  {
    return true;
  }
  return false;
}

/**
 * Process websocket events
 */
void NobleApi::onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_DISCONNECTED)
  {
    clientDisconnectCleanup(client);
    Serial.printf("[%u] Disconnected!\n", client);
    if (ws->connectedClients() == 0)
    {
      BLEApi::stopScan();
      // just do a restart to cleanup everything for now
      // ESP.restart();
    }
    meminfo();
  }
  else if (type == WStype_CONNECTED)
  {
    meminfo();
    IPAddress ip = ws->remoteIP(client);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", client, ip[0], ip[1], ip[2], ip[3], payload);
    // Generate IV and send auth request
    initClient(client);
    sendAuthMessage(client);
  }
  else if (type == WStype_TEXT)
  {
    // Serial.printf("[%u] get  Text: %s\n", client, payload);

    StaticJsonDocument<1024> command;
    DeserializationError error = deserializeJson(command, payload, length);

    if (error != DeserializationError::Ok)
    { //Check for errors in parsing
      Serial.printf("Message parsing failed: ");
      Serial.println(error.f_str());
      return;
    }
    else
    {
      const char *action = command["action"];
      if (action && strlen(action) > 0)
      {
        bool authenticated = false;
        if (isEmptyChallenge(challenges[client]))
        {
          authenticated = true;
        }
        if (!authenticated)
        {
          // client is not authenticated, only respond to auth
          if (strcmp(action, "auth") == 0)
          {
            const char *response = command["response"];
            if (strlen(response) > 0)
            {
              checkAuth(client, response);
            }
          }
        }
        else
        {
          // client is authenticated, allow other commands

          // scan sfuff
          if (strcmp(action, "startScanning") == 0)
          {
            // TODO: if the scan is already running, send the list of discovered devices
            // TODO: setup (per connection ?) services filter

            // more or less a hack to allow for passive scanning as noble API does not have such parameter
            const bool active = command["allowDuplicates"];
            if (active) {
              BLEApi::startScan(0, false);
            } else {
              BLEApi::startScan(0, true);
            }
          }
          else if (strcmp(action, "stopScanning") == 0)
          {
            BLEApi::stopScan();
          }
          else
          {
            // stuff that requires periperalUuid
            const char *tempUuid = command["peripheralUuid"];
            if (tempUuid != nullptr)
            {
              BLEPeripheralID peripheralUuid = BLEApi::idFromString(tempUuid);

              // connection
              if (strcmp(action, "connect") == 0)
              {
                // check if peripheralUuid is not asigned to another client, asign client to periperhalUuid, check connection
                if (clientCanConnect(client, peripheralUuid))
                {
                  addClient(peripheralUuid, client);
                  // TODO: check if re-connection to peripheral is ok (in case client sends multiple connect but no disconnect)
                  bool connected = BLEApi::connect(peripheralUuid);
                  if (connected)
                  {
                    sendConnected(client, peripheralUuid);
                  }
                  else
                  {
                    delClient(peripheralUuid);
                    sendDisconnected(client, peripheralUuid, "failed");
                  }
                }
                else
                {
                  sendDisconnected(client, peripheralUuid, "denied");
                }
              }
              // actions that require a connection
              else if (clientConnected(client, peripheralUuid))
              {
                if (strcmp(action, "discoverServices") == 0)
                {
                  std::vector<NimBLERemoteService *> *services = BLEApi::discoverServices(peripheralUuid);
                  if (services != nullptr)
                  {
                    sendServices(client, peripheralUuid, services);
                  }
                }
                else if (strcmp(action, "discoverCharacteristics") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::vector<NimBLERemoteCharacteristic *> *characteristics = BLEApi::discoverCharacteristics(peripheralUuid, serviceUuid);
                  if (characteristics != nullptr)
                  {
                    sendCharacteristics(client, peripheralUuid, serviceUuid, characteristics);
                  }
                  else
                  {
                    delClient(peripheralUuid);
                    sendDisconnected(client, peripheralUuid, "aborted");
                  }
                }
                else if (strcmp(action, "read") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  std::string value = BLEApi::readCharacteristic(peripheralUuid, serviceUuid, characteristicUuid);
                  sendCharacteristicValue(client, peripheralUuid, serviceUuid, characteristicUuid, value);
                }
                else if (strcmp(action, "write") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  std::string dataHex = command["data"];
                  size_t length = dataHex.length() / 2;
                  uint8_t data[length];
                  sec->fromHex(dataHex.c_str(), length * 2, data);
                  bool withoutResponse = command["withoutResponse"];
                  if (BLEApi::writeCharacteristic(peripheralUuid, serviceUuid, characteristicUuid, data, length, withoutResponse))
                  { // success
                    sendCharacteristicWrite(client, peripheralUuid, serviceUuid, characteristicUuid);
                  }
                  else
                  { // failed
                    sendCharacteristicWrite(client, peripheralUuid, serviceUuid, characteristicUuid);
                  }
                }
                else if (strcmp(action, "notify") == 0)
                {
                  std::string serviceUuid = command["serviceUuid"];
                  std::string characteristicUuid = command["characteristicUuid"];
                  bool notify = command["notify"];
                  // subscribe or unsubscribe
                  BLEApi::notifyCharacteristic(peripheralUuid, serviceUuid, characteristicUuid, notify);
                  sendCharacteristicNotification(client, peripheralUuid, serviceUuid, characteristicUuid, notify);
                }
              }
              else
              {
                sendDisconnected(client, peripheralUuid, "not connected");
              }
            }
            else
            {
              // failed to read peripheralUuid
            }
          }
        }
      }
      command.clear();
    }
  }
  else if (type == WStype_PONG)
  {
    // do nothing
  }
  else
  {
    Serial.printf("Type not implemented [%u]\n", type);
  }
}

void NobleApi::onBLEDeviceFound(NimBLEAdvertisedDevice *advertisedDevice, BLEPeripheralID id)
{
  StaticJsonDocument<1024> command;
  command["type"] = "discover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["address"] = advertisedDevice->getAddress().toString();
  if (advertisedDevice->getAddressType() == BLE_ADDR_PUBLIC || advertisedDevice->getAddressType() == BLE_ADDR_PUBLIC_ID)
  {
    command["addressType"] = "public";
  }
  else if (advertisedDevice->getAddressType() == BLE_ADDR_RANDOM || advertisedDevice->getAddressType() == BLE_ADDR_RANDOM_ID)
  {
    command["addressType"] = "random";
  }
  else
  {
    command["addressType"] = "unknown";
  }
  command["connectable"] = "true";
  command["rssi"] = advertisedDevice->getRSSI();
  command["advertisement"]["localName"] = advertisedDevice->getName();
  if (advertisedDevice->haveTXPower())
  {
    command["advertisement"]["txPowerLevel"] = advertisedDevice->getTXPower();
  }
  if (advertisedDevice->haveServiceUUID())
  {
    JsonArray serviceUuids = command["advertisement"].createNestedArray("serviceUuids");
    serviceUuids.add(advertisedDevice->getServiceUUID().toString());
  }
  if (advertisedDevice->haveManufacturerData())
  {
    char manufacturerData[advertisedDevice->getManufacturerData().length() * 2 + 1];
    sec->toHex((uint8_t *)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length(), manufacturerData);
    manufacturerData[advertisedDevice->getManufacturerData().length() * 2 + 1] = '\0';
    command["advertisement"]["manufacturerData"] = manufacturerData;
  }

  sendJsonMessage(command);
  command.clear();
}

void NobleApi::onBLEDeviceDisconnected(BLEPeripheralID id)
{
  uint8_t client = getClient(id);
  if (client != INVALID_CLIENT)
  {
    sendDisconnected(client, id);
    delClient(id);
  }
}

void NobleApi::onCharacteristicNotification(BLEPeripheralID id, std::string service, std::string characteristic, std::string data, bool isNotify)
{
  uint8_t client = getClient(id);
  if (client != INVALID_CLIENT)
  {
    sendCharacteristicValue(
        client,
        id,
        service,
        characteristic,
        data,
        isNotify);
  }
}

void NobleApi::initClient(uint8_t client)
{
  sec->generateIV((uint8_t *)challenges[client]);
}

void NobleApi::checkAuth(uint8_t client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    if (!isEmptyChallenge(challenges[client]))
    {
      uint8_t encryptedResponse[responseLength / 2];
      size_t encryptedResponseLength = sec->fromHex(response, responseLength, encryptedResponse);

      uint8_t decryptedResponse[encryptedResponseLength + 1];
      size_t decryptedResponseLength = sec->decrypt((uint8_t *)challenges[client], encryptedResponse, encryptedResponseLength, decryptedResponse);
      decryptedResponse[encryptedResponseLength] = '\0';

      if (strcmp((char *)decryptedResponse, "admin:admin") == 0)
      {
        clearChallenge(challenges[client]);
        sendState(client);
      }
      else
      {
        Serial.println("Authentication failed");
        ESP_LOG_BUFFER_HEXDUMP("Decrypted", decryptedResponse, decryptedResponseLength + 1, esp_log_level_t::ESP_LOG_INFO);
        sendAuthMessage(client);
      }
    }
  }
}

void NobleApi::sendJsonMessage(JsonDocument &command, const uint8_t client)
{
  size_t messageLength = measureJson(command) + 1;
  // Serial.println(messageLength);
  char buffer[messageLength];
  // String buffer;
  serializeJson(command, buffer, messageLength);
  buffer[messageLength] = '\0';
  command.clear();
  // ESP_LOG_BUFFER_HEXDUMP("Send", buffer, messageLength, esp_log_level_t::ESP_LOG_INFO);
  // Serial.printf("[%u] sent Text: %s\n", client, buffer);
  ws->sendTXT(client, buffer);
  command.clear();
}

void NobleApi::sendJsonMessage(JsonDocument &command)
{
  size_t messageLength = measureJson(command) + 1;
  char buffer[messageLength];
  serializeJson(command, buffer, messageLength);
  buffer[messageLength] = '\0';
  command.clear();

  std::map<uint32_t, std::string>::iterator it;
  for (uint8_t client = 0; client < WEBSOCKETS_SERVER_CLIENT_MAX; client++)
  {
    if (ws->clientIsConnected(client))
    {
      // only send to auth clients
      if (isEmptyChallenge(challenges[client]))
      {
        // ESP_LOG_BUFFER_HEXDUMP("Send", buffer, messageLength, esp_log_level_t::ESP_LOG_INFO);
        // Serial.printf("[%u] sent Text: %s\n", client, buffer);
        // TODO: use service filter in case of discovery events
        ws->sendTXT(client, buffer);
      }
    }
  }
}

void NobleApi::sendAuthMessage(const uint8_t client)
{
  if (!isEmptyChallenge(challenges[client]))
  {
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    char challenge[BLOCK_SIZE * 2 + 1];
    sec->toHex((uint8_t *)challenges[client], BLOCK_SIZE, challenge);
    command["challenge"] = challenge;
    sendJsonMessage(command, client);
    command.clear();
  }
}

void NobleApi::sendState(const uint8_t client)
{
  StaticJsonDocument<64> command;
  command["type"] = "stateChange";
  if (BLEApi::isReady())
  {
    command["state"] = "poweredOn";
  }
  else
  {
    command["state"] = "poweredOff";
  }
  sendJsonMessage(command, client);
  command.clear();
}

void NobleApi::sendConnected(const uint8_t client, BLEPeripheralID id)
{
  StaticJsonDocument<128> command;
  command["type"] = "connect";
  command["peripheralUuid"] = BLEApi::idToString(id);
  sendJsonMessage(command, client);
}

void NobleApi::sendDisconnected(const uint8_t client, BLEPeripheralID id)
{
  sendDisconnected(client, id, "");
}

void NobleApi::sendDisconnected(const uint8_t client, BLEPeripheralID id, std::string reason)
{
  StaticJsonDocument<128> command;
  command["type"] = "disconnect";
  command["peripheralUuid"] = BLEApi::idToString(id);
  if (reason != "")
  {
    command["reason"] = reason;
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendServices(const uint8_t client, BLEPeripheralID id, std::vector<NimBLERemoteService *> *services)
{
  StaticJsonDocument<512> command;
  command["type"] = "servicesDiscover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  JsonArray serviceUuids = command.createNestedArray("serviceUuids");
  for (NimBLERemoteService *service : *services)
  {
    serviceUuids.add(service->getUUID().to128().toString());
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristics(const uint8_t client, BLEPeripheralID id, std::string service, std::vector<NimBLERemoteCharacteristic *> *characteristics)
{
  StaticJsonDocument<512> command;
  command["type"] = "characteristicsDiscover";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  JsonArray characteristicsUuids = command.createNestedArray("characteristics");
  for (NimBLERemoteCharacteristic *characteristic : *characteristics)
  {
    JsonObject charact = characteristicsUuids.createNestedObject();
    charact["uuid"] = characteristic->getUUID().to128().toString();
    JsonArray properties = charact.createNestedArray("properties");
    if (characteristic->canRead())
    {
      properties.add("read");
    }
    if (characteristic->canWrite())
    {
      properties.add("write");
    }
    if (characteristic->canWriteNoResponse())
    {
      properties.add("writeWithoutResponse");
    }
    if (characteristic->canNotify())
    {
      properties.add("notify");
    }
    if (characteristic->canIndicate())
    {
      properties.add("indicate");
    }
    if (characteristic->canBroadcast())
    {
      properties.add("broadcast");
    }
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicValue(
    const uint8_t client,
    BLEPeripheralID id,
    std::string service,
    std::string characteristic,
    std::string value,
    bool isNotification)
{
  StaticJsonDocument<384> command;
  command["type"] = "read";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  if (value.length() > 0)
  {
    char hexValue[value.length() * 2 + 1];
    sec->toHex((uint8_t *)value.c_str(), value.length(), hexValue);
    command["data"] = std::string(hexValue);
  }
  else
  {
    command["data"] = "00";
  }
  command["isNotification"] = isNotification;
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicNotification(
    const uint8_t client,
    BLEPeripheralID id,
    std::string service,
    std::string characteristic,
    bool state)
{
  StaticJsonDocument<256> command;
  command["type"] = "notify";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  command["state"] = state;
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicWrite(const uint8_t client, BLEPeripheralID id, std::string service, std::string characteristic)
{
  StaticJsonDocument<256> command;
  command["type"] = "write";
  command["peripheralUuid"] = BLEApi::idToString(id);
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  sendJsonMessage(command, client);
}

bool NobleApi::addClient(BLEPeripheralID id, uint8_t client)
{
  if (activeConnections < MAX_CLIENT_CONNECTIONS)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].client == INVALID_CLIENT)
      {
        peripheralConnections[i].client = client;
        peripheralConnections[i].id = id;
        activeConnections++;
        return true;
      }
    }
  }
  return false;
}

uint8_t NobleApi::getClient(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].id == id)
      {
        return peripheralConnections[i].client;
      }
    }
  }
  return INVALID_CLIENT;
}

void NobleApi::delClient(BLEPeripheralID id)
{
  if (activeConnections > 0)
  {
    for (auto i = 0; i < MAX_CLIENT_CONNECTIONS; i++)
    {
      if (peripheralConnections[i].id == id)
      {
        peripheralConnections[i].client = INVALID_CLIENT;
        activeConnections--;
      }
    }
  }
}