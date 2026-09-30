/*
 * bsp_gpt_timer.c
 *
 *  Created on: 2026年9月13日
 *      Author: 87245
 */

#include "bsp_gpt_timer.h"
#include <adc/bsp_adc.h>
#include <dac/bsp_dac.h>
#include "led/bsp_led.h"

/* GPT周期中断计数 */
volatile uint32_t g_gpt_tick = 0;
volatile bool adc_sign = false;

/**
 * @brief GPT初始化
 */
fsp_err_t GPT_Init(void)
{
    fsp_err_t err;

    /* 打开GPT */
    err = R_GPT_Open(&g_timer0_ctrl, &g_timer0_cfg);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* 启动GPT */
    err = R_GPT_Start(&g_timer0_ctrl);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return FSP_SUCCESS;
}


/**
 * @brief 启动GPT
 */
fsp_err_t GPT_Start(void)
{
    return R_GPT_Start(&g_timer0_ctrl);
}


/**
 * @brief 停止GPT
 */
fsp_err_t GPT_Stop(void)
{
    return R_GPT_Stop(&g_timer0_ctrl);
}


/**
 * @brief 清零GPT软件计数值
 */
void GPT_TickClear(void)
{
    g_gpt_tick = 0;
}


/**
 * @brief 获取GPT软件计数值
 */
uint32_t GPT_TickGet(void)
{
    return g_gpt_tick;
}


/**
 * @brief GPT周期中断回调函数
 */
//void gpt_callback(timer_callback_args_t * p_args)
//{
//    if (TIMER_EVENT_CYCLE_END == p_args->event)
//    {

//    }
//}

