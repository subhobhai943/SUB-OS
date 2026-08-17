#ifndef _DRIVERS_PTY_H
#define _DRIVERS_PTY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PTY_MAX_PAIRS 8
#define PTY_BUFFER_SZ 1024

typedef struct {
    uint32_t pty_num;
    char     master_name[16]; // /dev/ptmx
    char     slave_name[16];  // /dev/pts/0
    char     m2s_buf[PTY_BUFFER_SZ];
    size_t   m2s_head;
    size_t   m2s_tail;
    char     s2m_buf[PTY_BUFFER_SZ];
    size_t   s2m_head;
    size_t   s2m_tail;
    bool     echo_enabled;
    bool     raw_mode;
    bool     master_open;
    bool     slave_open;
    bool     in_use;
} pty_pair_t;

void pty_init(void);
int  pty_open_master(void);
int  pty_open_slave(int master_fd);
int  pty_master_write(int master_fd, const char* data, size_t len);
int  pty_master_read(int master_fd, char* data, size_t max_len);
int  pty_slave_write(int slave_fd, const char* data, size_t len);
int  pty_slave_read(int slave_fd, char* data, size_t max_len);

size_t pty_get_active_count(void);
const pty_pair_t* pty_get_pair(size_t index);

#endif // _DRIVERS_PTY_H
