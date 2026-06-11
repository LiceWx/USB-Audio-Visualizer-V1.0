#ifndef __AUDIO_DSP_H
#define __AUDIO_DSP_H

#include <stdint.h>

#define FFT_SIZE          512U
#define FILTER_BANDS      256U   /* max filterbank bands (24/octave → ~220) */
#define POOLED_BINS        24U   /* spectrogram display bins */
#define SPECTRO_BIN_WIDTH   4U   /* pixels per spectrogram bin */
#define SPECTRO_GAP         1U   /* pixels between bins */
#define SPECTRO_STRIDE      5U   /* bin + gap */
#define FRAME_SAMPLES     1024U  /* 21.3 ms @ 48 kHz, must match WIN_LEN */

/* SuperFlux onset detection */
#define SF_DIFF_FRAMES      2U   /* lookback frames (~40 ms at 50 fps) */
#define SF_MAX_BINS         3U   /* max-filter width in freq direction */
#define SF_ODF_HISTORY    128U   /* display window = full 128‑col width */

void   MelSpectrogram_Init(void);
void   MelSpectrogram_Process(const uint16_t *audio_mono, float *mel_out);
float  MelSpectrogram_GetMax(void);
float  MelSpectrogram_GetOnset(void);

void   SuperFlux_Init(void);
float  SuperFlux_Process(const float *bands, uint16_t n_bands);
void   SuperFlux_UpdateRMS(float rms_value);
const float * SuperFlux_GetODFHistory(void);
const float * SuperFlux_GetRMSHistory(void);
float  SuperFlux_GetODFMax(void);

#endif
