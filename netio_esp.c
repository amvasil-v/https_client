#include "netio_esp.h"

#include <stdlib.h>

static int netio_esp_send(void *ctx, const unsigned char *buf, size_t len)
{
    netio_t *io = (netio_t *)ctx;
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    printf("Send %d bytes\n", len);
    return esp_bridge_write(br, buf, len);
}

static int netio_esp_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)
{
    netio_t *io = (netio_t *)ctx;
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    int res = esp_bridge_read_timeout(br, buf, len, timeout);
    printf("Recv %d bytes, res %d\n", len, res);
    return res;
}

static int netio_esp_connect(void *ctx, const char *host, const char *port)
{
    netio_t *io = (netio_t *)ctx;
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return -1; //TODO
}

static uint8_t netio_esp_connected(void *ctx)
{
    netio_t *io = (netio_t *)ctx;
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return esp_bridge_connected(br);
} 

netio_t *netio_esp_create(void)
{
    netio_esp_t *ns_io = (netio_esp_t *)malloc(sizeof(netio_esp_t));
    netio_t *io = &ns_io->io;

    _Static_assert(offsetof(netio_esp_t, io) == 0, "Offset of netio_esp_t->io must be 0");
    io->send = netio_esp_send;
    io->recv_timeout = netio_esp_recv_timeout;
    io->connect = netio_esp_connect;
    io->opened = netio_esp_connected;
    io->connected = netio_esp_connected;
    io->ctx = &ns_io->br;    
    mbedtls_net_init((esp_bridge_t *)io->ctx);
    return io;
}


int netio_esp_establish_bridge(netio_t *io, const char *host, uint16_t port)
{
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return esp_bridge_connect(br, host, port);
}


void netio_esp_free(netio_t *io)
{
    esp_bridge_t *br = (esp_bridge_t *)io->ctx;
    return esp_bridge_disconnect(br);
    free(io);
}