/*
 * spectral_subtract.c
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#include "spectral_subtract.h"

#include <math.h>
#include <string.h>

#define SPEC_SUB_ALPHA        (1.35f)
#define SPEC_SUB_GAIN_FLOOR   (0.08f)

static arm_rfft_fast_instance_f32 s_fft;

static float32_t s_window[SPEC_SUB_FFT_SIZE];
static float32_t s_time[SPEC_SUB_FFT_SIZE];
static float32_t s_freq[SPEC_SUB_FFT_SIZE];
static float32_t s_noise_acc[SPEC_SUB_BINS];
static float32_t s_noise_mag[SPEC_SUB_BINS];
static float32_t s_overlap[SPEC_SUB_HOP_SIZE];

static uint32_t s_noise_frames = 0U;
static bool s_noise_ready = false;

static float32_t clampf_local(float32_t x, float32_t lo, float32_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void create_sqrt_hann(void)
{
    const float32_t two_pi = 6.2831853071795864769f;

    for (uint32_t n = 0U; n < SPEC_SUB_FFT_SIZE; n++)
    {
        /* Periodic Hann gives good 50% COLA behavior. */
        float32_t h = 0.5f -
                      0.5f * arm_cos_f32(two_pi *
                                        (float32_t)n /
                                        (float32_t)SPEC_SUB_FFT_SIZE);
        if (h < 0.0f) h = 0.0f;
        s_window[n] = sqrtf(h);
    }
}

static void packed_magnitude(const float32_t * packed,
                             float32_t       * mag)
{
    mag[0] = fabsf(packed[0]);
    mag[SPEC_SUB_FFT_SIZE / 2U] = fabsf(packed[1]);

    arm_cmplx_mag_f32(&packed[2],
                      &mag[1],
                      (SPEC_SUB_FFT_SIZE / 2U) - 1U);
}

void SpectralSubtract_Init(void)
{
    (void)arm_rfft_fast_init_f32(&s_fft, SPEC_SUB_FFT_SIZE);
    create_sqrt_hann();
    SpectralSubtract_ResetNoise();
}

void SpectralSubtract_ResetNoise(void)
{
    memset(s_noise_acc, 0, sizeof(s_noise_acc));
    memset(s_noise_mag, 0, sizeof(s_noise_mag));
    memset(s_overlap, 0, sizeof(s_overlap));
    s_noise_frames = 0U;
    s_noise_ready = false;
}

void SpectralSubtract_LearnNoise(const float32_t * frame)
{
    if (frame == NULL)
    {
        return;
    }

    float32_t mean = 0.0f;
    arm_mean_f32(frame, SPEC_SUB_FFT_SIZE, &mean);

    for (uint32_t i = 0U; i < SPEC_SUB_FFT_SIZE; i++)
    {
        s_time[i] = (frame[i] - mean) * s_window[i];
    }

    arm_rfft_fast_f32(&s_fft, s_time, s_freq, 0);

    float32_t mag[SPEC_SUB_BINS];
    packed_magnitude(s_freq, mag);

    for (uint32_t k = 0U; k < SPEC_SUB_BINS; k++)
    {
        s_noise_acc[k] += mag[k];
    }

    s_noise_frames++;
}

void SpectralSubtract_FinalizeNoise(void)
{
    if (s_noise_frames == 0U)
    {
        s_noise_ready = false;
        return;
    }

    float32_t inv = 1.0f / (float32_t)s_noise_frames;

    for (uint32_t k = 0U; k < SPEC_SUB_BINS; k++)
    {
        s_noise_mag[k] = s_noise_acc[k] * inv;
    }

    s_noise_ready = true;
}

bool SpectralSubtract_HasNoiseProfile(void)
{
    return s_noise_ready;
}

static void apply_gain_to_packed_spectrum(void)
{
    /* DC */
    {
        float32_t mag = fabsf(s_freq[0]);
        float32_t gain = 1.0f -
                         SPEC_SUB_ALPHA * s_noise_mag[0] /
                         (mag + 1.0e-12f);
        gain = clampf_local(gain, SPEC_SUB_GAIN_FLOOR, 1.0f);
        s_freq[0] *= gain;
    }

    /* Nyquist */
    {
        uint32_t k = SPEC_SUB_FFT_SIZE / 2U;
        float32_t mag = fabsf(s_freq[1]);
        float32_t gain = 1.0f -
                         SPEC_SUB_ALPHA * s_noise_mag[k] /
                         (mag + 1.0e-12f);
        gain = clampf_local(gain, SPEC_SUB_GAIN_FLOOR, 1.0f);
        s_freq[1] *= gain;
    }

    for (uint32_t k = 1U; k < (SPEC_SUB_FFT_SIZE / 2U); k++)
    {
        float32_t re = s_freq[2U * k];
        float32_t im = s_freq[2U * k + 1U];

        float32_t mag = sqrtf(re * re + im * im);

        float32_t gain = 1.0f -
                         SPEC_SUB_ALPHA * s_noise_mag[k] /
                         (mag + 1.0e-12f);

        gain = clampf_local(gain, SPEC_SUB_GAIN_FLOOR, 1.0f);

        s_freq[2U * k]     = re * gain;
        s_freq[2U * k + 1U] = im * gain;
    }
}

void SpectralSubtract_ProcessADC12InPlace(uint16_t * samples,
                                          uint32_t   count)
{
    if ((samples == NULL) || (count == 0U) || (!s_noise_ready))
    {
        return;
    }

    memset(s_overlap, 0, sizeof(s_overlap));

    for (uint32_t start = 0U; start < count; start += SPEC_SUB_HOP_SIZE)
    {
        uint32_t available = count - start;
        uint32_t copy_n = (available > SPEC_SUB_FFT_SIZE) ?
                          SPEC_SUB_FFT_SIZE : available;

        float32_t mean = 0.0f;

        for (uint32_t i = 0U; i < copy_n; i++)
        {
            s_time[i] =
                ((float32_t)((int32_t)samples[start + i] - 2048))
                / 2048.0f;
        }

        for (uint32_t i = copy_n; i < SPEC_SUB_FFT_SIZE; i++)
        {
            s_time[i] = 0.0f;
        }

        arm_mean_f32(s_time, copy_n, &mean);

        for (uint32_t i = 0U; i < SPEC_SUB_FFT_SIZE; i++)
        {
            s_time[i] = (s_time[i] - mean) * s_window[i];
        }

        arm_rfft_fast_f32(&s_fft, s_time, s_freq, 0);
        apply_gain_to_packed_spectrum();
        arm_rfft_fast_f32(&s_fft, s_freq, s_time, 1);

        for (uint32_t i = 0U; i < SPEC_SUB_FFT_SIZE; i++)
        {
            s_time[i] *= s_window[i];
        }

        uint32_t out_n = (available > SPEC_SUB_HOP_SIZE) ?
                         SPEC_SUB_HOP_SIZE : available;

        for (uint32_t i = 0U; i < out_n; i++)
        {
            float32_t y = s_time[i] + s_overlap[i];
            y = clampf_local(y, -1.0f, 0.9995f);

            int32_t q = (int32_t)(y * 2048.0f) + 2048;
            if (q < 0) q = 0;
            if (q > 4095) q = 4095;

            samples[start + i] = (uint16_t)q;
        }

        for (uint32_t i = 0U; i < SPEC_SUB_HOP_SIZE; i++)
        {
            s_overlap[i] = s_time[i + SPEC_SUB_HOP_SIZE];
        }
    }
}

