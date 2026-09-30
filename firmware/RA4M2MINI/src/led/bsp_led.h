/*
 * bsp_led.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef LED_BSP_LED_H_
#define LED_BSP_LED_H_

#include "hal_data.h"
/* LED 引脚置低电平 LED 灯亮 */
#define LED1_ON R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_11,BSP_IO_LEVEL_LOW)
/* LED 引脚置高电平 LED 灯灭 */
#define LED1_OFF R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_11,BSP_IO_LEVEL_HIGH)

/* 使用寄存器来实现 LED 灯翻转 */
#define LED1_TOGGLE R_PORT1->PODR ^= 1<<(BSP_IO_PORT_01_PIN_11 & 0xFF)
/* LED 初始化函数 */
void LED_Init(void);


#endif /* LED_BSP_LED_H_ */

