#ifndef _KERNEL_CPP_ANALYTICS_H
#define _KERNEL_CPP_ANALYTICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The C++ analytics engine keeps a rolling time-series per channel and computes
// online statistics (min/max/mean/variance via Welford) over it. Channels are:
//   0 = CPU load %      1 = Memory used %
//   2 = Kernel heap %   3 = Network packets/interval
#define CPP_ANALYTICS_CHANNELS 4
#define CPP_ANALYTICS_HISTORY  64

// Bring up the engine (registers channels + an example observer). Idempotent.
void cpp_analytics_init(void);

// Pull one live sample from the kernel (metrics/pmm/heap/NIC) into every
// channel. Call this periodically (the GUI Analytics app and the `cppstat`
// applet both drive it). Returns the total number of samples taken so far.
uint64_t cpp_analytics_sample(void);

// Copy up to `max` most-recent values of a channel into `out` (oldest first,
// newest last). Returns the number written.
int cpp_analytics_get_series(int channel, uint32_t* out, int max);

// Fetch the online statistics for a channel. Any out pointer may be NULL.
void cpp_analytics_get_stats(int channel, uint32_t* out_min, uint32_t* out_max,
                             uint32_t* out_avg, uint32_t* out_last,
                             uint32_t* out_stddev);

// Human-readable channel name / unit suffix and the current scale ceiling used
// for graphing (e.g. 100 for a percentage channel).
const char* cpp_analytics_channel_name(int channel);
const char* cpp_analytics_channel_unit(int channel);
uint32_t    cpp_analytics_channel_scale(int channel);

int         cpp_analytics_channel_count(void);
uint64_t    cpp_analytics_sample_count(void);

// Print a textual dashboard (used by the `cppstat` lazybox applet).
void cpp_analytics_dump(void);

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_CPP_ANALYTICS_H
