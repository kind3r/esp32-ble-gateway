#ifndef ESP_GW_WEB_H
#define ESP_GW_WEB_H

#ifndef ESP_GW_WEBSERVER_PORT
#define ESP_GW_WEBSERVER_PORT 80
#endif

#ifndef ESP_GW_WEBSERVER_SECURE_PORT
#define ESP_GW_WEBSERVER_SECURE_PORT 443
#endif

#ifndef ESP_GW_WEBSERVER_BUFFER_SIZE
#define ESP_GW_WEBSERVER_BUFFER_SIZE 512
#endif

#include "gw_settings.h"
#include "util.h"
#include "security.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

#include <SPIFFS.h>

using namespace httpsserver;

class WebManager {
  public:
    static bool init();
    static void loop();
  private:
    static HTTPServer *server;
    static uint8_t *certData;
    static uint8_t *pkData;
    static SSLCert * cert;
    static HTTPSServer *serverSecure;
    static bool rebootRequired;
    static bool rebootNextLoop;
    static uint8_t *buffer;

    static bool initCertificate();
    static void clearCertificate();
    static void middlewareAuthentication(HTTPRequest * req, HTTPResponse * res, std::function<void()> next);
    static void handleHome(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigGet(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigSet(HTTPRequest * req, HTTPResponse * res);
    static void handleFactoryReset(HTTPRequest * req, HTTPResponse * res);
    static void handleRedirect(HTTPRequest * req, HTTPResponse * res);
    static void handleNotFound(HTTPRequest *req, HTTPResponse *res);
};

#endif