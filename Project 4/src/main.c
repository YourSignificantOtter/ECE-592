/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include <stdint.h>
#include "gpio_defs.h"

#include "timers.h"
#include "delay.h"
#include "LEDS.h"

#include "HBLED.h"
#include "debug.h"

volatile int g_dly_cntr = 0;
volatile int g_set_current=0;

volatile int measured_current;
volatile int16_t duty_cycle=0;
volatile int error;

enum {BangBang, Incremental, Proportional, PID} control_mode=BangBang;

typedef struct {
	float dState; // Last position input
	float iState; // Integrator state
	float iMax, iMin; // Maximum and minimum allowable integrator state
	float iGain, // integral gain
	pGain, // proportional gain
	dGain; // derivative gain
} SPid;

SPid plantPID = {0, // dState
	0, // iState
	LIM_DUTY_CYCLE, // iMax
	-LIM_DUTY_CYCLE, // iMin
	2.3153, // iGain
	1.3712, // pGain
	1.01319  // dGain
};


float UpdatePID(SPid * pid, float error, float position){
	float pTerm, dTerm, iTerm;

	// calculate the proportional term
	pTerm = pid->pGain * error;
	// calculate the integral state with appropriate limiting
	pid->iState += error;
	if (pid->iState > pid->iMax) 
		pid->iState = pid->iMax;
	else if (pid->iState < pid->iMin) 
		pid->iState = pid->iMin;
	iTerm = pid->iGain * pid->iState; // calculate the integral term
	dTerm = pid->dGain * (position - pid->dState);
	pid->dState = position;

	return pTerm + iTerm - dTerm;
}

void Control_HBLED(void) {
	uint16_t res;
	PTB->PSOR = MASK(DBG_CONTROLLER);
	
	res = ADC0->R[0];
	measured_current = (res*V_REF*MA_SCALING_FACTOR)/(ADC_FULL_SCALE*R_SENSE);

	switch (control_mode) {
		case BangBang:
			if (measured_current < g_set_current)
				duty_cycle = LIM_DUTY_CYCLE;
			else
				duty_cycle = 0;
			break;
		case Incremental:
			if (measured_current < g_set_current)
				duty_cycle+=10;
			else
				duty_cycle-=10;
			break;
		case Proportional:
			duty_cycle += g_set_current - measured_current;
			break;
		case PID:
			duty_cycle = UpdatePID(&plantPID, g_set_current - measured_current, measured_current);
			break;
		default:
			break;
	}

	if (duty_cycle < 0)
		duty_cycle = 0;
	else if (duty_cycle > LIM_DUTY_CYCLE)
		duty_cycle = LIM_DUTY_CYCLE;
	PWM_Set_Value(TPM0, PWM_HBLED_CHANNEL, duty_cycle);
	PTB->PCOR = MASK(DBG_CONTROLLER);
}

void Set_DAC_mA(unsigned int current) {
	unsigned int code = MA_TO_DAC_CODE(current);
	DAC0->DAT[0].DATL = DAC_DATL_DATA0(code);
	DAC0->DAT[0].DATH = DAC_DATH_DATA1(code>>8);
}


void Set_DAC(unsigned int code) {
	DAC0->DAT[0].DATL = DAC_DATL_DATA0(code);
	DAC0->DAT[0].DATH = DAC_DATH_DATA1(code>>8);
}

void Init_DAC(void) {
  // Enable clock to DAC and Port E
	SIM->SCGC6 |= SIM_SCGC6_DAC0_MASK;
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
	
	// Select analog for pin
	PORTE->PCR[DAC_POS] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[DAC_POS] |= PORT_PCR_MUX(0);	
		
	// Disable buffer mode
	DAC0->C1 = 0;
	DAC0->C2 = 0;
	
	// Enable DAC, select VDDA as reference voltage
	DAC0->C0 = DAC_C0_DACEN_MASK | DAC_C0_DACRFS_MASK;
	Set_DAC(0);
}

void Init_ADC(void) {
	// Configure ADC to read Ch 8 (PTB 0)
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK; 
	ADC0->CFG1 = 0x0C; // 16 bit
	ADC0_SC1A = ADC_SC1_AIEN_MASK; // enable inerrupts
	ADC0->SC2 = ADC_SC2_REFSEL(0);
	
	/* Enable Interrupts */
	NVIC_SetPriority(ADC0_IRQn, 128); // 0, 64, 128 or 192
	NVIC_ClearPendingIRQ(ADC0_IRQn); 
	NVIC_EnableIRQ(ADC0_IRQn);
	
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	PORTB->PCR[0]  &= PORT_PCR_MUX(7); 
}

void ADC0_IRQHandler()
{
	PTB->PSOR = MASK(DBG_IRQ_ADC);
	Control_HBLED(); //The flag will be cleared when we read the ADC value in this function
	PTB->PCOR = MASK(DBG_IRQ_ADC);
}

void TPM0_IRQHandler()
{
	static int every_other = 0;
	PTB->PSOR = MASK(DBG_IRQ_TPM);
	TPM0->SC |= TPM_SC_TOF_MASK;
	if(every_other)
	{
		// Start conversion
		ADC0->SC1[0] = ADC_SC1_AIEN(1) | ADC_SENSE_CHANNEL;
	}
	every_other^=1; //flip the bit so we call control every 2 times through @ 8KHz
	PTB->PCOR = MASK(DBG_IRQ_TPM);
}

/*----------------------------------------------------------------------------
  MAIN function	
 *----------------------------------------------------------------------------*/
int main (void) {

	Init_Debug_Signals();
	Init_DAC();
	Init_RGB_LEDs();
	Control_RGB_LEDs(1,1,0);
	Delay(100);
	Control_RGB_LEDs(0,0,1);
	Init_ADC();
	
	// Configure PTE31 for TPM0 Ch 4
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
	PORTE->PCR[31]  &= PORT_PCR_MUX(7);
	PORTE->PCR[31]  |= PORT_PCR_MUX(3);
	
	PWM_Init(TPM0, PWM_HBLED_CHANNEL, PWM_PERIOD, 0);
	PWM_Set_Value(TPM0, PWM_HBLED_CHANNEL, 0);

	PIT_Init(4800); //4800 gets us 5KHz, 5000 gets us 4.8KHz
	PIT_Start();

	while (1) {
		g_set_current = 0;
		Set_DAC_mA(g_set_current);
		Delay(300);
		g_dly_cntr = 0;
		PTB->PSOR = MASK(DBG_LED_ON);
		for (g_set_current = 10; g_set_current < 50; g_set_current += 10) {
			Set_DAC_mA(g_set_current);
			while(g_dly_cntr < 5)
				;
			g_dly_cntr = 0;
		}
		for (g_set_current = 50; g_set_current >= 0; g_set_current -= 10) {
			Set_DAC_mA(g_set_current);
			while(g_dly_cntr < 5)
				;
			g_dly_cntr = 0;
		}
		PTB->PCOR = MASK(DBG_LED_ON);
	}
}

