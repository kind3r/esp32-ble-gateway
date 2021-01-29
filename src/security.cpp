#include "security.h"
#include "stdlib.h"
#include "string.h"
#include <esp_system.h>
#include <esp_log.h>
#include <esp_types.h>
#include "Arduino.h"

Security::Security(const char *aesKey)
{
  esp_aes_init(&aesContext);
  setKey(aesKey);
}

Security::Security()
{
  keyLength = 0;
  esp_aes_init(&aesContext);
}

Security::~Security()
{
  esp_aes_free(&aesContext);
}

void Security::generateKey(char *newKey) {
  uint8_t key[BLOCK_SIZE];
  esp_fill_random(key, BLOCK_SIZE);
  toHex(key, BLOCK_SIZE, newKey);
}

void Security::setKey(const char *aesKey)
{
  keyLength = strlen(aesKey) / 2;
  uint8_t localKey[keyLength];
  fromHex(aesKey, keyLength * 2, localKey);
  ESP_LOG_BUFFER_HEX("KEY      ", localKey, keyLength);
  memcpy(key, localKey, BLOCK_SIZE);
  esp_aes_setkey(&aesContext, key, keyLength * 8);
}

void Security::getKey(uint8_t *aesKey) {
  memcpy(aesKey, key, keyLength);
}

void Security::generateIV(uint8_t IV[BLOCK_SIZE])
{
  uint8_t localIV[BLOCK_SIZE];
  esp_fill_random(localIV, BLOCK_SIZE);
  for (uint8_t i = 0; i < BLOCK_SIZE; i++)
  {
    uint8_t nib1 = (localIV[i] >> 4) & 0x0F;
    uint8_t nib2 = (localIV[i] >> 0) & 0x0F;
    IV[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
    IV[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  }
  IV[BLOCK_SIZE] = '\0';
}

size_t Security::getPadedSize(size_t dataLength)
{
  return dataLength + BLOCK_SIZE - dataLength % BLOCK_SIZE;
}

size_t Security::encrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *encrypted)
{
  uint8_t localIV[BLOCK_SIZE];
  memcpy(localIV, IV, BLOCK_SIZE);

  // Zero padding
  size_t padedDataLength = getPadedSize(dataLength);
  uint8_t padedData[padedDataLength];
  memset(padedData, 0, padedDataLength);
  memcpy(padedData, data, dataLength);
  int result = esp_aes_crypt_cbc(&aesContext, ESP_AES_ENCRYPT, padedDataLength, localIV, padedData, encrypted);
  // check result
  if (result != 0)
  {
    return 0;
  }
  return padedDataLength;
}

size_t Security::decrypt(const uint8_t IV[BLOCK_SIZE], const uint8_t *data, size_t dataLength, uint8_t *decrypted)
{
  uint8_t localIV[BLOCK_SIZE];
  memcpy(localIV, IV, BLOCK_SIZE);

  int result = esp_aes_crypt_cbc(&aesContext, ESP_AES_DECRYPT, dataLength, localIV, data, decrypted);
  if (result != 0)
  {
    return 0;
  }
  // remove the end 0 padding ?
  return dataLength;
}

uint8_t Security::fromHex(const char *data, const size_t dataLength, uint8_t *out)
{
  uint8_t outLen = dataLength / 2;
  char tmp[3];
  tmp[2] = '\0';
  for (int i = 0; i < outLen; i++)
  {
    tmp[0] = data[i * 2];
    tmp[1] = data[i * 2 + 1];
    out[i] = strtoul(tmp, NULL, 16);
    // ESP_LOG_BUFFER_HEXDUMP("Response", tmp, 2, esp_log_level_t::ESP_LOG_INFO);
    // Serial.println(out[i], HEX);
  }
  return outLen;
}

uint8_t Security::toHex(const uint8_t *data, const size_t dataLength, char *out)
{
  for (uint8_t i = 0; i < dataLength; i++)
  {
    uint8_t nib1 = (data[i] >> 4) & 0x0F;
    uint8_t nib2 = (data[i] >> 0) & 0x0F;
    out[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
    out[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  }
  out[dataLength * 2] = '\0';
  return dataLength * 2 + 1;
}