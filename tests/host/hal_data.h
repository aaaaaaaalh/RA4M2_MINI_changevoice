#ifndef HAL_DATA_H
#define HAL_DATA_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int fsp_err_t;
#define FSP_SUCCESS 0
#define ADC_CHANNEL_1 1
#define ADC_EVENT_SCAN_COMPLETE 1
#define UART_EVENT_TX_COMPLETE 2
#define __DMB() ((void)0)
#define __NOP() ((void)0)
#define __disable_irq() ((void)0)
typedef struct {int event;} adc_callback_args_t;
typedef struct {int event;} uart_callback_args_t;
extern int g_adc0_ctrl,g_adc0_cfg,g_adc0_channel_cfg,g_elc_ctrl,g_elc_cfg,g_uart9_ctrl,g_uart9_cfg;
int R_ADC_Open(void*,const void*);
int R_ADC_ScanCfg(void*,const void*);
int R_ELC_Open(void*,const void*);
int R_ELC_Enable(void*);
int R_ADC_ScanStart(void*);
int R_ADC_Read(void*,int,uint16_t*);
int R_SCI_UART_Open(void*,const void*);
int R_SCI_UART_Write(void*,const uint8_t*,uint32_t);
#endif
