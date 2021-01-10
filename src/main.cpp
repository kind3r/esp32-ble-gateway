#include <config.h>
#include "esp_log.h"
#include "Arduino.h"

#include <WiFi.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
// const char *PARAM_MESSAGE = "message";
AsyncWebSocket ws("/noble");
// AsyncEventSource events("/events");

// #include <WebSocketsServer.h>
// WebSocketsServer webSocket = WebSocketsServer(80);

#include <ArduinoJson.h>

#include "security.h"
Security sec = Security();

std::map<uint32_t, bool> isAuthenticated;
std::map<uint32_t, std::string> challenges;

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

bool setupWifi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("WiFi Failed!\n");
    return false;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void addClient(AsyncWebSocketClient *client)
{
  isAuthenticated[client->id()] = false;
  uint8_t iv[BLOCK_SIZE];
  sec.generateIV(iv);
  char charIV[BLOCK_SIZE * 2 + 1];
  sec.toHex(iv, BLOCK_SIZE, charIV);
  charIV[BLOCK_SIZE * 2] = '\0';
  challenges[client->id()] = charIV;
  // challenges[client->id()] = "39424146334239304534394533343945"; // pair with dbda816261cca33b0253bdaa96befde8 response
}

void removeClient(AsyncWebSocketClient *client)
{
  isAuthenticated.erase(client->id());
  challenges.erase(client->id());
}

void sendJsonMessage(AsyncWebSocketClient *client, const JsonObject command)
{
  size_t commandLength = measureJson(command) + 1;
  char *buffer = new char[commandLength];
  serializeJson(command, buffer, commandLength);
  // client->text(buffer);
  client->printf(buffer);
  ESP_LOG_BUFFER_HEXDUMP("Send", buffer, commandLength, esp_log_level_t::ESP_LOG_INFO);
}

void sendState(AsyncWebSocketClient *client)
{
  StaticJsonDocument<64> command;
  command["type"] = "stateChange";
  command["state"] = "poweredOff";
  sendJsonMessage(client, command.as<JsonObject>());
  command.clear();
}

void sendAuthMessage(AsyncWebSocketClient *client)
{
  std::string stringIV = challenges[client->id()];
  if (stringIV.length() > 0)
  {
    StaticJsonDocument<128> command;
    command["type"] = "auth";
    command["challenge"] = stringIV;
    sendJsonMessage(client, command.as<JsonObject>());
    command.clear();
  }
}

void checkAuth(AsyncWebSocketClient *client, const char *response)
{
  // check response length
  const size_t responseLength = strlen(response);
  if (responseLength % BLOCK_SIZE == 0)
  {
    std::string stringIV = challenges[client->id()];
    if (stringIV.length() > 0)
    {
      uint8_t iv[stringIV.length() / 2];
      sec.fromHex(stringIV.c_str(), stringIV.length(), iv);

      uint8_t encryptedResponse[responseLength / 2];
      size_t encryptedResponseLength = sec.fromHex(response, responseLength, encryptedResponse);

      uint8_t decryptedResponse[encryptedResponseLength + 1];
      size_t decryptedResponseLength = sec.decrypt(iv, encryptedResponse, encryptedResponseLength, decryptedResponse);
      decryptedResponse[encryptedResponseLength] = '\0';

      if (strcmp((char *)decryptedResponse, "admin:admin") == 0)
      {
        isAuthenticated[client->id()] = true;
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

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    //client connected
    addClient(client);
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    // client->ping();
    sendAuthMessage(client);
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
            bool authenticated = isAuthenticated[client->id()];
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
            }
          }
        }
      }
      // else
      // {
      //   for (size_t i = 0; i < info->len; i++)
      //   {
      //     Serial.printf("%02x ", data[i]);
      //   }
      //   Serial.printf("\n");
      // }
      // if (info->opcode == WS_TEXT)
      //   client->text("I got your text message");
      // else
      //   client->binary("I got your binary message");
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT)
      {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);
      }
      else
      {
        for (size_t i = 0; i < len; i++)
        {
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if ((info->index + len) == info->len)
      {
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

void setupServer()
{
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello, world");
  });

  server.onNotFound(notFound);

  server.begin();
}

void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);

  sec.setKey(aesKey);

  do
  {
    delay(200);
  } while (!setupWifi());

  setupServer();

  Serial.println("Setup complete");

  // const char test[] = "7c0b809dbdb685b2ccefc507dad92c96";
  // uint8_t testArr[16];
  // sec.fromHex(test, 32, testArr);
  // ESP_LOG_BUFFER_HEX("TEST", testArr, 16);
}

void loop()
{
  ws.cleanupClients();
}