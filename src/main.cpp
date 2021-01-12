#include <config.h>
#include "Arduino.h"

#include <WiFi.h>

#include <WebSocketsServer.h>
WebSocketsServer webSocket = WebSocketsServer(80);

#include <ArduinoJson.h>

#include "security.h"
Security sec = Security();

#include "noble_api.h"
NobleApi noble = NobleApi(&sec, &webSocket);

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

void setupServer()
{
  webSocket.begin();
  webSocket.onEvent(std::bind(
    &NobleApi::onWsEvent, noble, 
    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4
  ));
}


void setupBLE() {
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  BLEApi::init();
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

  setupBLE();

  setupServer();

  Serial.println("Setup complete");
  meminfo("After full setup");
}

void loop()
{
  webSocket.loop();
}