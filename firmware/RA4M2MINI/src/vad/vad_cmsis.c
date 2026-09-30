/*
 * vad_cmsis.c
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#include "vad_cmsis.h"

#include <math.h>
#include <string.h>

static float32_t s_vad_time[FFT_SIZE];

static uint32_t  s_calibration_count = 0U;
static float32_t s_noise_rms         = VAD_MIN_RMS;
static uint32_t  s_attack_count      = 0U;
static uint32_t  s_hangover_count    = 0U;
static bool      s_voice_state       = false;

static float32_t max_f32(float32_t a, float32_t b)
{
    return (a > b) ? a : b;
}

static uint32_t hz_to_bin_ceil(float32_t hz)
{
    uint32_t bin = (uint32_t) ceilf(hz / FFT_BIN_HZ);

    if (bin >= FFT_BINS)
    {
        bin = FFT_BINS - 1U;
    }

    return bin;
}

static uint32_t hz_to_bin_floor(float32_t hz)
{
    uint32_t bin = (uint32_t) floorf(hz / FFT_BIN_HZ);

    if (bin >= FFT_BINS)
    {
        bin = FFT_BINS - 1U;
    }

    return bin;
}

static float32_t compute_rms_without_dc(const float32_t * input)
{
    float32_t mean = 0.0f;
    float32_t rms  = 0.0f;

    arm_copy_f32(input, s_vad_time, FFT_SIZE);

    arm_mean_f32(s_vad_time,
                 FFT_SIZE,
                 &mean);

    arm_offset_f32(s_vad_time,
                   -mean,
                   s_vad_time,
                   FFT_SIZE);

    arm_rms_f32(s_vad_time,
                FFT_SIZE,
                &rms);

    return rms;
}

static void compute_spectral_features(const float32_t * magnitude,
                                      float32_t       * band_ratio,
                                      float32_t       * spectral_crest)
{
    const uint32_t total_start = 1U;
    const uint32_t total_count = FFT_BINS - 1U;

    uint32_t speech_start = hz_to_bin_ceil(VAD_SPEECH_LOW_HZ);
    uint32_t speech_end   = hz_to_bin_floor(VAD_SPEECH_HIGH_HZ);

    if (speech_start < 1U)
    {
        speech_start = 1U;
    }

    if (speech_end < speech_start)
    {
        speech_end = speech_start;
    }

    float32_t total_power  = 0.0f;
    float32_t speech_power = 0.0f;

    arm_power_f32(&magnitude[total_start],
                  total_count,
                  &total_power);

    arm_power_f32(&magnitude[speech_start],
                  speech_end - speech_start + 1U,
                  &speech_power);

    *band_ratio = speech_power / (total_power + 1.0e-12f);

    float32_t max_mag = 0.0f;
    uint32_t max_index = 0U;

    arm_max_f32(&magnitude[total_start],
                total_count,
                &max_mag,
                &max_index);

    (void) max_index;

    float32_t mean_power =
        total_power / (float32_t) total_count;

    *spectral_crest =
        (max_mag * max_mag) / (mean_power + 1.0e-12f);
}

void VAD_Init(void)
{
    VAD_Reset();
}

void VAD_Reset(void)
{
    memset(s_vad_time, 0, sizeof(s_vad_time));

    s_calibration_count = 0U;
    s_noise_rms         = VAD_MIN_RMS;
    s_attack_count      = 0U;
    s_hangover_count    = 0U;
    s_voice_state       = false;
}
/* VAD功能函数 */
bool VAD_ProcessFrame(const float32_t * time_frame,
                      const float32_t * magnitude,
                      vad_result_t     * result)
{
    if ((NULL == time_frame) ||
        (NULL == magnitude) ||
        (NULL == result))
    {
        return false;
    }

    float32_t rms = compute_rms_without_dc(time_frame);

    float32_t band_ratio = 0.0f;
    float32_t spectral_crest = 0.0f;

    compute_spectral_features(magnitude,
                              &band_ratio,
                              &spectral_crest);

    float32_t peak_frequency_hz =
        FFT_Spectrum_GetPeakFrequency(magnitude);

    if (s_calibration_count < VAD_CALIBRATION_FRAMES)
    {
        if (0U == s_calibration_count)
        {
            s_noise_rms = max_f32(rms, 1.0e-5f);
        }
        else
        {
            float32_t n = (float32_t) s_calibration_count;

            s_noise_rms =
                ((s_noise_rms * n) + rms) / (n + 1.0f);
        }

        s_calibration_count++;

        s_voice_state    = false;
        s_attack_count   = 0U;
        s_hangover_count = 0U;

        result->is_voice          = false;
        result->raw_voice         = false;
        result->calibrated        =
            (s_calibration_count >= VAD_CALIBRATION_FRAMES);
        result->rms               = rms;
        result->noise_rms         = s_noise_rms;
        result->snr_db            = 0.0f;
        result->band_ratio        = band_ratio;
        result->spectral_crest    = spectral_crest;
        result->peak_frequency_hz = peak_frequency_hz;

        return false;
    }

    float32_t energy_threshold =
        max_f32(VAD_MIN_RMS,
                s_noise_rms * VAD_NOISE_RATIO);

    bool energy_ok =
        (rms >= energy_threshold);

    bool spectrum_ok =
        ((band_ratio >= VAD_MIN_BAND_RATIO) ||
         (spectral_crest >= VAD_MIN_SPECTRAL_CREST));

    bool raw_voice =
        energy_ok && spectrum_ok;

    if (!raw_voice)
    {
        s_noise_rms =
            (VAD_NOISE_ALPHA * s_noise_rms) +
            ((1.0f - VAD_NOISE_ALPHA) * rms);

        if (s_noise_rms < 1.0e-5f)
        {
            s_noise_rms = 1.0e-5f;
        }
    }

    if (raw_voice)
    {
        if (s_attack_count < VAD_ATTACK_FRAMES)
        {
            s_attack_count++;
        }

        if (s_attack_count >= VAD_ATTACK_FRAMES)
        {
            s_voice_state = true;
            s_hangover_count = VAD_HANGOVER_FRAMES;
        }
    }
    else
    {
        s_attack_count = 0U;

        if (s_voice_state)
        {
            if (s_hangover_count > 0U)
            {
                s_hangover_count--;
            }
            else
            {
                s_voice_state = false;
            }
        }
    }

    float32_t snr_db =
        20.0f * log10f((rms + 1.0e-9f) /
                       (s_noise_rms + 1.0e-9f));

    result->is_voice          = s_voice_state;
    result->raw_voice         = raw_voice;
    result->calibrated        = true;
    result->rms               = rms;
    result->noise_rms         = s_noise_rms;
    result->snr_db            = snr_db;
    result->band_ratio        = band_ratio;
    result->spectral_crest    = spectral_crest;
    result->peak_frequency_hz = peak_frequency_hz;

    return s_voice_state;
}
