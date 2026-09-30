/*
 * fft_spectrum.h
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */

#ifndef FFT_FFT_SPECTRUM_H_
#define FFT_FFT_SPECTRUM_H_


#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"
#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FFT_SAMPLE_RATE     16000U /* 采样频率8khz */
#define FFT_SIZE            256U /* 采样点数256 */
#define FFT_SPECTRUM_SIZE       256U
#define FFT_SPECTRUM_BINS       129U
/* 实数FFT有效频点：0 ~ Fs/2 */
#define FFT_BINS            (FFT_SIZE / 2U + 1U)

/* 频率分辨率：8000 / 256 = 31.25 Hz */
#define FFT_BIN_HZ          ((float32_t)FFT_SAMPLE_RATE / FFT_SIZE)
bool FFT_Spectrum_Init(void);

/* 原来的 float32 FFT 接口 */
void FFT_Spectrum_Analyze(const float32_t *input,
                          float32_t *magnitude);

/* 新增：直接输入ADC 12bit原始数据 */
void FFT_Spectrum_AnalyzeADC12(const uint16_t *adc_input,
                               float32_t *magnitude);

float32_t FFT_Spectrum_GetPeakFrequency(
        const float32_t *magnitude);

/**
 * @brief FFT频谱分析初始化
 *
 * 初始化CMSIS-DSP RFFT和Hann窗。
 *
 * @return true  初始化成功
 * @return false 初始化失败
 */
bool FFT_Spectrum_Init(void);

/**
 * @brief 对256点时域信号进行FFT频谱分析
 *
 * 处理过程：
 * 1. 复制输入数据
 * 2. 去除直流分量
 * 3. 加Hann窗
 * 4. 256点实数FFT
 * 5. 计算单边幅值谱
 *
 * @param input     输入时域数据，长度必须为FFT_SIZE
 * @param magnitude 输出幅值频谱，长度必须为FFT_BINS
 */
void FFT_Spectrum_Analyze(const float32_t *input,
                          float32_t *magnitude);

/**
 * @brief 获取幅值最大的频率
 *
 * 默认忽略直流分量0 Hz。
 *
 * @param magnitude FFT幅值谱
 *
 * @return 主峰频率，单位Hz
 */
float32_t FFT_Spectrum_GetPeakFrequency(const float32_t *magnitude);

#ifdef __cplusplus
}
#endif


#endif /* FFT_FFT_SPECTRUM_H_ */
