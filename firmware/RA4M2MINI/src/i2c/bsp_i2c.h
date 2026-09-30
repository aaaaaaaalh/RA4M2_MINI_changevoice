/*
 * bsp_i2c.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef I2C_BSP_I2C_H_
#define I2C_BSP_I2C_H_
#include "hal_data.h"
#include "stdlib.h"

#define OLED_SCLK_Clr() R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_08,BSP_IO_LEVEL_LOW)//SDA IIC接口的数据信号//SCL IIC接口的时钟信号
#define OLED_SCLK_Set() R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_08,BSP_IO_LEVEL_HIGH)

#define OLED_SDIN_Clr() R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_07,BSP_IO_LEVEL_LOW)//SDA IIC接口的数据信号
#define OLED_SDIN_Set() R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_04_PIN_07,BSP_IO_LEVEL_HIGH)

void IIC_Start();
void IIC_Stop();
void Write_IIC_Command(unsigned char IIC_Command);
void Write_IIC_Data(unsigned char IIC_Data);
void Write_IIC_Byte(unsigned char IIC_Byte);
void IIC_Wait_Ack();


#endif /* I2C_BSP_I2C_H_ */
