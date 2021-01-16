#include "esp_utils.h"

const char *esp_find_substr(const char *hay, size_t hay_len, const char *needle, size_t needle_len)
{
    int i,j;
    int found;

    if (hay_len == 0 || needle_len == 0 || hay_len < needle_len)
        return NULL;

    for (i = 0; i < hay_len - needle_len + 1; i++) {
        if (hay[i] == needle[0]) {
            found = 1;
            for (j = 1; j < needle_len; j++) {
                if (hay[i+j] != needle[j]) {
                    found = 0;
                    break;
                }
            }
            if (found)
                return &hay[i];
        }
    }

    return NULL;
}