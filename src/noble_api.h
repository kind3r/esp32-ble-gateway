#ifndef ESP_GW_NOBLE_API_H
#define ESP_GW_NOBLE_API_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <map>
#include "security.h"

class NobleApi {
  public:
    NobleApi(Security *security, WebSocketsServer *server);
    ~NobleApi();
    void onWsEvent(uint8_t client, WStype_t type, uint8_t * payload, size_t length);
  private:
    Security *sec;
    WebSocketsServer *ws;
    std::map<uint32_t, std::string> challenges;
    void initClient(uint8_t client);
    void checkAuth(uint8_t client, const char *response);
    void sendJsonMessage(uint8_t client, JsonDocument &command);
    void sendAuthMessage(uint8_t client);
    void sendState(uint8_t client);
};

#endif