/*
 * bsp_debug_uart.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef DEBUG_UART_BSP_DEBUG_UART_H_
#define DEBUG_UART_BSP_DEBUG_UART_H_
#include "hal_data.h"
#include "stdio.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_UART_BLOCK_SAMPLES     256U

/* PCM有效数据 */
#define AUDIO_UART_PAYLOAD_BYTES     (AUDIO_UART_BLOCK_SAMPLES * 2U)

/* 帧头 */
#define AUDIO_UART_HEADER_SIZE       6U

/* 整个UART数据包 */
#define AUDIO_UART_PACKET_BYTES      \
        (AUDIO_UART_HEADER_SIZE + AUDIO_UART_PAYLOAD_BYTES)

/* 环形缓冲 */
#define AUDIO_UART_FIFO_SIZE         2048U


void AudioUART_Init(void);

bool AudioUART_PushSample(int16_t sample);

void AudioUART_Task(void);

//void audio_uart_callback(uart_callback_args_t *p_args);

uint32_t AudioUART_GetOverflowCount(void);

void AudioUART_Trans(const uint16_t *ADC_IN);

void Debug_UART9_Init(void);

#endif /* DEBUG_UART_BSP_DEBUG_UART_H_ */
