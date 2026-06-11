#include "audio_dsp.h"
#include "arm_math.h"
#include <math.h>

/* ── CMSIS-DSP state ── */
static arm_rfft_fast_instance_f32  rfft_inst;
static float32_t                   window[FFT_SIZE];
static float32_t                   fft_in[FFT_SIZE];
static float32_t                   fft_out[FFT_SIZE];   /* packed format */
static float32_t                   mag[FFT_SIZE / 2];   /* magnitude per bin */
static uint8_t                     dsp_ready = 0;

/* ── Log‑scale filterbank (SuperFlux style: 24 bands/octave, 440 Hz centred) ── */
static uint16_t fb_center[FILTER_BANDS];
static uint16_t fb_left[FILTER_BANDS];
static uint16_t fb_right[FILTER_BANDS];
static uint16_t fb_count = 0;
static uint8_t  fb_ready = 0;

#define FB_FMIN   30.0f
#define FB_FMAX   17000.0f
#define FB_BPO    24U
#define FB_CENTRE 440.0f

static void Filterbank_Init(void)
{
  if (fb_ready) return;

  float32_t  fft_bin_hz = 48000.0f / (float32_t)FFT_SIZE;
  uint16_t   n_fft      = FFT_SIZE / 2U;

  static float32_t up[FILTER_BANDS], down[FILTER_BANDS];
  static float32_t freq_list[FILTER_BANDS * 2];
  static uint16_t  bin_list[FILTER_BANDS * 2];
  uint16_t  n_up = 0, n_down = 0;
  uint16_t  n_freqs = 0;
  float32_t step = powf(2.0f, 1.0f / (float32_t)FB_BPO);
  float32_t f;

  f = FB_CENTRE;
  while (f <= FB_FMAX) { up[n_up++] = f; f *= step; }

  f = FB_CENTRE / step;
  while (f >= FB_FMIN) { down[n_down++] = f; f /= step; }

  uint16_t iu = 0, id = n_down;
  while (iu < n_up || id > 0) {
    if (iu >= n_up)
      freq_list[n_freqs++] = down[--id];
    else if (id == 0)
      freq_list[n_freqs++] = up[iu++];
    else if (up[iu] < down[id - 1U])
      freq_list[n_freqs++] = up[iu++];
    else
      freq_list[n_freqs++] = down[--id];
  }

  uint16_t n_bins = 0;
  for (uint16_t i = 0; i < n_freqs; i++)
  {
    uint16_t b = (uint16_t)(freq_list[i] / fft_bin_hz + 0.5f);
    if (b >= n_fft) b = n_fft - 1U;
    if (n_bins == 0 || b != bin_list[n_bins - 1U])
      bin_list[n_bins++] = b;
  }

  if (n_bins < 3U) { fb_count = 0; fb_ready = 1; return; }
  fb_count = n_bins - 2U;
  if (fb_count > FILTER_BANDS) fb_count = FILTER_BANDS;

  for (uint16_t m = 0; m < fb_count; m++)
  {
    fb_left[m]   = bin_list[m];
    fb_center[m] = bin_list[m + 1U];
    fb_right[m]  = bin_list[m + 2U];
    if (fb_left[m]  >= fb_center[m] && fb_center[m] > 0U) fb_left[m]  = fb_center[m] - 1U;
    if (fb_right[m] <= fb_center[m])                      fb_right[m] = fb_center[m] + 1U;
  }
  fb_ready = 1;
}

static void Filterbank_Apply(const float32_t *mag, uint16_t n_fft, float32_t *out)
{
  /* Pre-computed triangular filterbank: 24 bands, log-spaced edges.
   * Each band edge is the FFT bin where triangle rises from 0, peaks, falls to 0.
   * Band shapes: rising [left, centre), falling [centre, right). */
  static const uint16_t edge[POOLED_BINS + 2] = {
      1,  2,  3,  4,  5,  6,  8, 10, 12, 15, 19, 23,
     28, 34, 41, 50, 60, 73, 88,106,128,155,187,226,239,255
  };

  for (uint16_t m = 0; m < POOLED_BINS; m++)
  {
    uint16_t lo = edge[m];          /* left  edge: weight=0, then rises */
    uint16_t c  = edge[m + 1];      /* centre:     weight=1             */
    uint16_t hi = edge[m + 2];      /* right edge: weight=0 after fall  */
    float32_t sum = 0.0f;

    /* Rising edge: lo → c */
    if (c > lo) {
      float32_t denom = 1.0f / (float32_t)(c - lo);
      uint16_t end = c;
      if (end >= n_fft) end = n_fft - 1U;
      for (uint16_t k = lo; k < end; k++)
        sum += mag[k] * (float32_t)(k - lo + 1U) * denom;
    }

    /* Peak bin */
    if (c < n_fft)
      sum += mag[c];

    /* Falling edge: c+1 → hi */
    if (hi > c + 1U) {
      uint16_t start = c + 1U;
      float32_t denom = 1.0f / (float32_t)(hi - c);
      for (uint16_t k = start; k < hi && k < n_fft; k++)
        sum += mag[k] * (float32_t)(hi - k) * denom;
    }

    out[m] = sum;
  }
}

/* ── Per‑frame max (no smoothing) ── */
static float32_t spec_max_raw = 0.001f;
static float32_t sf_current_onset = 0.0f;

/* ── SuperFlux state ── */
static float32_t sf_bands_hist[SF_DIFF_FRAMES + 1U][FILTER_BANDS];
static uint16_t  sf_bands_cnt = 0;
static uint8_t   sf_wr = 0;
static float32_t sf_odf_hist[SF_ODF_HISTORY];
static float32_t sf_rms_hist[SF_ODF_HISTORY];
static uint8_t   sf_ready = 0;

void SuperFlux_Init(void)
{
  if (sf_ready) return;
  for (uint16_t i = 0; i <= SF_DIFF_FRAMES; i++)
    for (uint16_t j = 0; j < FILTER_BANDS; j++)
      sf_bands_hist[i][j] = 0.0f;
  for (uint16_t i = 0; i < SF_ODF_HISTORY; i++) {
    sf_odf_hist[i] = 0.0f;
    sf_rms_hist[i] = 0.0f;
  }
  sf_ready = 1;
}

float SuperFlux_Process(const float *bands, uint16_t n_bands)
{
  if (!sf_ready || n_bands == 0) return 0.0f;

  sf_bands_cnt = n_bands;
  if (n_bands > FILTER_BANDS) n_bands = FILTER_BANDS;

  for (uint16_t j = 0; j < n_bands; j++)
    sf_bands_hist[sf_wr][j] = bands[j];
  uint16_t cur  = sf_wr;
  uint16_t prev = (cur + 1U) % (SF_DIFF_FRAMES + 1U);

  static float32_t max_filt[FILTER_BANDS];
  for (uint16_t j = 0; j < n_bands; j++)
  {
    float32_t m = sf_bands_hist[prev][j];
    uint16_t half  = SF_MAX_BINS / 2U;
    uint16_t start = (j > half) ? (j - half) : 0U;
    uint16_t end   = (j + half + 1U < n_bands) ? (j + half + 1U) : n_bands;
    for (uint16_t k = start; k < end; k++)
      if (sf_bands_hist[prev][k] > m) m = sf_bands_hist[prev][k];
    max_filt[j] = m;
  }

  float32_t onset_func = 0.0f;
  for (uint16_t j = 0; j < n_bands; j++)
  {
    float32_t diff = bands[j] - max_filt[j];
    if (diff > 0.0f) onset_func += diff;
  }

  sf_wr = (sf_wr + 1U) % (SF_DIFF_FRAMES + 1U);

  for (uint16_t i = 0; i < SF_ODF_HISTORY - 1U; i++) {
    sf_odf_hist[i] = sf_odf_hist[i + 1U];
    sf_rms_hist[i] = sf_rms_hist[i + 1U];
  }
  sf_odf_hist[SF_ODF_HISTORY - 1U] = onset_func;

  return onset_func;
}

void SuperFlux_UpdateRMS(float rms_value)
{
  sf_rms_hist[SF_ODF_HISTORY - 1U] = rms_value;
}

const float * SuperFlux_GetODFHistory(void)
{
  return sf_odf_hist;
}

const float * SuperFlux_GetRMSHistory(void)
{
  return sf_rms_hist;
}

float SuperFlux_GetODFMax(void)
{
  float32_t m = sf_odf_hist[0];
  for (uint16_t i = 1; i < SF_ODF_HISTORY; i++)
    if (sf_odf_hist[i] > m) m = sf_odf_hist[i];
  if (m < 0.0001f) m = 0.0001f;
  return m;
}

/* ── Public API ── */
void MelSpectrogram_Init(void)
{
  if (dsp_ready) return;

  arm_rfft_fast_init_f32(&rfft_inst, FFT_SIZE);
  arm_hanning_f32(window, FFT_SIZE);
  Filterbank_Init();
  SuperFlux_Init();
  dsp_ready = 1;
}

void MelSpectrogram_Process(const uint16_t *audio_mono, float *mel_out)
{
  uint16_t n_fft = FFT_SIZE / 2U;

  /* Load last 512 samples from mono buffer, apply Hann window */
  for (uint16_t i = 0; i < FFT_SIZE; i++)
  {
    uint16_t idx = FRAME_SAMPLES - FFT_SIZE + i;
    int32_t  s   = (int32_t)audio_mono[idx] - 32768;
    fft_in[i] = (float32_t)s * (1.0f / 32768.0f) * window[i];
  }

  /* Real FFT (CMSIS-DSP) → packed output */
  arm_rfft_fast_f32(&rfft_inst, fft_in, fft_out, 0);

  /* Magnitudes from packed RFFT format */
  mag[0] = fabsf(fft_out[0]);
  for (uint16_t k = 1U; k < n_fft - 1U; k++)
  {
    float32_t re = fft_out[2U * k];
    float32_t im = fft_out[2U * k + 1U];
    mag[k] = sqrtf(re * re + im * im);
  }
  mag[n_fft - 1U] = fabsf(fft_out[1]);

  /* Triangular filterbank → 24 mel bins (pre-computed edges) */
  Filterbank_Apply(mag, n_fft, mel_out);

  arm_max_no_idx_f32(mel_out, POOLED_BINS, &spec_max_raw);
  if (spec_max_raw < 0.0001f) spec_max_raw = 0.0001f;

  /* SuperFlux onset detection */
  sf_current_onset = SuperFlux_Process(mel_out, POOLED_BINS);
}

float MelSpectrogram_GetMax(void)
{
  return (float)spec_max_raw;
}

float MelSpectrogram_GetOnset(void)
{
  return (float)sf_current_onset;
}
