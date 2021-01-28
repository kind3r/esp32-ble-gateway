#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "gw_settings.h"
#include "web.h"
#include "noble_api.h"
#include "util.h"

#define WIFI_CONNECT_RETRY 5
#define WIFI_CONFIGURE_DNS_PORT 53

DNSServer *dnsServer = nullptr;
bool connected = false;

bool setupWifi()
{
  // wifi logic:
  // - are credentials set then try to connect
  //    - after few faild connect attemtps, revert to config mode
  //    - retry connection after a while if no new configuration provided (or restart ?)
  // - credentials not set, enter config mode

  // we have stored wifi credentials, use them
  if (GwSettings::isConfigured()) {
    Serial.printf("Connecting to configured WiFi [%s][%s]\n", GwSettings::getSsid(), GwSettings::getPass());
    WiFi.mode(WIFI_STA);

    uint8_t wifiResult;
    int8_t wifiRetry = WIFI_CONNECT_RETRY;
    do {
      WiFi.begin(GwSettings::getSsid(), GwSettings::getPass());
      wifiResult = WiFi.waitForConnectResult();
      if (wifiResult != WL_CONNECTED && wifiRetry != WIFI_CONNECT_RETRY && wifiRetry > 1) {
        Serial.println("Retrying to connect to WiFi");
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      wifiRetry--;
    } while (wifiResult != WL_CONNECTED && wifiRetry > 0);

    if (wifiResult == WL_CONNECTED) {
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      connected = true;
      return true;
    }

    Serial.printf("Failed to connect to configured WiFi [%s]\n", GwSettings::getSsid());
  }

  // no credentials or connect failed, setup AP
  Serial.println("Starting AP for configuration");
  WiFi.softAP("ESP32GW", "87654321");
  std::string dnsName = "";
  dnsName += GwSettings::getName();
  dnsName += ".local";
  dnsServer = new DNSServer();
  dnsServer->start(WIFI_CONFIGURE_DNS_PORT, dnsName.c_str(), WiFi.softAPIP());

  return true;
}

bool setupWeb() {
  return WebManager::init();
}

void setup()
{
  Serial.begin(921600);
  delay(200);
  Serial.println();
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  GwSettings::init();

  setupWifi();

  setupWeb();

  bool mdnsSuccess = false;
  do {
    mdnsSuccess = MDNS.begin(GwSettings::getName());
  } while (mdnsSuccess == false);

  MDNS.addService("http", "tcp", ESP_GW_WEBSERVER_PORT);

  NobleApi::init();

  MDNS.addService("ws", "tcp", ESP_GW_WEBSOCKET_PORT);

  Serial.println("Setup complete");
  meminfo();
}

void loop()
{
  if (dnsServer != nullptr) {
    // when in configuration mode handle DNS requests
    dnsServer->processNextRequest();
  }
  NobleApi::loop();
  WebManager::loop();
}