#ifndef ESP_GW_NOBLE_API_ASYNC_H
#define ESP_GW_NOBLE_API_ASYNC_H

// #include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "security.h"

class NobleApiAsync {
  public:
    NobleApiAsync(Security *security);
    ~NobleApiAsync();
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
  private:
    Security *sec;
    void initClient(AsyncWebSocketClient &client);
    void checkAuth(AsyncWebSocketClient &client, const char *response);
    void sendJsonMessage(AsyncWebSocketClient &client, JsonDocument &command);
    void sendAuthMessage(AsyncWebSocketClient &client);
    void sendState(AsyncWebSocketClient &client);
};

#endif