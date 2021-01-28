#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "web.h"
#include "noble_api.h"
#include "util.h"

#define WIFI_CONNECT_RETRY 5
#define WIFI_CONFIGURE_DNS_PORT 53

Preferences prefs;
DNSServer *dnsServer = nullptr;
bool configured = false;
bool connected = false;
char *name;

bool setupWifi()
{
  // wifi logic:
  // - are credentials set then try to connect
  //    - after few faild connect attemtps, revert to config mode
  //    - retry connection after a while if no new configuration provided (or restart ?)
  // - credentials not set, enter config mode

  // we have stored wifi credentials, use them
  if (prefs.isKey("wifi_ssid") && prefs.isKey("wifi_pass")) {
    configured = true;
    size_t ssidLen = prefs.getBytesLength("wifi_ssid");
    char ssid[ssidLen + 1] = {};
    prefs.getBytes("wifi_ssid", ssid, ssidLen);

    size_t passLen = prefs.getBytesLength("wifi_pass");
    char pass[passLen + 1] = {};
    prefs.getBytes("wifi_pass", pass, passLen);

    Serial.printf("Connecting to configured WiFi [%s]\n", ssid);
    WiFi.mode(WIFI_STA);

    uint8_t wifiResult;
    int8_t wifiRetry = WIFI_CONNECT_RETRY;
    do {
      WiFi.begin(ssid, pass);
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

    Serial.printf("Failed to connect to configured WiFi [%s]\n", ssid);
  }

  // no credentials or connect failed, setup AP
  Serial.println("Starting AP for configuration");
  WiFi.softAP("ESP32GW", "87654321");
  std::string dnsName = "";
  dnsName += name;
  dnsName += ".local";
  dnsServer = new DNSServer();
  dnsServer->start(WIFI_CONFIGURE_DNS_PORT, dnsName.c_str(), WiFi.softAPIP());

  return true;
}

bool setupWeb() {
  return WebManager::init(&prefs);
}

void setup()
{
  Serial.begin(921600);
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  prefs.begin("ESP32GW");

  // initial name
  if (!prefs.isKey("name")) {
    prefs.putBytes("name", "esp32gw", 7);
  }

  size_t nameLen = prefs.getBytesLength("name");
  name = new char[nameLen+1];
  prefs.getBytes("name", name, nameLen);
  name[nameLen] = '\0';

  setupWifi();

  setupWeb();

  bool mdnsSuccess = false;
  do {
    mdnsSuccess = MDNS.begin(name);
  } while (mdnsSuccess == false);

  MDNS.addService("http", "tcp", ESP_GW_WEBSERVER_PORT);

  NobleApi::init(&prefs);

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