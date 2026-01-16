#include "syllable_detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WAV Header struct
typedef struct {
  char riff[4];
  int overall_size;
  char wave[4];
  char fmt_chunk_marker[4];
  int length_of_fmt;
  short format_type;
  short channels;
  int sample_rate;
  int byterate;
  short block_align;
  short bits_per_sample;
  char data_chunk_header[4];
  int data_size;
} WavHeader;

#include <math.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <input.wav> [output.wav]\n", argv[0]);
    return 1;
  }

  const char *input_filename = argv[1];
  const char *output_filename = (argc >= 3) ? argv[2] : NULL;

  FILE *in_fp = fopen(input_filename, "rb");
  if (!in_fp) {
    printf("Could not open input file %s\n", input_filename);
    return 1;
  }

  WavHeader header;
  if (fread(&header, sizeof(WavHeader), 1, in_fp) != 1) {
    printf("Failed to read WAV header\n");
    fclose(in_fp);
    return 1;
  }

  printf("Processing %s\n", input_filename);
  printf("Sample Rate: %d\n", header.sample_rate);
  printf("Channels: %d\n", header.channels);
  printf("Bits: %d\n", header.bits_per_sample);

  if (header.channels != 1) {
    printf("Warning: Only mono supported. Processing first channel only if "
           "interleaved.\n");
  }

  FILE *out_fp = NULL;
  if (output_filename) {
    out_fp = fopen(output_filename, "wb");
    if (!out_fp) {
      printf("Could not open output file %s\n", output_filename);
      fclose(in_fp);
      return 1;
    }
    // Write header to output (will update size later if needed, strictly we
    // should but for now copy is ok)
    fwrite(&header, sizeof(WavHeader), 1, out_fp);
  }

  // Config
  SyllableConfig config = syllable_default_config(header.sample_rate);
  const char *threshold_env = getenv("SYLLABLE_THRESHOLD");
  if (threshold_env && threshold_env[0] != '\0') {
    config.threshold_peak_rate = (float)atof(threshold_env);
  }
  const char *adaptive_k_env = getenv("SYLLABLE_ADAPT_K");
  if (adaptive_k_env && adaptive_k_env[0] != '\0') {
    config.adaptive_peak_rate_k = (float)atof(adaptive_k_env);
  }
  const char *adaptive_tau_env = getenv("SYLLABLE_ADAPT_TAU_MS");
  if (adaptive_tau_env && adaptive_tau_env[0] != '\0') {
    config.adaptive_peak_rate_tau_ms = (float)atof(adaptive_tau_env);
  }
  const char *voiced_hold_env = getenv("SYLLABLE_VOICED_HOLD_MS");
  if (voiced_hold_env && voiced_hold_env[0] != '\0') {
    config.voiced_hold_ms = (float)atof(voiced_hold_env);
  }
  printf("PeakRate floor: %.6f\n", config.threshold_peak_rate);
  printf("Adaptive k: %.2f\n", config.adaptive_peak_rate_k);
  printf("Adaptive tau (ms): %.1f\n", config.adaptive_peak_rate_tau_ms);
  printf("Voiced hold (ms): %.1f\n", config.voiced_hold_ms);
  SyllableDetector *detector = syllable_create(&config);
  if (!detector) {
    printf("Failed to create detector.\n");
    if (out_fp)
      fclose(out_fp);
    fclose(in_fp);
    return 1;
  }

// Process loop
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64

  // We assume 16-bit PCM for now.
  short pcm_buffer[BUFFER_SIZE];
  float float_buffer[BUFFER_SIZE];
  SyllableEvent events[MAX_EVENTS];

  long long total_samples_read = 0;
  int sample_rate = header.sample_rate;

  int samples_read;
  while ((samples_read = fread(pcm_buffer, sizeof(short), BUFFER_SIZE, in_fp)) >
         0) {
    // Convert to float
    for (int i = 0; i < samples_read; i++) {
      // Simple conversion for 16-bit
      float_buffer[i] = pcm_buffer[i] / 32768.0f;
    }

    int count = syllable_process(detector, float_buffer, samples_read, events,
                                 MAX_EVENTS);

    // If outputting, mix pulses
    if (out_fp) {
      for (int i = 0; i < count; i++) {
        if (!events[i].is_accented)
          continue;

        // Calculate sample index relative to the start of THIS buffer
        // events[i].time_seconds is the global time
        double global_time = events[i].time_seconds;
        long long event_sample_global = (long long)(global_time * sample_rate);

        // We need to inject the pulse into the pcm_buffer
        // The pulse might span across buffers, but for simplicity let's just
        // draw it if it falls in the current buffer.
        // A 10ms pulse is ~160 samples at 16k.

        // To do this strictly correctly given the streaming nature (events
        // might be slightly delayed or past?) syllable_process returns events
        // that were detected *just now*. Their timestamps 'should' be within
        // the recent past.

        // Let's assume we can just overwrite the buffer at the corresponding
        // location. We need to map time_seconds back to an index in the current
        // float_buffer ??? Wait, syllable_process might return an event that
        // happened a bit ago due to smoothing delay. If the delay is larger
        // than BUFFER_SIZE, we can't write it to the *current* buffer
        // effectively if we've already written that buffer to disk. BUT,
        // usually we write to disk *after* processing. Let's check: we haven't
        // written `pcm_buffer` to `out_fp` yet in this loop.

        // However, if the event time is older than `total_samples_read` (start
        // of this buffer), then we missed our chance to write it to the file
        // for *that* exact sample. Let's just write the pulse at end of the
        // buffer or "now" if it's too late? Or better, just inject a "beep" at
        // the *current* position in the audio stream whenever an event is
        // emitted. The precise timing alignment in the output audio might be
        // slightly off (delayed) but for "listening" it serves the purpose of
        // "marking detection".

        // Actually, let's try to be somewhat accurate.
        // relative_index = event_sample_global - total_samples_read;
        // If relative_index is within [0, samples_read), we mix here.
        // If it's negative (happened in past buffer), we just mix at 0 (or
        // ignore?). If it's positive (future?), mix there.

        // Let's print the delay to see.
        // long long current_buffer_start = total_samples_read;
        // long long diff = event_sample_global - current_buffer_start;
        // printf("Diff: %lld\n", diff);

        // For a simple 'mark', let's just create a 100ms tone at the *current*
        // frame being processed where the event was detected. Since `events`
        // are output *after* processing `samples_read`, let's just burn the
        // mark into the *next* detected spots or right here.

        // Simpler approach: Just beep at the location corresponding to
        // `event.time_seconds`. If that location is already written to disk, we
        // can't easily go back with streaming write unless we fseek. `out_fp`
        // is a file, so we CAN fseek.

        long long global_pos =
            (long long)(events[i].time_seconds * sample_rate);
        long file_offset = sizeof(WavHeader) + global_pos * sizeof(short);

        // Remember current pos
        long current_write_pos = ftell(out_fp);

        // Seek and write pulse
        if (fseek(out_fp, file_offset, SEEK_SET) == 0) {
          // Generate 100ms beep
          int beep_len = sample_rate / 20; // 50ms
          short *beep_buf = malloc(beep_len * sizeof(short));
          // Read existing if possible to mix? Reading from write_mode file
          // might be tricky. Let's just overwrite for maximum clarity "Pulse".
          for (int k = 0; k < beep_len; ++k) {
            beep_buf[k] = (short)(15000.0 * sin(2.0 * 3.14159 * 1000.0 * k /
                                                sample_rate));
          }
          fwrite(beep_buf, sizeof(short), beep_len, out_fp);
          free(beep_buf);

          // Restore pos
          fseek(out_fp, current_write_pos, SEEK_SET);
        }
      }

      // Write the original buffer (or mixed if we did memory mixing)
      // Since we did fseek mixing, we can just write the original buffer now.
      // WAIT. If we write the original buffer NOW, we might OVERWRITE the pulse
      // we just wrote if the pulse was inside THIS buffer range. Yes.

      // Better Strategy:
      // 1. Modify `pcm_buffer` in memory if the event is inside
      // [total_samples_read, total_samples_read + samples_read].
      // 2. If the event is in the PAST (previous buffers), fseek backwards and
      // modify file.

      for (int i = 0; i < count; i++) {
        if (!events[i].is_accented)
          continue;

        long long global_pos =
            (long long)(events[i].time_seconds * sample_rate);
        long long rel_pos = global_pos - total_samples_read;

        int beep_len = sample_rate / 20; // 50ms

        for (int k = 0; k < beep_len; ++k) {
          long long p = rel_pos + k;
          short sample_val =
              (short)(15000.0 * sin(2.0 * 3.14159 * 1000.0 * k / sample_rate));

          // If inside current buffer
          if (p >= 0 && p < samples_read) {
            pcm_buffer[p] =
                sample_val; // Overwrite or Mix? Overwrite is clearer.
          } else if (p < 0) {
            // Inside previous buffer - modify file
            long file_offset =
                sizeof(WavHeader) + (total_samples_read + p) * sizeof(short);
            long current = ftell(out_fp); // Should be start of this write? No,
                                          // we haven't written yet.
            // Actually `out_fp` points to where we are ABOUT to write
            // `pcm_buffer`. So `ftell(out_fp)` should be `sizeof(WavHeader) +
            // total_samples_read * 2`.

            if (fseek(out_fp, file_offset, SEEK_SET) == 0) {
              fwrite(&sample_val, sizeof(short), 1, out_fp);
              fseek(out_fp, current, SEEK_SET);
            }
          }
          // If p >= samples_read, it spills into next buffer. We can't handle
          // it easily here unless we carry over state. But typically detections
          // describe things that just happened, so p shouldn't be far in
          // future. Actually delay is usually positive, so `global_pos` <
          // `current_time`. `rel_pos` is likely negative.
        }

        printf("[Syllable] Time: %.3fs | Peak: %.3f | Eng: %.3f | F0: %.1f | "
               "Sc: %.2f\n",
               events[i].time_seconds, events[i].peak_rate, events[i].energy,
               events[i].f0, events[i].prominence_score);
      }

      // Write buffer
      fwrite(pcm_buffer, sizeof(short), samples_read, out_fp);
    }

    for (int i = 0; i < count; i++) {
      if (!out_fp) {
        printf("[Syllable] Time: %.3fs | Peak: %.3f | Eng: %.3f | F0: %.1f | "
               "Sc: %.2f\n",
               events[i].time_seconds, events[i].peak_rate, events[i].energy,
               events[i].f0, events[i].prominence_score);
      }
    }

    total_samples_read += samples_read;
  }

  // Flush
  int count = syllable_flush(detector, events, MAX_EVENTS);
  for (int i = 0; i < count; i++) {
    printf("[Syllable] Time: %.3fs | PeakRate: %.3f | Energy: %.3f | F0: %.1f "
           "| Score: %.2f\n",
           events[i].time_seconds, events[i].peak_rate, events[i].energy,
           events[i].f0, events[i].prominence_score);
    // Flush pulses could be added too, similar logic.
    if (out_fp) {
      if (events[i].is_accented) {
        long long global_pos =
            (long long)(events[i].time_seconds * sample_rate);
        int beep_len = sample_rate / 20; // 50ms
        for (int k = 0; k < beep_len; k++) {
          short sample_val =
              (short)(15000.0 * sin(2.0 * 3.14159 * 1000.0 * k / sample_rate));
          long file_offset =
              sizeof(WavHeader) + (global_pos + k) * sizeof(short);
          fseek(out_fp, file_offset, SEEK_SET); // YOLO checks
          fwrite(&sample_val, sizeof(short), 1, out_fp);
        }
        // Seek back to end
        fseek(out_fp, 0, SEEK_END);
      }
    }
  }

  syllable_destroy(detector);
  if (out_fp)
    fclose(out_fp);
  fclose(in_fp);
  return 0;
}
