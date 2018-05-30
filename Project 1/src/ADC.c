#include "ADC.h"
#include <MKL25Z4.H>

extern volatile uint16_t g_voltage_samples[6000];
extern volatile uint16_t g_num_adc_samples;

void INIT_ADC(void)
{
	
}

int8_t ADC_Set_Channel(uint16_t channel)
{
	static const uint8_t valid_channels[16] = { 0, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 23, 26, 27}; //all valid ADC Channels
	uint8_t i;
	for(i = 0; i < 16; i++)
	{
		if(channel == valid_channels[i])
		{
			//do stuff
			return 1;
		}
	}
	
	return -1; //illegal channel num
}


int8_t ADC_Set_Num_Samples(uint16_t num)
{
	if(num < 1 || num > 6000)
		return -1;
	
	//do stuff
	g_num_adc_samples = num;
	return 1;
}

uint16_t ADC_Get_Sample(void)
{
	uint16_t value = 0;
	return value;
}
