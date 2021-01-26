#include "web.h"

WebServer *WebManager::server = nullptr;

bool WebManager::init() {
  server = new WebServer(ESP_GW_WEBSERVER_PORT);
  server->on("/", testHandler);
  server->onNotFound(handleNotFound);
  server->begin();
  return true;
}

void WebManager::loop() {
  server->handleClient();
}

void WebManager::testHandler() {
  // server->send(200, "text/plain", "hello from esp32!");

  SPIFFS.begin();
  File file = SPIFFS.open("/index.html.gz", "r");
  server->streamFile(file, "text/html");
  file.close();
  SPIFFS.end();
}

void WebManager::handleNotFound() {
  server->send(404, "text/plain", "NOT FOUND");
}