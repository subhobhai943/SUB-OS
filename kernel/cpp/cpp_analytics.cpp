// C++ Kernel Analytics Engine for SUB-OS.
//
// A freestanding C++17 telemetry pipeline that demonstrates templates, virtual
// dispatch (the Observer pattern) and RAII inside the kernel while doing real
// work: it samples live system metrics (CPU, memory, heap, network) into rolling
// per-channel time-series and computes windowed statistics (min / max / mean /
// standard deviation) using pure integer arithmetic -- the kernel is built with
// -mno-sse, so no floating point is available.
//
// The GUI "Analytics" app and the `cppstat` shell applet both render the series
// this engine maintains.

#include "cpp_containers.hpp"
#include <kernel/cpp_analytics.h>

extern "C" {
    #include <kernel/printk.h>
    #include <kernel/metrics.h>
    #include <mm/pmm.h>
    #include <mm/kmalloc.h>
    #include <lib/string.h>
    void cpp_call_global_constructors(void);
}

namespace kernel {

// Integer square root (Newton's method) -- for standard deviation without FP.
static uint32_t isqrt64(uint64_t n) {
    if (n == 0) return 0;
    uint64_t x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return static_cast<uint32_t>(x);
}

// -----------------------------------------------------------------------------
// TimeSeries<N>: a fixed-capacity ring of samples plus on-demand windowed stats.
// -----------------------------------------------------------------------------
template<size_t N>
class TimeSeries {
private:
    uint32_t m_ring[N];
    size_t   m_head = 0;   // index of the next write
    size_t   m_count = 0;  // number of valid samples (<= N)
    uint64_t m_total = 0;  // all-time sample count (for the running average)
    uint64_t m_sum_all = 0;

public:
    void push(uint32_t v) {
        m_ring[m_head] = v;
        m_head = (m_head + 1) % N;
        if (m_count < N) m_count++;
        m_total++;
        m_sum_all += v;
    }

    size_t count() const { return m_count; }
    size_t capacity() const { return N; }
    uint64_t total_samples() const { return m_total; }

    // Copy the window oldest-first into out[], returning how many were written.
    int copy(uint32_t* out, int max) const {
        int n = static_cast<int>(m_count < static_cast<size_t>(max) ? m_count : static_cast<size_t>(max));
        // The oldest element sits at (m_head - m_count) modulo N.
        size_t start = (m_head + N - m_count) % N;
        for (int i = 0; i < n; i++) out[i] = m_ring[(start + i) % N];
        return n;
    }

    uint32_t last() const {
        if (m_count == 0) return 0;
        return m_ring[(m_head + N - 1) % N];
    }

    void stats(uint32_t& mn, uint32_t& mx, uint32_t& avg, uint32_t& stddev) const {
        if (m_count == 0) { mn = mx = avg = stddev = 0; return; }
        uint32_t lo = 0xFFFFFFFFu, hi = 0;
        uint64_t sum = 0;
        for (size_t i = 0; i < m_count; i++) {
            uint32_t v = m_ring[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
            sum += v;
        }
        uint32_t mean = static_cast<uint32_t>(sum / m_count);
        // Windowed variance = mean of squared deviations (integer).
        uint64_t acc = 0;
        for (size_t i = 0; i < m_count; i++) {
            int64_t d = static_cast<int64_t>(m_ring[i]) - static_cast<int64_t>(mean);
            acc += static_cast<uint64_t>(d * d);
        }
        mn = lo; mx = hi; avg = mean;
        stddev = isqrt64(acc / m_count);
    }
};

// -----------------------------------------------------------------------------
// Observer pattern: subscribers are notified of every sample on every channel.
// -----------------------------------------------------------------------------
class IAnalyticsObserver {
public:
    virtual ~IAnalyticsObserver() = default;
    virtual void on_sample(int channel, uint32_t value) = 0;
};

// A concrete observer that counts threshold breaches per channel -- a small but
// real use of virtual dispatch (e.g. "CPU load crossed 90%").
class ThresholdAlarmObserver : public IAnalyticsObserver {
private:
    uint32_t m_threshold[CPP_ANALYTICS_CHANNELS];
    uint64_t m_breaches[CPP_ANALYTICS_CHANNELS];

public:
    ThresholdAlarmObserver() {
        for (int i = 0; i < CPP_ANALYTICS_CHANNELS; i++) { m_threshold[i] = 90; m_breaches[i] = 0; }
    }
    void set_threshold(int ch, uint32_t t) {
        if (ch >= 0 && ch < CPP_ANALYTICS_CHANNELS) m_threshold[ch] = t;
    }
    void on_sample(int channel, uint32_t value) override {
        if (channel < 0 || channel >= CPP_ANALYTICS_CHANNELS) return;
        if (value >= m_threshold[channel]) m_breaches[channel]++;
    }
    uint64_t breaches(int ch) const {
        return (ch >= 0 && ch < CPP_ANALYTICS_CHANNELS) ? m_breaches[ch] : 0;
    }
};

// -----------------------------------------------------------------------------
// A single metric channel: name, unit, graph scale and its time-series.
// -----------------------------------------------------------------------------
struct Channel {
    const char* name;
    const char* unit;
    uint32_t    scale;               // graph ceiling (0 => auto-scale to max)
    TimeSeries<CPP_ANALYTICS_HISTORY> series;
};

// -----------------------------------------------------------------------------
// The engine singleton.
// -----------------------------------------------------------------------------
class AnalyticsEngine {
private:
    Channel  m_ch[CPP_ANALYTICS_CHANNELS];
    Vector<IAnalyticsObserver*> m_observers;   // non-owning
    ThresholdAlarmObserver*     m_alarm = nullptr;
    uint64_t m_samples = 0;

    // Previous cumulative packet counter, to derive a per-interval rate.
    uint64_t m_prev_net_pkts = 0;
    bool     m_have_prev = false;

    AnalyticsEngine() {
        m_ch[0].name = "CPU Load";   m_ch[0].unit = "%";     m_ch[0].scale = 100;
        m_ch[1].name = "Memory";     m_ch[1].unit = "%";     m_ch[1].scale = 100;
        m_ch[2].name = "Kernel Heap";m_ch[2].unit = "%";     m_ch[2].scale = 100;
        m_ch[3].name = "Net Traffic";m_ch[3].unit = "pkt/s"; m_ch[3].scale = 0; // auto
    }

public:
    static AnalyticsEngine& instance() {
        static AnalyticsEngine* s = nullptr;
        if (!s) s = new AnalyticsEngine();
        return *s;
    }

    void add_observer(IAnalyticsObserver* o) { if (o) m_observers.push_back(o); }

    void ensure_alarm() {
        if (!m_alarm) {
            m_alarm = new ThresholdAlarmObserver();
            add_observer(m_alarm);
        }
    }

    ThresholdAlarmObserver* alarm() { return m_alarm; }

    void notify(int ch, uint32_t v) {
        for (auto* o : m_observers) if (o) o->on_sample(ch, v);
    }

    uint64_t sample() {
        system_metrics_t m;
        metrics_sample(&m);

        uint32_t cpu = m.cpu_user_pct + m.cpu_system_pct;
        if (cpu > 100) cpu = 100;

        uint32_t mem = 0;
        if (m.mem_total_kb) mem = static_cast<uint32_t>((m.mem_used_kb * 100) / m.mem_total_kb);
        if (mem > 100) mem = 100;

        uint32_t heap = 0;
        size_t ht = heap_get_total_bytes();
        if (ht) heap = static_cast<uint32_t>((static_cast<uint64_t>(heap_get_used_bytes()) * 100) / ht);
        if (heap > 100) heap = 100;

        uint64_t pkts = m.net_rx_packets + m.net_tx_packets;
        uint32_t rate = 0;
        if (m_have_prev && pkts >= m_prev_net_pkts) rate = static_cast<uint32_t>(pkts - m_prev_net_pkts);
        m_prev_net_pkts = pkts;
        m_have_prev = true;

        uint32_t vals[CPP_ANALYTICS_CHANNELS] = { cpu, mem, heap, rate };
        for (int i = 0; i < CPP_ANALYTICS_CHANNELS; i++) {
            m_ch[i].series.push(vals[i]);
            notify(i, vals[i]);
        }
        m_samples++;
        return m_samples;
    }

    int channels() const { return CPP_ANALYTICS_CHANNELS; }
    uint64_t sample_count() const { return m_samples; }

    Channel* channel(int i) {
        if (i < 0 || i >= CPP_ANALYTICS_CHANNELS) return nullptr;
        return &m_ch[i];
    }
};

} // namespace kernel

// -----------------------------------------------------------------------------
// C-FFI bridge
// -----------------------------------------------------------------------------
using kernel::AnalyticsEngine;

extern "C" {

void cpp_analytics_init(void) {
    cpp_call_global_constructors();
    auto& e = AnalyticsEngine::instance();
    e.ensure_alarm();
    // Seed a few samples so the graphs are not empty on first paint.
    for (int i = 0; i < 4; i++) e.sample();
    printk(KERN_INFO "CXX: Analytics Engine online (%d channels, %d-sample history, Observer pattern)\n",
           CPP_ANALYTICS_CHANNELS, CPP_ANALYTICS_HISTORY);
}

uint64_t cpp_analytics_sample(void) {
    return AnalyticsEngine::instance().sample();
}

int cpp_analytics_get_series(int channel, uint32_t* out, int max) {
    if (!out || max <= 0) return 0;
    auto* c = AnalyticsEngine::instance().channel(channel);
    if (!c) return 0;
    return c->series.copy(out, max);
}

void cpp_analytics_get_stats(int channel, uint32_t* out_min, uint32_t* out_max,
                             uint32_t* out_avg, uint32_t* out_last,
                             uint32_t* out_stddev) {
    auto* c = AnalyticsEngine::instance().channel(channel);
    uint32_t mn = 0, mx = 0, avg = 0, sd = 0, last = 0;
    if (c) { c->series.stats(mn, mx, avg, sd); last = c->series.last(); }
    if (out_min) *out_min = mn;
    if (out_max) *out_max = mx;
    if (out_avg) *out_avg = avg;
    if (out_last) *out_last = last;
    if (out_stddev) *out_stddev = sd;
}

const char* cpp_analytics_channel_name(int channel) {
    auto* c = AnalyticsEngine::instance().channel(channel);
    return c ? c->name : "";
}

const char* cpp_analytics_channel_unit(int channel) {
    auto* c = AnalyticsEngine::instance().channel(channel);
    return c ? c->unit : "";
}

uint32_t cpp_analytics_channel_scale(int channel) {
    auto* c = AnalyticsEngine::instance().channel(channel);
    if (!c) return 100;
    if (c->scale) return c->scale;
    // Auto-scale: ceil to a sensible ceiling above the observed max.
    uint32_t mn, mx, avg, sd;
    c->series.stats(mn, mx, avg, sd);
    uint32_t ceil = mx + mx / 4 + 1;
    return ceil < 10 ? 10 : ceil;
}

int cpp_analytics_channel_count(void) {
    return AnalyticsEngine::instance().channels();
}

uint64_t cpp_analytics_sample_count(void) {
    return AnalyticsEngine::instance().sample_count();
}

void cpp_analytics_dump(void) {
    auto& e = AnalyticsEngine::instance();
    e.sample();
    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS C++ Analytics Engine  (samples: %llu)\n" ANSI_RESET,
           static_cast<unsigned long long>(e.sample_count()));
    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BOLD "%-14s %6s %6s %6s %6s %8s\n" ANSI_RESET,
           "CHANNEL", "LAST", "MIN", "AVG", "MAX", "STDDEV");
    printk("-----------------------------------------------------------------\n");
    for (int i = 0; i < e.channels(); i++) {
        uint32_t mn, mx, avg, last, sd;
        cpp_analytics_get_stats(i, &mn, &mx, &avg, &last, &sd);
        char label[24];
        snprintf(label, sizeof(label), "%s(%s)", cpp_analytics_channel_name(i), cpp_analytics_channel_unit(i));
        printk(ANSI_BRIGHT_YELLOW "%-14s" ANSI_RESET " %6u %6u " ANSI_BRIGHT_GREEN "%6u" ANSI_RESET " %6u %8u\n",
               label, last, mn, avg, mx, sd);
    }

    auto* alarm = e.alarm();
    if (alarm) {
        printk("-----------------------------------------------------------------\n");
        printk("  Threshold alarm breaches (Observer): CPU=%llu  MEM=%llu  HEAP=%llu\n",
               static_cast<unsigned long long>(alarm->breaches(0)),
               static_cast<unsigned long long>(alarm->breaches(1)),
               static_cast<unsigned long long>(alarm->breaches(2)));
    }
    printk("\n");
}

} // extern "C"
