#ifndef ESP_MODEM_H
#define ESP_MODEM_H

#include "esp_bridge.h"

struct _esp_modem_t {
    esp_bridge_t *br;
    char *tx_buf;
    char *rx_buf;
    char *rx_wptr;
    char *rx_data_buf;
    char *rx_data_wptr;
};
typedef struct _esp_modem_t esp_modem_t;

struct _esp_string_list_t;
struct _esp_string_list_t {
    const char *str;
    size_t len;
    struct _esp_string_list_t *next;
};
typedef struct _esp_string_list_t esp_string_list_t;

esp_string_list_t *esp_string_tokenize(const char *str, const char *delim);
void esp_string_list_free(esp_string_list_t *list);
int esp_append_check_complete(char *buf, char **append, size_t len, const char *str);

int esp_modem_tcp_connect(esp_modem_t *esp, const char *host, const char *port, uint32_t timeout);
void esp_modem_init(esp_modem_t *esp, esp_bridge_t *bridge);
void esp_modem_release(esp_modem_t *esp);
int esp_modem_tcp_send(esp_modem_t *esp, const char *buf, size_t len);
int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout);

#endif