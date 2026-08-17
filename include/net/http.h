#ifndef _NET_HTTP_H
#define _NET_HTTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HTTP_PORT 80
#define HTTP_MAX_PAYLOAD 2048

typedef struct http_request {
    char method[8];      // GET, POST, HEAD
    char path[128];      // /index.html, /api/status
    char version[16];   // HTTP/1.1
    char user_agent[64];
    uint32_t content_length;
    char body[HTTP_MAX_PAYLOAD];
} http_request_t;

typedef struct http_response {
    int status_code;     // 200, 404, 500
    char content_type[32]; // text/html, application/json, text/plain
    char body[HTTP_MAX_PAYLOAD];
    size_t body_len;
} http_response_t;

void httpd_init(void);
int  httpd_start(uint16_t port);
int  httpd_stop(void);
bool httpd_is_running(void);
uint32_t httpd_get_requests_served(void);

// Process a raw HTTP request and generate HTTP response
int httpd_handle_request(const char* raw_request, char* raw_response, size_t max_resp_len);

// Client operations
int http_client_get(const char* url, char* response_buf, size_t max_len);

#endif // _NET_HTTP_H
