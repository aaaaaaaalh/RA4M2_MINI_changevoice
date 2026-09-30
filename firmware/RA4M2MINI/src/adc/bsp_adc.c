#include "bsp_adc.h"
#include "audio_capture.h"

/* Single producer (ADC ISR), single consumer (foreground).
 * Publish only COMPLETE blocks. Never reuse a slot until consumer releases it.
 * Drop whole incoming blocks when full; sequence still advances. */
#define QUEUE_BLOCKS 4U
static uint16_t blocks[QUEUE_BLOCKS][CAPTURE_BLOCK];
static uint16_t sequences[QUEUE_BLOCKS];
static volatile uint32_t produced, consumed;
static uint16_t position, next_sequence;
static bool dropping;
volatile uint32_t g_capture_samples, g_capture_dropped_blocks, g_capture_adc_errors;
volatile uint16_t g_capture_last_adc;
volatile fsp_err_t g_capture_error = FSP_SUCCESS;

void Capture_Check(fsp_err_t code)
{
    if (FSP_SUCCESS != code)
    {
        g_capture_error = code;
        __disable_irq();
        while (1) { __NOP(); } /* Inspect g_capture_error with debugger. */
    }
}
void Capture_Init(void)
{
    produced = consumed = 0U; position = next_sequence = 0U;
    dropping = false; g_capture_samples = g_capture_dropped_blocks = g_capture_adc_errors = 0U;
}
void Capture_Sample(uint16_t value)
{
    g_capture_last_adc = value; g_capture_samples++;
    if (position == 0U)
    {
        dropping = ((uint32_t)(produced - consumed) >= QUEUE_BLOCKS);
        __DMB();
    }
    if (!dropping) { blocks[produced % QUEUE_BLOCKS][position] = value; }
    if (++position == CAPTURE_BLOCK)
    {
        if (dropping) { g_capture_dropped_blocks++; }
        else
        {
            sequences[produced % QUEUE_BLOCKS] = next_sequence;
            __DMB(); produced++;
        }
        next_sequence++; position = 0U;
    }
}
bool Capture_Pop(uint16_t *samples, uint16_t *sequence)
{
    uint32_t slot = consumed % QUEUE_BLOCKS;
    if (consumed == produced) { return false; }
    __DMB();
    for (uint32_t i = 0; i < CAPTURE_BLOCK; ++i) { samples[i] = blocks[slot][i]; }
    *sequence = sequences[slot];
    __DMB(); consumed++;
    return true;
}
void adc0_callback(adc_callback_args_t *args)
{
    if (args->event == ADC_EVENT_SCAN_COMPLETE)
    {
        uint16_t value;
        if (R_ADC_Read(&g_adc0_ctrl, ADC_CHANNEL_1, &value) == FSP_SUCCESS)
        { Capture_Sample(value); }
        else { g_capture_adc_errors++; }
    }
}
void ADC_Init(void)
{
    Capture_Check(R_ADC_Open(&g_adc0_ctrl, &g_adc0_cfg));
    Capture_Check(R_ADC_ScanCfg(&g_adc0_ctrl, &g_adc0_channel_cfg));
    Capture_Check(R_ELC_Open(&g_elc_ctrl, &g_elc_cfg));
    Capture_Check(R_ELC_Enable(&g_elc_ctrl));
    Capture_Check(R_ADC_ScanStart(&g_adc0_ctrl)); /* Arms configured ELC trigger. */
}
double Read_ADC_Voltage_Value(void)
{
    return (double)g_capture_last_adc * 3.3 / 4095.0;
}
