#include "Timer.h"
#include <MKL25Z4.H>

extern volatile uint16_t g_sample_period; //stored in MS;

void INIT_Timer(void) //Taken from Chaper 7 page 197 of textbook
{
	SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
	
	SIM->SOPT2 |= (SIM_SOPT2_TPMSRC(1) | SIM_SOPT2_PLLFLLSEL_MASK);
	
	TPM0->CNT = TPM_CNT_COUNT(0); //There some kind of bug where every sampling event after the first happens too early. I believe it is because there is some count
	// left in the timer. this doesnt seem to right that issue though.
	
	TPM0->MOD = (F_TPM_CLOCK/(F_TPM_OVERFLOW*32))-1;
	
	TPM0->SC = TPM_SC_CMOD(1) | TPM_SC_PS(5) | TPM_SC_TOIE_MASK | TPM_SC_CPWMS(0);
		
	NVIC_SetPriority(TPM0_IRQn, 3);
	NVIC_ClearPendingIRQ(TPM0_IRQn);
	NVIC_EnableIRQ(TPM0_IRQn);
}

void DEINIT_Timer(void)
{
	TPM0->CNT = TPM_CNT_COUNT(0); //There some kind of bug where every sampling event after the first happens too early. I believe it is because there is some count
	// left in the timer. this doesnt seem to right that issue though.
	
	SIM->SCGC6 &= ~SIM_SCGC6_TPM0_MASK; //Not sure if this is the right way to do this but it works
}

int8_t Set_Sampling_Period(uint16_t period)
{
	if(period < 1 || period > 60000)
		return -1;
	
	//do stuff
	g_sample_period = period;
	return 1;
}

void Timer_Delay(void)
{
	
}
