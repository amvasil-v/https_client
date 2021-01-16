#ifndef ESP_UTILS_H
#define ESP_UTILS_H

#include <stddef.h>

const char *esp_find_substr(const char *hay, size_t hay_len, const char *needle, size_t needle_len);

#endif 