#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "https.h"

#include "netio_netsocket.h"
#include "esp_bridge.h"
#include "netio_esp.h"
#include "bridge_config.h"

#define NETIO_DEVICE_SOCKETS 1
#define NETIO_DEVICE_ESP_BRIDGE 2

#define NETIO_DEVICE    NETIO_DEVICE_SOCKETS

static int https_image_download(HTTP_INFO *hi, netio_t *io);

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

//#define HTTP_LIB_USE
#ifdef HTTP_LIB_USE
    // Init http session. verify: check the server CA cert.
    http_init(&hi, TRUE, io);

    //url = "http://kspt.icc.spbstu.ru/en/";
    //url = "http://httpbin.org/ip";
    //url = "https://xkcd.com/1/info.0.json";

    ret = http_get(&hi, url, response, sizeof(response), io);
    printf("return code: %d \n", ret);
    printf("return body:\n%s\n", response);

    http_close(&hi);
    io->disconnect(io);
#endif

#define HTTPS_IMG_DOWNLOAD
#ifdef HTTPS_IMG_DOWNLOAD
    http_init(&hi, TRUE, io);
    https_image_download(&hi, io);
    http_close(&hi);
    io->disconnect(io);
#endif

#ifdef RAW_ESP_REQUEST
    if (io->connect(io, "httpbin.org", "80")) {
        printf("Failed to connect to TCP server\n");
    }
    else {
        printf("Connected to TCP server\n");
    }

    const char *request = "GET /ip HTTP/1.1\r\nHost:httpbin.org\r\n";
    if (io->send(io, request, strlen(request)) <= 0) {
        printf("Failed to send request\n");
    }
    else {
        printf("HTTP request sent\n");
    }

    printf("Received HTTP:\n");
    char buf[2048];
    int res;
    while(1) {
        memset(buf, 0, 2048);
        printf("HTTP resp read:\n");
        if ((res = io->recv_timeout(io, buf, 400, 2000)) <= 0)
            break;
        printf("HTTP: %d:\n", res, buf);
        int found_zero = 0;
        for(int i=0;i<res;i++) {
            putc(buf[i], stdout);
            if (!found_zero && buf[i] == 0) {
                found_zero = 1;
                printf("0");
            }
        }
        printf("\nHTTP DONE\n");
    }
    printf("Done\n");
#endif

#if NETIO_DEVICE == NETIO_DEVICE_SOCKETS
    netio_netsocket_free(io);
#elif NETIO_DEVICE == NETIO_DEVICE_ESP_BRIDGE
    netio_esp_free(io);
#endif

    return 0;
}

#define HTTPS_RANGED_SIZE   12

static int https_download(HTTP_INFO *hi, netio_t *io, int fd, const char *url)
{
    char buf[HTTPS_RANGED_SIZE+1];
    size_t content_len = 0;
    size_t pos = 0;

    while (content_len == 0 || pos < content_len) { 
        size_t range_end = pos + HTTPS_RANGED_SIZE - 1;
        size_t len = 0;
        if (content_len && range_end >= content_len)
            range_end = content_len - 1;

        printf("Get ranged %d to %d\n", pos, range_end);
        int ret = http_get_ranged(hi, url, buf, pos, range_end, &len, io);
        if ( len == 0) {
            printf("Failed to get ranged HTTPS\n");
            return -1;
        }
        if (content_len == 0)
            content_len = len;
        else if (content_len != len) {
            printf("Invalid content len %d\n", len);
            return -1;
        }

        size_t write_len = range_end - pos + 1;
        printf("Write %d\n", write_len);
        ssize_t res = write(fd, buf, write_len);
        if (write_len != res) {
            printf("Write failed\n");
            return -1;
        }

        pos = range_end + 1;
    }

    return 0;
}

static int https_image_download(HTTP_INFO *hi, netio_t *io)
{
    const char *url = "https://raw.githubusercontent.com/amvasil-v/https_client/master/README.md";    
    int fd = open("out.txt", O_CREAT | O_RDWR | O_TRUNC, 0666);
    int ret;

    if (fd <= 0) {
        printf("Failed to open out file\n");
        return -1;
    }
    printf("Download image from %s\n", url);
    ret = https_download(hi, io, fd, url);
    if (!ret)
        printf("Download complete\n");
    else
        printf("Download failed\n");

    close(fd);

    return ret;    

}
