/*
 * spectral_subtract.h
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#ifndef SPECTRAL_SUBTRACT_SPECTRAL_SUBTRACT_H_
#define SPECTRAL_SUBTRACT_SPECTRAL_SUBTRACT_H_

#include "hal_data.h"
#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPEC_SUB_FS_HZ       (8000U)
#define SPEC_SUB_FFT_SIZE    (256U)
#define SPEC_SUB_HOP_SIZE    (128U)
#define SPEC_SUB_BINS        (SPEC_SUB_FFT_SIZE / 2U + 1U)

void SpectralSubtract_Init(void);
void SpectralSubtract_ResetNoise(void);

/**
 * @brief Learn one noise-only frame. Input must be 256 normalized float samples.
 */
void SpectralSubtract_LearnNoise(const float32_t * frame);

/**
 * @brief Freeze / finalize the average noise spectrum.
 */
void SpectralSubtract_FinalizeNoise(void);

bool SpectralSubtract_HasNoiseProfile(void);

/**
 * @brief In-place spectral subtraction on recorded ADC12 samples.
 *
 * Uses 256-point RFFT, 50% overlap and sqrt-Hann analysis/synthesis windows.
 */
void SpectralSubtract_ProcessADC12InPlace(uint16_t * samples,
                                          uint32_t   count);

#ifdef __cplusplus
}
#endif
#endif /* SPECTRAL_SUBTRACT_SPECTRAL_SUBTRACT_H_ */
