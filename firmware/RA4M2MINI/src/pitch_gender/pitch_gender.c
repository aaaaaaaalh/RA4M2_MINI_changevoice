/*
 * pitch_gender.c
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#include "pitch_gender.h"


#include <math.h>
#include <string.h>

#define F0_MIN_HZ           (70.0f)
#define F0_MAX_HZ           (350.0f)
#define F0_MIN_RMS          (0.015f)
#define F0_MIN_CORRELATION  (0.35f)

/* Educational thresholds only; voices overlap strongly in this region. */
#define F0_MALE_MAX_HZ      (165.0f)
#define F0_FEMALE_MIN_HZ    (180.0f)

static float32_t s_frame[PITCH_GENDER_FRAME_SIZE];

static float32_t estimate_f0(const uint16_t * p)
{
    for (uint32_t i = 0U; i < PITCH_GENDER_FRAME_SIZE; i++)
    {
        s_frame[i] =
            ((float32_t)((int32_t)p[i] - 2048)) / 2048.0f;
    }

    float32_t mean = 0.0f;
    arm_mean_f32(s_frame, PITCH_GENDER_FRAME_SIZE, &mean);
    arm_offset_f32(s_frame, -mean, s_frame, PITCH_GENDER_FRAME_SIZE);

    float32_t rms = 0.0f;
    arm_rms_f32(s_frame, PITCH_GENDER_FRAME_SIZE, &rms);

    if (rms < F0_MIN_RMS)
    {
        return 0.0f;
    }

    uint32_t min_lag = (uint32_t)((float32_t)PITCH_GENDER_FS_HZ / F0_MAX_HZ);
    uint32_t max_lag = (uint32_t)((float32_t)PITCH_GENDER_FS_HZ / F0_MIN_HZ);

    float32_t best_corr = 0.0f;
    uint32_t best_lag = 0U;

    for (uint32_t lag = min_lag; lag <= max_lag; lag++)
    {
        uint32_t n = PITCH_GENDER_FRAME_SIZE - lag;

        float32_t dot = 0.0f;
        float32_t e0  = 0.0f;
        float32_t e1  = 0.0f;

        arm_dot_prod_f32(&s_frame[0], &s_frame[lag], n, &dot);
        arm_power_f32(&s_frame[0], n, &e0);
        arm_power_f32(&s_frame[lag], n, &e1);

        float32_t denom = sqrtf(e0 * e1) + 1.0e-12f;
        float32_t corr = dot / denom;

        if (corr > best_corr)
        {
            best_corr = corr;
            best_lag = lag;
        }
    }

    if ((best_lag == 0U) || (best_corr < F0_MIN_CORRELATION))
    {
        return 0.0f;
    }

    return (float32_t)PITCH_GENDER_FS_HZ / (float32_t)best_lag;
}

pitch_gender_t PitchGender_AnalyzeADC12(const uint16_t * samples,
                                        uint32_t         count,
                                        float32_t      * p_f0_hz)
{
    if (p_f0_hz != NULL)
    {
        *p_f0_hz = 0.0f;
    }

    if ((samples == NULL) || (count < PITCH_GENDER_FRAME_SIZE))
    {
        return PITCH_GENDER_UNKNOWN;
    }

    float32_t f0_sum = 0.0f;
    uint32_t accepted = 0U;

    for (uint32_t start = 0U;
         (start + PITCH_GENDER_FRAME_SIZE) <= count;
         start += PITCH_GENDER_HOP_SIZE)
    {
        float32_t f0 = estimate_f0(&samples[start]);

        if ((f0 >= F0_MIN_HZ) && (f0 <= F0_MAX_HZ))
        {
            f0_sum += f0;
            accepted++;
        }
    }

    if (accepted < 3U)
    {
        return PITCH_GENDER_UNKNOWN;
    }

    float32_t avg_f0 = f0_sum / (float32_t)accepted;

    if (p_f0_hz != NULL)
    {
        *p_f0_hz = avg_f0;
    }

    if (avg_f0 <= F0_MALE_MAX_HZ)
    {
        return PITCH_GENDER_MALE;
    }

    if (avg_f0 >= F0_FEMALE_MIN_HZ)
    {
        return PITCH_GENDER_FEMALE;
    }

    return PITCH_GENDER_UNKNOWN;
}

