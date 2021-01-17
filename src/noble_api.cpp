#include "noble_api.h"

Security *NobleApi::sec = nullptr;
WebSocketsServer *NobleApi::ws = nullptr;
std::map<uint32_t, std::string> *NobleApi::challenges = nullptr;

void NobleApi::init()
{
  sec = new Security(aesKey);
  challenges = new std::map<uint32_t, std::string>();
  BLEApi::init();
  BLEApi::onDeviceFound(onBLEDeviceFound);
  ws = new WebSocketsServer(ESP_GW_WEBSOCKET_PORT);
  ws->begin();
  ws->onEvent(onWsEvent);
}

void NobleApi::loop()
{
  ws->loop();
}

void NobleApi::onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_DISCONNECTED)
  {
    Serial.printf("[%u] Disconnected!\n", client);
    challenges->erase(client);
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
        if (challenges->find(client) == challenges->end())
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
              command.clear();
            }
          }
        }
        else
        {
          // client is authenticated, allow other commands
          if (strcmp(action, "startScanning") == 0)
          {
            command.clear();
            BLEApi::startScan();
          }
          else if (strcmp(action, "stopScanning") == 0)
          {
            command.clear();
            BLEApi::stopScan();
          }
          else if (strcmp(action, "connect") == 0)
          {
            const char *peripheralUuid = command["peripheralUuid"];
            BLEApi::connect(peripheralUuid);
            command.clear();
          }
        }
      }
    }
  }
  else
  {
    Serial.printf("Type not implemented [%u]\n", type);
  }
}

void NobleApi::onBLEDeviceFound(BLEAdvertisedDevice advertisedDevice)
{
  // Serial.print("BLE found: ");
  // Serial.println(advertisedDevice.toString().c_str());
  /**
   * type: 'discover',
    peripheralUuid: peripheral.uuid,
    address: peripheral.address,
    addressType: peripheral.addressType,
    connectable: peripheral.connectable,
    advertisement: {
      localName: peripheral.advertisement.localName,
      txPowerLevel: peripheral.advertisement.txPowerLevel,
      serviceUuids: peripheral.advertisement.serviceUuids,
      manufacturerData: (peripheral.advertisement.manufacturerData ? peripheral.advertisement.manufacturerData.toString('hex') : null),
      serviceData: (peripheral.advertisement.serviceData ? peripheral.advertisement.serviceData.toString('hex') : null)
    },
    rssi: peripheral.rssi
    */
  StaticJsonDocument<1024> command;
  command["type"] = "discover";
  command["peripheralUuid"] = advertisedDevice.getAddress().toString();
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
  challenges->insert({client, stringIV});
}

void NobleApi::checkAuth(uint8_t client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    std::map<uint32_t, std::string>::iterator it = challenges->find(client);
    if (it != challenges->end())
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
        challenges->erase(client);
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

void NobleApi::sendJsonMessage(uint8_t client, JsonDocument &command)
{
  size_t messageLength = measureJson(command) + 1;
  Serial.println(messageLength);
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
  ws->connectedClients();
  std::map<uint32_t, std::string>::iterator it;
  for (uint8_t client = 0; client < WEBSOCKETS_SERVER_CLIENT_MAX; client++)
  {
    if (ws->clientIsConnected(client))
    {
      // only send to auth clients
      it = challenges->find(client);
      if (it == challenges->end())
      {
        sendJsonMessage(client, command);
      }
    }
  }
}

void NobleApi::sendAuthMessage(uint8_t client)
{
  std::map<uint32_t, std::string>::iterator it = challenges->find(client);
  if (it != challenges->end())
  {
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    command["challenge"] = it->second;
    sendJsonMessage(client, command);
    command.clear();
  }
}

void NobleApi::sendState(uint8_t client)
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
  sendJsonMessage(client, command);
  command.clear();
}
