#ifndef ESP_GW_WEB_H
#define ESP_GW_WEB_H

#ifndef ESP_GW_WEBSERVER_PORT
#define ESP_GW_WEBSERVER_PORT 80
#endif

#include <WebServer.h>
#include <SPIFFS.h>

class WebManager {
  public:
    static bool init();
    static void loop();
  private:
    static WebServer *server;

    static void testHandler();
    static void handleNotFound();
};

#endif