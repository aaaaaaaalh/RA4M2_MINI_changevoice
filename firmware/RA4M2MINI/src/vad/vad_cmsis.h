/*
 * vad_cmsis.h
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#ifndef VAD_VAD_CMSIS_H_
#define VAD_VAD_CMSIS_H_

#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"
#include "fft/fft_spectrum.h"
#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VAD_CALIBRATION_FRAMES      (30U)
#define VAD_SPEECH_LOW_HZ           (300.0f)
#define VAD_SPEECH_HIGH_HZ          (3400.0f)
#define VAD_MIN_RMS                 (0.010f)
#define VAD_NOISE_RATIO             (2.0f)
#define VAD_MIN_BAND_RATIO          (0.50f)
#define VAD_MIN_SPECTRAL_CREST      (4.0f)
#define VAD_ATTACK_FRAMES           (2U)
#define VAD_HANGOVER_FRAMES         (6U)
#define VAD_NOISE_ALPHA             (0.97f)

typedef struct
{
    bool      is_voice;
    bool      raw_voice;
    bool      calibrated;

    float32_t rms;
    float32_t noise_rms;
    float32_t snr_db;
    float32_t band_ratio;
    float32_t spectral_crest;
    float32_t peak_frequency_hz;
} vad_result_t;

void VAD_Init(void);
void VAD_Reset(void);

bool VAD_ProcessFrame(const float32_t * time_frame,
                      const float32_t * magnitude,
                      vad_result_t     * result);

#ifdef __cplusplus
}
#endif


#endif /* VAD_VAD_CMSIS_H_ */
