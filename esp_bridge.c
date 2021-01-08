#include "esp_bridge.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

esp_bridge_t *esp_bridge_create()
{
    esp_bridge_t *br = (esp_bridge_t *)malloc(sizeof(esp_bridge_t));
    esp_bridge_init(br);
    return br;
}

void esp_bridge_init(esp_bridge_t * br)
{
    memset(br, 0, sizeof(esp_bridge_t));
}

int esp_bridge_connect(esp_bridge_t * br, const char *host, uint16_t port)
{
    int                ret = 0;
	int                conn_fd;
	struct sockaddr_in server_addr = { 0 };

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    ret = inet_pton(AF_INET, host, &server_addr.sin_addr);
	if (ret != 1) {
		if (ret == -1) {
			perror("inet_pton");
		}
		fprintf(stderr,
		        "failed to convert address %s "
		        "to binary net address\n",
		        host);
		return -1;
	}
    fprintf(stdout, "CONNECTING: address=%s port=%d\n", host, port);
    conn_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (conn_fd == -1) {
		perror("socket");
		return -1;
	}

    ret = connect(conn_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (ret == -1) {
		perror("connect");
		return -1;
	}

    br->socket_fd = conn_fd;
    return 0;
}

void esp_bridge_disconnect(esp_bridge_t * br)
{
    int ret;

    if (br && br->socket_fd > 0)
    {
        ret = shutdown(br->socket_fd, SHUT_RDWR);
        if (ret == -1)
        {
            perror("shutdown");
        }
        ret = close(br->socket_fd);
	    if (ret == -1) {
		    perror("close");
	    }
        br->socket_fd = 0;
    }
}

void esp_bridge_free(esp_bridge_t *br)
{
    esp_bridge_disconnect(br);
    free(br);
}

uint8_t esp_bridge_connected(esp_bridge_t * br)
{
    return br && (br->socket_fd > 0);
}

int esp_bridge_write(esp_bridge_t *br, const char *buf, size_t len)
{
    if (!esp_bridge_connected(br))
        return -1;
    return write(br->socket_fd, buf, len);
}

int esp_bridge_read_timeout(esp_bridge_t * br, const char *buf, size_t len, uint32_t timeout)
{
    
    int ret;
    struct timeval tv;
    fd_set read_fds;
    int fd = br->socket_fd;

    if (!esp_bridge_connected(br))
        return -1;

    FD_ZERO( &read_fds );
    FD_SET( fd, &read_fds );

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = ( timeout % 1000 ) * 1000;

    ret = select( fd + 1, &read_fds, NULL, NULL, timeout == 0 ? NULL : &tv );

    /* Zero fds ready means we timed out */
    if( ret == 0 )
        return -1;

    return read(fd, buf, len);
}