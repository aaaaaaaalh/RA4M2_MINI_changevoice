/*
 * pitch_gender.h
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#ifndef PITCH_GENDER_PITCH_GENDER_H_
#define PITCH_GENDER_PITCH_GENDER_H_
#include "hal_data.h"
#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PITCH_GENDER_FS_HZ        (8000U)
#define PITCH_GENDER_FRAME_SIZE   (512U)
#define PITCH_GENDER_HOP_SIZE     (256U)

typedef enum
{
    PITCH_GENDER_UNKNOWN = 0,
    PITCH_GENDER_MALE,
    PITCH_GENDER_FEMALE
} pitch_gender_t;

/**
 * @brief Estimate average F0 from a recorded 12-bit ADC buffer and classify
 *        pitch tendency as male/female.
 *
 * This is an educational heuristic based mainly on fundamental frequency.
 * It is not a biological-sex classifier and is not reliable for every voice.
 *
 * @param samples ADC12 samples (0..4095, silence around 2048).
 * @param count   Number of samples.
 * @param p_f0_hz Receives average accepted F0, or 0 if unavailable.
 */
pitch_gender_t PitchGender_AnalyzeADC12(const uint16_t * samples,
                                        uint32_t         count,
                                        float32_t      * p_f0_hz);

#ifdef __cplusplus
}
#endif

#endif /* PITCH_GENDER_PITCH_GENDER_H_ */
