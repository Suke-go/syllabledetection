#ifndef SYLLABLE_DETECTOR_H
#define SYLLABLE_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef EXPORT_DLL
#define SYLLABLE_API __declspec(dllexport)
#else
#define SYLLABLE_API __declspec(dllimport)
#endif
#else
#define SYLLABLE_API
#endif

// --- Configuration ---

typedef struct {
  int sample_rate;

  // ZFF Config
  float zff_trend_window_ms; // Window for removing low-frequency trend
                             // (default: 10.0)

  // PeakRate Config
  float peak_rate_band_min; // Bandpass min Hz (default: 500.0)
  float peak_rate_band_max; // Bandpass max Hz (default: 3200.0)

  // Detection Logic
  float min_syllable_dist_ms; // Minimum distance between syllables (default:
                              // 100.0)
  float threshold_peak_rate;  // Absolute floor for peakRate threshold (default:
                              // 0.0003)
  float adaptive_peak_rate_k; // Adaptive threshold = mean + k*std (0 disables,
                              // default: 4.0)
  float adaptive_peak_rate_tau_ms; // Time constant for adaptive stats (default:
                                   // 500.0)
  float voiced_hold_ms;            // Voicing hold window (default: 30.0)

  // Hysteresis (to prevent chattering)
  float hysteresis_on_factor;  // Multiplier for ON threshold (default: 1.3)
  float hysteresis_off_factor; // Multiplier for OFF threshold (default: 0.7)

  // Prominence Context
  int context_size; // Number of syllables to look ahead/behind (default: 2)

  // User Memory (Optional, set to NULL to use malloc/free)
  void *(*user_malloc)(size_t);
  void (*user_free)(void *);
} SyllableConfig;

// --- Output Event ---

typedef struct {
  uint64_t timestamp_samples; // Sample index of the onset
  double time_seconds;        // Time in seconds

  // Feature Values
  float peak_rate;  // Max slope at onset
  float pr_slope;   // PeakRate rise slope (peak_rate / rise_time_s)
  float energy;     // Integrated energy
  float f0;         // Fundamental frequency (ZFF derived)
  float delta_f0;   // F0 difference from context median
  float duration_s; // Estimated duration

  // Prominence / Accent
  float prominence_score; // 0.0 to 1.0 (or higher), relative to context
  int is_accented;        // 1 if accented, 0 otherwise
} SyllableEvent;

// --- Opaque Handle ---
typedef struct SyllableDetector SyllableDetector;

// --- API Functions ---

// Create a new detector instance
SYLLABLE_API SyllableDetector *syllable_create(const SyllableConfig *config);

// Reset internal state (e.g. for new file)
SYLLABLE_API void syllable_reset(SyllableDetector *detector);

// Process a block of audio samples (mono, float)
// Returns the number of events detected in this block (that are now "ready"
// after context delay) Populates 'events_out' up to 'max_events' capacity.
SYLLABLE_API int syllable_process(SyllableDetector *detector,
                                  const float *input, int num_samples,
                                  SyllableEvent *events_out, int max_events);

// Flush any remaining events in the buffer (e.g. at end of file)
SYLLABLE_API int syllable_flush(SyllableDetector *detector,
                                SyllableEvent *events_out, int max_events);

// Destroy the instance
SYLLABLE_API void syllable_destroy(SyllableDetector *detector);

// Helper to get default config
SYLLABLE_API SyllableConfig syllable_default_config(int sample_rate);

#ifdef __cplusplus
}
#endif

#endif // SYLLABLE_DETECTOR_H
