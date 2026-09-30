/*
 * bsp_dac.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef DAC_BSP_DAC_H_
#define DAC_BSP_DAC_H_

#include "hal_data.h"

void DAC_Init();
void DAC_SetVoltage(float voltage);
void DAC_SinWave_Cycle(uint32_t time_interval);


#endif /* DAC_BSP_DAC_H_ */
