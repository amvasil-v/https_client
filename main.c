#include <stdio.h>
#include <string.h>
#include "https.h"

#include "netio_netsocket.h"
#include "esp_bridge.h"
#include "netio_esp.h"
#include "bridge_config.h"

int main(int argc, char *argv[])
{
    char *url;
    char data[1024], response[4096];
    int i, ret, size;

    HTTP_INFO hi;

    netio_t *esp_io = netio_esp_create();
    if (netio_esp_establish_bridge(esp_io, ESP_BRIDGE_ADDRESS, ESP_BRIDGE_PORT)) {
        printf("Failed to establish bridge\n");
    }
    else {
        printf("Connected to remote ESP\n");
    }

    if (esp_io->connect(esp_io, "httpbin.org", "80")) {
        printf("Failed to connect to TCP server\n");
    }
    else {
        printf("Connected to TCP server\n");
    }

    const char *request = "GET /ip HTTP/1.1\r\nHost:httpbin.org\r\n";
    if (esp_io->send(esp_io, request, strlen(request))) {
        printf("Failed to send request\n");
    }
    else {
        printf("HTTP request sent\n");
    }

    printf("Received HTTP:\n");
    char buf[2048];
    while(1) {
        if (esp_io->recv_timeout(esp_io, buf, 2048, 5000))
            break;
        printf("%s", buf);
    }
    printf("\nDone\n");

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
