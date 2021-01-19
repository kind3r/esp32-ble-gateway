#include "noble_api.h"

Security *NobleApi::sec = nullptr;
WebSocketsServer *NobleApi::ws = nullptr;
std::map<uint32_t, std::string> NobleApi::challenges;
std::map<std::string, uint8_t> NobleApi::peripheralConnections;

/**
   * Initialize API
   */
void NobleApi::init()
{
  sec = new Security(aesKey);
  BLEApi::init();
  BLEApi::onDeviceFound(onBLEDeviceFound);
  ws = new WebSocketsServer(ESP_GW_WEBSOCKET_PORT);
  ws->begin();
  ws->onEvent(onWsEvent);
}

/**
 * Loop to process websocket
 */
void NobleApi::loop()
{
  // Process websocket events
  ws->loop();
  // TODO: disconnect clients that did not authenticate in a resonable timeframe
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
  std::set<std::string> peripherals;
  for (std::map<std::string, uint8_t>::iterator peripheralConnection = peripheralConnections.begin(); peripheralConnection != peripheralConnections.end(); ++peripheralConnection)
  {
    if (peripheralConnection->second == client)
    {
      BLEApi::disconnect(peripheralConnection->first);
      peripherals.insert(peripheralConnection->first);
    }
  }

  if (peripherals.size() > 0)
  {
    for (std::set<std::string>::iterator peripheral = peripherals.begin(); peripheral != peripherals.end(); ++peripheral)
    {
      peripheralConnections.erase(*peripheral);
    }
  }

  challenges.erase(client);
}

/**
 * Can the client connect to device ?
 */
bool NobleApi::clientCanConnect(uint8_t client, std::string peripheral)
{
  std::map<std::string, uint8_t>::iterator peripheralConnection = peripheralConnections.find(peripheral);
  if (peripheralConnection != peripheralConnections.end() && peripheralConnection->second != client)
  {
    return false;
  }
  return true;
}

/**
 * Is client connected to the device ?
 */
bool NobleApi::clientConnected(uint8_t client, std::string peripheral)
{
  std::map<std::string, uint8_t>::iterator peripheralConnection = peripheralConnections.find(peripheral);
  if (peripheralConnection != peripheralConnections.end() && peripheralConnection->second == client)
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
    }
  }
  else if (type == WStype_CONNECTED)
  {
    IPAddress ip = ws->remoteIP(client);
    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", client, ip[0], ip[1], ip[2], ip[3], payload);
    // Generate IV and send auth request
    initClient(client);
    sendAuthMessage(client);
  }
  else if (type == WStype_TEXT)
  {
    Serial.printf("[%u] get  Text: %s\n", client, payload);

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
        if (challenges.find(client) == challenges.end())
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
          if (strcmp(action, "startScanning") == 0)
          {
            // TODO: if the scan is already running, send the list of discovered devices
            // TODO: setup services filter
            BLEApi::startScan();
          }
          else if (strcmp(action, "stopScanning") == 0)
          {
            BLEApi::stopScan();
          }
          else if (strcmp(action, "connect") == 0)
          {
            std::string peripheralUuid = command["peripheralUuid"];

            // check if peripheralUuid is not asigned to another client, asign client to periperhalUuid, check connection
            if (clientCanConnect(client, peripheralUuid))
            {
              peripheralConnections[peripheralUuid] = client;
              // TODO: check if re-connection to peripheral is ok (in case client sends multiple connect but no disconnect)
              bool connected = BLEApi::connect(peripheralUuid);
              if (connected)
              {
                sendConnected(client, peripheralUuid);
              }
              else
              {
                peripheralConnections.erase(peripheralUuid);
                sendDisconnected(client, peripheralUuid, "failed");
              }
            }
            else
            {
              sendDisconnected(client, peripheralUuid, "denied");
            }
          }
          else if (strcmp(action, "discoverServices") == 0)
          {
            std::string peripheralUuid = command["peripheralUuid"];
            if (clientConnected(client, peripheralUuid))
            {
              std::map<std::string, BLERemoteService *> *services = BLEApi::discoverServices(peripheralUuid);
              if (services != nullptr)
              {
                sendServices(client, peripheralUuid, services);
              }
            }
            else
            {
              sendDisconnected(client, peripheralUuid, "not connected");
            }
          }
          else if (strcmp(action, "discoverCharacteristics") == 0)
          {
            std::string peripheralUuid = command["peripheralUuid"];
            if (clientConnected(client, peripheralUuid))
            {
              std::string serviceUuid = command["serviceUuid"];
              std::map<std::string, BLERemoteCharacteristic *> *characteristics = BLEApi::discoverCharacteristics(peripheralUuid, serviceUuid);
              if (characteristics != nullptr)
              {
                sendCharacteristics(client, peripheralUuid, serviceUuid, characteristics);
              }
              else
              {
                peripheralConnections.erase(peripheralUuid);
                sendDisconnected(client, peripheralUuid, "aborted");
              }
            }
            else
            {
              sendDisconnected(client, peripheralUuid, "not connected");
            }
          }
          else if (strcmp(action, "read") == 0)
          {
            std::string peripheralUuid = command["peripheralUuid"];
            if (clientConnected(client, peripheralUuid))
            {
              std::string serviceUuid = command["serviceUuid"];
              std::string characteristicUuid = command["characteristicUuid"];
              std::string value = BLEApi::readCharacteristic(peripheralUuid, serviceUuid, characteristicUuid);
              sendCharacteristicValue(client, peripheralUuid, serviceUuid, characteristicUuid, value);
            }
            else
            {
              sendDisconnected(client, peripheralUuid, "not connected");
            }
          }
        }
      }
      command.clear();
    }
  }
  else
  {
    Serial.printf("Type not implemented [%u]\n", type);
  }
}

void NobleApi::onBLEDeviceFound(BLEAdvertisedDevice advertisedDevice, std::string id)
{
  StaticJsonDocument<1024> command;
  command["type"] = "discover";
  command["peripheralUuid"] = id;
  command["address"] = advertisedDevice.getAddress().toString();
  if (advertisedDevice.getAddressType() == BLE_ADDR_TYPE_PUBLIC || advertisedDevice.getAddressType() == BLE_ADDR_TYPE_RPA_PUBLIC)
  {
    command["addressType"] = "public";
  }
  else if (advertisedDevice.getAddressType() == BLE_ADDR_TYPE_RANDOM || advertisedDevice.getAddressType() == BLE_ADDR_TYPE_RPA_RANDOM)
  {
    command["addressType"] = "random";
  }
  else
  {
    command["addressType"] = "unknown";
  }
  command["connectable"] = "true";
  command["rssi"] = advertisedDevice.getRSSI();
  command["advertisement"]["localName"] = advertisedDevice.getName();
  if (advertisedDevice.haveTXPower())
  {
    command["advertisement"]["txPowerLevel"] = advertisedDevice.getTXPower();
  }
  if (advertisedDevice.haveServiceUUID())
  {
    JsonArray serviceUuids = command["advertisement"].createNestedArray("serviceUuids");
    serviceUuids.add(advertisedDevice.getServiceUUID().toString());
  }
  if (advertisedDevice.haveManufacturerData())
  {
    char manufacturerData[advertisedDevice.getManufacturerData().length() * 2 + 1];
    sec->toHex((uint8_t *)advertisedDevice.getManufacturerData().data(), advertisedDevice.getManufacturerData().length(), manufacturerData);
    manufacturerData[advertisedDevice.getManufacturerData().length() * 2 + 1] = '\0';
    command["advertisement"]["manufacturerData"] = manufacturerData;
  }

  sendJsonMessage(command);
  command.clear();
}

void NobleApi::initClient(uint8_t client)
{
  uint8_t iv[BLOCK_SIZE];
  sec->generateIV(iv);
  char stringIV[BLOCK_SIZE * 2 + 1];
  sec->toHex(iv, BLOCK_SIZE, stringIV);
  challenges.insert({client, stringIV});
}

void NobleApi::checkAuth(uint8_t client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    std::map<uint32_t, std::string>::iterator it = challenges.find(client);
    if (it != challenges.end())
    {
      uint8_t iv[BLOCK_SIZE];
      sec->fromHex(it->second.c_str(), it->second.length(), iv);

      uint8_t encryptedResponse[responseLength / 2];
      size_t encryptedResponseLength = sec->fromHex(response, responseLength, encryptedResponse);

      uint8_t decryptedResponse[encryptedResponseLength + 1];
      size_t decryptedResponseLength = sec->decrypt(iv, encryptedResponse, encryptedResponseLength, decryptedResponse);
      decryptedResponse[encryptedResponseLength] = '\0';

      if (strcmp((char *)decryptedResponse, "admin:admin") == 0)
      {
        challenges.erase(client);
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
  // ESP_LOG_BUFFER_HEXDUMP("Send", buffer, messageLength, esp_log_level_t::ESP_LOG_INFO);
  Serial.printf("[%u] sent Text: %s\n", client, buffer);
  ws->sendTXT(client, buffer);
}

void NobleApi::sendJsonMessage(JsonDocument &command)
{
  size_t messageLength = measureJson(command) + 1;
  char buffer[messageLength];
  serializeJson(command, buffer, messageLength);
  buffer[messageLength] = '\0';

  std::map<uint32_t, std::string>::iterator it;
  for (uint8_t client = 0; client < WEBSOCKETS_SERVER_CLIENT_MAX; client++)
  {
    if (ws->clientIsConnected(client))
    {
      // only send to auth clients
      it = challenges.find(client);
      if (it == challenges.end())
      {
        // ESP_LOG_BUFFER_HEXDUMP("Send", buffer, messageLength, esp_log_level_t::ESP_LOG_INFO);
        Serial.printf("[%u] sent Text: %s\n", client, buffer);
        ws->sendTXT(client, buffer);
      }
    }
  }
}

void NobleApi::sendAuthMessage(const uint8_t client)
{
  std::map<uint32_t, std::string>::iterator it = challenges.find(client);
  if (it != challenges.end())
  {
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    command["challenge"] = it->second;
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

void NobleApi::sendConnected(const uint8_t client, std::string id)
{
  StaticJsonDocument<128> command;
  command["type"] = "connect";
  command["peripheralUuid"] = id;
  sendJsonMessage(command, client);
}

void NobleApi::sendDisconnected(const uint8_t client, std::string id)
{
  sendDisconnected(client, id, "");
}

void NobleApi::sendDisconnected(const uint8_t client, std::string id, std::string reason)
{
  StaticJsonDocument<128> command;
  command["type"] = "disconnect";
  command["peripheralUuid"] = id;
  if (reason != "")
  {
    command["reason"] = reason;
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendServices(const uint8_t client, std::string id, std::map<std::string, BLERemoteService *> *services)
{
  StaticJsonDocument<128> command;
  command["type"] = "servicesDiscover";
  command["peripheralUuid"] = id;
  JsonArray serviceUuids = command.createNestedArray("serviceUuids");
  for (std::map<std::string, BLERemoteService *>::iterator it = services->begin(); it != services->end(); ++it)
  {
    serviceUuids.add(it->first.c_str());
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristics(const uint8_t client, std::string id, std::string service, std::map<std::string, BLERemoteCharacteristic *> *characteristics)
{
  StaticJsonDocument<512> command;
  command["type"] = "characteristicsDiscover";
  command["peripheralUuid"] = id;
  command["serviceUuid"] = service;
  JsonArray characteristicsUuids = command.createNestedArray("characteristics");
  for (std::map<std::string, BLERemoteCharacteristic *>::iterator it = characteristics->begin(); it != characteristics->end(); ++it)
  {
    JsonObject characteristic = characteristicsUuids.createNestedObject();
    characteristic["uuid"] = it->first.c_str();
    JsonArray properties = characteristic.createNestedArray("properties");
    if (it->second->canRead())
    {
      properties.add("read");
    }
    if (it->second->canWrite())
    {
      properties.add("write");
    }
    if (it->second->canWriteNoResponse())
    {
      properties.add("writeWithoutResponse");
    }
    if (it->second->canNotify())
    {
      properties.add("notify");
    }
    if (it->second->canIndicate())
    {
      properties.add("indicate");
    }
    if (it->second->canBroadcast())
    {
      properties.add("broadcast");
    }
  }
  sendJsonMessage(command, client);
}

void NobleApi::sendCharacteristicValue(
    const uint8_t client,
    std::string id,
    std::string service,
    std::string characteristic,
    std::string value)
{
  sendCharacteristicValue(client, id, service, characteristic, value, false);
}

void NobleApi::sendCharacteristicValue(
    const uint8_t client,
    std::string id,
    std::string service,
    std::string characteristic,
    std::string value,
    bool isNotification)
{
  StaticJsonDocument<384> command;
  command["type"] = "read";
  command["peripheralUuid"] = id;
  command["serviceUuid"] = service;
  command["characteristicUuid"] = characteristic;
  if (value.length() > 0)
  {
    char hexValue[value.length() * 2 + 1];
    sec->toHex((uint8_t *)value.c_str(), value.length(), hexValue);
    command["data"] = hexValue;
  }
  else
  {
    command["data"] = "";
  }
  command["isNotification"] = isNotification;
  sendJsonMessage(command, client);
}