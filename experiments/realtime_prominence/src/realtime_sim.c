/*
 * realtime_sim.c - Real-time Simulation from WAV file
 *
 * WAVファイルをリアルタイム風に処理し、プロミネンス検出をシミュレート。
 * syllabledetection ライブラリを使用。
 *
 * Usage:
 *   ./realtime_sim input.wav [--speed 1.0] [--fast]
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#include "syllable_detector.h"

/* Configuration */
#define CHUNK_SIZE 256 /* Samples per processing chunk (~16ms at 16kHz) */
#define MAX_EVENTS 16

/* WAV header structure */
#pragma pack(push, 1)
typedef struct {
  char riff[4];
  uint32_t file_size;
  char wave[4];
  char fmt[4];
  uint32_t fmt_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
} WavHeader;
#pragma pack(pop)

/* Print event with explainable features */
static void print_event(const SyllableEvent *event) {
  const char *onset_str = "UNKNOWN";
  switch (event->onset_type) {
  case ONSET_TYPE_VOICED:
    onset_str = "VOICED";
    break;
  case ONSET_TYPE_UNVOICED:
    onset_str = "UNVOICED";
    break;
  case ONSET_TYPE_MIXED:
    onset_str = "MIXED";
    break;
  }

  printf("\n[%6.2fs] PROMINENCE | Score: %.2f | PR:%.2f SF:%.2f HF:%.2f | %s\n",
         event->time_seconds, event->fusion_score, event->peak_rate,
         event->spectral_flux, event->high_freq_energy, onset_str);

  /* Explainable feedback */
  printf("  -> Feedback: ");
  if (event->peak_rate < 0.4f && event->prominence_score > 0.5f) {
    printf("Vowel onset is gradual - make it crisper\n");
  } else if (event->spectral_flux < 0.3f && event->high_freq_energy < 0.3f) {
    printf("Consonant release unclear - articulate more\n");
  } else if (event->prominence_score > 0.7f) {
    printf("Good prominence - well stressed!\n");
  } else {
    printf("Moderate prominence detected\n");
  }
  fflush(stdout);
}

/* Progress bar */
static void print_progress(double percent, int events) {
  int width = 40;
  int filled = (int)(width * percent / 100.0);

  printf("\r[");
  for (int i = 0; i < width; i++) {
    if (i < filled)
      printf("=");
    else
      printf(" ");
  }
  printf("] %.1f%% | Events: %d  ", percent, events);
  fflush(stdout);
}

int main(int argc, char *argv[]) {
  const char *input_file = NULL;
  float speed = 1.0f;
  int simulate_realtime = 1;

  /* Parse arguments */
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) {
      speed = (float)atof(argv[++i]);
    } else if (strcmp(argv[i], "--fast") == 0) {
      simulate_realtime = 0;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("Usage: %s input.wav [options]\n", argv[0]);
      printf("Options:\n");
      printf("  --speed X   Playback speed multiplier (default: 1.0)\n");
      printf("  --fast      Process as fast as possible (no simulation)\n");
      return 0;
    } else if (argv[i][0] != '-') {
      input_file = argv[i];
    }
  }

  if (!input_file) {
    fprintf(stderr, "Usage: %s input.wav [--speed X] [--fast]\n", argv[0]);
    return 1;
  }

  /* Open WAV file */
  FILE *fp = fopen(input_file, "rb");
  if (!fp) {
    fprintf(stderr, "Failed to open: %s\n", input_file);
    return 1;
  }

  /* Read WAV header */
  WavHeader header;
  if (fread(&header, sizeof(header), 1, fp) != 1) {
    fprintf(stderr, "Failed to read WAV header\n");
    fclose(fp);
    return 1;
  }

  /* Validate */
  if (memcmp(header.riff, "RIFF", 4) != 0 ||
      memcmp(header.wave, "WAVE", 4) != 0) {
    fprintf(stderr, "Not a valid WAV file\n");
    fclose(fp);
    return 1;
  }

  printf("\n");
  printf("========================================================\n");
  printf("  Real-time Prominence Detection Simulator\n");
  printf("========================================================\n");
  printf("  File:         %s\n", input_file);
  printf("  Sample Rate:  %u Hz\n", header.sample_rate);
  printf("  Channels:     %u\n", header.num_channels);
  printf("  Bits/Sample:  %u\n", header.bits_per_sample);
  printf("  Speed:        %.1fx%s\n", speed,
         simulate_realtime ? "" : " (fast mode)");
  printf("========================================================\n\n");

  /* Find data chunk */
  char chunk_id[4];
  uint32_t chunk_size;
  while (fread(chunk_id, 4, 1, fp) == 1) {
    if (fread(&chunk_size, 4, 1, fp) != 1)
      break;
    if (memcmp(chunk_id, "data", 4) == 0)
      break;
    fseek(fp, chunk_size, SEEK_CUR);
  }

  /* Calculate duration */
  uint32_t data_size = chunk_size;
  int bytes_per_sample = header.bits_per_sample / 8;
  uint32_t total_samples = data_size / bytes_per_sample / header.num_channels;
  double duration = (double)total_samples / header.sample_rate;

  printf("Duration: %.1f seconds\n\n", duration);

  /* Initialize detector */
  SyllableConfig config = syllable_default_config((int)header.sample_rate);
  SyllableDetector *detector = syllable_create(&config);
  if (!detector) {
    fprintf(stderr, "Failed to create detector\n");
    fclose(fp);
    return 1;
  }

  /* Processing loop */
  float buffer[CHUNK_SIZE];
  int16_t raw_buffer[CHUNK_SIZE * 2]; /* Stereo support */
  SyllableEvent events[MAX_EVENTS];

  uint64_t samples_processed = 0;
  int event_count = 0;
  int chunk_delay_ms = (int)(1000.0 * CHUNK_SIZE / header.sample_rate / speed);

  while (!feof(fp)) {
    /* Read chunk */
    size_t samples_read;
    if (header.bits_per_sample == 16) {
      samples_read = fread(raw_buffer, sizeof(int16_t) * header.num_channels,
                           CHUNK_SIZE, fp);
      /* Convert to float, mono */
      for (size_t i = 0; i < samples_read; i++) {
        if (header.num_channels == 2) {
          buffer[i] = (raw_buffer[i * 2] + raw_buffer[i * 2 + 1]) / 65536.0f;
        } else {
          buffer[i] = raw_buffer[i] / 32768.0f;
        }
      }
    } else if (header.bits_per_sample == 32) {
      /* Assume float */
      samples_read = fread(buffer, sizeof(float), CHUNK_SIZE, fp);
    } else {
      fprintf(stderr, "Unsupported bit depth: %d\n", header.bits_per_sample);
      break;
    }

    if (samples_read == 0)
      break;

    /* Process */
    int num_events = syllable_process(detector, buffer, (int)samples_read,
                                      events, MAX_EVENTS);

    /* Print events */
    for (int i = 0; i < num_events; i++) {
      print_event(&events[i]);
      event_count++;
    }

    samples_processed += samples_read;
    double percent = 100.0 * samples_processed / total_samples;

    /* Show progress */
    if (simulate_realtime) {
      print_progress(percent, event_count);
      SLEEP_MS(chunk_delay_ms);
    } else if (samples_processed % (header.sample_rate / 4) == 0) {
      print_progress(percent, event_count);
    }
  }

  /* Flush remaining events */
  int flush_count = syllable_flush(detector, events, MAX_EVENTS);
  for (int i = 0; i < flush_count; i++) {
    print_event(&events[i]);
    event_count++;
  }

  /* Cleanup */
  syllable_destroy(detector);
  fclose(fp);

  /* Summary */
  printf("\n\n");
  printf("========================================================\n");
  printf("  Processing Complete\n");
  printf("========================================================\n");
  printf("  Duration:     %.1f seconds\n", duration);
  printf("  Events:       %d prominences detected\n", event_count);
  if (duration > 0) {
    printf("  Rate:         %.2f events/sec (%.1f syllables/sec)\n",
           event_count / duration, event_count / duration);
  }
  printf("========================================================\n");

  return 0;
}
