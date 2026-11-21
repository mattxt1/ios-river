/* Portable, bounded mappings shared by the native bridge and host checks. */
#ifndef RIVER_HARDWARE_POLICY_H
#define RIVER_HARDWARE_POLICY_H
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
enum {
 RIVER_FEEDBACK_SELECTION, RIVER_FEEDBACK_IMPACT, RIVER_FEEDBACK_SUCCESS,
 RIVER_FEEDBACK_WARNING, RIVER_FEEDBACK_ERROR
};
struct river_feedback_pulse { unsigned offset_ms, duration_ms, gain; };
struct river_feedback_pattern { unsigned count; struct river_feedback_pulse pulses[3]; };

static inline uint64_t river_brightness_fixed(double level) {
 if (!isfinite(level)) return 0;
 if (level < 0) level = 0;
 if (level > 1) level = 1;
 return (uint64_t)llround(level * (500u << 16));
}
static inline bool river_feedback_pattern(unsigned kind, double intensity, struct river_feedback_pattern *out) {
 if (!out || !isfinite(intensity) || intensity <= 0 || kind > RIVER_FEEDBACK_ERROR) return false;
 if (intensity > 1) intensity = 1;
 *out = (struct river_feedback_pattern){0};
 unsigned gain = (unsigned)lround(intensity * 0x28);
 if (!gain) return false;
 switch (kind) {
 case RIVER_FEEDBACK_SELECTION:
  out->count = 1; out->pulses[0] = (struct river_feedback_pulse){0, 15, (gain + 1) / 2}; break;
 case RIVER_FEEDBACK_IMPACT:
  out->count = 1; out->pulses[0] = (struct river_feedback_pulse){0, 30, gain}; break;
 case RIVER_FEEDBACK_SUCCESS:
  out->count = 2;
  out->pulses[0] = (struct river_feedback_pulse){0, 20, (gain + 1) / 2};
  out->pulses[1] = (struct river_feedback_pulse){80, 30, gain}; break;
 case RIVER_FEEDBACK_WARNING:
  out->count = 2;
  out->pulses[0] = (struct river_feedback_pulse){0, 30, gain};
  out->pulses[1] = (struct river_feedback_pulse){100, 30, gain}; break;
 case RIVER_FEEDBACK_ERROR:
  out->count = 3;
  for (unsigned i = 0; i < 3; ++i) out->pulses[i] = (struct river_feedback_pulse){i * 90, 25, gain}; break;
 }
 return true;
}
#endif
