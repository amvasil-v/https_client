#include "esp_modem.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ESP_MODEM_RX_BUF_SIZE 256
#define ESP_MODEM_RX_DATA_BUF_SIZE 2048

#define MAX(X,Y) ((X)>(Y)?(X):(Y))
#define MIN(X,Y) ((X)<(Y)?(X):(Y))

static const char *esp_modem_ipd_str = "\r\n+IPD,";

static void string_list_append(esp_string_list_t **head, esp_string_list_t **tail, const char *str, size_t len)
{
    esp_string_list_t *res;
    esp_string_list_t *new_elem = (esp_string_list_t *)malloc(sizeof(esp_string_list_t));
    if (!(*head)) {
        *head = *tail = new_elem;
    } else {
        (*tail)->next = new_elem;
        *tail = new_elem;
    }
    new_elem->next = NULL;
    new_elem->str = str;
    new_elem->len = len;
}

void esp_string_list_free(esp_string_list_t *list)
{
    while(list) {
        esp_string_list_t *ptr = list->next;
        free(list);
        list = ptr;
    }
}

esp_string_list_t *esp_string_tokenize(const char *str, const char *delim)
{
    char buf[2048];
    esp_string_list_t *head = NULL, *tail = NULL;

    if (!str || !delim)
        return head;

    strncpy(buf, str, 2048);
    char *token = strtok(buf, delim);

    while (token != NULL)
    {
        string_list_append(&head, &tail, strstr(str, token), strlen(token));
        token = strtok(NULL, delim);
    }
    return head;
}

uint8_t esp_receive_buf_complete(const char *buf)
{
    return strstr(buf, "\r\nOK\r\n") || strstr(buf, "\r\nERROR\r\n");
}

uint8_t esp_receive_buf_ready_tx(const char *buf)
{
    return strstr(buf, "\r\nOK\r\n>");
}

int esp_append_check_complete(char *buf, char **append, size_t len, const char *str)
{
    size_t msg_len = strlen(str);
    if (*append - buf + msg_len > len)
        return -1;
    strcpy(*append, str);
    *append += msg_len;
    if (esp_receive_buf_complete(buf))
        return 1;
    return 0;
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
        if (esp_receive_buf_complete(esp->rx_buf)) {
            if (strstr(esp->rx_buf, "\r\nCONNECT\r\n") ||
                strstr(esp->rx_buf, "\r\nALREADY CONNECTED\r\n")) {
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

    // reset append read pointers
    esp->rx_wptr = esp->rx_buf;
    esp->rx_data_wptr = esp->rx_data_buf;

    for (i = 0; i < 10; i++) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, 43, 100);
        if (res <= 0)
            continue;
        esp->rx_wptr += res;
        esp_modem_terminate_rx_buf(esp);
        confirm = strstr(esp->rx_buf, confirm_str);
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
        if (esp_receive_buf_ready_tx(esp->rx_buf)) {
            printf("Ready to TX TCP\n");
            res = esp_bridge_write(esp->br, buf, len);
            if (res < 0)
                return -1;
            res = esp_bridge_write(esp->br, newline, strlen(newline));
            if (res < 0)
                return -1;
            return esp_confirm_tcp_send(esp);
        }
    }
    printf("TX timeout\n");
    return -1;
}

#define ESP_MODEM_IPD_MAX_LEN       4
#define ESP_MODEM_IPD_MAX_VALUE     1400
#define ESP_MODEM_READ_CHUNK_SIZE   128

char *esp_modem_get_ipd(const char *str, uint32_t *out_length)
{
    int i;
    char buf[ESP_MODEM_IPD_MAX_LEN + 1] = {0};
    int out;

    for (i = 1; i <= 4; i++) {
        if (str[i] == ':') {
            strncpy(buf, str, i);
            out = atoi(buf);
            if (out <= 0 || out >= ESP_MODEM_IPD_MAX_VALUE)
                return -1;
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

static int esp_modem_receive_data(esp_modem_t *esp, char *out_buf, size_t req_len,
                                  char *data_ptr, size_t ipd_len, uint32_t timeout)
{
    int ret = -1;
    size_t rx_data_len = esp->rx_data_wptr - esp->rx_data_buf;
    size_t curr_len = esp->rx_wptr - data_ptr;
    char *out_ptr = out_buf;
    size_t written_len = 0;
    size_t rx_data_read_len = MIN(rx_data_len, req_len);
    size_t ipd_copy_len;
    uint32_t tim = 0;
    const uint32_t read_timeout = timeout > 100 ? 100 : timeout;

    if (rx_data_read_len) {
        // all required data was already in the data buffer
        memcpy(out_ptr, esp->rx_data_buf, rx_data_read_len);
        out_ptr += rx_data_read_len;
        // copy remaining bytes to the start of the data buffer
        copy_bytes(esp->rx_data_buf, esp->rx_data_buf + rx_data_read_len, esp->rx_data_wptr);
        esp->rx_data_wptr -= rx_data_read_len;
        written_len += rx_data_read_len;
    }

    ipd_copy_len = MIN(req_len - written_len, curr_len);
    if (ipd_copy_len) {
        memcpy(out_ptr, data_ptr, ipd_copy_len);
        out_ptr += ipd_copy_len;
        written_len += ipd_copy_len;
    }

    if (ipd_copy_len < curr_len) {
        uint32_t rem_ipd = ipd_len - ipd_copy_len;
        memcpy(esp->rx_data_wptr, data_ptr + ipd_copy_len, rem_ipd);
        esp->rx_data_wptr += rem_ipd;
    }

    if (written_len == req_len)
        return req_len;
    
    while (tim < timeout && written_len < ipd_len) {
        int res = esp_bridge_read_timeout(esp->br, esp->rx_data_wptr, req_len - written_len, read_timeout);
        if (res <= 0) {
            tim += read_timeout;
            continue;
        }
    };

    printf("TCP read timeout\n");
    return ret;
}

int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout)
{
    int res;
    uint32_t tim = 0;
    const uint32_t read_timeout = timeout > 100 ? 100 : timeout;
    char *ptr;
    char *data_ptr;
    uint32_t ipd_len = 0;

    if (len == 0)
        return 0;   

    while (tim < timeout) {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, strlen(esp_modem_ipd_str + 5), read_timeout);
        if (res <= 0) {
            tim += read_timeout;
            continue;
        }
        esp->rx_wptr += res;
        esp_modem_terminate_rx_buf(esp);
        ptr = strstr(esp->rx_buf, esp_modem_ipd_str);
        if (!ptr) {
            continue;
        }
        data_ptr = esp_modem_get_ipd(ptr + strlen(esp_modem_ipd_str), &ipd_len);
        if (!data_ptr) {
            printf("Failed to parse IPD\n");
            return -1;
        }
        printf("Found IPD: %d\n", ipd_len);
        esp->rx_wptr = esp->rx_data_buf;
        return esp_modem_receive_data(esp, buf, len, data_ptr, ipd_len, timeout - tim);
    };

    printf("TCP read timeout\n");
    return -1;
}

#if 0
int esp_modem_tcp_receive(esp_modem_t *esp, char *buf, size_t len, uint32_t timeout)
{
    int res;
    int i;
    uint32_t tim;
    char *ptr;
    uint32_t read_timeout = timeout > 100 ? 100 : timeout;
    static const char *str_ipd = "\r\n+IPD,";
    char len_buf[5];

    for (tim = 0; tim < timeout; tim += read_timeout)
    {
        res = esp_bridge_read_timeout(esp->br, esp->rx_wptr, len, read_timeout);

        ptr = strstr(esp->rx_buf, str_ipd);
        if (!ptr || ptr > esp->rx_wptr + res) {
            if (res > 0)
                 esp->rx_wptr += res;
            continue;
        }
        const char *len_start = ptr + strlen(str_ipd);
        int ipd_field_len = 0;
        for (i = 1; i <= 4; i++) {
            if (len_start[i] == ':') {
                ipd_field_len = i;
                break;
            }
        }
        if (!ipd_field_len) {
            printf("Failed to parse +IDP length\n");
            return -1;
        }
        strncpy(len_buf, len_start, ipd_field_len);
        printf("IDP length: %s\n", len_buf);
        int ipd_len = atoi(len_buf);
        if (ipd_len <= 0 || ipd_len >= 1400) {
            printf("Read invalid +IDP length %d\n", ipd_len);
            return -1;
        }
        //TODO:
        if (len < ipd_len) {
            printf("Too small buffer provided\n");
            return -1;
        }
        const char *data_start = len_start + ipd_field_len + 1;
        int curr_size = esp->rx_wptr + res - data_start;
        if (curr_size < ipd_len) {
            printf("read %d bytes, need to read more\n", curr_size);
            if (res > 0)
                 esp->rx_wptr += res;
            continue;
        }
        memcpy(buf, data_start, ipd_len);
        // copy remaining bytes to the start of the RX buffer
        int copy_size = curr_size - ipd_len;
        memcpy(esp->rx_buf, data_start + ipd_len, copy_size);
        esp->rx_wptr = esp->rx_buf + copy_size;
        return ipd_len;        
    }
    printf("TCP read timeout\n");
    return -1;
}
#endif