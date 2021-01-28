#include "web.h"

Preferences *WebManager::prefs = nullptr;
HTTPServer *WebManager::server = nullptr;
uint8_t *WebManager::certData = nullptr;
uint8_t *WebManager::pkData = nullptr;
SSLCert *WebManager::cert = nullptr;
HTTPSServer *WebManager::serverSecure = nullptr;
bool WebManager::rebootRequired = false;
bool WebManager::rebootNextLoop = false;
char *WebManager::password = nullptr;
uint8_t *WebManager::buffer = new uint8_t[ESP_GW_WEBSERVER_BUFFER_SIZE];

bool WebManager::init(Preferences *preferences)
{
  prefs = preferences;

  if (!initCertificate())
  {
    Serial.println("Could not init HTTPS certificate");
    return false;
  }

  size_t passwordLen = prefs->getBytesLength("password");
  password = new char[passwordLen + 1];
  prefs->getBytes("password", password, passwordLen);
  password[passwordLen] = '\0';

  serverSecure = new HTTPSServer(cert, ESP_GW_WEBSERVER_SECURE_PORT, 1);
  serverSecure->addMiddleware(middlewareAuthentication);
  serverSecure->registerNode(new ResourceNode("/", "GET", handleHome));
  serverSecure->registerNode(new ResourceNode("/config", "GET", handleConfigGet));
  serverSecure->registerNode(new ResourceNode("/config", "POST", handleConfigSet));
  serverSecure->registerNode(new ResourceNode("/factoryReset", "GET", handleFactoryReset));
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
  if (rebootRequired)
  {
    // delay the reboot one more loop
    if (rebootNextLoop)
    {
      ESP.restart();
    }
    rebootNextLoop = true;
  }
  server->loop();
  serverSecure->loop();
}

bool WebManager::initCertificate()
{
  // gateway name (as set by the user)
  size_t nameLen = prefs->getBytesLength("name");
  char name[nameLen + 1] = {};
  prefs->getBytes("name", name, nameLen);

  if (prefs->isKey("name") && prefs->isKey("cert") && prefs->isKey("pk"))
  {
    Serial.println("Loading stored HTTPS certificate");

    // certificate name (last generated)
    size_t cert_nameLen = prefs->getBytesLength("cert_name");
    char cert_name[nameLen + 1] = {};
    prefs->getBytes("cert_name", cert_name, cert_nameLen);

    // if name does not match the certificate name, don't load but regenerate instead
    if (strcmp(cert_name, name) == 0)
    {
      size_t certLen = prefs->getBytesLength("cert");
      certData = new uint8_t[certLen];
      // uint8_t certData[certLen];
      prefs->getBytes("cert", certData, certLen);

      size_t pkLen = prefs->getBytesLength("pk");
      pkData = new uint8_t[pkLen];
      // uint8_t pkData[pkLen];
      prefs->getBytes("pk", pkData, pkLen);

      Serial.printf("Loaded cert from nvs [%s.local][cert=%d][pk=%d]\n", name, certLen, pkLen);

      cert = new SSLCert(certData, certLen, pkData, pkLen);
    }
  }

  if (cert == nullptr)
  {
    // requires larger stack #define CONFIG_ARDUINO_LOOP_STACK_SIZE 10240
    Serial.println("Generating new HTTPS certificate");

    cert = new SSLCert();
    std::string dn = "CN=";
    dn += name;
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
    Serial.printf("Creating the certificate was successful [%s.local][cert=%d][pk=%d]\n", name, cert->getCertLength(), cert->getPKLength());
    prefs->putBytes("cert", cert->getCertData(), cert->getCertLength());
    prefs->putBytes("pk", cert->getPKData(), cert->getPKLength());
    prefs->putBytes("cert_name", name, nameLen);
  }

  return true;
}

void WebManager::middlewareAuthentication(HTTPRequest *req, HTTPResponse *res, std::function<void()> next)
{
  Serial.println("Auth middleware started");
  std::string reqPassword = req->getBasicAuthPassword();

  if (reqPassword.length() == 0 || strcmp(password, reqPassword.c_str()) != 0)
  {
    res->setStatusCode(401);
    res->setStatusText("Unauthorized");
    res->setHeader("Content-Type", "text/plain");
    res->setHeader("WWW-Authenticate", "Basic realm=\"Gateway admin area\"");
    // Small error text on the response document. In a real-world scenario, you
    // shouldn't display the login information on this page, of course ;-)
    res->println("401. Unauthorized (defaults are admin/admin)");
    Serial.println("Auth failed");
  } else {
    Serial.println("Auth success");
    next();
  }
}

void WebManager::handleHome(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Content-Encoding", "gzip");

  SPIFFS.begin();
  File file = SPIFFS.open("/index.html.gz", "r");
  size_t length = 0;
  do
  {
    length = file.read(buffer, ESP_GW_WEBSERVER_BUFFER_SIZE);
    res->write(buffer, length);
  } while (length > 0);
  file.close();
  SPIFFS.end();
  
  meminfo();
}

void WebManager::handleConfigGet(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Content-Type", "application/json");
  res->setHeader("Connection", "close");

  StaticJsonDocument<128> config;

  size_t nameLen = prefs->getBytesLength("name");
  char name[nameLen + 1] = {};
  prefs->getBytes("name", name, nameLen);
  config["name"] = name;

  if (prefs->isKey("wifi_ssid"))
  {
    size_t ssidLen = prefs->getBytesLength("wifi_ssid");
    char ssid[ssidLen + 1] = {};
    prefs->getBytes("wifi_ssid", ssid, ssidLen);
    config["wifi_ssid"] = ssid;
  }

  if (prefs->isKey("wifi_pass"))
  {
    size_t passLen = prefs->getBytesLength("wifi_pass");
    char pass[passLen + 1] = {};
    prefs->getBytes("wifi_pass", pass, passLen);
    config["wifi_pass"] = pass;
  }

  char aesKey[BLOCK_SIZE * 2 + 1] = {};
  prefs->getBytes("aes", aesKey, BLOCK_SIZE * 2);
  config["aes_key"] = aesKey;

  size_t configLength = serializeJson(config, buffer, ESP_GW_WEBSERVER_BUFFER_SIZE);
  config.clear();

  res->write((uint8_t *)buffer, configLength);

  meminfo();
}

void WebManager::handleConfigSet(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Connection", "close");

  size_t idx = 0;
  // while "not everything read" or "buffer is full"
  while (!req->requestComplete() && idx < ESP_GW_WEBSERVER_BUFFER_SIZE)
  {
    idx += req->readChars((char *)buffer + idx, ESP_GW_WEBSERVER_BUFFER_SIZE - idx);
  }

  if (!req->requestComplete())
  {
    Serial.println("Request entity too large");
    Serial.println((char *)buffer);
    res->setStatusCode(413);
    res->setStatusText("Request entity too large");
    res->println("413 Request entity too large");
    return;
  }

  buffer[idx + 1] = '\0';

  StaticJsonDocument<128> config;
  DeserializationError error = deserializeJson(config, buffer, idx + 1);

  if (error != DeserializationError::Ok)
  { //Check for errors in parsing
    Serial.println("Invalid JSON format");
    Serial.println((char *)buffer);
    res->setStatusCode(400);
    res->setStatusText("Invalid JSON format");
    res->println("400 Invalid JSON format");
    return;
  }

  Serial.println("Checking for config changes");

  const char *name = config["name"];
  if (name && strlen(name) > 0)
  {
    Serial.printf("Setting new name [%s]\n", name);
    prefs->putBytes("name", name, strlen(name));
    rebootRequired = true;
  }

  const char *newPassword = config["password"];
  if (newPassword && strlen(newPassword) > 0)
  {
    Serial.printf("Setting new admin password [%s]\n", newPassword);
    prefs->putBytes("password", newPassword, strlen(newPassword));
    delete[] password;
    password = new char[strlen(newPassword) + 1];
    memcpy(password, newPassword, strlen(newPassword) + 1);
    delete[] newPassword;
  }

  const char *ssid = config["wifi_ssid"];
  if (ssid && strlen(ssid) > 0)
  {
    Serial.printf("Setting new WiFi SSID [%s]\n", ssid);
    prefs->putBytes("wifi_ssid", ssid, strlen(ssid));
    rebootRequired = true;
  }

  const char *pass = config["wifi_pass"];
  if (pass && strlen(pass) > 0)
  {
    Serial.printf("Setting new WiFi Password [%s]\n", pass);
    prefs->putBytes("wifi_pass", pass, strlen(pass));
    rebootRequired = true;
  }

  res->setHeader("Content-Type", "text/plain");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");

  if (rebootRequired)
  {
    Serial.println("Rebooting");
  }
  meminfo();
}

void WebManager::handleFactoryReset(HTTPRequest *req, HTTPResponse *res)
{
  prefs->clear();
  res->setHeader("Content-Type", "text/html");
  res->setHeader("Connection", "close");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->print("OK");

  Serial.println("Rebooting");
  rebootRequired = true;
}

void WebManager::handleRedirect(HTTPRequest *req, HTTPResponse *res)
{
  res->setHeader("Connection", "close");

  size_t nameLen = prefs->getBytesLength("name");
  char name[nameLen + 1] = {};
  prefs->getBytes("name", name, nameLen);

  std::string dn;
  dn = "https://";
  dn += name;
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