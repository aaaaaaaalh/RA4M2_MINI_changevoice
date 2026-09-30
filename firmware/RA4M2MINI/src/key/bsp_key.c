/*
 * bsp_key.c
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#include "bsp_key.h"

/* 获取GPIO端口输入高/低电平 */
bsp_io_level_t key_io_level;

void KEY_Init(void)
{
    /* 该功能已在R_BSP_WarmStart()中实现 */
}

uint8_t KEY_Scan(bsp_io_port_pin_t pin)
{
    /* 获取GPIO引脚输入状态 */
    R_IOPORT_PinRead (&g_ioport_ctrl, pin, &key_io_level);
    /* 判断是否输入低电平 */
    if (key_io_level == BSP_IO_LEVEL_LOW)
    {
        /* 循环获取GPIO引脚输入状态 直到按键松开 */
        do
        {
            R_IOPORT_PinRead (&g_ioport_ctrl, pin, &key_io_level);
        }
        while (key_io_level == BSP_IO_LEVEL_LOW);
        return KEY_ON;
    }
    else
        return KEY_OFF;
}
