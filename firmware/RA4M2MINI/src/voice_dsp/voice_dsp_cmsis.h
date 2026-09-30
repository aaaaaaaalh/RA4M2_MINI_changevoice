/*
 * voice_dsp_cmsis.h
 *
 *  Created on: 2026年9月13日
 *      Author: 87245
 */

#ifndef VOICE_DSP_VOICE_DSP_CMSIS_H_
#define VOICE_DSP_VOICE_DSP_CMSIS_H_


#include "hal_data.h"
#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOICE_DSP_FS_HZ              (8000U)
#define VOICE_DSP_ADC_MID            (2048U)
#define VOICE_DSP_ADC_MAX            (4095U)
#define VOICE_DSP_BLOCK_MAX          (32U)

#define VOICE_DSP_ROBOT_FREQ_DEFAULT (180.0f)
#define VOICE_DSP_FEMALE_DEFAULT     (1.28f)
#define VOICE_DSP_MALE_DEFAULT       (0.82f)
#define VOICE_DSP_GAIN_DEFAULT       (0.90f)

typedef enum
{
    VOICE_DSP_MODE_NORMAL = 0,
    VOICE_DSP_MODE_ROBOT,
    VOICE_DSP_MODE_FEMALE,
    VOICE_DSP_MODE_MALE
} voice_dsp_mode_t;

void VoiceDSP_Init(void);
void VoiceDSP_Reset(void);

void VoiceDSP_SetMode(voice_dsp_mode_t mode);
voice_dsp_mode_t VoiceDSP_GetMode(void);

void VoiceDSP_SetRobotFrequency(float32_t freq_hz);
void VoiceDSP_SetFemaleRatio(float32_t ratio);
void VoiceDSP_SetMaleRatio(float32_t ratio);
void VoiceDSP_SetOutputGain(float32_t gain);

void VoiceDSP_ProcessBlockADC12(const uint16_t * p_in,
                                uint16_t       * p_out,
                                uint32_t         block_size);

uint16_t VoiceDSP_ProcessSampleADC12(uint16_t adc_sample);

#ifdef __cplusplus
}
#endif


#endif /* VOICE_DSP_VOICE_DSP_CMSIS_H_ */
