#ifndef TIMER_H
#define TIMER_H

#include <MKL25Z4.H>

#define F_TPM_CLOCK (48000000) //48 MHz
#define F_TPM_OVERFLOW (1000) //1KHz overflow = 1 overflow per ms

void INIT_Timer(void);
void DEINIT_Timer(void);

int8_t Set_Sampling_Period(uint16_t period);

void Timer_Delay(void); //Delays by the period set via Set_Sampling_Period

#endif
/////
