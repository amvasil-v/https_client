#include "netio_netsocket.h"

#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>

/*
 * Initiate a TCP connection with host:port and the given protocol
 * waiting for timeout (ms)
 */
static int mbedtls_net_connect_timeout( mbedtls_net_context *ctx, const char *host, const char *port,
                                        int proto, uint32_t timeout )
{
    int ret;
    struct addrinfo hints, *addr_list, *cur;


    signal( SIGPIPE, SIG_IGN );

    /* Do name resolution with both IPv6 and IPv4 */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

    if( getaddrinfo( host, port, &hints, &addr_list ) != 0 )
        return( MBEDTLS_ERR_NET_UNKNOWN_HOST );

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for( cur = addr_list; cur != NULL; cur = cur->ai_next )
    {
        ctx->fd = (int) socket( cur->ai_family, cur->ai_socktype,
                                cur->ai_protocol );
        if( ctx->fd < 0 )
        {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if( mbedtls_net_set_nonblock( ctx ) < 0 )
        {
            close( ctx->fd );
            ctx->fd = -1;
            ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
            break;
        }

        if( connect( ctx->fd, cur->ai_addr, cur->ai_addrlen ) == 0 )
        {
            ret = 0;
            break;
        }
        else if( errno == EINPROGRESS )
        {
            int            fd = (int)ctx->fd;
            int            opt;
            socklen_t      slen;
            struct timeval tv;
            fd_set         fds;

            while(1)
            {
                FD_ZERO( &fds );
                FD_SET( fd, &fds );

                tv.tv_sec  = timeout / 1000;
                tv.tv_usec = ( timeout % 1000 ) * 1000;

                ret = select( fd+1, NULL, &fds, NULL, timeout == 0 ? NULL : &tv );
                if( ret == -1 )
                {
                    if(errno == EINTR) continue;
                }
                else if( ret == 0 )
                {
                    close( fd );
                    ctx->fd = -1;
                    ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
                }
                else
                {
                    ret = 0;

                    slen = sizeof(int);
                    if( (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&opt, &slen) == 0) && (opt > 0) )
                    {
                        close( fd );
                        ctx->fd = -1;
                        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
                    }
                }

                break;
            }

            break;
        }

        close( ctx->fd );
        ctx->fd = -1;
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    freeaddrinfo( addr_list );

    if( (ret == 0) && (mbedtls_net_set_block( ctx ) < 0) )
    {
        close( ctx->fd );
        ctx->fd = -1;
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    return( ret );
}

static int netio_netsocket_send(void *ctx, const unsigned char *buf, size_t len)
{
    netio_t *io = (netio_t *)ctx;
    printf("Send %d bytes\n", len);
    return mbedtls_net_send(io->ctx, buf, len);
}

static int netio_netsocket_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)
{
    netio_t *io = (netio_t *)ctx;
    int res = mbedtls_net_recv_timeout(io->ctx, buf, len, timeout);
    printf("Recv %d bytes\n", len);
    return res;
}

static int netio_netsocket_connect(void *ctx, const char *host, const char *port)
{
    netio_t *io = (netio_t *)ctx;
    return mbedtls_net_connect_timeout(io->ctx, host, port, MBEDTLS_NET_PROTO_TCP, 5000);
}

static uint8_t netio_netsocket_opened(void *ctx)
{
    netio_t *io = (netio_t *)ctx;
    mbedtls_net_context *net_ctx = (mbedtls_net_context *)io->ctx;
    return net_ctx->fd > 0;
}

static uint8_t netio_netsocket_connected(void *ctx)
{
    netio_t *io = (netio_t *)ctx;
    mbedtls_net_context *net_ctx = (mbedtls_net_context *)io->ctx;
    int sock_fd = net_ctx->fd;
    int slen = sizeof(int);
    int opt;

    if ((getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, (void *)&opt, &slen) < 0) || (opt > 0))
    {
        return 0;
    }
    return 1;
}

netio_t *netio_netsocket_create(void)
{
    netio_netsocket_t *ns_io = (netio_netsocket_t *)malloc(sizeof(netio_netsocket_t));
    netio_t *io = &ns_io->io;

    _Static_assert(offsetof(netio_netsocket_t, io) == 0, "Offset of netio_netsocket_t->io must be 0");
    io->send = netio_netsocket_send;
    io->recv_timeout = netio_netsocket_recv_timeout;
    io->connect = netio_netsocket_connect;
    io->opened = netio_netsocket_opened;
    io->connected = netio_netsocket_connected;
    io->ctx = &ns_io->net_ctx;    
    mbedtls_net_init((mbedtls_net_context *)io->ctx);
    return io;
}

void netio_netsocket_free(netio_t *io)
{
    mbedtls_net_free((mbedtls_net_context *)io->ctx);
    free(io);
}
