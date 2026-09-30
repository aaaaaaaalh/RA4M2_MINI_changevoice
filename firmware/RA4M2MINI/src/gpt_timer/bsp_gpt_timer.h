/*
 * bsp_gpt_timer.h
 *
 *  Created on: 2026年9月13日
 *      Author: 87245
 */

#ifndef GPT_TIMER_BSP_GPT_TIMER_H_
#define GPT_TIMER_BSP_GPT_TIMER_H_



#include "hal_data.h"

#define AUDIO_SAMPLE_RATE      8000U
#define AUDIO_RECORD_SECONDS   5U
#define AUDIO_SAMPLE_COUNT     40000U
static uint16_t g_audio_buffer[AUDIO_SAMPLE_COUNT];

static volatile uint32_t g_record_index = 0;
static volatile uint32_t g_play_index   = 0;


/* GPT累计中断次数 */
extern volatile uint32_t g_gpt_tick;

/* GPT初始化并启动 */
fsp_err_t GPT_Init(void);

/* 启动GPT */
fsp_err_t GPT_Start(void);

/* 停止GPT */
fsp_err_t GPT_Stop(void);

/* 清零软件计数值 */
void GPT_TickClear(void);

/* 获取软件计数值 */
uint32_t GPT_TickGet(void);

/* GPT中断回调函数 */
//void gpt_callback(timer_callback_args_t * p_args);

#endif /* GPT_TIMER_BSP_GPT_TIMER_H_ */
