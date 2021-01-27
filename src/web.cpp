#include "web.h"

Preferences *WebManager::prefs = nullptr;
HTTPServer *WebManager::server = nullptr;
uint8_t *WebManager::certData = nullptr;
uint8_t *WebManager::pkData = nullptr;
SSLCert *WebManager::cert = nullptr;
HTTPSServer *WebManager::serverSecure = nullptr;

bool WebManager::init(Preferences *preferences)
{
  prefs = preferences;

  if (!initCertificate())
  {
    Serial.println("Could not init HTTPS certificate");
    return false;
  }

  serverSecure = new HTTPSServer(cert, ESP_GW_WEBSERVER_SECURE_PORT, 2);
  serverSecure->registerNode(new ResourceNode("/", "GET", handleHome));
  serverSecure->registerNode(new ResourceNode("/config", "GET", handleConfigGet));
  serverSecure->registerNode(new ResourceNode("/config", "POST", handleConfigSet));
  serverSecure->registerNode(new ResourceNode("/clearCertificate", "GET", handleClearCertificate));
  serverSecure->setDefaultNode(new ResourceNode("", "", handleNotFound));
  serverSecure->start();

  Serial.println("HTTPS started");
  meminfo();

  server = new HTTPServer(ESP_GW_WEBSERVER_PORT, 1);
  server->setDefaultNode(new ResourceNode("", "", handleRedirect));
  server->start();
  Serial.println("HTTP started");
  meminfo();

  return true;
}

void WebManager::loop()
{
  server->loop();
  serverSecure->loop();
}

bool WebManager::initCertificate()
{
  if (prefs->isKey("name") && prefs->isKey("cert") && prefs->isKey("pk"))
  {
    Serial.println("Loading stored HTTPS certificate");
    size_t nameLen = prefs->getBytesLength("name");
    char name[nameLen+1] = {};
    prefs->getBytes("name", name, nameLen);
    // if name does not match, don't load but regenerate instead
    if (strcmp(gatewayName, name) == 0)
    {
      size_t certLen = prefs->getBytesLength("cert");
      certData = new uint8_t[certLen];
      // uint8_t certData[certLen];
      prefs->getBytes("cert", certData, certLen);

      size_t pkLen = prefs->getBytesLength("pk");
      pkData = new uint8_t[pkLen];
      // uint8_t pkData[pkLen];
      prefs->getBytes("pk", pkData, pkLen);

      Serial.printf("Loaded cert from nvs [%s][cert=%d][pk=%d]\n", name, certLen, pkLen);

      cert = new SSLCert(certData, certLen, pkData, pkLen);
    }
  }

  if (cert == nullptr)
  {
    // requires larger stack #define CONFIG_ARDUINO_LOOP_STACK_SIZE 10240
    Serial.println("Generating new HTTPS certificate");
    cert = new SSLCert();
    std::string dn = "CN=";
    dn += gatewayName;
    dn += ".local,O=FancyCompany,C=RO";
    int createCertResult = createSelfSignedCert(
        *cert,
        KEYSIZE_2048,
        dn,
        "20190101000000",
        "20300101000000");

    if (createCertResult != 0)
    {
      Serial.printf("Cerating certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
      return false;
    }
    Serial.printf("Creating the certificate was successful [%s][cert=%d][pk=%d]\n", gatewayName, cert->getCertLength(), cert->getPKLength());
    prefs->putBytes("cert", cert->getCertData(), cert->getCertLength());
    prefs->putBytes("pk", cert->getPKData(), cert->getPKLength());
    prefs->putBytes("name", gatewayName, strlen(gatewayName));
  }

  return true;
}

void WebManager::clearCertificate()
{
  prefs->remove("cert");
  prefs->remove("pk");
}

void WebManager::handleHome(HTTPRequest *req, HTTPResponse *res)
{
  // server->send(200, "text/plain", "hello from esp32!");

  res->setHeader("Content-Type", "text/html");
  res->setHeader("Content-Encoding", "gzip");
  // res->setHeader("Connection", "close");
  SPIFFS.begin();
  File file = SPIFFS.open("/index.html.gz", "r");
  uint8_t buffer[256];
  size_t length = 0;
  do
  {
    length = file.read(buffer, 256);
    res->write(buffer, length);
  } while (length > 0);
  file.close();
  SPIFFS.end();
}

void WebManager::handleConfigGet(HTTPRequest * req, HTTPResponse * res) {
  res->setHeader("Content-Type", "application/json");
  res->setHeader("Connection", "close");

  StaticJsonDocument<128> config;

  size_t nameLen = prefs->getBytesLength("name");
  char name[nameLen + 1];
  prefs->getBytes("name", name, nameLen);
  name[nameLen] = '\0';
  config["name"] = name;

  if (prefs->isKey("wifi_ssid")) {
    size_t ssidLen = prefs->getBytesLength("wifi_ssid");
    char ssid[ssidLen + 1];
    prefs->getBytes("wifi_ssid", ssid, ssidLen);
    ssid[ssidLen] = '\0';
    config["wifi_ssid"] = ssid;
  }

  if (prefs->isKey("wifi_paass")) {
    size_t passLen = prefs->getBytesLength("wifi_pass");
    char pass[passLen + 1];
    prefs->getBytes("wifi_pass", pass, passLen);
    pass[passLen] = '\0';
    config["wifi_pass"] = pass;
  }

  char aesKey[BLOCK_SIZE * 2 + 1];
  prefs->getBytes("aes", aesKey, BLOCK_SIZE * 2);
  aesKey[BLOCK_SIZE * 2] = '\0';
  config["aes_key"] = aesKey;

  size_t configLength = measureJson(config) + 1;
  char buffer[configLength];
  serializeJson(config, buffer, configLength);
  config.clear();

  res->write((uint8_t *)buffer, configLength - 1);
}

void WebManager::handleConfigSet(HTTPRequest * req, HTTPResponse * res) {
  res->setHeader("Connection", "close");

  const size_t capacity = JSON_OBJECT_SIZE(4) + 180;

  char *buffer = new char[capacity + 1];
  size_t idx = 0;
  // while "not everything read" or "buffer is full"
  while (!req->requestComplete() && idx < capacity) {
    idx += req->readChars(buffer + idx, capacity-idx);
  }

  if (!req->requestComplete()) {
    res->setStatusCode(413);
    res->setStatusText("Request entity too large");
    res->println("413 Request entity too large");
    // Clean up
    delete[] buffer;
    return;
  }

  buffer[idx + 1] = '\0';

  StaticJsonDocument<128> config;
  DeserializationError error = deserializeJson(config, buffer, idx + 1);

  if (error != DeserializationError::Ok)
  { //Check for errors in parsing
    res->setStatusCode(400);
    res->setStatusText("Invalid JSON format");
    res->println("400 Invalid JSON format");
    // Clean up
    delete[] buffer;
    return;
  }

  delete[] buffer;
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->println("OK");
}

void WebManager::handleClearCertificate(HTTPRequest *req, HTTPResponse *res)
{
  clearCertificate();
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Connection", "close");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->println("OK");
  res->finalize();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  ESP.restart();
}

void WebManager::handleRedirect(HTTPRequest * req, HTTPResponse * res) {
  res->setHeader("Connection", "close");

  std::string dn;
  dn = "https://";
  dn += gatewayName;
  dn += ".local";
  res->setHeader("Location", dn);
  res->setStatusCode(301);
  res->setStatusText("Moved Permanently");
}

void WebManager::handleNotFound(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Connection", "close");
  res->setStatusCode(404);
  res->setStatusText("NOT FOUND");
  res->println("NOT FOUND");
}