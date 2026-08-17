#include <drivers/pty.h>
#include <kernel/printk.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>

static pty_pair_t pty_pairs[PTY_MAX_PAIRS];

void pty_init(void) {
    memset(pty_pairs, 0, sizeof(pty_pairs));
    for (size_t i = 0; i < PTY_MAX_PAIRS; i++) {
        pty_pairs[i].pty_num = (uint32_t)i;
        strcpy(pty_pairs[i].master_name, "/dev/ptmx");
        snprintf(pty_pairs[i].slave_name, sizeof(pty_pairs[i].slave_name), "/dev/pts/%u", (uint32_t)i);
        pty_pairs[i].echo_enabled = true;
        pty_pairs[i].raw_mode = false;
    }
    printk(KERN_INFO "PTY: Unix98 Pseudo-Terminal Subsystem initialized (8 pts channels)\n");
}

int pty_open_master(void) {
    for (size_t i = 0; i < PTY_MAX_PAIRS; i++) {
        if (!pty_pairs[i].in_use) {
            pty_pairs[i].in_use = true;
            pty_pairs[i].master_open = true;
            pty_pairs[i].slave_open = false;
            pty_pairs[i].m2s_head = pty_pairs[i].m2s_tail = 0;
            pty_pairs[i].s2m_head = pty_pairs[i].s2m_tail = 0;
            return (int)i;
        }
    }
    return -1;
}

int pty_open_slave(int master_fd) {
    if (master_fd < 0 || master_fd >= PTY_MAX_PAIRS || !pty_pairs[master_fd].in_use) return -1;
    pty_pairs[master_fd].slave_open = true;
    return master_fd;
}

int pty_master_write(int master_fd, const char* data, size_t len) {
    if (master_fd < 0 || master_fd >= PTY_MAX_PAIRS || !pty_pairs[master_fd].in_use || !data) return -1;

    pty_pair_t* pty = &pty_pairs[master_fd];
    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        size_t next = (pty->m2s_head + 1) % PTY_BUFFER_SZ;
        if (next != pty->m2s_tail) {
            pty->m2s_buf[pty->m2s_head] = data[i];
            pty->m2s_head = next;
            written++;
        }
    }
    return (int)written;
}

int pty_master_read(int master_fd, char* data, size_t max_len) {
    if (master_fd < 0 || master_fd >= PTY_MAX_PAIRS || !pty_pairs[master_fd].in_use || !data) return -1;

    pty_pair_t* pty = &pty_pairs[master_fd];
    size_t read_bytes = 0;
    while (pty->s2m_tail != pty->s2m_head && read_bytes < max_len) {
        data[read_bytes++] = pty->s2m_buf[pty->s2m_tail];
        pty->s2m_tail = (pty->s2m_tail + 1) % PTY_BUFFER_SZ;
    }
    return (int)read_bytes;
}

int pty_slave_write(int slave_fd, const char* data, size_t len) {
    if (slave_fd < 0 || slave_fd >= PTY_MAX_PAIRS || !pty_pairs[slave_fd].in_use || !data) return -1;

    pty_pair_t* pty = &pty_pairs[slave_fd];
    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        size_t next = (pty->s2m_head + 1) % PTY_BUFFER_SZ;
        if (next != pty->s2m_tail) {
            pty->s2m_buf[pty->s2m_head] = data[i];
            pty->s2m_head = next;
            written++;
        }
    }
    return (int)written;
}

int pty_slave_read(int slave_fd, char* data, size_t max_len) {
    if (slave_fd < 0 || slave_fd >= PTY_MAX_PAIRS || !pty_pairs[slave_fd].in_use || !data) return -1;

    pty_pair_t* pty = &pty_pairs[slave_fd];
    size_t read_bytes = 0;
    while (pty->m2s_tail != pty->m2s_head && read_bytes < max_len) {
        data[read_bytes++] = pty->m2s_buf[pty->m2s_tail];
        pty->m2s_tail = (pty->m2s_tail + 1) % PTY_BUFFER_SZ;
    }
    return (int)read_bytes;
}

size_t pty_get_active_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < PTY_MAX_PAIRS; i++) {
        if (pty_pairs[i].in_use) count++;
    }
    return count;
}

const pty_pair_t* pty_get_pair(size_t index) {
    if (index >= PTY_MAX_PAIRS || !pty_pairs[index].in_use) return NULL;
    return &pty_pairs[index];
}
