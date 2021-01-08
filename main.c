#include <stdio.h>
#include <string.h>
#include "https.h"

#include "netio_netsocket.h"
#include "esp_bridge.h"
#include "netio_esp.h"

int main(int argc, char *argv[])
{
    char *url;
    char data[1024], response[4096];
    int i, ret, size;

    HTTP_INFO hi;

    esp_bridge_t *br = esp_bridge_create();
    while(1) {
        if (esp_bridge_connect(br, "172.24.0.1", 5555)) {
            printf("Failed to connect to ESP\n");
            break;
        }
        const char *esp_cmd = "AT\r\n";
        char resp[256] = {0};
        size_t write_len = strlen(esp_cmd);
        if (esp_bridge_write(br, esp_cmd, write_len) != write_len) {
            printf("Failed to send command\n");
            break;
        }
        if (esp_bridge_read_timeout(br, resp, 256, 1000) < 0) {
            printf("Failed to receive resp\n");
            break;
        }
        printf("ESP:\r\n%s\r\n", resp);
        break;
    };
    esp_bridge_free(br);

    netio_t *esp_io = netio_esp_create();
    if (netio_esp_establish_bridge(esp_io, "172.24.0.1", 5555)) {
        printf("Failed to establish bridge\n");
    }
    else {
        printf("Connected to remote ESP\n");
    }
    netio_esp_free(esp_io);

    /*// Init http session. verify: check the server CA cert.
    netio_t *io = netio_netsocket_create();
    http_init(&hi, TRUE, io);*/

    // Test https get method.

    /*url = "https://xkcd.com/info.0.json";

    ret = http_get(&hi, url, response, sizeof(response), io);

    printf("return code: %d \n", ret);
    printf("return body: %s \n", response);

    url = "https://xkcd.com/1/info.0.json";

    ret = http_get(&hi, url, response, sizeof(response), io);

    printf("return code: %d \n", ret);
    printf("return body: %s \n", response);

    url = "http://kspt.icc.spbstu.ru/";

    ret = http_get(&hi, url, response, sizeof(response), io);

    printf("return code: %d \n", ret);
    printf("return body: %s \n", response);*/

    /*http_close(&hi);
    netio_netsocket_free(io);*/

    return 0;
}
