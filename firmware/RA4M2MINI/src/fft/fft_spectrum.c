/*
 * fft_spectrum.c
 *
 *  Created on: 2026年9月14日
 *      Author: Administrator
 */


#include "fft_spectrum.h"

#include <math.h>
#define ADC12_MID_VALUE     2048.0f
#define ADC12_SCALE         2048.0f

static float32_t g_adc_float_buffer[FFT_SPECTRUM_SIZE];
/* RFFT实例 */
static arm_rfft_fast_instance_f32 g_rfft;

/* Hann窗 */
static float32_t g_hann[FFT_SIZE];

/* FFT工作缓冲区 */
static float32_t g_time_buffer[FFT_SIZE];
static float32_t g_fft_buffer[FFT_SIZE];


/**
 * @brief 生成Hann窗
 */
static void FFT_CreateHannWindow(void)
{
    const float32_t two_pi = 6.28318530717958647692f;

    for (uint32_t i = 0; i < FFT_SIZE; i++)
    {
        float32_t phase =
                two_pi *
                (float32_t)i /
                (float32_t)(FFT_SIZE - 1U);

        g_hann[i] =
                0.5f -
                0.5f * arm_cos_f32(phase);
    }
}


/**
 * @brief FFT初始化
 */
bool FFT_Spectrum_Init(void)
{
    arm_status status;

    status = arm_rfft_fast_init_f32(
            &g_rfft,
            FFT_SIZE);

    if (ARM_MATH_SUCCESS != status)
    {
        return false;
    }

    FFT_CreateHannWindow();

    return true;
}


/**
 * @brief FFT频谱分析
 */
/* FFT功能函数 */
void FFT_Spectrum_Analyze(const float32_t *input,
                          float32_t *magnitude)
{
    float32_t mean;

    /* --------------------------------
     * 1. 复制输入数据
     * -------------------------------- */
    arm_copy_f32(input,
                 g_time_buffer,
                 FFT_SIZE);


    /* --------------------------------
     * 2. 去除直流分量
     *
     * mean = 平均值
     * x[n] = x[n] - mean
     * -------------------------------- */
    arm_mean_f32(g_time_buffer,
                 FFT_SIZE,
                 &mean);

    arm_offset_f32(g_time_buffer,
                   -mean,
                   g_time_buffer,
                   FFT_SIZE);


    /* --------------------------------
     * 3. Hann窗
     *
     * x[n] = x[n] * w[n]
     * -------------------------------- */
    arm_mult_f32(g_time_buffer,
                 g_hann,
                 g_time_buffer,
                 FFT_SIZE);


    /* --------------------------------
     * 4. 实数FFT
     * -------------------------------- */
    arm_rfft_fast_f32(&g_rfft,
                      g_time_buffer,
                      g_fft_buffer,
                      0);


    /*
     * arm_rfft_fast_f32输出格式比较特殊：
     *
     * g_fft_buffer[0] = DC
     * g_fft_buffer[1] = Nyquist
     *
     * bin 1:
     * g_fft_buffer[2] = real
     * g_fft_buffer[3] = imag
     *
     * bin 2:
     * g_fft_buffer[4] = real
     * g_fft_buffer[5] = imag
     *
     * ...
     */


    /* --------------------------------
     * 5. DC
     * -------------------------------- */
    magnitude[0] =
            fabsf(g_fft_buffer[0]);


    /* --------------------------------
     * 6. bin 1 ~ 127
     * -------------------------------- */
    arm_cmplx_mag_f32(
            &g_fft_buffer[2],
            &magnitude[1],
            (FFT_SIZE / 2U) - 1U);

    /* --------------------------------
     * 7. Nyquist
     *
     * bin 128 = 4000 Hz
     * -------------------------------- */
    magnitude[FFT_SIZE / 2U] =
            fabsf(g_fft_buffer[1]);

    /*
     * --------------------------------
     * 8. 幅值归一化
     *
     * Hann窗的相干增益约为0.5。
     *
     * 普通正频率：
     *
     * amplitude = magnitude * 4 / N
     *
     * DC / Nyquist：
     *
     * amplitude = magnitude * 2 / N
     * --------------------------------
     */

    magnitude[0] *=
            2.0f / (float32_t)FFT_SIZE;

    for (uint32_t i = 1;
         i < FFT_SIZE / 2U;
         i++)
    {
        magnitude[i] *=
                4.0f / (float32_t)FFT_SIZE;
    }

    magnitude[FFT_SIZE / 2U] *=
            2.0f / (float32_t)FFT_SIZE;
}


/**
 * @brief 获取FFT最大频率
 */
float32_t FFT_Spectrum_GetPeakFrequency(
        const float32_t *magnitude)
{
    float32_t max_value;
    uint32_t max_index;

    /*
     * 跳过bin 0，即DC。
     *
     * magnitude[1] ~ magnitude[128]
     */
    arm_max_f32(
            &magnitude[1],
            FFT_SIZE / 2U,
            &max_value,
            &max_index);

    /*
     * 因为搜索从 magnitude[1] 开始，
     * 所以实际bin需要+1。
     */
    max_index += 1U;

    return
        (float32_t)max_index *
        FFT_BIN_HZ;
}

void FFT_Spectrum_AnalyzeADC12(const uint16_t *adc_input,
                               float32_t *magnitude)
{
    if ((adc_input == NULL) || (magnitude == NULL))
    {
        return;
    }

    /*
     * ADC 12bit:
     *
     *     0    -> -1.0
     *     2048 ->  0.0
     *     4095 -> 接近 +1.0
     *
     * 麦克风信号通常偏置在1.65V，
     * 所以ADC静态中心约为2048。
     */
    for (uint32_t i = 0;
         i < FFT_SPECTRUM_SIZE;
         i++)
    {
        g_adc_float_buffer[i] =
            ((float32_t)adc_input[i]
             - ADC12_MID_VALUE)
            / ADC12_SCALE;
    }

    /*
     * 调用原来的FFT分析函数
     *
     * FFT_Spectrum_Analyze()内部继续负责：
     *
     * 去均值
     * Hann窗
     * RFFT
     * 幅值计算
     */
    FFT_Spectrum_Analyze(g_adc_float_buffer,
                         magnitude);
}
