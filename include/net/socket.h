#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/types.h>

#define AF_INET     2
#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

typedef struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char     sin_zero[8];
} sockaddr_in_t;

// Values for socket_t.state.
#define SOCK_STATE_OPEN      0   // created, no peer or port committed yet
#define SOCK_STATE_CONNECTED 1   // peer address known (and, for a stream, open)
#define SOCK_STATE_LISTEN    2   // stream socket accepting inbound connections

typedef struct socket {
    int domain;
    int type;
    int protocol;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    int state;
    bool in_use;
} socket_t;

void socket_subsystem_init(void);
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const struct sockaddr_in* addr, size_t addrlen);
int sys_connect(int sockfd, const struct sockaddr_in* addr, size_t addrlen);

// Stream sockets only. sys_listen claims the bound port; sys_accept returns a
// new socket descriptor for the next connection to complete its handshake, or
// -1 if none arrives within the wait. `addr`, when given, receives the peer.
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, struct sockaddr_in* addr, size_t* addrlen);
ssize_t sys_send(int sockfd, const void* buf, size_t len, int flags);
ssize_t sys_recv(int sockfd, void* buf, size_t len, int flags);

// Address-carrying variants for connectionless (SOCK_DGRAM) use.
ssize_t sys_sendto(int sockfd, const void* buf, size_t len, int flags,
                   const struct sockaddr_in* dest, size_t addrlen);
ssize_t sys_recvfrom(int sockfd, void* buf, size_t len, int flags,
                     struct sockaddr_in* src, size_t* addrlen);

int sys_close_socket(int sockfd);

// Introspection for netstat / sockstat.
int  socket_get_count(void);
const socket_t* socket_get(int idx);
void socket_get_stats(uint64_t* tx_dgrams, uint64_t* rx_dgrams, uint64_t* drops);

#endif // _NET_SOCKET_H
