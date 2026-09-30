/*
 * bsp_key.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef KEY_BSP_KEY_H_
#define KEY_BSP_KEY_H_
#include "hal_data.h"
/* 定义宏 KEY_ON 表示按键按下
定义宏 KEY_OFF 表示按键没有按下
*/

#define    KEY_ON             0
#define    KEY_OFF            1

/* 按键引脚宏定义 详见野火官方开发板原理图 */
#define    KEY1_PORT_PIN      BSP_IO_PORT_00_PIN_00

uint8_t KEY_Scan(bsp_io_port_pin_t pin);
void KEY_Init(void);

#endif /* KEY_BSP_KEY_H_ */
