/*
 * SUB-OS Network Monitor.
 *
 * A live view of the networking stack: the interface's configuration, a
 * rolling chart of receive and transmit rates taken from the NIC's own
 * counters, and the TCP engine's connection table with every connection's
 * real state and direction -- the same data `netstat` prints, refreshed in
 * place.
 *
 * Rates are differences between successive samples of the driver counters,
 * taken on a fixed ~2 Hz cadence rather than once per repaint, so the chart
 * reads in packets per half-second regardless of how often the compositor
 * happens to redraw the window.
 */

#include <gui/gui_apps.h>
#include <gui/gui_widgets.h>
#include <gui/gui_gfx.h>
#include <gui/gui_theme.h>
#include <gui/gui_icons.h>
#include <kernel/ktime.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <net/net.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/socket.h>
#include <net/http_client.h>
#include <drivers/e1000.h>

#define NETMON_HISTORY   64
#define NETMON_PERIOD_MS 500   // sampling cadence for the rate chart

#define NETMON_RX_COLOR  GUI_THEME_SUCCESS
#define NETMON_TX_COLOR  GUI_THEME_PRIMARY

typedef struct {
    uint16_t rx[NETMON_HISTORY];   // packets received per sample interval
    uint16_t tx[NETMON_HISTORY];
    int      head;                 // next slot to write
    int      count;

    uint64_t last_sample_ms;
    uint64_t prev_rx_pkts;
    uint64_t prev_tx_pkts;
    bool     primed;               // a baseline reading has been taken

    int      scroll;               // first connection row shown

    // Internet reachability probe (async, via the HTTP worker).
    http_fetch_t* probe;
    int      inet_state;           // 0 unknown, 1 checking, 2 online, 3 offline
    char     inet_detail[72];
    bool     auto_checked;
} netmon_data_t;

#define INET_UNKNOWN  0
#define INET_CHECKING 1
#define INET_ONLINE   2
#define INET_OFFLINE  3

#define NETMON_PROBE_HOST "example.com/"   // a plain-HTTP host to reach for

// Advance the reachability probe: collect a finished result, or note that one
// is still in flight. Runs every paint; the fetch itself is on the worker.
static void netmon_poll_probe(netmon_data_t* nd) {
    if (!nd->probe) return;

    int st = nd->probe->state;
    if (st == HTTP_STATE_RUNNING) { nd->inet_state = INET_CHECKING; return; }

    if (st == HTTP_STATE_DONE && nd->probe->status_code > 0) {
        char ipx[16];
        ip_to_str(nd->probe->ip, ipx);
        nd->inet_state = INET_ONLINE;
        snprintf(nd->inet_detail, sizeof(nd->inet_detail), "%s  HTTP %d  %u ms",
                 ipx, nd->probe->status_code, nd->probe->elapsed_ms);
    } else {
        nd->inet_state = INET_OFFLINE;
        snprintf(nd->inet_detail, sizeof(nd->inet_detail), "%s",
                 st == HTTP_STATE_ERROR ? nd->probe->err : "no response");
    }

    http_fetch_release(nd->probe);
    nd->probe = NULL;
}

static void netmon_start_probe(netmon_data_t* nd) {
    if (nd->probe) return;                    // one in flight already
    nd->probe = http_fetch_start(NETMON_PROBE_HOST, 4096);
    if (nd->probe) {
        nd->inet_state = INET_CHECKING;
    } else {
        // The single worker slot is busy (e.g. the Web app is fetching).
        nd->inet_state = INET_UNKNOWN;
        snprintf(nd->inet_detail, sizeof(nd->inet_detail), "worker busy, try again");
    }
}

// ===========================================================================
// Sampling
// ===========================================================================
static void netmon_sample(netmon_data_t* nd, uint64_t now_ms) {
    uint64_t rx = e1000_get_rx_packets();
    uint64_t tx = e1000_get_tx_packets();

    // The first reading only establishes a baseline: without it the whole
    // counter total since boot would be charted as one enormous first spike.
    if (!nd->primed) {
        nd->prev_rx_pkts = rx;
        nd->prev_tx_pkts = tx;
        nd->primed = true;
        nd->last_sample_ms = now_ms;
        return;
    }

    if (now_ms - nd->last_sample_ms < NETMON_PERIOD_MS) return;
    nd->last_sample_ms = now_ms;

    uint64_t drx = (rx >= nd->prev_rx_pkts) ? rx - nd->prev_rx_pkts : 0;
    uint64_t dtx = (tx >= nd->prev_tx_pkts) ? tx - nd->prev_tx_pkts : 0;
    nd->prev_rx_pkts = rx;
    nd->prev_tx_pkts = tx;

    nd->rx[nd->head] = (drx > 0xFFFF) ? 0xFFFF : (uint16_t)drx;
    nd->tx[nd->head] = (dtx > 0xFFFF) ? 0xFFFF : (uint16_t)dtx;
    nd->head = (nd->head + 1) % NETMON_HISTORY;
    if (nd->count < NETMON_HISTORY) nd->count++;
}

// Oldest-first read of a ring, so the chart scrolls left with time.
static uint16_t ring_at(const uint16_t* ring, int head, int count, int i) {
    int start = (head + NETMON_HISTORY - count) % NETMON_HISTORY;
    return ring[(start + i) % NETMON_HISTORY];
}

// ===========================================================================
// Rate chart
// ===========================================================================
static void netmon_draw_chart(netmon_data_t* nd, int ox, int oy, int x, int y, int w, int h) {
    gui_gfx_fill_rect(ox + x, oy + y, w, h, GUI_THEME_BG_DARK);
    gui_gfx_draw_rect(ox + x, oy + y, w, h, GUI_THEME_BORDER);

    for (int gy = y + h / 4; gy < y + h; gy += h / 4) {
        gui_gfx_draw_line(ox + x + 1, oy + gy, ox + x + w - 2, oy + gy, 0xFF1E293B);
    }

    // Scale to the tallest sample in view, with a floor so an idle link does
    // not magnify single stray packets into a full-height sawtooth.
    uint16_t peak = 4;
    for (int i = 0; i < nd->count; i++) {
        uint16_t r = ring_at(nd->rx, nd->head, nd->count, i);
        uint16_t t = ring_at(nd->tx, nd->head, nd->count, i);
        if (r > peak) peak = r;
        if (t > peak) peak = t;
    }

    if (nd->count >= 2) {
        int step = (w - 8) / (NETMON_HISTORY - 1);
        if (step < 1) step = 1;

        for (int pass = 0; pass < 2; pass++) {
            const uint16_t* ring = pass ? nd->tx : nd->rx;
            uint32_t col = pass ? NETMON_TX_COLOR : NETMON_RX_COLOR;

            int prev_x = 0, prev_y = 0;
            for (int i = 0; i < nd->count; i++) {
                uint16_t v = ring_at(ring, nd->head, nd->count, i);
                int px = x + 4 + i * step;
                int py = y + h - 4 - ((int)v * (h - 10)) / peak;
                if (px > x + w - 3) break;

                if (i > 0) {
                    gui_gfx_draw_line(ox + prev_x, oy + prev_y, ox + px, oy + py, col);
                }
                prev_x = px;
                prev_y = py;
            }
        }
    } else {
        gui_gfx_draw_string(ox + x + 8, oy + y + h / 2 - 4,
                            "sampling...", GUI_THEME_TEXT_DIM);
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "peak %u pkt/%ums", peak, NETMON_PERIOD_MS);
    gui_gfx_draw_string(ox + x + 6, oy + y + 5, buf, GUI_THEME_TEXT_DIM);

    // Legend, right-aligned inside the plot.
    int lx = x + w - 88;
    gui_gfx_fill_rect(ox + lx, oy + y + 7, 8, 2, NETMON_RX_COLOR);
    gui_gfx_draw_string(ox + lx + 12, oy + y + 5, "rx", NETMON_RX_COLOR);
    gui_gfx_fill_rect(ox + lx + 34, oy + y + 7, 8, 2, NETMON_TX_COLOR);
    gui_gfx_draw_string(ox + lx + 46, oy + y + 5, "tx", NETMON_TX_COLOR);
}

// ===========================================================================
// Paint
// ===========================================================================
static void netmon_paint(gui_window_t* win) {
    netmon_data_t* nd = (netmon_data_t*)win->user_data;
    if (!nd) return;

    netmon_sample(nd, ktime_ms());

    gui_widget_begin(win);
    int w  = gui_widget_client_width();
    int h  = gui_widget_client_height();
    int ox = win->x + 1;
    int oy = win->y + GUI_TITLEBAR_HEIGHT + 1;
    if (w < 200 || h < 140) { gui_widget_end(); return; }

    char buf[96];
    net_if_t* nif = net_get_primary_if();
    bool link = e1000_is_link_up();

    // --- interface header -------------------------------------------------
    char ip[16], mask[16], gw[16];
    if (nif) {
        ip_to_str(nif->ip, ip);
        ip_to_str(nif->subnet, mask);
        ip_to_str(nif->gateway, gw);
    } else {
        strcpy(ip, "0.0.0.0"); strcpy(mask, "0.0.0.0"); strcpy(gw, "0.0.0.0");
    }

    gui_gfx_draw_string_16_shadow(ox + 10, oy + 8, nif ? nif->name : "eth0",
                                  GUI_THEME_PRIMARY, GUI_COLOR_BLACK);
    gui_badge(58, 10, link ? "UP" : "DOWN",
              link ? GUI_THEME_SUCCESS : GUI_THEME_DANGER, GUI_COLOR_BLACK);

    uint8_t mac[6];
    e1000_get_mac(mac);
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    gui_label_aligned(0, 12, w - 12, buf, GUI_THEME_TEXT_DIM, GUI_ALIGN_RIGHT);

    snprintf(buf, sizeof(buf), "inet %s   mask %s   gw %s", ip, mask, gw);
    gui_label(10, 30, buf, GUI_THEME_TEXT_MUTED);

    // --- rate chart -------------------------------------------------------
    int chart_y = 46;
    int chart_h = (h < 300) ? 70 : 96;
    netmon_draw_chart(nd, ox, oy, 10, chart_y, w - 20, chart_h);

    // --- counters ---------------------------------------------------------
    int row = chart_y + chart_h + 8;
    snprintf(buf, sizeof(buf), "RX %llu pkts / %llu B",
             (unsigned long long)e1000_get_rx_packets(),
             (unsigned long long)e1000_get_rx_bytes());
    gui_label(10, row, buf, NETMON_RX_COLOR);

    snprintf(buf, sizeof(buf), "TX %llu pkts / %llu B",
             (unsigned long long)e1000_get_tx_packets(),
             (unsigned long long)e1000_get_tx_bytes());
    gui_label(10, row + 12, buf, NETMON_TX_COLOR);

    uint64_t urx, utx, udrop;
    udp_get_stats(&urx, &utx, &udrop);
    snprintf(buf, sizeof(buf), "udp  %llu in  %llu out  %llu dropped",
             (unsigned long long)urx, (unsigned long long)utx,
             (unsigned long long)udrop);
    gui_label_aligned(0, row, w - 12, buf, GUI_THEME_TEXT_MUTED, GUI_ALIGN_RIGHT);

    snprintf(buf, sizeof(buf), "tcp  %u connection(s)  %d listener(s)",
             (unsigned)tcp_get_connections_count(), tcp_get_listener_count());
    gui_label_aligned(0, row + 12, w - 12, buf, GUI_THEME_TEXT_MUTED, GUI_ALIGN_RIGHT);

    // --- internet reachability -------------------------------------------
    // Kick off one probe automatically the first time the window paints, then
    // let the user re-check on demand. The fetch runs on the worker thread, so
    // this stays responsive while a check is in flight.
    if (!nd->auto_checked) { nd->auto_checked = true; netmon_start_probe(nd); }
    netmon_poll_probe(nd);

    int irow = row + 28;
    static const char* const inet_labels[] = { "UNKNOWN", "checking...", "ONLINE", "OFFLINE" };
    uint32_t inet_col = (nd->inet_state == INET_ONLINE)  ? GUI_THEME_SUCCESS
                      : (nd->inet_state == INET_OFFLINE) ? GUI_THEME_DANGER
                      : (nd->inet_state == INET_CHECKING)? GUI_THEME_WARNING
                                                         : GUI_THEME_TEXT_DIM;
    gui_label(10, irow, "Internet:", GUI_THEME_TEXT_MUTED);
    gui_badge(84, irow - 1, inet_labels[nd->inet_state],
              inet_col, GUI_COLOR_BLACK);
    if (nd->inet_detail[0]) {
        gui_label(150, irow, nd->inet_detail, GUI_THEME_TEXT_DIM);
    }
    if (gui_button(5, w - 82, irow - 3, 72, 18,
                   nd->probe ? "checking" : "Check")) {
        netmon_start_probe(nd);
    }

    // --- connection table -------------------------------------------------
    int tbl_y = row + 46;
    gui_separator(10, tbl_y - 4, w - 20);
    gui_label_bold(10, tbl_y, "PROTO  LOCAL", GUI_THEME_TEXT_DIM);
    gui_label(w / 2 + 6, tbl_y, "REMOTE", GUI_THEME_TEXT_DIM);
    gui_label_aligned(0, tbl_y, w - 12, "STATE", GUI_THEME_TEXT_DIM, GUI_ALIGN_RIGHT);

    // Reserve the bottom strip for the scroll controls whether or not they are
    // needed, so the last row never ends up underneath them.
    int line_h   = 12;
    int list_top = tbl_y + 14;
    int rows_fit = (h - 26 - list_top) / line_h;
    if (rows_fit < 1) rows_fit = 1;

    // Build the display list: listeners first, then live connections. It is
    // rebuilt every frame because the engine's table is the only source of
    // truth and entries come and go on their own.
    int shown = 0;
    int index = 0;

    for (int i = 0; i < tcp_get_listener_count(); i++) {
        uint16_t lp = tcp_get_listener_port(i);
        if (!lp) continue;
        if (index++ < nd->scroll) continue;
        if (shown >= rows_fit) break;

        int ly = list_top + shown * line_h;
        snprintf(buf, sizeof(buf), "tcp    0.0.0.0:%u", lp);
        gui_label(10, ly, buf, GUI_THEME_TEXT_MAIN);
        gui_label(w / 2 + 6, ly, "*:*", GUI_THEME_TEXT_DIM);
        snprintf(buf, sizeof(buf), "LISTEN (%d)", tcp_get_listener_backlog(i));
        gui_label_aligned(0, ly, w - 12, buf, GUI_THEME_WARNING, GUI_ALIGN_RIGHT);
        shown++;
    }

    for (int i = 0; i < tcp_conn_table_size(); i++) {
        const tcp_conn_t* c = tcp_get_connection(i);
        if (!c) continue;
        if (index++ < nd->scroll) continue;
        if (shown >= rows_fit) break;

        int ly = list_top + shown * line_h;
        char lip[16], rip[16];
        ip_to_str(c->local_ip, lip);
        ip_to_str(c->remote_ip, rip);

        snprintf(buf, sizeof(buf), "tcp    %s:%u", lip, c->local_port);
        gui_label(10, ly, buf, GUI_THEME_TEXT_MAIN);

        snprintf(buf, sizeof(buf), "%s:%u", rip, c->remote_port);
        gui_label(w / 2 + 6, ly, buf, GUI_THEME_TEXT_MUTED);

        // Established is the state worth spotting at a glance; the transient
        // handshake and teardown states are dimmer.
        uint32_t col = (c->state == TCP_STATE_ESTABLISHED) ? GUI_THEME_SUCCESS
                     : (c->state == TCP_STATE_CLOSED)      ? GUI_THEME_TEXT_DIM
                                                           : GUI_THEME_WARNING;
        snprintf(buf, sizeof(buf), "%s %s", tcp_state_name(c->state),
                 c->role == TCP_ROLE_CLIENT   ? "out" :
                 c->role == TCP_ROLE_ACCEPTED ? "in"  : "svc");
        gui_label_aligned(0, ly, w - 12, buf, col, GUI_ALIGN_RIGHT);
        shown++;
    }

    if (shown == 0) {
        gui_label(10, list_top, "(no sockets open)", GUI_THEME_TEXT_DIM);
    }

    // Scroll only matters once the list outgrows the window.
    if (index > rows_fit) {
        snprintf(buf, sizeof(buf), "%d-%d of %d", nd->scroll + 1,
                 nd->scroll + shown, index);
        gui_label(10, h - 20, buf, GUI_THEME_TEXT_DIM);

        if (gui_button(1, w - 58, h - 24, 24, 18, "^") && nd->scroll > 0) nd->scroll--;
        if (gui_button(2, w - 32, h - 24, 24, 18, "v") && nd->scroll < index - rows_fit) {
            nd->scroll++;
        }
    } else {
        nd->scroll = 0;
    }

    gui_widget_end();
}

static void netmon_event(gui_window_t* win, const gui_event_t* ev) {
    gui_widget_feed_event(win, ev);

    if (ev->type == GUI_EVENT_CLOSE && win->user_data) {
        netmon_data_t* nd = (netmon_data_t*)win->user_data;
        if (nd->probe) http_fetch_release(nd->probe);   // safe mid-fetch
        kfree(nd);
        win->user_data = NULL;
    }
}

void gui_app_netmon_launch(int x, int y, int w, int h) {
    gui_window_t* win = gui_wm_create_window("Network Monitor", x, y,
                                             (w > 0) ? w : 560, (h > 0) ? h : 400);
    if (!win) return;

    netmon_data_t* nd = (netmon_data_t*)kzalloc(sizeof(netmon_data_t));
    if (!nd) {
        gui_wm_destroy_window(win->id);
        return;
    }

    win->user_data    = nd;
    win->paint        = netmon_paint;
    win->handle_event = netmon_event;
}
