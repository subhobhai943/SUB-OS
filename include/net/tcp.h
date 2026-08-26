#ifndef _NET_TCP_H
#define _NET_TCP_H

#include <net/net.h>

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RCVD    3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT1   5
#define TCP_STATE_FIN_WAIT2   6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset_reserved;
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
} __attribute__((packed)) tcp_header_t;

// Which end opened the connection. Server connections are created by an
// inbound SYN to a listening service; client connections by tcp_connect().
#define TCP_ROLE_SERVER 0
#define TCP_ROLE_CLIENT 1

#define TCP_RX_BUF_SIZE 4096
#define TCP_MSS         1460

typedef struct tcp_conn {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint8_t  state;
    uint32_t seq_num;      // next sequence number we will send
    uint32_t ack_num;      // next sequence number we expect to receive
    bool     in_use;

    // --- client / stream state, unused by the server path ----------------
    uint8_t  role;
    uint32_t iss;          // initial send sequence number
    uint32_t snd_una;      // oldest sequence number not yet acknowledged
    uint8_t* rx_buf;       // receive ring, allocated on active open
    uint16_t rx_head;      // read cursor
    uint16_t rx_count;     // bytes available to the reader
    bool     peer_fin;     // peer sent FIN: no more data will arrive
    bool     reset;        // peer sent RST
    uint64_t timer;        // tick of the last transmission awaiting an ACK
    uint64_t closed_at;    // tick TIME_WAIT began
} tcp_conn_t;

void tcp_init(void);
void tcp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip);
void tcp_send_packet(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                     uint32_t seq, uint32_t ack, uint8_t flags,
                     const void* payload, uint16_t payload_len);

// --- Active open (client side) ------------------------------------------
//
// Every call below is bounded: a silent or unreachable peer produces a
// timeout, never a hang. They busy-wait against the timer tick with the CPU
// halted between checks, the same way net_ping and the DNS resolver do, so
// they must be called from a context where inbound packets can still be
// serviced by the NIC interrupt.

// Perform the three-way handshake with dst_ip:dst_port, retransmitting the
// SYN until the handshake completes or timeout_ms elapses. Returns the
// connection, or NULL if it could not be established.
tcp_conn_t* tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);

// Send len bytes, segmented to the MSS. Stop-and-wait: each segment is
// retransmitted until acknowledged before the next is sent. Returns the number
// of bytes acknowledged, or -1 if the connection is unusable.
int tcp_send(tcp_conn_t* c, const void* data, uint16_t len);

// Read up to len bytes, waiting up to timeout_ms for data to arrive. Returns
// the byte count, or -1 on reset. A return of 0 means no data became
// available: use tcp_peer_closed() to tell a finished stream from an idle one.
int tcp_recv(tcp_conn_t* c, void* buf, uint16_t len, uint32_t timeout_ms);

// Orderly shutdown, then release the connection. The handle must not be used
// afterwards.
void tcp_close(tcp_conn_t* c);

bool tcp_is_established(const tcp_conn_t* c);

// True once the peer's FIN has been received, so no further data can arrive.
bool tcp_peer_closed(const tcp_conn_t* c);

size_t tcp_get_connections_count(void);
const tcp_conn_t* tcp_get_connection(size_t index);
const char* tcp_state_name(uint8_t state);

#endif // _NET_TCP_H
