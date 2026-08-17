#ifndef _NET_SSH_H
#define _NET_SSH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SSH_DEFAULT_PORT 22
#define SSH_MAX_SESSIONS 8
#define SSH_BANNER "SSH-2.0-SUB_OS_SSH_1.0 (Titan-Enterprise)\r\n"
#define SSH_BUFFER_SIZE 2048

typedef enum {
    SSH_STATE_DISCONNECTED = 0,
    SSH_STATE_BANNER_SENT,
    SSH_STATE_KEX_INIT,
    SSH_STATE_AUTH_REQUEST,
    SSH_STATE_AUTHENTICATED,
    SSH_STATE_CHANNEL_OPEN,
    SSH_STATE_SHELL_ACTIVE
} ssh_state_t;

typedef struct ssh_session {
    uint32_t session_id;
    char username[32];
    uint32_t client_ip;
    uint16_t client_port;
    ssh_state_t state;
    bool authenticated;
    uint64_t connect_time;
    uint64_t bytes_rx;
    uint64_t bytes_tx;
    bool in_use;
} ssh_session_t;

void sshd_init(void);
int  sshd_start(uint16_t port);
int  sshd_stop(void);
bool sshd_is_running(void);
uint16_t sshd_get_port(void);
uint32_t sshd_get_sessions_count(void);
const ssh_session_t* sshd_get_session(size_t index);

// SSH Remote Execution & Handshake Processor
int sshd_process_packet(const char* in_data, size_t in_len, char* out_resp, size_t max_resp_len);

// SSH Client Command Runner (e.g. ssh user@host "command")
int ssh_client_execute(const char* username, const char* password, const char* host, uint16_t port, const char* command, char* out_buf, size_t max_len);

#endif // _NET_SSH_H
