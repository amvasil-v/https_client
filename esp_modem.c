#include "esp_modem.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_utils.h"

#define ESP_MODEM_RX_BUF_SIZE 256
#define ESP_MODEM_RX_DATA_BUF_SIZE 2048

#define MAX(X,Y) ((X)>(Y)?(X):(Y))
#define MIN(X,Y) ((X)<(Y)?(X):(Y))

static const char *esp_modem_ipd_str = "\r\n+IPD,";
static const char *esp_ok_str = "\r\nOK\r\n";
static const char *esp_error_str = "\r\nERROR\r\n";

static uint8_t esp_receive_buf_complete(const char *buf, size_t len)
{
    
    return esp_find_substr(buf, len, esp_ok_str, strlen(esp_ok_str)) ||
           esp_find_substr(buf, len, esp_error_str, strlen(esp_error_str));
}

static uint8_t esp_receive_buf_ready_tx(const char *buf, size_t len)
{
    return esp_find_substr(buf, len, esp_ok_str, strlen(esp_ok_str)) != NULL;
}

void esp_modem_init(esp_modem_t *esp, esp_bridge_t *bridge)
{
    esp->br = bridge;
    esp->rx_buf = (char *)malloc(2048);
    esp->tx_buf = (char *)malloc(ESP_MODEM_RX_BUF_SIZE);
    esp->rx_data_buf = (char *)malloc(ESP_MODEM_RX_DATA_BUF_SIZE);
}

void esp_modem_release(esp_modem_t *esp)
{
    free(esp->rx_buf);
    free(esp->tx_buf);
    free(esp->rx_data_buf);
}

int esp_modem_tcp_connect(esp_modem_t *esp, const char *host, const char *port, uint32_t timeout)
{
    uint32_t tim;
    uint32_t read_timeout = 10;
    int res;
    size_t rx_len = 0;
    static const char *connect_str = "CONNECT\r\n";
    static const char *acon_str = "ALREADY CONNECTED\r\n";
    if (!esp_bridge_connected(esp->br))
        return -1;
    esp->rx_wptr = esp->rx_buf;

    sprintf(esp->tx_buf, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", host, port);
    res = esp_bridge_write(esp->br, esp->tx_buf, strlen(esp->tx_buf));
    if (res < 0)
        return -1;
    char *ptr = esp->rx_buf;
    for (tim = 0; tim < timeout; tim += read_timeout) {
        res = esp_bridge_read_timeout(esp->br, ptr, 128, read_timeout);
        if (res <= 0)
            continue;
        ptr += res;
        rx_len += res;
        if (esp_receive_buf_complete(esp->rx_buf, rx_len)) {
            if (esp_find_substr(esp->rx_buf, rx_len, connect_str, strlen(connect_str)) ||
                esp_find_substr(esp->rx_buf, rx_len, acon_str, strlen(acon_str))) {
                    printf("TCP: connected to %s:%s\n", host, port);
                    return 0;
                }
            printf("Received invalid response: %s\n", esp->rx_buf);
            return -1;
        }
        if (ptr > esp->rx_buf + ESP_MODEM_RX_BUF_SIZE)
            return -1;
    }
    printf("Connection timeout\n");
    return -1;
}

static int esp_modem_terminate_rx_buf(esp_modem_t *esp)
{
    if ((ptrdiff_t)(esp->rx_wptr - esp->rx_buf) >= ESP_MODEM_RX_BUF_SIZE)
    {
        esp->rx_wptr = 0;
        printf("TCP rx buffer overflow\n");
        return -1;
    }
    *esp->rx_wptr = '\0';
    return 0;
}

static int esp_confirm_tcp_send(esp_modem_t *esp)
{
    int res;
    int i;
    char *confirm;
    static const char *confirm_str = "\r\nSEND OK\r\n";
    size_t rx_len = 0;

    // reset append read pointers
    esp->rx_wptr = esp->rx_buf;
    esp->rx_data_wptr = esp->rx_data_buf;

    for (i = 0; i < 10; i++) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, 43, 100);
        if (res <= 0)
            continue;
        esp->rx_wptr += res;
        rx_len += res;
        esp_modem_terminate_rx_buf(esp);
        confirm = esp_find_substr(esp->rx_buf, rx_len, confirm_str, strlen(confirm_str));
        if (confirm) {
            printf("TX confirmed\n");
            return 0;
        }
    }
    printf("TX confirm timeout\n");
    return -1;
}

int esp_modem_tcp_send(esp_modem_t *esp, const char *buf, size_t len)
{
    int res;
    int i;
    static const char *newline = "\r\n";
    if (!esp_bridge_connected(esp->br))
        return -1;    
    int cipsend_len = len + strlen(newline);
    sprintf(esp->tx_buf, "AT+CIPSEND=%d\r\n", cipsend_len);
    esp->ipd_len = 0; // reset IPD state
    res = esp_bridge_write(esp->br, esp->tx_buf, strlen(esp->tx_buf));
    if (res < 0)
        return -1;
    printf("Sent cipsend %d\n", cipsend_len);

    char *ptr = esp->rx_buf;
    for (i = 0; i < 10; i++) {
        res = esp_bridge_read_timeout(esp->br, ptr, 128, 10);
        if (res <= 0)
            continue;
        ptr += res;
        if (esp_receive_buf_ready_tx(esp->rx_buf, ptr - esp->rx_buf)) {
            printf("Ready to TX TCP\n");
            res = esp_bridge_write(esp->br, buf, len);
            if (res < 0)
                return -1;
            res = esp_bridge_write(esp->br, newline, strlen(newline));
            if (res < 0)
                return -1;
            if (!esp_confirm_tcp_send(esp))
                return len;
            else
                return -1;
        }
    }
    printf("TX timeout\n");
    return -1;
}

#define ESP_MODEM_IPD_MAX_LEN       4
#define ESP_MODEM_IPD_MAX_VALUE     1400
#define ESP_MODEM_READ_CHUNK_SIZE   128

static const char *esp_modem_get_ipd(const char *str, uint32_t *out_length)
{
    int i;
    char buf[ESP_MODEM_IPD_MAX_LEN + 1] = {0};
    int out;

    for (i = 1; i <= 4; i++) {
        if (str[i] == ':') {
            strncpy(buf, str, i);
            out = atoi(buf);
            if (out <= 0 || out >= ESP_MODEM_IPD_MAX_VALUE)
                return NULL;
            *out_length = out;
            return &str[i+1];
        }
    }

    return NULL;
}

static void copy_bytes(char *dst, char *from, char *to)
{
    while (from != to) {
        *dst++ = *from++;
    }
}

static int esp_modem_receive_data(esp_modem_t *esp, char *out_buf, size_t req_len, uint32_t timeout)
{
    size_t rx_data_len = esp->rx_data_wptr - esp->rx_data_buf;
    char *out_ptr = out_buf;
    size_t written_len = 0;
    size_t rx_data_read_len = MIN(rx_data_len, req_len);

    if (rx_data_read_len) {
        memcpy(out_ptr, esp->rx_data_buf, rx_data_read_len);
        out_ptr += rx_data_read_len;
        written_len += rx_data_read_len;
        if (rx_data_len > req_len) {
            // copy remaining bytes to the start of the data buffer
            copy_bytes(esp->rx_data_buf, esp->rx_data_buf + rx_data_read_len, esp->rx_data_wptr);
            esp->rx_data_wptr -= rx_data_read_len;
        }     
    }

    if (written_len == req_len) {
        esp->ipd_len -= written_len;
        return req_len;
    }

    size_t rem_len = MIN(req_len, esp->ipd_len) - written_len;
    printf("Reading %lu bytes\n", rem_len);
    int res = esp_bridge_read_timeout(esp->br, esp->rx_data_wptr, rem_len, timeout);
    if (res <= 0)
    {
        printf("TCP read data timeout %d\n", res);
        return -1;
    }
    memcpy(out_ptr, esp->rx_data_wptr, res);
    written_len += res;
    esp->ipd_len -= written_len;
    esp->rx_data_wptr = esp->rx_data_buf;
    return written_len;    
}

static int esp_modem_receive_ipd(esp_modem_t *esp, uint32_t timeout)
{
    int res;
    char *ipd_ptr;
    char *data_ptr;
    uint32_t tim = 0;
    uint32_t ipd_len = 0;
    const uint32_t read_timeout = timeout > 100 ? 100 : timeout;

    while (tim < timeout) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, strlen(esp_modem_ipd_str) + 6, read_timeout);
        if (res <= 0) {
            tim += read_timeout;
            continue;
        }
        esp->rx_wptr += res;
        esp_modem_terminate_rx_buf(esp);
        ipd_ptr = esp_find_substr(esp->rx_buf, esp->rx_wptr - esp->rx_buf, esp_modem_ipd_str, strlen(esp_modem_ipd_str));
        if (!ipd_ptr) {
            continue;
        }
        data_ptr = (char *)esp_modem_get_ipd(ipd_ptr + strlen(esp_modem_ipd_str), &ipd_len);
        if (!data_ptr) {
            printf("Failed to parse IPD\n");
            return -1;
        }
        printf("Found IPD: %d\n", ipd_len);
        // copy extra bytes to the data buffer
        size_t extra = esp->rx_wptr - data_ptr;
        memcpy(esp->rx_data_buf, data_ptr, extra);
        esp->rx_data_wptr = esp->rx_data_buf + extra;

        esp->rx_wptr = esp->rx_data_buf;
        esp->ipd_len = ipd_len;
        return 0;
    };

    printf("TCP IPD read timeout\n");
    return -1;
}

int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout)
{
    int res;

    if (len == 0)
        return 0;

    if (!esp->ipd_len)
    {
        res = esp_modem_receive_ipd(esp, timeout);
        if (res)
            return -1;
        esp->rx_wptr = esp->rx_buf;
    }
    if (esp->ipd_len)
    {
        return esp_modem_receive_data(esp, buf, len, timeout);
    }

    printf("TCP read timeout\n");
    return -1;
}

