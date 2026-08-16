#include <net/socket.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define MAX_SOCKETS 32
static socket_t socket_table[MAX_SOCKETS];

void socket_subsystem_init(void) {
    memset(socket_table, 0, sizeof(socket_table));
    printk(KERN_INFO "NET: BSD Sockets API Layer initialized (AF_INET support)\n");
}

int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!socket_table[i].in_use) {
            socket_table[i].in_use = true;
            socket_table[i].domain = domain;
            socket_table[i].type = type;
            socket_table[i].protocol = protocol;
            socket_table[i].state = 0;
            return i;
        }
    }
    return -1;
}

int sys_bind(int sockfd, const struct sockaddr_in* addr, size_t addrlen) {
    (void)addrlen;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use || !addr) return -1;
    socket_table[sockfd].local_ip = addr->sin_addr;
    socket_table[sockfd].local_port = addr->sin_port;
    return 0;
}

int sys_connect(int sockfd, const struct sockaddr_in* addr, size_t addrlen) {
    (void)addrlen;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use || !addr) return -1;
    socket_table[sockfd].remote_ip = addr->sin_addr;
    socket_table[sockfd].remote_port = addr->sin_port;
    socket_table[sockfd].state = 1; // Connected
    return 0;
}

ssize_t sys_send(int sockfd, const void* buf, size_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use || !buf) return -1;
    return (ssize_t)len;
}

ssize_t sys_recv(int sockfd, void* buf, size_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use || !buf) return -1;
    return 0;
}

int sys_close_socket(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !socket_table[sockfd].in_use) return -1;
    memset(&socket_table[sockfd], 0, sizeof(socket_t));
    return 0;
}
