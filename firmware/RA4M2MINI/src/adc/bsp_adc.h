/*
 * bsp_adc.h
 *
 *  Created on: 2026年9月12日
 *      Author: 87245
 */

#ifndef ADC_BSP_ADC_H_
#define ADC_BSP_ADC_H_

#include "hal_data.h"

void ADC_Init(void);
double Read_ADC_Voltage_Value(void);
#endif /* ADC_BSP_ADC_H_ */
