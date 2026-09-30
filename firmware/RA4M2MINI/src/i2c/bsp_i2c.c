/*
 * bsp_i2c.c
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#include "hal_data.h"
#include "debug_uart/bsp_debug_uart.h"
#include "bsp_i2c.h"

void IIC_Start()
{
    OLED_SCLK_Set() ;
    OLED_SDIN_Set();
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    OLED_SDIN_Clr();
    OLED_SCLK_Clr();
}

/**********************************************
//IIC Stop
**********************************************/
void IIC_Stop()
{
    OLED_SCLK_Set() ;
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    OLED_SDIN_Clr();
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    OLED_SDIN_Set();
}

void IIC_Wait_Ack()
{
    OLED_SCLK_Set() ;
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    OLED_SCLK_Clr();
}
/**********************************************
// IIC Write byte
**********************************************/

void Write_IIC_Byte(unsigned char IIC_Byte)
{
    unsigned char i;
    unsigned char m,da;
    da=IIC_Byte;
    OLED_SCLK_Clr();
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    for(i=0;i<8;i++)
    {
            m=da;
        m=m&0x80;
        if(m==0x80)
        {
      OLED_SDIN_Set();
      R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    }
        else
        {
            OLED_SDIN_Clr();
            R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
        }
        da=da<<1;
        OLED_SCLK_Set();
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
        OLED_SCLK_Clr();
    }


}
/**********************************************
// IIC Write Command
**********************************************/
void Write_IIC_Command(unsigned char IIC_Command)
{
   IIC_Start();
   Write_IIC_Byte(0x78);            //Slave address,SA0=0
    IIC_Wait_Ack();
   Write_IIC_Byte(0x00);            //write command
    IIC_Wait_Ack();
   Write_IIC_Byte(IIC_Command);
    IIC_Wait_Ack();
   IIC_Stop();
}
/**********************************************
// IIC Write Data
**********************************************/
void Write_IIC_Data(unsigned char IIC_Data)
{
   IIC_Start();
   Write_IIC_Byte(0x78);            //D/C#=0; R/W#=0
    IIC_Wait_Ack();
   Write_IIC_Byte(0x40);            //write data
    IIC_Wait_Ack();
   Write_IIC_Byte(IIC_Data);
    IIC_Wait_Ack();
   IIC_Stop();
}

/* Callback function */
i2c_master_event_t i2c_event = I2C_MASTER_EVENT_ABORTED;
void i2c_master_callback(i2c_master_callback_args_t *p_args)
{
    i2c_event = I2C_MASTER_EVENT_ABORTED;
    if (NULL != p_args)
    {
        /* capture callback event for validating the i2c transfer event*/
        i2c_event = p_args->event;
    }
}
