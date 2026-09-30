/*
 * bsp_key_irq.c
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */


#include "bsp_key_irq.h"

volatile bool key1_press = false;

/* 外部中断初始化函数 */
void KEY_IRQ_Init(void)
{
    /*  */
     fsp_err_t err;
     /* 打开外部中断 */
     err = g_external_irq6.p_api->open (g_external_irq6.p_ctrl, g_external_irq6.p_cfg);
     /* 断言 若条件为假则中断程序 */
     assert(err == FSP_SUCCESS);

     /* 使能外部中断 */
     err = g_external_irq6.p_api->enable (g_external_irq6.p_ctrl);
     /* 断言 若条件为假则中断程序 */
     assert(err == FSP_SUCCESS);
}

/* 外部中断回调函数 */
void key_external_irq_callback(external_irq_callback_args_t *p_args)
{
    /* 判断中断来源 */
    if (p_args->channel == 6)
    {   /* 标志位置1 */
        if (key1_press == false)
        {
            key1_press = true;
        }
        else if(key1_press == true)
        {
            key1_press = false;
        }
    }
    //R_ADC_ScanStart(&g_adc0_ctrl);

}


