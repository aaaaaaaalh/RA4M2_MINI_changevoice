#include "bsp_debug_uart.h"
#include "audio_capture.h"
static uint8_t packet[AUDIO_UART_PACKET_BYTES];
static uint16_t samples[CAPTURE_BLOCK];
static volatile bool busy;
volatile uint32_t g_capture_uart_errors, g_capture_sent_packets;

void Debug_UART9_Init(void)
{
    busy = false;
    Capture_Check(R_SCI_UART_Open(&g_uart9_ctrl, &g_uart9_cfg));
}
void AudioUART_Init(void) { Debug_UART9_Init(); }
void debug_uart9_callback(uart_callback_args_t *args)
{
    if (args->event == UART_EVENT_TX_COMPLETE) { __DMB(); busy = false; }
    /* Input commands are not part of v0.2 protocol. */
}
void AudioUART_Task(void)
{
    uint16_t sequence;
    if (busy || !Capture_Pop(samples, &sequence)) { return; }
    packet[0] = 0xAA; packet[1] = 0x55;
    packet[2] = 0x00; packet[3] = 0x02; /* 512 payload bytes */
    packet[4] = (uint8_t)sequence; packet[5] = (uint8_t)(sequence >> 8);
    for (uint32_t i = 0; i < CAPTURE_BLOCK; ++i)
    {
        /* Multiplication avoids undefined left shift of a negative signed value.
         * Fixed offset preserves ADC levels for host-side diagnostics. */
        int32_t pcm = ((int32_t)samples[i] - 2048) * 16;
        uint16_t u = (uint16_t)(int16_t)pcm;
        packet[6 + 2*i] = (uint8_t)u; packet[7 + 2*i] = (uint8_t)(u >> 8);
    }
    busy = true;
    fsp_err_t code = R_SCI_UART_Write(&g_uart9_ctrl, packet, sizeof(packet));
    if (code != FSP_SUCCESS) { busy = false; g_capture_uart_errors++; }
    else { g_capture_sent_packets++; }
}
uint32_t AudioUART_GetOverflowCount(void) { return g_capture_dropped_blocks; }
