#include "util.h"
#include <Arduino.h>

void meminfo()
{
  const int min_free_8bit_cap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const int min_free_32bit_cap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);

  Serial.printf("||   Miniumum Free DRAM\t|   Minimum Free IRAM\t|| \n");
  Serial.printf("||\t%-6d\t\t|\t%-6d\t\t||\n",
         min_free_8bit_cap, (min_free_32bit_cap - min_free_8bit_cap));
}

void meminfo(const char description[]) {
  Serial.println(description);
  meminfo();
}