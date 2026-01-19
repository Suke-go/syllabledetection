#include "syllable_detector.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Minimal WAV header for output (standard 44-byte PCM)
#pragma pack(push, 1)
typedef struct {
  char riff[4];           // "RIFF"
  unsigned int file_size; // File size - 8
  char wave[4];           // "WAVE"
  char fmt_marker[4];     // "fmt "
  unsigned int fmt_size;  // 16 for PCM
  unsigned short format;  // 1 for PCM
  unsigned short channels;
  unsigned int sample_rate;
  unsigned int byte_rate;
  unsigned short block_align;
  unsigned short bits_per_sample;
  char data_marker[4]; // "data"
  unsigned int data_size;
} WavHeaderOut;
#pragma pack(pop)

// Helper to find a chunk in WAV file
static int find_chunk(FILE *fp, const char *id, unsigned int *size) {
  char chunk_id[4];
  unsigned int chunk_size;

  while (fread(chunk_id, 1, 4, fp) == 4) {
    if (fread(&chunk_size, 4, 1, fp) != 1)
      return 0;

    if (memcmp(chunk_id, id, 4) == 0) {
      *size = chunk_size;
      return 1;
    }
    // Skip this chunk
    fseek(fp, chunk_size, SEEK_CUR);
  }
  return 0;
}

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

  // Read RIFF header
  char riff[4], wave[4];
  unsigned int riff_size;
  fread(riff, 1, 4, in_fp);
  fread(&riff_size, 4, 1, in_fp);
  fread(wave, 1, 4, in_fp);

  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
    printf("Not a valid WAV file\n");
    fclose(in_fp);
    return 1;
  }

  // Find fmt chunk
  unsigned int fmt_size;
  if (!find_chunk(in_fp, "fmt ", &fmt_size)) {
    printf("Could not find fmt chunk\n");
    fclose(in_fp);
    return 1;
  }

  unsigned short format, channels, bits_per_sample, block_align;
  unsigned int sample_rate, byte_rate;

  fread(&format, 2, 1, in_fp);
  fread(&channels, 2, 1, in_fp);
  fread(&sample_rate, 4, 1, in_fp);
  fread(&byte_rate, 4, 1, in_fp);
  fread(&block_align, 2, 1, in_fp);
  fread(&bits_per_sample, 2, 1, in_fp);

  // Skip rest of fmt chunk if larger than 16 bytes
  if (fmt_size > 16) {
    fseek(in_fp, fmt_size - 16, SEEK_CUR);
  }

  printf("Processing %s\n", input_filename);
  printf("Sample Rate: %u\n", sample_rate);
  printf("Channels: %hu\n", channels);
  printf("Bits: %hu\n", bits_per_sample);
  printf("Format: %hu (1=PCM)\n", format);

  if (channels != 1) {
    printf("Warning: Only mono supported.\n");
  }
  if (bits_per_sample != 16) {
    printf("Warning: Only 16-bit supported.\n");
  }

  // Find data chunk
  unsigned int data_size;
  if (!find_chunk(in_fp, "data", &data_size)) {
    printf("Could not find data chunk\n");
    fclose(in_fp);
    return 1;
  }

  int num_samples = data_size / sizeof(short);
  printf("Data size: %u bytes (%d samples)\n", data_size, num_samples);

  short *pcm_data = (short *)malloc(data_size);
  if (!pcm_data) {
    printf("Memory allocation failed.\n");
    fclose(in_fp);
    return 1;
  }

  size_t read_count = fread(pcm_data, sizeof(short), num_samples, in_fp);
  fclose(in_fp);

  if (read_count != num_samples) {
    printf("Warning: Expected %d samples but read %zu\n", num_samples,
           read_count);
    num_samples = (int)read_count;
  }

  // Convert to float
  float *float_data = (float *)malloc(num_samples * sizeof(float));
  if (!float_data) {
    free(pcm_data);
    return 1;
  }
  for (int i = 0; i < num_samples; i++) {
    float_data[i] = pcm_data[i] / 32768.0f;
  }

  // Config
  SyllableConfig config = syllable_default_config(sample_rate);
  const char *threshold_env = getenv("SYLLABLE_THRESHOLD");
  if (threshold_env && threshold_env[0] != '\0')
    config.threshold_peak_rate = (float)atof(threshold_env);
  const char *adaptive_k_env = getenv("SYLLABLE_ADAPT_K");
  if (adaptive_k_env && adaptive_k_env[0] != '\0')
    config.adaptive_peak_rate_k = (float)atof(adaptive_k_env);
  const char *adaptive_tau_env = getenv("SYLLABLE_ADAPT_TAU_MS");
  if (adaptive_tau_env && adaptive_tau_env[0] != '\0')
    config.adaptive_peak_rate_tau_ms = (float)atof(adaptive_tau_env);
  const char *voiced_hold_env = getenv("SYLLABLE_VOICED_HOLD_MS");
  if (voiced_hold_env && voiced_hold_env[0] != '\0')
    config.voiced_hold_ms = (float)atof(voiced_hold_env);

  printf("PeakRate floor: %.6f\n", config.threshold_peak_rate);
  printf("Adaptive k: %.2f\n", config.adaptive_peak_rate_k);
  printf("Adaptive tau (ms): %.1f\n", config.adaptive_peak_rate_tau_ms);
  printf("Voiced hold (ms): %.1f\n", config.voiced_hold_ms);

  SyllableDetector *detector = syllable_create(&config);
  if (!detector) {
    printf("Failed to create detector.\n");
    free(pcm_data);
    free(float_data);
    return 1;
  }

  // Store all events
  int max_total_events = 2000;
  SyllableEvent *all_events =
      (SyllableEvent *)malloc(max_total_events * sizeof(SyllableEvent));
  int total_event_count = 0;

  // Process in chunks
  int chunk_size = 1024;
  SyllableEvent buffer_events[64];

  for (int i = 0; i < num_samples; i += chunk_size) {
    int n = (num_samples - i < chunk_size) ? (num_samples - i) : chunk_size;
    int count =
        syllable_process(detector, &float_data[i], n, buffer_events, 64);
    for (int k = 0; k < count; k++) {
      if (total_event_count < max_total_events) {
        all_events[total_event_count++] = buffer_events[k];
      }
    }
  }

  int count_flush = syllable_flush(detector, buffer_events, 64);
  for (int k = 0; k < count_flush; k++) {
    if (total_event_count < max_total_events) {
      all_events[total_event_count++] = buffer_events[k];
    }
  }

  syllable_destroy(detector);

  // Print and mix pulses
  const char *onset_type_names[] = {"V", "U", "M"}; // Voiced, Unvoiced, Mixed

  printf("\n=== Detected Syllables ===\n");
  printf("%-8s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-6s %-5s %-4s\n",
         "Time", "Peak", "SF", "HFE", "MFCC", "Wav", "Fuse", "F0", "dF0",
         "Score", "Type", "Acc");
  printf("---------------------------------------------------------------------"
         "------------\n");

  for (int i = 0; i < total_event_count; i++) {
    printf(
        "%-8.3f %-6.3f %-6.3f %-6.3f %-6.3f %-6.3f %-6.2f %-6.1f %-6.1f %-6.2f "
        "%-5s %s\n",
        all_events[i].time_seconds, all_events[i].peak_rate,
        all_events[i].spectral_flux, all_events[i].high_freq_energy,
        all_events[i].mfcc_delta, all_events[i].wavelet_score,
        all_events[i].fusion_score, all_events[i].f0, all_events[i].delta_f0,
        all_events[i].prominence_score,
        onset_type_names[all_events[i].onset_type],
        all_events[i].is_accented ? "*" : "");

    if (output_filename && all_events[i].is_accented) {
      int pos = (int)(all_events[i].time_seconds * sample_rate);
      int beep_len = sample_rate / 20;    // 50ms
      int start_pos = pos - beep_len / 2; // Center beep on timestamp
      for (int k = 0; k < beep_len; k++) {
        if (start_pos + k >= 0 && start_pos + k < num_samples) {
          float val = 0.5f * sinf(2.0f * 3.14159f * 1000.0f * k / sample_rate);
          float_data[start_pos + k] += val;
          if (float_data[start_pos + k] > 1.0f)
            float_data[start_pos + k] = 1.0f;
          if (float_data[start_pos + k] < -1.0f)
            float_data[start_pos + k] = -1.0f;
        }
      }
    }
  }

  // Write output
  if (output_filename) {
    FILE *out_fp = fopen(output_filename, "wb");
    if (!out_fp) {
      printf("Could not open output file %s\n", output_filename);
    } else {
      // Convert back to short
      for (int i = 0; i < num_samples; i++) {
        pcm_data[i] = (short)(float_data[i] * 32767.0f);
      }

      // Build standard 44-byte WAV header
      WavHeaderOut hdr;
      memcpy(hdr.riff, "RIFF", 4);
      hdr.file_size = 36 + num_samples * sizeof(short);
      memcpy(hdr.wave, "WAVE", 4);
      memcpy(hdr.fmt_marker, "fmt ", 4);
      hdr.fmt_size = 16;
      hdr.format = 1;
      hdr.channels = 1;
      hdr.sample_rate = sample_rate;
      hdr.byte_rate = sample_rate * sizeof(short);
      hdr.block_align = sizeof(short);
      hdr.bits_per_sample = 16;
      memcpy(hdr.data_marker, "data", 4);
      hdr.data_size = num_samples * sizeof(short);

      fwrite(&hdr, sizeof(WavHeaderOut), 1, out_fp);
      fwrite(pcm_data, sizeof(short), num_samples, out_fp);
      fclose(out_fp);
      printf("Written result to %s (%d samples)\n", output_filename,
             num_samples);
    }
  }

  free(pcm_data);
  free(float_data);
  free(all_events);

  return 0;
}
