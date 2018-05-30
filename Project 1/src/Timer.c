#include "Timer.h"
#include <MKL25Z4.H>

extern volatile uint16_t g_sample_period; //stored in MS;

int8_t Set_Sampling_Period(uint16_t period)
{
	if(period < 1 || period > 60000)
		return -1;
	
	//do stuff
	g_sample_period = period;
	return 1;
}
