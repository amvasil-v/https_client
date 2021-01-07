#include <stdio.h>
#include <string.h>
#include "https.h"

#include "netio_netsocket.h"

int main(int argc, char *argv[])
{
    char *url;
    char data[1024], response[4096];
    int i, ret, size;

    HTTP_INFO hi;

    // Init http session. verify: check the server CA cert.
    netio_t *io = netio_netsocket_create();
    http_init(&hi, TRUE, io);

    // Test https get method.

    url = "https://xkcd.com/info.0.json";

    ret = http_get(&hi, url, response, sizeof(response), io);

    printf("return code: %d \n", ret);
    printf("return body: %s \n", response);

    http_close(&hi);

    netio_netsocket_free(io);

    return 0;
}
