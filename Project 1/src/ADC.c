#include "ADC.h"
#include <MKL25Z4.H>
#include <math.h>
#include "UART.h"

extern volatile uint16_t g_voltage_samples[6000];
extern volatile uint16_t g_num_adc_samples;

uint8_t Channel_to_port_pos(uint8_t channel)
{
	uint8_t pin_num;
	switch(channel)
	{
		case 0:
			return 20;
		case 3:
			break;
		case 4:
			break;
		case 6:
			break;
		case 7:
			break;
		case 11:
			break;
		case 14:
			break;
		case 23:
			break;
		case 26:
			break;
		case 27:
			break;
		default:
			break;
	}
	return 1;
}

void INIT_ADC(void)
{
	//Setup the pins for each of the channels
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK;
	
	PORTE->PCR[21] &= ~PORT_PCR_MUX_MASK;          
	PORTE->PCR[21] |= PORT_PCR_MUX(0);
	
	//Put clock to ADC
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
	
	//Choose the clock used by the ADC and set the scalar, txtbook page 169
	ADC0->CFG1 = ADC_CFG1_MODE(3) | ADC_CFG1_ADICLK(1);
	
	//Set the channel 
	ADC0_SC1A = ADC_SC1_ADCH(4);
	
	// Default voltage ref
	ADC0->SC2 = ADC_SC2_REFSEL(0);
}

int8_t ADC_Set_Channel(uint16_t channel)
{
	static const uint8_t valid_channels[10] = { 0, 3, 4, 6, 7, 11, 14, 23, 26, 27}; //all valid ADC Channels
	uint8_t i;
	for(i = 0; i < 10; i++)
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
	
	g_num_adc_samples = num;
	return 1;
}

uint16_t ADC_Get_Sample(void)
{
	uint16_t the_val = 0;
	static uint16_t index = 0;
	//Start the conversion
	ADC0_SC1A = 4 & ADC_SC1_ADCH_MASK;
	//Poll for completion
	while(!(ADC0_SC1A & ADC_SC1_COCO_MASK))
		;
	//Save the value into the voltage array
	the_val = ADC0_RA;
	g_voltage_samples[index] = ADC0_RA;
	
	if(index == g_num_adc_samples)
		index = 0;
	else
		index++;
	
	return the_val;
}

void Print_ADC_Result(void)
{
	uint16_t index = 0;
	uint8_t string[13];
	string[12] = '\0';
	while(index < g_num_adc_samples)
	{
		float voltage = g_voltage_samples[index] * (3.3f / 65535.0f); //Assuming the ADC reference voltage is 3.3V, so there is no "correction factor" needed
		sprintf(string, "%i: %4.3f V\n\r", index+1, voltage);
		Send_String_Poll(string);
		index++;
	}
}
