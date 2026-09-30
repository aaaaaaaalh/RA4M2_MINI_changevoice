#include "hal_data.h"
#include "audio_capture.h"
#include "debug_uart/bsp_debug_uart.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int g_adc0_ctrl,g_adc0_cfg,g_adc0_channel_cfg,g_elc_ctrl,g_elc_cfg,g_uart9_ctrl,g_uart9_cfg;
int R_ADC_Open(void*a,const void*b){(void)a;(void)b;return 0;}
int R_ADC_ScanCfg(void*a,const void*b){(void)a;(void)b;return 0;}
int R_ELC_Open(void*a,const void*b){(void)a;(void)b;return 0;}
int R_ELC_Enable(void*a){(void)a;return 0;}
int R_ADC_ScanStart(void*a){(void)a;return 0;}
int R_ADC_Read(void*a,int b,uint16_t*c){(void)a;(void)b;*c=2048;return 0;}
int R_SCI_UART_Open(void*a,const void*b){(void)a;(void)b;return 0;}
static uint8_t sent[518]; static const uint8_t *active; static int writes, fail;
int R_SCI_UART_Write(void*a,const uint8_t*b,uint32_t n){
 (void)a; assert(n==518); if(fail)return 1;
 memcpy(sent,b,n);active=b;writes++;return 0;
}
void debug_uart9_callback(uart_callback_args_t*);
static void block(uint16_t value){for(int i=0;i<256;i++)Capture_Sample(value);}
int main(void){
 uint16_t out[256],seq;
 Capture_Init();
 for(int i=0;i<255;i++)Capture_Sample(123);
 assert(!Capture_Pop(out,&seq)); Capture_Sample(123);
 assert(Capture_Pop(out,&seq)&&seq==0&&out[255]==123);
 for(int i=0;i<6;i++)block((uint16_t)i);
 assert(g_capture_dropped_blocks==2);
 for(int i=0;i<4;i++){assert(Capture_Pop(out,&seq)&&seq==(uint16_t)(i+1)&&out[0]==i);}
 block(100);assert(Capture_Pop(out,&seq)&&seq==7&&out[0]==100);
 Capture_Init();for(uint32_t i=0;i<65538;i++){block(2048);assert(Capture_Pop(out,&seq)&&seq==(uint16_t)i);}
 Capture_Init();Debug_UART9_Init();
 Capture_Sample(0);Capture_Sample(4095);for(int i=2;i<256;i++)Capture_Sample(2048);
 AudioUART_Task();assert(writes==1);
 assert(sent[0]==170&&sent[1]==85&&sent[2]==0&&sent[3]==2);
 assert(sent[6]==0&&sent[7]==128&&sent[8]==240&&sent[9]==127&&sent[10]==0&&sent[11]==0);
 block(3000);AudioUART_Task();assert(writes==1&&memcmp(active,sent,518)==0);
 uart_callback_args_t done={UART_EVENT_TX_COMPLETE};debug_uart9_callback(&done);
 AudioUART_Task();assert(writes==2&&sent[4]==1);
 debug_uart9_callback(&done);block(2048);fail=1;AudioUART_Task();assert(g_capture_uart_errors==1);
 fail=0;block(2048);AudioUART_Task();assert(writes==3&&sent[4]==3);
 puts("PASS: partial blocks, whole-block overflow, sequence wrap, PCM encoding, asynchronous buffer ownership, UART failure gap");
 return 0;
}
