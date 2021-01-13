#include <config.h>
#include "Arduino.h"

#include <WiFi.h>

#include <ArduinoJson.h>

#include "noble_api.h"

#include <esp_bt.h>
#include <BLEDevice.h>
#include "ble_api.h"

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

void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);

  do
  {
    delay(200);
  } while (!setupWifi());

  NobleApi::init();

  Serial.println("Setup complete");
  meminfo("After full setup");
}

void loop()
{
  NobleApi::loop();
}