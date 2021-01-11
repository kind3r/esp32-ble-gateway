#include "noble_api.h"
#include "ble_api.h"

#define LOG_LOCAL_LEVEL 5

NobleApi::NobleApi(Security *security, WebSocketsServer *server)
{
  sec = security;
  ws = server;
}

NobleApi::~NobleApi()
{
}

void NobleApi::onWsEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length)
{

  if (type == WStype_DISCONNECTED)
  {
    Serial.printf("[%u] Disconnected!\n", client);
    challenges.erase(client);
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

    if (error)
    { //Check for errors in parsing
      Serial.println("Parsing failed");
      Serial.println(error.f_str());
      return;
    }
    else
    {
      const char *action = command["action"];
      if (action)
      {
        bool authenticated = false;
        std::string stringIV = challenges[client];
        if (stringIV.length() == 0)
        {
          authenticated = true;
        }
        if (!authenticated)
        {
          // client is not authenticated, only respond to auth
          if (strcmp(action, "auth") == 0)
          {
            const char *response = command["response"];
            if (response)
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
            BLEApi::startScan();
          } else if (strcmp(action, "stopScanning") == 0) {
            BLEApi::stopScan();
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

void NobleApi::initClient(uint8_t client)
{
  uint8_t iv[BLOCK_SIZE];
  sec->generateIV(iv);
  char stringIV[BLOCK_SIZE * 2 + 1];
  sec->toHex(iv, BLOCK_SIZE, stringIV);
  challenges[client] = stringIV;
}

void NobleApi::checkAuth(uint8_t client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    std::string stringIV = challenges[client];
    if (stringIV.length() > 0)
    {
      uint8_t iv[BLOCK_SIZE];
      sec->fromHex(stringIV.c_str(), stringIV.length(), iv);

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

void NobleApi::sendAuthMessage(uint8_t client)
{
  std::string stringIV = challenges[client];
  if (stringIV.length() > 0)
  {
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    command["challenge"] = stringIV;
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
