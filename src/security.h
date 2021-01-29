#ifndef ESP_GW_SECURITY_H
#define ESP_GW_SECURITY_H

#include <hwcrypto/aes.h>

#define BLOCK_SIZE 16

class Security {
  public:
    Security();
    Security(const char *aesKey);
    ~Security();
    void setKey(const char *aesKey);
    void getKey(uint8_t *aesKey);
    void generateIV(uint8_t IV[BLOCK_SIZE]);
    size_t getPadedSize(size_t dataLength);
    size_t encrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *encrypted);
    size_t decrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *decrypted);
    
    static void generateKey(char *newKey);
    static uint8_t fromHex(const char *data, const size_t dataLength, uint8_t *out);
    static uint8_t toHex(const uint8_t *data, const size_t dataLength, char *out);
  private:
    uint8_t key[BLOCK_SIZE];
    size_t keyLength;
    esp_aes_context aesContext;
};

#endif