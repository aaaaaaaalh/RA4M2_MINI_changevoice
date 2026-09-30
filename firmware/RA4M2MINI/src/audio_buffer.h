/*
 * audio_buffer.h
 *
 *  Created on: 2026年9月15日
 *      Author: Administrator
 */

#ifndef AUDIO_BUFFER_H_
#define AUDIO_BUFFER_H_

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_FRAME_SIZE 512U

/* 这里只声明，不分配内存 */
extern uint16_t g_adc_buffer[2][AUDIO_FRAME_SIZE];

extern volatile bool g_adc_buffer_ready[2];

extern volatile uint8_t  g_adc_write_buffer;
extern volatile uint16_t g_adc_index;
extern volatile uint16_t g_adc_buf;
extern volatile bool g_adc_sign;
#endif

#endif /* AUDIO_BUFFER_H_ */
