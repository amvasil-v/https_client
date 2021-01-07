#ifndef NETIO_NETSOCKET_H
#define NETIO_NETSOCKET_H

#include "netio.h"
#include "mbedtls/net.h"

struct _netio_netsocket_t
{
    netio_t io;
    mbedtls_net_context net_ctx;
};

typedef struct _netio_netsocket_t netio_netsocket_t;

netio_t *netio_netsocket_create(void);
void netio_netsocket_free(netio_t *io);

#endif