#include <net/tcp.h>
#include <net/net.h>
#include <net/ssh.h>
#include <net/http.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <kernel/printk.h>

#define MAX_TCP_CONNS 32

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_header_t;

static tcp_conn_t tcp_connections[MAX_TCP_CONNS];

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const tcp_header_t* tcp, const void* payload, uint16_t payload_len) {
    uint16_t tcp_total_len = sizeof(tcp_header_t) + payload_len;

    tcp_pseudo_header_t ph;
    ph.src_ip = src_ip;
    ph.dst_ip = dst_ip;
    ph.zero = 0;
    ph.protocol = IP_PROTO_TCP;
    ph.tcp_len = htons(tcp_total_len);

    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)&ph;
    for (size_t i = 0; i < sizeof(ph) / 2; i++) {
        sum += *ptr++;
    }

    ptr = (const uint16_t*)tcp;
    for (size_t i = 0; i < sizeof(tcp_header_t) / 2; i++) {
        sum += *ptr++;
    }

    if (payload && payload_len > 0) {
        ptr = (const uint16_t*)payload;
        size_t len = payload_len;
        while (len > 1) {
            sum += *ptr++;
            len -= 2;
        }
        if (len > 0) {
            sum += *(const uint8_t*)ptr;
        }
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

void tcp_init(void) {
    memset(tcp_connections, 0, sizeof(tcp_connections));
    printk(KERN_INFO "NET: TCP (Transmission Control Protocol) state machine online\n");
}

void tcp_send_packet(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
                     uint32_t seq, uint32_t ack, uint8_t flags,
                     const void* payload, uint16_t payload_len) {
    net_if_t* netif = net_get_primary_if();
    uint32_t src_ip = netif ? netif->ip : 0x0A00020F; // 10.0.2.15

    uint16_t total_len = sizeof(tcp_header_t) + payload_len;
    uint8_t* buf = (uint8_t*)kzalloc(total_len);
    if (!buf) return;

    tcp_header_t* tcp = (tcp_header_t*)buf;
    tcp->src_port = htons(src_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num  = htonl(seq);
    tcp->ack_num  = htonl(ack);
    tcp->data_offset_reserved = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags    = flags;
    tcp->window_size = htons(65535);
    tcp->urgent_pointer = 0;

    if (payload && payload_len > 0) {
        memcpy(buf + sizeof(tcp_header_t), payload, payload_len);
    }

    tcp->checksum = 0;
    tcp->checksum = tcp_checksum(src_ip, dst_ip, tcp, payload, payload_len);

    net_send_ip(dst_ip, IP_PROTO_TCP, buf, total_len);
    kfree(buf);
}

static tcp_conn_t* find_or_alloc_conn(uint32_t r_ip, uint16_t r_port, uint16_t l_port) {
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        if (tcp_connections[i].in_use &&
            tcp_connections[i].remote_ip == r_ip &&
            tcp_connections[i].remote_port == r_port &&
            tcp_connections[i].local_port == l_port) {
            return &tcp_connections[i];
        }
    }
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        if (!tcp_connections[i].in_use) {
            tcp_connections[i].in_use = true;
            tcp_connections[i].remote_ip = r_ip;
            tcp_connections[i].remote_port = r_port;
            tcp_connections[i].local_port = l_port;
            tcp_connections[i].local_ip = 0x0A00020F;
            tcp_connections[i].state = TCP_STATE_CLOSED;
            tcp_connections[i].seq_num = 1000;
            tcp_connections[i].ack_num = 0;
            return &tcp_connections[i];
        }
    }
    return NULL;
}

void tcp_receive(const uint8_t* packet, uint16_t length, uint32_t src_ip) {
    if (!packet || length < sizeof(tcp_header_t)) return;

    const tcp_header_t* tcp = (const tcp_header_t*)packet;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint32_t seq_num  = ntohl(tcp->seq_num);
    uint32_t ack_num  = ntohl(tcp->ack_num);
    uint8_t flags     = tcp->flags;
    (void)ack_num;

    uint8_t header_len = (tcp->data_offset_reserved >> 4) * 4;
    const uint8_t* payload = packet + header_len;
    uint16_t payload_len = length > header_len ? length - header_len : 0;

    // Check if target port is SSH (22) or HTTP (80)
    bool is_ssh_port = (dst_port == sshd_get_port() && sshd_is_running());
    bool is_http_port = (dst_port == 80 && httpd_is_running());

    if (!is_ssh_port && !is_http_port) {
        // Port not open: send TCP RST
        if (!(flags & TCP_FLAG_RST)) {
            tcp_send_packet(src_ip, dst_port, src_port, 0, seq_num + 1, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    tcp_conn_t* conn = find_or_alloc_conn(src_ip, src_port, dst_port);
    if (!conn) return;

    // Handle TCP SYN (Connection Initiation from Client)
    if (flags & TCP_FLAG_SYN) {
        conn->state = TCP_STATE_SYN_RCVD;
        conn->ack_num = seq_num + 1;
        conn->seq_num = 10000;

        // Send SYN-ACK
        tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                        TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
        conn->seq_num++;
        return;
    }

    // Handle TCP FIN (Connection Termination)
    if (flags & TCP_FLAG_FIN) {
        conn->ack_num = seq_num + 1;
        tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                        TCP_FLAG_ACK | TCP_FLAG_FIN, NULL, 0);
        conn->in_use = false;
        conn->state = TCP_STATE_CLOSED;
        return;
    }

    // Handle TCP RST
    if (flags & TCP_FLAG_RST) {
        conn->in_use = false;
        conn->state = TCP_STATE_CLOSED;
        return;
    }

    // Handle Data / ACK
    if (conn->state == TCP_STATE_SYN_RCVD) {
        conn->state = TCP_STATE_ESTABLISHED;
        if (is_ssh_port && payload_len == 0) {
            const char* banner = SSH_BANNER;
            uint16_t blen = (uint16_t)strlen(banner);
            tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                            TCP_FLAG_PSH | TCP_FLAG_ACK, banner, blen);
            conn->seq_num += blen;
            return;
        }
    }
    conn->state = TCP_STATE_ESTABLISHED;

    if (payload_len > 0) {
        conn->ack_num = seq_num + payload_len;

        if (is_ssh_port) {
            char resp[1024];
            int rlen = sshd_process_packet((const char*)payload, payload_len, resp, sizeof(resp));
            if (rlen > 0) {
                tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                                TCP_FLAG_PSH | TCP_FLAG_ACK, resp, (uint16_t)rlen);
                conn->seq_num += rlen;
            } else {
                tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                                TCP_FLAG_ACK, NULL, 0);
            }
        } else if (is_http_port) {
            char resp[2048];
            int rlen = httpd_handle_request((const char*)payload, resp, sizeof(resp));
            if (rlen > 0) {
                tcp_send_packet(src_ip, dst_port, src_port, conn->seq_num, conn->ack_num,
                                TCP_FLAG_PSH | TCP_FLAG_ACK, resp, (uint16_t)rlen);
                conn->seq_num += rlen;
            }
        }
    }
}

size_t tcp_get_connections_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TCP_CONNS; i++) {
        if (tcp_connections[i].in_use) count++;
    }
    return count;
}

const tcp_conn_t* tcp_get_connection(size_t index) {
    if (index >= MAX_TCP_CONNS || !tcp_connections[index].in_use) return NULL;
    return &tcp_connections[index];
}
