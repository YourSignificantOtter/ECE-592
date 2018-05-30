#ifndef ADC_H
#define ADC_H

#include <MKL25Z4.H>

void INIT_ADC(void);

int8_t ADC_Set_Channel(uint16_t channel);
int8_t ADC_Set_Num_Samples(uint16_t num);

uint16_t ADC_Get_Sample(void);

#endif
///
