#include "hal_data.h"
#include "audio_capture.h"
#include "adc/bsp_adc.h"
#include "debug_uart/bsp_debug_uart.h"
#include "gpt_timer/bsp_gpt_timer.h"

/* Kept for the supplied OLED module's extern reference; OLED is idle in v0.2. */
fsp_err_t err = FSP_SUCCESS;
void hal_entry(void)
{
    Capture_Init();
    Debug_UART9_Init();
    ADC_Init();
    Capture_Check(GPT_Init());
    while (1)
    {
        AudioUART_Task();
        /* No printf, OLED refresh, DAC or DSP in the acquisition path. */
    }
}
#if BSP_TZ_SECURE_BUILD
FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void);
void template_nonsecure_callable(void) { }
FSP_CPP_FOOTER
#endif
