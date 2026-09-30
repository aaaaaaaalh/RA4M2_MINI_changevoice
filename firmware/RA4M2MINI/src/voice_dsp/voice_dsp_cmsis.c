/*
 * voice_dsp_cmsis.c
 *
 *  Created on: 2026年9月13日
 *      Author: 87245
 */

#include "voice_dsp_cmsis.h"

#include <string.h>

#define PITCH_BUF_SIZE       (1024U)
#define PITCH_BUF_MASK       (PITCH_BUF_SIZE - 1U)
#define PITCH_DMIN           (64.0f)
#define PITCH_DSPAN          (320.0f)
#define VOICE_TWO_PI         (6.2831853071795864769f)

static float32_t s_in[VOICE_DSP_BLOCK_MAX];
static float32_t s_work[VOICE_DSP_BLOCK_MAX];
static float32_t s_carrier[VOICE_DSP_BLOCK_MAX];

/*
 * DC blocker:
 * y[n] = x[n] - x[n-1] + 0.995*y[n-1]
 * CMSIS DF2T coefficient order: {b0, b1, b2, a1, a2}
 */
static float32_t s_dc_coeffs[5] =
{
     1.0f,
    -1.0f,
     0.0f,
     0.995f,
     0.0f
};

static float32_t s_dc_state[2];
static arm_biquad_cascade_df2T_instance_f32 s_dc_filter;

static voice_dsp_mode_t s_mode = VOICE_DSP_MODE_FEMALE;

static float32_t s_robot_freq = VOICE_DSP_ROBOT_FREQ_DEFAULT;
static float32_t s_robot_phase = 0.0f;

static float32_t s_female_ratio = VOICE_DSP_FEMALE_DEFAULT;
static float32_t s_male_ratio = VOICE_DSP_MALE_DEFAULT;
static float32_t s_output_gain = VOICE_DSP_GAIN_DEFAULT;

static float32_t s_pitch_buf[PITCH_BUF_SIZE];
static uint16_t  s_pitch_wr = 0U;
static float32_t s_pitch_phase = 0.25f;

static float32_t clamp_f32(float32_t x, float32_t lo, float32_t hi)
{
    if (x < lo)
    {
        return lo;
    }

    if (x > hi)
    {
        return hi;
    }

    return x;
}

static float32_t wrap01(float32_t x)
{
    while (x >= 1.0f)
    {
        x -= 1.0f;
    }

    while (x < 0.0f)
    {
        x += 1.0f;
    }

    return x;
}

static float32_t tri_window(float32_t phase)
{
    if (phase < 0.5f)
    {
        return 2.0f * phase;
    }

    return 2.0f * (1.0f - phase);
}

static float32_t pitch_read_linear(float32_t delay)
{
    float32_t pos = (float32_t)s_pitch_wr - delay;

    while (pos < 0.0f)
    {
        pos += (float32_t)PITCH_BUF_SIZE;
    }

    while (pos >= (float32_t)PITCH_BUF_SIZE)
    {
        pos -= (float32_t)PITCH_BUF_SIZE;
    }

    int32_t i0 = (int32_t)pos;
    int32_t i1 = (i0 + 1) & (int32_t)PITCH_BUF_MASK;
    float32_t frac = pos - (float32_t)i0;

    return ((1.0f - frac) * s_pitch_buf[i0]) +
           (frac * s_pitch_buf[i1]);
}

static float32_t pitch_process_sample(float32_t x, float32_t ratio)
{
    s_pitch_buf[s_pitch_wr] = x;

    float32_t phase_a = s_pitch_phase;
    float32_t phase_b = wrap01(s_pitch_phase + 0.5f);

    float32_t delay_a = PITCH_DMIN + phase_a * PITCH_DSPAN;
    float32_t delay_b = PITCH_DMIN + phase_b * PITCH_DSPAN;

    float32_t ya = pitch_read_linear(delay_a);
    float32_t yb = pitch_read_linear(delay_b);

    float32_t wa = tri_window(phase_a);
    float32_t wb = tri_window(phase_b);

    float32_t denom = wa + wb;
    if (denom < 0.000001f)
    {
        denom = 0.000001f;
    }

    float32_t y = ((ya * wa) + (yb * wb)) / denom;

    s_pitch_phase =
        wrap01(s_pitch_phase + ((1.0f - ratio) / PITCH_DSPAN));

    s_pitch_wr = (uint16_t)((s_pitch_wr + 1U) & PITCH_BUF_MASK);

    return y;
}

static void robot_make_carrier(float32_t * p_carrier, uint32_t n)
{
    float32_t phase_inc = s_robot_freq / (float32_t)VOICE_DSP_FS_HZ;

    for (uint32_t i = 0U; i < n; i++)
    {
        p_carrier[i] = arm_sin_f32(VOICE_TWO_PI * s_robot_phase);

        s_robot_phase += phase_inc;
        if (s_robot_phase >= 1.0f)
        {
            s_robot_phase -= 1.0f;
        }
    }
}

static uint16_t float_to_adc12(float32_t x)
{
    x = clamp_f32(x, -1.0f, 0.99951171875f);

    int32_t v = (int32_t)(x * 2048.0f) + (int32_t)VOICE_DSP_ADC_MID;

    if (v < 0)
    {
        v = 0;
    }
    else if (v > (int32_t)VOICE_DSP_ADC_MAX)
    {
        v = (int32_t)VOICE_DSP_ADC_MAX;
    }

    return (uint16_t)v;
}

static void process_chunk(const uint16_t * p_in,
                          uint16_t       * p_out,
                          uint32_t         n)
{
    for (uint32_t i = 0U; i < n; i++)
    {
        uint16_t raw = p_in[i];
        if (raw > VOICE_DSP_ADC_MAX)
        {
            raw = VOICE_DSP_ADC_MAX;
        }

        s_in[i] =
            ((float32_t)((int32_t)raw - (int32_t)VOICE_DSP_ADC_MID)) /
            2048.0f;
    }

    /* CMSIS-DSP IIR filtering */
    arm_biquad_cascade_df2T_f32(&s_dc_filter,
                                s_in,
                                s_in,
                                n);

    switch (s_mode)
    {
        case VOICE_DSP_MODE_ROBOT:
        {
            robot_make_carrier(s_carrier, n);

            /* CMSIS-DSP vector multiplication */
            arm_mult_f32(s_in, s_carrier, s_work, n);
            break;
        }

        case VOICE_DSP_MODE_FEMALE:
        {
            for (uint32_t i = 0U; i < n; i++)
            {
                s_work[i] = pitch_process_sample(s_in[i], s_female_ratio);
            }
            break;
        }

        case VOICE_DSP_MODE_MALE:
        {
            for (uint32_t i = 0U; i < n; i++)
            {
                s_work[i] = pitch_process_sample(s_in[i], s_male_ratio);
            }
            break;
        }

        case VOICE_DSP_MODE_NORMAL:
        default:
        {
            /* CMSIS-DSP vector copy */
            arm_copy_f32(s_in, s_work, n);
            break;
        }
    }

    /* CMSIS-DSP vector gain */
    arm_scale_f32(s_work,
                  s_output_gain,
                  s_work,
                  n);

    for (uint32_t i = 0U; i < n; i++)
    {
        p_out[i] = float_to_adc12(s_work[i]);
    }
}

void VoiceDSP_Init(void)
{
    s_mode = VOICE_DSP_MODE_NORMAL;

    s_robot_freq = VOICE_DSP_ROBOT_FREQ_DEFAULT;
    s_female_ratio = VOICE_DSP_FEMALE_DEFAULT;
    s_male_ratio = VOICE_DSP_MALE_DEFAULT;
    s_output_gain = VOICE_DSP_GAIN_DEFAULT;

    arm_biquad_cascade_df2T_init_f32(&s_dc_filter,
                                     1U,
                                     s_dc_coeffs,
                                     s_dc_state);

    VoiceDSP_Reset();
}

void VoiceDSP_Reset(void)
{
    memset(s_dc_state, 0, sizeof(s_dc_state));

    s_robot_phase = 0.0f;

    memset(s_pitch_buf, 0, sizeof(s_pitch_buf));
    s_pitch_wr = 0U;
    s_pitch_phase = 0.25f;
}

void VoiceDSP_SetMode(voice_dsp_mode_t mode)
{
    if (mode > VOICE_DSP_MODE_MALE)
    {
        mode = VOICE_DSP_MODE_NORMAL;
    }

    if (mode != s_mode)
    {
        s_mode = mode;
        VoiceDSP_Reset();
    }
}

voice_dsp_mode_t VoiceDSP_GetMode(void)
{
    return s_mode;
}

void VoiceDSP_SetRobotFrequency(float32_t freq_hz)
{
    if (freq_hz < 1.0f)
    {
        freq_hz = 1.0f;
    }

    if (freq_hz > 1000.0f)
    {
        freq_hz = 1000.0f;
    }

    s_robot_freq = freq_hz;
    s_robot_phase = 0.0f;
}

void VoiceDSP_SetFemaleRatio(float32_t ratio)
{
    s_female_ratio = clamp_f32(ratio, 1.0f, 1.6f);
}

void VoiceDSP_SetMaleRatio(float32_t ratio)
{
    s_male_ratio = clamp_f32(ratio, 0.6f, 1.0f);
}

void VoiceDSP_SetOutputGain(float32_t gain)
{
    s_output_gain = clamp_f32(gain, 0.0f, 1.5f);
}

void VoiceDSP_ProcessBlockADC12(const uint16_t * p_in,
                                uint16_t       * p_out,
                                uint32_t         block_size)
{
    if ((p_in == 0) || (p_out == 0) || (block_size == 0U))
    {
        return;
    }

    while (block_size > 0U)
    {
        uint32_t n = block_size;

        if (n > VOICE_DSP_BLOCK_MAX)
        {
            n = VOICE_DSP_BLOCK_MAX;
        }

        process_chunk(p_in, p_out, n);

        p_in += n;
        p_out += n;
        block_size -= n;
    }
}

uint16_t VoiceDSP_ProcessSampleADC12(uint16_t adc_sample)
{
    uint16_t out = VOICE_DSP_ADC_MID;

    VoiceDSP_ProcessBlockADC12(&adc_sample, &out, 1U);

    return out;
}

