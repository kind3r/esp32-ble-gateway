#include "gw_settings.h"

bool GwSettings::ready = false;
Preferences GwSettings::prefs;
char *GwSettings::name = nullptr;
size_t GwSettings::nameLen = 0;
char *GwSettings::password = nullptr;
size_t GwSettings::passwordLen = 0;
char *GwSettings::ssid = nullptr;
size_t GwSettings::ssidLen = 0;
char *GwSettings::pass = nullptr;
size_t GwSettings::passLen = 0;
char *GwSettings::aes = nullptr;
char *GwSettings::certName = nullptr;
size_t GwSettings::certNameLen = 0;
uint8_t *GwSettings::cert = nullptr;
size_t GwSettings::certLen = 0;
uint8_t *GwSettings::pk = nullptr;
size_t GwSettings::pkLen = 0;

bool GwSettings::init()
{
  prefs.begin("ESP32GW");

  if (!prefs.isKey("name"))
  { // default name
    nameLen = 7;
    name = "esp32gw";
    prefs.putBytes("name", name, nameLen);
  }
  else
  {
    nameLen = prefs.getBytesLength("name") + 1;
    name = new char[nameLen];
    prefs.getBytes("name", name, nameLen - 1);
    name[nameLen - 1] = '\0';
  }

  if (!prefs.isKey("password"))
  { // default admin password
    passwordLen = 5;
    password = "admin";
    prefs.putBytes("password", password, passwordLen);
  }
  else
  {
    passwordLen = prefs.getBytesLength("password") + 1;
    password = new char[passwordLen];
    prefs.getBytes("password", password, passwordLen - 1);
    password[passwordLen - 1] = '\0';
  }

  if (prefs.isKey("ssid"))
  {
    ssidLen = prefs.getBytesLength("ssid") + 1;
    ssid = new char[ssidLen];
    prefs.getBytes("ssid", ssid, ssidLen - 1);
    ssid[ssidLen - 1] = '\0';
  }

  if (prefs.isKey("pass"))
  {
    passLen = prefs.getBytesLength("pass") + 1;
    pass = new char[passLen];
    prefs.getBytes("pass", pass, passLen - 1);
    pass[passLen - 1] = '\0';
  }

  aes = new char[BLOCK_SIZE * 2 + 1];
  if (!prefs.isKey("aes"))
  { // new AES key
    Security::generateKey(aes);
    prefs.putBytes("aes", (uint8_t *)aes, BLOCK_SIZE * 2);
  }
  else
  {
    prefs.getBytes("aes", aes, BLOCK_SIZE * 2);
    aes[BLOCK_SIZE * 2] = '\0';
  }

  if (prefs.isKey("cert_name")) {
    certNameLen = prefs.getBytesLength("cert_name") + 1;
    certName = new char[certNameLen];
    prefs.getBytes("cert_name", certName, certNameLen - 1);
    certName[certNameLen - 1] = '\0';
  }

  if (prefs.isKey("cert")) {
    certLen = prefs.getBytesLength("cert");
    cert = new uint8_t[certLen];
    prefs.getBytes("cert", cert, certLen);
  }

  if (prefs.isKey("pk")) {
    pkLen = prefs.getBytesLength("pk");
    pk = new uint8_t[pkLen];
    prefs.getBytes("pk", pk, pkLen);
  }

  ready = true;
  return true;
}

bool GwSettings::isConfigured()
{
  return (ssidLen > 0 && passLen > 0);
}

void GwSettings::clear() {
  prefs.clear();
}

char *GwSettings::getName()
{
  return name;
}

size_t GwSettings::getNameLen()
{
  return nameLen;
}

void GwSettings::setName(const char *val, size_t len) {
  prefs.putBytes("name", val, len - 1);
  delete[] name;
  nameLen = len;
  name = new char[nameLen];
  memcpy(name, val, nameLen);
}

char *GwSettings::getPassword()
{
  return password;
}

size_t GwSettings::getPasswordLen()
{
  return passwordLen;
}

void GwSettings::setPassword(const char *val, size_t len) {
  prefs.putBytes("password", val, len - 1);
  delete[] password;
  passwordLen = len;
  password = new char[passwordLen];
  memcpy(password, val, passwordLen);
}

char *GwSettings::getSsid()
{
  return ssid;
}

size_t GwSettings::getSsidLen()
{
  return ssidLen;
}

void GwSettings::setSsid(const char *val, size_t len) {
  prefs.putBytes("ssid", val, len - 1);
  delete[] ssid;
  ssidLen = len;
  ssid = new char[ssidLen];
  memcpy(ssid, val, ssidLen);
}

char *GwSettings::getPass()
{
  return pass;
}

size_t GwSettings::getPassLen()
{
  return passLen;
}

void GwSettings::setPass(const char *val, size_t len) {
  prefs.putBytes("pass", val, len - 1);
  delete[] pass;
  passLen = len;
  pass = new char[passLen];
  memcpy(pass, val, passLen);
}

char *GwSettings::getAes()
{
  return aes;
}

bool GwSettings::hasCert() {
  return (certNameLen > 0 && certLen > 0 && pkLen > 0);
}

char *GwSettings::getCertName() {
  return certName;
}

size_t GwSettings::getCertNameLen() {
  return certNameLen;
}

void GwSettings::setCertName(const char *val, size_t len) {
  prefs.putBytes("cert_name", val, len - 1);
  delete[] certName;
  certNameLen = len;
  certName = new char[certNameLen];
  memcpy(certName, val, certNameLen);
}

uint8_t *GwSettings::getCert() {
  return cert;
}

size_t GwSettings::getCertLen() {
  return certLen;
}

void GwSettings::setCert(const uint8_t *val, size_t len) {
  prefs.putBytes("cert", val, len);
  delete[] cert;
  certLen = len;
  cert = new uint8_t[certLen];
  memcpy(cert, val, certLen);
}

uint8_t *GwSettings::getPk() {
  return pk;
}

size_t GwSettings::getPkLen() {
  return pkLen;
}

void GwSettings::setPk(const uint8_t *val, size_t len) {
  prefs.putBytes("pk", val, len);
  delete[] pk;
  pkLen = len;
  pk = new uint8_t[pkLen];
  memcpy(pk, val, pkLen);
}