#include "syllable_detector.h"
#include "dsp/biquad.h"
#include "dsp/envelope.h"
#include "dsp/zff.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


// --- Constants & Defaults ---
#define DEFAULT_SAMPLE_RATE 44100
#define PROMINENCE_BUFFER_SIZE 16 // Power of 2
#define SILENCE_THRESHOLD 0.001f

// --- Internal Structs ---

typedef struct {
  SyllableEvent event;
  int is_ready;
} BufferedEvent;

struct SyllableDetector {
  SyllableConfig config;
  uint64_t total_samples;

  // DSP Modules
  Biquad bp_filter;
  EnvelopeFollower env_follower;
  ZFF zff;

  // PeakRate State
  float prev_env;
  float current_peak_rate;
  float peak_rate_accum; // For smoothing if needed?? No, raw is better for
                         // distinct peaks.

  // ZFF State
  float last_zff_val;
  float last_zff_slope;
  int last_epoch_samples_ago;
  float current_f0;
  int voicing_counter; // How many voiced frames we've seen
  int unvoiced_counter;
  int is_voiced;
  int voiced_hold_samples;

  // Adaptive PeakRate Threshold
  int adaptive_enabled;
  float adaptive_mean;
  float adaptive_var;
  float adaptive_alpha;

  // State Machine
  enum {
    STATE_IDLE,
    STATE_ONSET_RISING, // PeakRate rising
    STATE_NUCLEUS,      // Valid syllable nucleus
    STATE_COOLDOWN      // Enforcing min distance
  } state;

  int state_timer; // Samples in current state

  // WIP Event (while building a syllable)
  SyllableEvent wip_event;
  float max_peak_rate_in_syllable;
  float energy_accum;
  uint64_t onset_timestamp;

  // Event Buffer (Ring Buffer) for Prominence Context
  BufferedEvent event_buffer[PROMINENCE_BUFFER_SIZE];
  int buf_write_idx;
  int buf_read_idx;
  int buf_count;

  // Memory
  void *(*alloc_fn)(size_t);
  void (*free_fn)(void *);
};

// --- Helpers ---

static void *default_malloc(size_t size) { return malloc(size); }
static void default_free(void *ptr) { free(ptr); }

static float bandpass_center_hz(const SyllableConfig *cfg) {
  return (cfg->peak_rate_band_min + cfg->peak_rate_band_max) * 0.5f;
}

static float bandpass_q_factor(const SyllableConfig *cfg) {
  float bandwidth = cfg->peak_rate_band_max - cfg->peak_rate_band_min;
  if (bandwidth < 1.0f)
    bandwidth = 1.0f;
  return bandpass_center_hz(cfg) / bandwidth;
}

static void configure_bandpass(Biquad *filter, const SyllableConfig *cfg) {
  float q = bandpass_q_factor(cfg);
  if (q < 0.1f)
    q = 0.1f;
  biquad_config_bandpass(filter, (float)cfg->sample_rate,
                         bandpass_center_hz(cfg), q);
}

SyllableConfig syllable_default_config(int sample_rate) {
  SyllableConfig cfg;
  cfg.sample_rate = sample_rate > 0 ? sample_rate : DEFAULT_SAMPLE_RATE;
  cfg.zff_trend_window_ms = 10.0f;
  cfg.peak_rate_band_min = 500.0f;
  cfg.peak_rate_band_max = 3200.0f;
  cfg.min_syllable_dist_ms = 100.0f;
  cfg.threshold_peak_rate = 0.0003f;
  cfg.adaptive_peak_rate_k = 4.0f;
  cfg.adaptive_peak_rate_tau_ms = 500.0f;
  cfg.voiced_hold_ms = 30.0f;
  cfg.context_size = 2;            // Look +/- 2 syllables
  cfg.user_malloc = NULL;
  cfg.user_free = NULL;
  return cfg;
}

static float calculate_prominence(SyllableDetector *d, int target_idx) {
  // Simple logic: Compare Energy & PeakRate to neighbors
  // target_idx is in the ring buffer

  BufferedEvent *target = &d->event_buffer[target_idx];
  if (!target->is_ready)
    return 0.0f;

  float local_avg_energy = 0.0f;
  float local_avg_pr = 0.0f;
  int count = 0;

  // Check neighbors
  for (int i = 1; i <= d->config.context_size; i++) {
    // Previous
    int prev_idx =
        (target_idx - i + PROMINENCE_BUFFER_SIZE) % PROMINENCE_BUFFER_SIZE;
    if (d->event_buffer[prev_idx].is_ready) {
      local_avg_energy += d->event_buffer[prev_idx].event.energy;
      local_avg_pr += d->event_buffer[prev_idx].event.peak_rate;
      count++;
    }

    // Next (we can only look ahead if those events exist,
    // but 'target' is usually the oldest one waiting to be flushed
    // unless we have lookahead delay.
    // THIS IMPLEMENTATION ASSUMES:
    // We output events with delay. So 'target' is at read_idx,
    // and we have newer events up to write_idx.)

    // Actually, if target_idx is the one we are about to output (read_idx),
    // then the "Next" syllables are at (read_idx + 1) etc.

    int next_idx = (target_idx + i) % PROMINENCE_BUFFER_SIZE;
    // Check if next_idx is valid (between read and write ptrs)
    // This circular buffer logic is tricky.
    // Let's rely on valid flags or just indices?

    // A simpler way: Iterate from read_idx to write_idx
    // If next_idx is logically "future", and it exists, use it.
    // We will just check if buffer[next_idx].is_ready.
    if (d->event_buffer[next_idx].is_ready) {
      local_avg_energy += d->event_buffer[next_idx].event.energy;
      local_avg_pr += d->event_buffer[next_idx].event.peak_rate;
      count++;
    }
  }

  if (count == 0)
    return 0.5f; // Default if no context

  local_avg_energy /= count;
  local_avg_pr /= count;

  // Score: Ratio to average? Diff?
  // Let's use simple ratio
  float e_score = (target->event.energy > 0)
                      ? (target->event.energy / (local_avg_energy + 0.0001f))
                      : 0.0f;
  float pr_score = (target->event.peak_rate > 0)
                       ? (target->event.peak_rate / (local_avg_pr + 0.0001f))
                       : 0.0f;

  // Normalize reasonably
  // >1.0 means prominent.
  float score = 0.5f * e_score + 0.5f * pr_score;

  return score;
}

// --- API Implementation ---

SyllableDetector *syllable_create(const SyllableConfig *config) {
  SyllableConfig cfg =
      config ? *config : syllable_default_config(DEFAULT_SAMPLE_RATE);

  void *(*alloc)(size_t) = cfg.user_malloc ? cfg.user_malloc : default_malloc;

  SyllableDetector *d = (SyllableDetector *)alloc(sizeof(SyllableDetector));
  if (!d)
    return NULL;

  memset(d, 0, sizeof(SyllableDetector));
  d->config = cfg;
  d->alloc_fn = alloc;
  d->free_fn = cfg.user_free ? cfg.user_free : default_free;

  // Init DSP
  biquad_reset(&d->bp_filter);
  configure_bandpass(&d->bp_filter, &cfg);

  envelope_init(&d->env_follower, (float)cfg.sample_rate, 5.0f,
                20.0f); // 5ms attack, 20ms release

  zff_init(&d->zff, cfg.sample_rate, cfg.zff_trend_window_ms, d->alloc_fn);

  d->voiced_hold_samples =
      (int)(cfg.voiced_hold_ms * 0.001f * cfg.sample_rate);
  if (d->voiced_hold_samples < 1)
    d->voiced_hold_samples = 1;

  d->adaptive_enabled =
      (cfg.adaptive_peak_rate_k > 0.0f && cfg.adaptive_peak_rate_tau_ms > 0.0f);
  if (d->adaptive_enabled) {
    float tau_s = cfg.adaptive_peak_rate_tau_ms * 0.001f;
    d->adaptive_alpha = 1.0f / (tau_s * cfg.sample_rate);
    if (d->adaptive_alpha > 1.0f)
      d->adaptive_alpha = 1.0f;
  } else {
    d->adaptive_alpha = 0.0f;
  }

  syllable_reset(d);

  return d;
}

void syllable_reset(SyllableDetector *d) {
  d->total_samples = 0;
  d->prev_env = 0.0f;
  d->state = STATE_IDLE;
  d->is_voiced = 0;
  d->voicing_counter = 0;
  d->unvoiced_counter = 0;
  d->buf_read_idx = 0;
  d->buf_write_idx = 0;
  d->buf_count = 0;

  // Clear buffer flags
  for (int i = 0; i < PROMINENCE_BUFFER_SIZE; i++)
    d->event_buffer[i].is_ready = 0;

  // Reset DSP
  // (Re-init functions act as reset)
  biquad_reset(&d->bp_filter);
  configure_bandpass(&d->bp_filter, &d->config);
  d->env_follower.output = 0.0f;
  if (d->zff.trend_buffer && d->zff.trend_buf_size > 0) {
    memset(d->zff.trend_buffer, 0,
           (size_t)d->zff.trend_buf_size * sizeof(float));
  }
  d->zff.int1 = 0.0;
  d->zff.int2 = 0.0;
  d->zff.trend_write_pos = 0;
  d->zff.trend_accum = 0.0f;

  d->adaptive_mean = 0.0f;
  d->adaptive_var = 0.0f;
}

void syllable_destroy(SyllableDetector *d) {
  if (!d)
    return;
  zff_destroy(&d->zff, d->free_fn);
  d->free_fn(d);
}

int syllable_process(SyllableDetector *d, const float *input, int num_samples,
                     SyllableEvent *events_out, int max_events) {
  int events_written = 0;

  for (int i = 0; i < num_samples; i++) {
    float in_sample = input[i];
    d->total_samples++;

    // 1. ZFF Detection
    float zff_out, zff_slope;
    zff_process(&d->zff, in_sample, &zff_out, &zff_slope);

    // Check for Zero Crossing (Positive) -> Epoch
    int is_epoch = 0;
    if (d->last_zff_val < 0.0f && zff_out >= 0.0f) {
      is_epoch = 1;

      // Calculate F0 from epoch distance
      if (d->last_epoch_samples_ago > 0) {
        float period_s =
            (float)d->last_epoch_samples_ago / d->config.sample_rate;
        float f0 = 1.0f / period_s;

        // Sanity check F0 (50Hz - 500Hz)
        if (f0 > 50 && f0 < 600) {
          d->current_f0 = f0;
          d->voicing_counter = 5; // Sustain voicing for a few samples
        }
      }
      d->last_epoch_samples_ago = 0;
    } else {
      d->last_epoch_samples_ago++;
    }

    d->last_zff_val = zff_out;

    // Voicing logic decay
    if (d->voicing_counter > 0)
      d->is_voiced = 1;
    else
      d->is_voiced = 0; // Strict

    // Decay counters
    if (is_epoch && d->voicing_counter > 0) {
      // Refill? Maybe only on F0 update.
    } else if (!is_epoch) {
      // Decay?? No, voicing_counter is frame-based or epoch-based?
      // Let's decrement every roughly 5ms?
      // Simple approach: d->is_voiced is true if epoch happened recently (<
      // 20ms)
      if (d->last_epoch_samples_ago > d->voiced_hold_samples) {
        d->is_voiced = 0;
      } else {
        d->is_voiced = 1;
      }
    }

    // 2. Formant Envelope & Peak Rate
    float bp_out = biquad_process(&d->bp_filter, in_sample);
    float env_out = envelope_process(&d->env_follower, bp_out);

    float diff = env_out - d->prev_env;
    float peak_rate = (diff > 0.0f) ? diff : 0.0f; // Half-wave rectification
    d->prev_env = env_out;
    d->current_peak_rate = peak_rate; // Current PR

    // Adaptive threshold update (voiced only)
    if (d->adaptive_enabled && d->is_voiced) {
      float delta = peak_rate - d->adaptive_mean;
      d->adaptive_mean += d->adaptive_alpha * delta;
      d->adaptive_var =
          (1.0f - d->adaptive_alpha) *
          (d->adaptive_var + d->adaptive_alpha * delta * delta);
    }

    float threshold = d->config.threshold_peak_rate;
    if (d->adaptive_enabled) {
      float std =
          (d->adaptive_var > 0.0f) ? sqrtf(d->adaptive_var) : 0.0f;
      float adaptive =
          d->adaptive_mean + d->config.adaptive_peak_rate_k * std;
      if (adaptive > threshold)
        threshold = adaptive;
    }

    // 3. State Machine

    if (d->state == STATE_IDLE) {
      // Trigger Condition: Rising PeakRate AND Voiced
      if (peak_rate > threshold && d->is_voiced) {
        d->state = STATE_ONSET_RISING;
        d->state_timer = 0;

        // Init Event
        d->wip_event.timestamp_samples = d->total_samples;
        d->wip_event.time_seconds =
            (double)d->total_samples / d->config.sample_rate;
        d->wip_event.peak_rate = peak_rate;
        d->wip_event.energy = env_out;
        d->wip_event.f0 = d->current_f0;
        d->wip_event.duration_s = 0;

        d->max_peak_rate_in_syllable = peak_rate;
        d->energy_accum = env_out;
        d->onset_timestamp = d->total_samples;
      }
    } else if (d->state == STATE_ONSET_RISING) {
      d->state_timer++;
      d->energy_accum += env_out;

      // Track Max PeakRate
      if (peak_rate > d->max_peak_rate_in_syllable) {
        d->max_peak_rate_in_syllable = peak_rate;
        d->wip_event.peak_rate = peak_rate; // Update event
      }

      // Transition to NUCLEUS if PeakRate drops or max time reached
      // Or if Voicing stops (plosive error?)
      if (peak_rate < d->max_peak_rate_in_syllable * 0.5f) {
        d->state = STATE_NUCLEUS;
      }

      if (!d->is_voiced) {
        // Formatting error?? Voicing lost too soon. Abort?
        // For now, accept it as short syllable.
        d->state = STATE_COOLDOWN;
      }
    } else if (d->state == STATE_NUCLEUS) {
      d->state_timer++;
      d->energy_accum += env_out;

      // End of Nucleus Condition: Energy drops or Unvoiced
      // Or fixed duration logic?

      if (env_out < d->wip_event.peak_rate * 0.1f || !d->is_voiced) {
        // Syllable End
        d->state = STATE_COOLDOWN;

        // Finalize Event
        d->wip_event.duration_s =
            (float)(d->total_samples - d->onset_timestamp) /
            d->config.sample_rate;
        d->wip_event.energy = d->energy_accum; // Total Integrated Energy
        d->wip_event.f0 =
            d->current_f0; // Last known F0? Or Average? Use last for now.

        // Push to Ring Buffer
        if (d->buf_count < PROMINENCE_BUFFER_SIZE) {
          d->event_buffer[d->buf_write_idx].event = d->wip_event;
          d->event_buffer[d->buf_write_idx].is_ready = 1;
          d->buf_write_idx = (d->buf_write_idx + 1) % PROMINENCE_BUFFER_SIZE;
          d->buf_count++;
        } else {
          // Buffer Overflow - Drop oldest (force flush?)
          // In real-time, might overwrite.
          d->event_buffer[d->buf_write_idx].event = d->wip_event;
          d->event_buffer[d->buf_write_idx].is_ready = 1;
          d->buf_write_idx = (d->buf_write_idx + 1) % PROMINENCE_BUFFER_SIZE;
          d->buf_read_idx = (d->buf_read_idx + 1) %
                            PROMINENCE_BUFFER_SIZE; // Advance read too
        }
      }
    } else if (d->state == STATE_COOLDOWN) {
      d->state_timer++;
      int min_dist_samples = (int)(d->config.min_syllable_dist_ms * 0.001f *
                                   d->config.sample_rate);
      if (d->state_timer > min_dist_samples) {
        d->state = STATE_IDLE;
      }
    }

    // 4. Delayed Event Emission
    // Check if we have enough context to release the oldest event in buffer
    // Or if buffer is getting full.
    int context_needed = d->config.context_size;

    // How many "future" events do we have relative to read_idx?
    // buf_count is total events.
    // We act as if the event at 'read_idx' is the "current" one to process for
    // output, and we need 'context_needed' events AFTER it. So if buf_count >
    // context_needed, we can release one.

    while (d->buf_count > context_needed && events_written < max_events) {
      // Process Prominence for the event at read_idx
      SyllableEvent *evt = &d->event_buffer[d->buf_read_idx].event;

      float score = calculate_prominence(d, d->buf_read_idx);
      evt->prominence_score = score;
      evt->is_accented = (score > 1.2f); // Threshold?

      // Output
      events_out[events_written++] = *evt;

      // Remove from buffer
      d->event_buffer[d->buf_read_idx].is_ready = 0;
      d->buf_read_idx = (d->buf_read_idx + 1) % PROMINENCE_BUFFER_SIZE;
      d->buf_count--;
    }
  }

  return events_written;
}

int syllable_flush(SyllableDetector *d, SyllableEvent *events_out,
                   int max_events) {
  int events_written = 0;

  // Flush all remaining events
  while (d->buf_count > 0 && events_written < max_events) {
    SyllableEvent *evt = &d->event_buffer[d->buf_read_idx].event;

    // Calculate prominence with whatever context we have left
    float score = calculate_prominence(d, d->buf_read_idx);
    evt->prominence_score = score;
    evt->is_accented = (score > 1.2f);

    events_out[events_written++] = *evt;

    d->event_buffer[d->buf_read_idx].is_ready = 0;
    d->buf_read_idx = (d->buf_read_idx + 1) % PROMINENCE_BUFFER_SIZE;
    d->buf_count--;
  }

  return events_written;
}
