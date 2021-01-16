#include <stdio.h>
#include <string.h>
#include "https.h"

#include "netio_netsocket.h"
#include "esp_bridge.h"
#include "netio_esp.h"
#include "bridge_config.h"

#define NETIO_DEVICE_SOCKETS 1
#define NETIO_DEVICE_ESP_BRIDGE 2

#define NETIO_DEVICE    NETIO_DEVICE_ESP_BRIDGE

int main(int argc, char *argv[])
{
    char *url;
    char data[1024], response[4096];
    int i, ret, size;

    HTTP_INFO hi;

#if NETIO_DEVICE == NETIO_DEVICE_SOCKETS
    netio_t *io = netio_netsocket_create();
#elif NETIO_DEVICE == NETIO_DEVICE_ESP_BRIDGE
    netio_t *io = netio_esp_create();
    if (netio_esp_establish_bridge(io, ESP_BRIDGE_ADDRESS, ESP_BRIDGE_PORT)) {
        printf("Failed to establish bridge\n");
    }
    else {
        printf("Connected to remote ESP\n");
    }
#else
#error "Invalid NETIO_DEVICE"
#endif

    // Init http session. verify: check the server CA cert.
    http_init(&hi, TRUE, io);

    //url = "http://kspt.icc.spbstu.ru/en/";
    //url = "http://httpbin.org/ip";
    url = "https://xkcd.com/1/info.0.json";

    ret = http_get(&hi, url, response, sizeof(response), io);

    printf("return code: %d \n", ret);
    printf("return body:\n%s\n", response);

    http_close(&hi);
#if NETIO_DEVICE == NETIO_DEVICE_SOCKETS
    netio_netsocket_free(io);
#elif NETIO_DEVICE == NETIO_DEVICE_ESP_BRIDGE
    netio_esp_free(io);
#endif

    return 0;
}
