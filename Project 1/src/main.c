/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include "states.h"
#include "LEDS.h"
#include "UART.h"
#include "ADC.h"
#include "Timer.h"

volatile uint16_t g_voltage_samples[6000];
volatile uint16_t g_num_adc_samples = 100;
volatile uint16_t g_sample_period = 10; //stored in MS;
volatile uint16_t g_elapsed_ms = 0; //Num of 1ms irq's that have occured
volatile uint16_t g_values_recorded = 0;
volatile uint8_t  g_waiting_flag = 1;


int main (void) 
{
	uint8_t RX_String[10], print_string[40], index = 0;
	int32_t value = 0;
	State curr_state = CLI_S, next_state = CLI_S;

	RX_String[9] = '\0';
	print_string[39] = '\0';
	
	Init_UART0(115200);
	Init_RGB_LEDs();
	Init_state_debug_pins();
	INIT_ADC();
	
	State_set_LEDs(curr_state);
	State_set_debug_pins(curr_state);

	Send_String_Poll("\n\r\n\rNicholas Herrmann Project 1 ECE 592\n\r");
	Send_String_Poll("Valid commands are R, S, C#, P#, and N#\n\r");
		
	// Code listing 8.9, p. 233
	while (1) 
	{
		switch(curr_state)
		{
			case(CLI_S):
				Send_String_Poll("Please enter a command: ");
			
				while(index < 7)
				{
					RX_String[index] = UART0_Receive_Poll();
					UART0_Transmit_Poll(RX_String[index]); //Get command character and print it out to the user	

					if(RX_String[index] == '\n' || RX_String[index] == '\r')
					{
						UART0_Transmit_Poll('\n');
						RX_String[index] = '\0';
						index = 0;
						break;
					}
					else
						index++;
				}
				
				if(index >= 7)
				{
					Send_Error("Input Too Long!\n\r");
					index = 0;
					break;
				}
				
				value = Parse_Command(RX_String, &next_state);
				if(value < 0 || next_state == INVALID_S)
				{
					Send_Error("\n\r");
					next_state = INVALID_S;
					break;
				}
				
				if(next_state == CLI_S) //Perform tasks for commands C P and N
				{
					if(RX_String[0] == 'C' || RX_String[0] == 'c')
					{
						if(ADC_Set_Channel(value) < 0)
							Send_Error("Bad ADC channel number!\n\r");
						else
						{
							sprintf(print_string, "Channel %i selected\n\r", value);
							Send_String_Poll(print_string);
						}
					}
					else if(RX_String[0] == 'P' || RX_String[0] == 'p')
					{
						if(Set_Sampling_Period(value) < 0)
							Send_Error("Bad ADC sampling period!\n\r");
						else
						{
							sprintf(print_string, "Sampling period set to %i ms.\n\r", g_sample_period);
							Send_String_Poll(print_string);
						}
					}
					else if(RX_String[0] == 'N' || RX_String[0] == 'n')
					{
						if(ADC_Set_Num_Samples(value) < 0)
						{
							Send_Error("Bad number of samples!\n\r");
						}
						else {
							sprintf(print_string, "Sample count set to %i.\n\r", g_num_adc_samples);
							Send_String_Poll(print_string);
						}
					}
				}
				break;
				
			case(SAMPLING_S):
				if(g_values_recorded == 0) //When entering this state for the first time this recording
				{
					next_state = WAITING_S; // busy wait
					INIT_Timer();
				}
				else if(g_values_recorded < g_num_adc_samples)
				{
					next_state = WAITING_S; // busy wait
					ADC_Get_Sample();
				}
				else
				{
					DEINIT_Timer();
					next_state = CLI_S;
					g_values_recorded = 0;
				}
				break;
				
			case(WAITING_S):
				while(g_waiting_flag)
					__NOP;
				
				next_state = SAMPLING_S;
				g_waiting_flag = 1;
				break;
				
			case(SENDING_DATA_S):
				sprintf(print_string, "Sending data (%i samples).\n\r", g_num_adc_samples);
				Send_String_Poll(print_string);
				Print_ADC_Result();
				next_state = CLI_S;
				break;
			
			default:
				//Send_String_Poll("ERROR! Invalid State Reached!\n\r");
				next_state = CLI_S;
				break;
		}
		//Advance the state 
		if(next_state != curr_state)
		{
			curr_state = next_state;
			State_set_LEDs(curr_state);
			State_set_debug_pins(curr_state);
			value = 0;
		}
		
	}
}

//Perform ADC Sampling in this IRQ
void TPM0_IRQHandler()
{
	TPM0->SC |= TPM_SC_TOIE_MASK; //reset overflow flag
	++g_elapsed_ms;
	if(g_elapsed_ms == g_sample_period)
	{
		g_elapsed_ms = 0;
		g_values_recorded++;
		g_waiting_flag = 0;
	}		
}
// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
