#ifndef ESP_BRIDGE_H
#define ESP_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

struct _esp_bridge_t {
    int socket_fd;
};
typedef struct _esp_bridge_t esp_bridge_t;

esp_bridge_t *esp_bridge_create();
void esp_bridge_init(esp_bridge_t * br);
int esp_bridge_connect(esp_bridge_t * br, const char *host, uint16_t port);
void esp_bridge_disconnect(esp_bridge_t * br);
void esp_bridge_free(esp_bridge_t * br);
int esp_bridge_write(esp_bridge_t * br, const char *buf, size_t len);
int esp_bridge_read_timeout(esp_bridge_t * br, const char *buf, size_t len, uint32_t timeout);
uint8_t esp_bridge_connected(esp_bridge_t * br);

#endif