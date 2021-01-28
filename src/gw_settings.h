#ifndef ESP_GW_SETTINGS_H
#define ESP_GW_SETTINGS_H

#include <Preferences.h>
#include "security.h"

class GwSettings {
  public:
    static bool init();
    static bool isConfigured();
    static void clear();

    static char *getName();
    static size_t getNameLen();
    static void setName(const char *val, size_t len);

    static char *getPassword();
    static size_t getPasswordLen();
    static void setPassword(const char *val, size_t len);

    static char *getSsid();
    static size_t getSsidLen();
    static void setSsid(const char *val, size_t len);

    static char *getPass();
    static size_t getPassLen();
    static void setPass(const char *val, size_t len);

    static char *getAes();

    static bool hasCert();

    static char *getCertName();
    static size_t getCertNameLen();
    static void setCertName(const char *val, size_t len);

    static uint8_t *getCert();
    static size_t getCertLen();
    static void setCert(const uint8_t *val, size_t len);

    static uint8_t *getPk();
    static size_t getPkLen();
    static void setPk(const uint8_t *val, size_t len);

  private:
    static bool ready;
    static Preferences prefs;

    static char *name;
    static size_t nameLen;

    static char *password;
    static size_t passwordLen;

    static char *ssid;
    static size_t ssidLen;

    static char *pass;
    static size_t passLen;

    static char *aes;

    static char *certName;
    static size_t certNameLen;

    static uint8_t *cert;
    static size_t certLen;

    static uint8_t *pk;
    static size_t pkLen;
};



#endif