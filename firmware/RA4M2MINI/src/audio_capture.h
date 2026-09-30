#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H
#include "hal_data.h"
#include <stdbool.h>
#define CAPTURE_BLOCK 256U
void Capture_Init(void);
void Capture_Sample(uint16_t value);
bool Capture_Pop(uint16_t *samples, uint16_t *sequence);
extern volatile uint32_t g_capture_samples;
extern volatile uint32_t g_capture_dropped_blocks;
extern volatile uint32_t g_capture_adc_errors;
extern volatile uint32_t g_capture_uart_errors;
extern volatile uint32_t g_capture_sent_packets;
extern volatile uint16_t g_capture_last_adc;
extern volatile fsp_err_t g_capture_error;
void Capture_Check(fsp_err_t code);
#endif
