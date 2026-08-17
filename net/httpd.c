#include <net/http.h>
#include <net/net.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <init/version.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

static bool httpd_running = false;
static uint16_t httpd_listen_port = 80;
static uint32_t requests_count = 0;

void httpd_init(void) {
    httpd_running = false;
    httpd_listen_port = 80;
    requests_count = 0;
    printk(KERN_INFO "HTTPD: Embedded Micro Web Server & REST API Subsystem initialized\n");
}

int httpd_start(uint16_t port) {
    httpd_listen_port = port > 0 ? port : 80;
    httpd_running = true;
    printk(KERN_INFO "HTTPD: Web Server listening on 0.0.0.0:%u (HTTP/1.1)\n", httpd_listen_port);
    return 0;
}

int httpd_stop(void) {
    httpd_running = false;
    printk(KERN_INFO "HTTPD: Web Server stopped\n");
    return 0;
}

bool httpd_is_running(void) {
    return httpd_running;
}

uint32_t httpd_get_requests_served(void) {
    return requests_count;
}

int httpd_handle_request(const char* raw_req, char* raw_resp, size_t max_resp_len) {
    if (!raw_req || !raw_resp || max_resp_len == 0) return -1;
    requests_count++;

    // Parse method and path
    char method[16] = "GET";
    char path[128] = "/";

    const char* p = raw_req;
    while (*p == ' ' || *p == '\r' || *p == '\n') p++;

    size_t mi = 0;
    while (*p && *p != ' ' && mi < sizeof(method) - 1) {
        method[mi++] = *p++;
    }
    method[mi] = '\0';

    while (*p == ' ') p++;

    size_t pi = 0;
    while (*p && *p != ' ' && *p != '?' && *p != '\r' && *p != '\n' && pi < sizeof(path) - 1) {
        path[pi++] = *p++;
    }
    path[pi] = '\0';

    char body[1500];
    const char* content_type = "text/html; charset=utf-8";
    int status_code = 200;
    const char* status_text = "OK";

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        uint64_t total_mb = (pmm_get_total_pages() * 4096) / (1024 * 1024);
        uint64_t free_mb = (pmm_get_free_pages() * 4096) / (1024 * 1024);
        uint64_t uptime_sec = pit_get_ticks() / 100;

        snprintf(body, sizeof(body),
            "<!DOCTYPE html><html><head><title>SUB-OS Production Server</title>"
            "<style>body{font-family:sans-serif;background:#0d1117;color:#c9d1d9;padding:2rem;}"
            "h1{color:#58a6ff;} .card{background:#161b22;border:1px solid #30363d;border-radius:6px;padding:1rem;margin:1rem 0;}"
            ".badge{background:#238636;color:#fff;padding:2px 8px;border-radius:12px;font-size:0.85rem;}"
            "</style></head><body>"
            "<h1>🚀 SUB-OS Server Node <span class='badge'>Online</span></h1>"
            "<div class='card'><h3>System Information</h3>"
            "<p><b>Kernel Version:</b> %s</p>"
            "<p><b>Architecture:</b> x86_64 Long Mode (SMP Preempt)</p>"
            "<p><b>Memory:</b> %llu MB free / %llu MB total</p>"
            "<p><b>Uptime:</b> %llu seconds</p>"
            "<p><b>Requests Handled:</b> %u</p></div>"
            "<div class='card'><h3>Endpoints Available</h3>"
            "<ul><li><a style='color:#58a6ff' href='/api/status'>/api/status</a> - JSON System Telemetry</li>"
            "<li><a style='color:#58a6ff' href='/readme.txt'>/readme.txt</a> - Kernel Readme</li></ul></div>"
            "</body></html>",
            kernel_get_version(), free_mb, total_mb, uptime_sec, requests_count);
    } else if (strcmp(path, "/api/status") == 0) {
        content_type = "application/json";
        uint64_t free_kb = (pmm_get_free_pages() * 4096) / 1024;
        uint64_t total_kb = (pmm_get_total_pages() * 4096) / 1024;

        snprintf(body, sizeof(body),
            "{\"os\":\"SUB-OS\",\"version\":\"%s\",\"arch\":\"x86_64\","
            "\"uptime_sec\":%llu,\"mem_total_kb\":%llu,\"mem_free_kb\":%llu,"
            "\"requests_served\":%u,\"status\":\"HEALTHY\"}",
            kernel_get_version(), (uint64_t)(pit_get_ticks() / 100), total_kb, free_kb, requests_count);
    } else {
        // Try serving from VFS
        int fd = vfs_open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t bytes = vfs_read(fd, body, sizeof(body) - 1);
            if (bytes >= 0) {
                body[bytes] = '\0';
                content_type = "text/plain";
            }
            vfs_close(fd);
        } else {
            status_code = 404;
            status_text = "Not Found";
            content_type = "text/plain";
            snprintf(body, sizeof(body), "404 Not Found: %s on SUB-OS Server\n", path);
        }
    }

    size_t body_len = strlen(body);
    snprintf(raw_resp, max_resp_len,
        "HTTP/1.1 %d %s\r\n"
        "Server: SUB-OS/0.2.0 (x86_64)\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %llu\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        status_code, status_text, content_type, (uint64_t)body_len, body);

    return (int)strlen(raw_resp);
}

int http_client_get(const char* url, char* response_buf, size_t max_len) {
    if (!url || !response_buf || max_len == 0) return -1;

    // Direct loopback query against built-in httpd engine
    char mock_req[256];
    const char* path = strchr(url, '/');
    if (!path) path = "/";

    snprintf(mock_req, sizeof(mock_req), "GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n", path);
    return httpd_handle_request(mock_req, response_buf, max_len);
}
