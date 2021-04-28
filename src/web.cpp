#include "web.h"

HTTPServer *WebManager::server = nullptr;
uint8_t *WebManager::certData = nullptr;
uint8_t *WebManager::pkData = nullptr;
SSLCert *WebManager::cert = nullptr;
HTTPSServer *WebManager::serverSecure = nullptr;
bool WebManager::rebootRequired = false;
bool WebManager::rebootNextLoop = false;
uint8_t *WebManager::buffer = new uint8_t[ESP_GW_WEBSERVER_BUFFER_SIZE];

bool WebManager::init()
{
  if (!initCertificate())
  {
    Serial.println("Could not init HTTPS certificate");
    return false;
  }

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
  if (GwSettings::hasCert())
  {
    Serial.println("Loading stored HTTPS certificate");

    // if name does not match the certificate name, don't load but regenerate instead
    if (strcmp(GwSettings::getCertName(), GwSettings::getName()) == 0)
    {
      Serial.printf("Loaded cert from nvs [%s.local][cert=%d][pk=%d]\n",
                    GwSettings::getCertName(),
                    GwSettings::getCertLen(),
                    GwSettings::getPkLen());

      cert = new SSLCert(GwSettings::getCert(), GwSettings::getCertLen(), GwSettings::getPk(), GwSettings::getPkLen());
    }
  }

  if (cert == nullptr)
  {
    // requires larger stack #define CONFIG_ARDUINO_LOOP_STACK_SIZE 10240
    // should run this in a separate task as sdk can't be configured via platformio.ini
    Serial.println("Generating new HTTPS certificate");

    cert = new SSLCert();
    std::string dn = "CN=";
    dn += GwSettings::getName();
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
    Serial.printf("Creating the certificate was successful [%s.local][cert=%d][pk=%d]\n", GwSettings::getName(), cert->getCertLength(), cert->getPKLength());
    GwSettings::setCertName(GwSettings::getName(), GwSettings::getNameLen());
    GwSettings::setCert(cert->getCertData(), cert->getCertLength());
    GwSettings::setPk(cert->getPKData(), cert->getPKLength());
  }

  return true;
}

void WebManager::middlewareAuthentication(HTTPRequest *req, HTTPResponse *res, std::function<void()> next)
{
  Serial.println("Auth middleware started");
  std::string reqPassword = req->getBasicAuthPassword();

  if (reqPassword.length() == 0 || strcmp(GwSettings::getPassword(), reqPassword.c_str()) != 0)
  {
    res->setStatusCode(401);
    res->setStatusText("Unauthorized");
    res->setHeader("Content-Type", "text/plain");
    res->setHeader("WWW-Authenticate", "Basic realm=\"Gateway admin area\"");
    // Small error text on the response document. In a real-world scenario, you
    // shouldn't display the login information on this page, of course ;-)
    res->println("401. Unauthorized (defaults are admin/admin)");
    Serial.println("Auth failed");
  }
  else
  {
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

  config["name"] = GwSettings::getName();

  if (GwSettings::getSsidLen() > 0)
  {
    config["wifi_ssid"] = GwSettings::getSsid();
  }

  if (GwSettings::getPassLen() > 0)
  {
    config["wifi_pass"] = GwSettings::getPass();
  }

  config["aes_key"] = GwSettings::getAes();

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
    GwSettings::setName(name, strlen(name) + 1);
    rebootRequired = true;
  }

  const char *newPassword = config["password"];
  if (newPassword && strlen(newPassword) > 0)
  {
    Serial.printf("Setting new admin password [%s]\n", newPassword);
    GwSettings::setPassword(newPassword, strlen(newPassword) + 1);
    // delete[] newPassword;
    rebootRequired = true;
  }

  const char *ssid = config["wifi_ssid"];
  if (ssid && strlen(ssid) > 0)
  {
    Serial.printf("Setting new WiFi SSID [%s]\n", ssid);
    GwSettings::setSsid(ssid, strlen(ssid) + 1);
    rebootRequired = true;
  }

  const char *pass = config["wifi_pass"];
  if (pass && strlen(pass) > 0)
  {
    Serial.printf("Setting new WiFi Password [%s]\n", pass);
    GwSettings::setPass(pass, strlen(pass) + 1);
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
  GwSettings::clear();
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

  std::string dn;
  dn = "https://";
  dn += GwSettings::getName();
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