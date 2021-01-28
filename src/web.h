#ifndef ESP_GW_WEB_H
#define ESP_GW_WEB_H

#ifndef ESP_GW_WEBSERVER_PORT
#define ESP_GW_WEBSERVER_PORT 80
#endif

#ifndef ESP_GW_WEBSERVER_SECURE_PORT
#define ESP_GW_WEBSERVER_SECURE_PORT 443
#endif

#include "util.h"
#include "security.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

#include <SPIFFS.h>

#include <Preferences.h>

using namespace httpsserver;

class WebManager {
  public:
    static bool init(Preferences *preferences);
    static void loop();
  private:
    static Preferences *prefs;
    static HTTPServer *server;
    static uint8_t *certData;
    static uint8_t *pkData;
    static SSLCert * cert;
    static HTTPSServer *serverSecure;
    static bool rebootRequired;
    static bool rebootNextLoop;

    static bool initCertificate();
    static void clearCertificate();
    static void handleHome(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigGet(HTTPRequest * req, HTTPResponse * res);
    static void handleConfigSet(HTTPRequest * req, HTTPResponse * res);
    static void handleFactoryReset(HTTPRequest * req, HTTPResponse * res);
    static void handleRedirect(HTTPRequest * req, HTTPResponse * res);
    static void handleNotFound(HTTPRequest *req, HTTPResponse *res);
};

#endif