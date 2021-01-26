#include <config.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "web.h"
#include "noble_api.h"
#include "util.h"

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

bool setupWeb() {
  return WebManager::init();
}

void setup()
{
  Serial.begin(921600);
  // esp_log_level_set("*", ESP_LOG_VERBOSE);

  do
  {
    delay(200);
  } while (!setupWifi());

  setupWeb();

  bool mdnsSuccess = false;
  do {
    mdnsSuccess = MDNS.begin(gatewayName);
  } while (mdnsSuccess == false);

  MDNS.addService("http", "tcp", ESP_GW_WEBSERVER_PORT);

  NobleApi::init();

  MDNS.addService("ws", "tcp", ESP_GW_WEBSOCKET_PORT);

  Serial.println("Setup complete");
  meminfo();
}

void loop()
{
  NobleApi::loop();
  WebManager::loop();
}