#include "noble_api_async.h"

#define LOG_LOCAL_LEVEL 5

NobleApiAsync::NobleApiAsync(Security *security)
{
  sec = security;
}

NobleApiAsync::~NobleApiAsync()
{
}

void NobleApiAsync::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (client->_tempObject)
  {
    Serial.println("temp object exists");
  }
  if (client->_tempObject == nullptr)
  {
    Serial.println("temp obj is nulptr");
  }
  if (type == WS_EVT_CONNECT)
  {
    //client connected
    initClient(*client);
    if (client->_tempObject)
    {
      Serial.println("temp object exists");
    }
    if (client->_tempObject == nullptr)
    {
      Serial.println("temp obj is nulptr");
    }
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    sendAuthMessage(*client);
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    //client disconnected
    // removeClient(client);
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    //error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    //pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    // testEncryption();
    //data packet
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
      if (info->opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);

        StaticJsonDocument<1024> command;
        DeserializationError error = deserializeJson(command, data);

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
            if (client->_tempObject == nullptr)
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
                  checkAuth(*client, response);
                }
              }
            }
            else
            {
              // client is authenticated, allow other commands
            }
          }
        }
      }
    }
  }
}

void NobleApiAsync::initClient(AsyncWebSocketClient &client)
{
  client._tempObject = new uint8_t[BLOCK_SIZE];
  sec->generateIV((uint8_t *)client._tempObject);
}

void NobleApiAsync::checkAuth(AsyncWebSocketClient &client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    if (client._tempObject != nullptr)
    {
      uint8_t encryptedResponse[responseLength / 2];
      size_t encryptedResponseLength = sec->fromHex(response, responseLength, encryptedResponse);

      uint8_t decryptedResponse[encryptedResponseLength + 1];
      size_t decryptedResponseLength = sec->decrypt((uint8_t *)client._tempObject, encryptedResponse, encryptedResponseLength, decryptedResponse);
      decryptedResponse[encryptedResponseLength] = '\0';

      if (strcmp((char *)decryptedResponse, "admin:admin") == 0)
      {
        free(client._tempObject);
        // client->_tempObject = nullptr;
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

void NobleApiAsync::sendJsonMessage(AsyncWebSocketClient &client, JsonDocument &command)
{
  size_t messageLength = measureJson(command) + 1;
  Serial.println(messageLength);
  char buffer[messageLength];
  // String buffer;
  serializeJson(command, buffer, messageLength);
  buffer[messageLength] = '\0';
  // ESP_LOG_BUFFER_HEXDUMP("Send", buffer, commandLength, esp_log_level_t::ESP_LOG_INFO);
  Serial.println(buffer);
  
  client.printf(buffer);
  // client.text(buffer, messageLength);
}

void NobleApiAsync::sendAuthMessage(AsyncWebSocketClient &client)
{
  if (client._tempObject != nullptr)
  {
    char iv[BLOCK_SIZE * 2 + 1];
    sec->toHex((uint8_t *)client._tempObject, BLOCK_SIZE, iv);
    iv[BLOCK_SIZE * 2 + 1] = '\0';
    ESP_LOG_BUFFER_HEX ("IV       ", (uint8_t *)client._tempObject, BLOCK_SIZE);
    ESP_LOG_BUFFER_CHAR("IV string", iv, BLOCK_SIZE * 2);
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    command["challenge"] = iv;
    sendJsonMessage(client, command);
    command.clear();
  }
}

void NobleApiAsync::sendState(AsyncWebSocketClient &client)
{
  StaticJsonDocument<64> command;
  command["type"] = "stateChange";
  command["state"] = "poweredOff";
  sendJsonMessage(client, command);
  command.clear();
}
