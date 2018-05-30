#include "states.h"
#include <MKL25Z4.H>
#include "LEDs.h"
#include "gpio_defs.h"

void Init_state_debug_pins(void)
{
	// Enable clock to port B
	SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
	
	// Make pins GPIO
	PORTB->PCR[CLI_DEBUG_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[CLI_DEBUG_POS] |= PORT_PCR_MUX(1);
	PORTB->PCR[SAMPLING_DEBUG_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[SAMPLING_DEBUG_POS] |= PORT_PCR_MUX(1);
	PORTB->PCR[WAITING_DEBUG_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[WAITING_DEBUG_POS] |= PORT_PCR_MUX(1);
	PORTB->PCR[SENDING_DATA_DEBUG_POS] &= ~PORT_PCR_MUX_MASK;          
	PORTB->PCR[SENDING_DATA_DEBUG_POS] |= PORT_PCR_MUX(1);
	
	// Set ports to outputs
	PTB->PDDR |= MASK(CLI_DEBUG_POS) | MASK(SAMPLING_DEBUG_POS) | MASK(WAITING_DEBUG_POS) | MASK(SENDING_DATA_DEBUG_POS);
}

void State_set_debug_pins(State s)
{
	PTB->PCOR = MASK(CLI_DEBUG_POS) | MASK(SAMPLING_DEBUG_POS) | MASK(WAITING_DEBUG_POS) | MASK(SENDING_DATA_DEBUG_POS);
	switch(s)
	{
		case(CLI_S):
			PTB->PSOR = MASK(CLI_DEBUG_POS);
			break;
		case(SAMPLING_S):
			PTB->PSOR = MASK(SAMPLING_DEBUG_POS);
			break;
		case(WAITING_S):
			PTB->PSOR = MASK(WAITING_DEBUG_POS);
			break;
		case(SENDING_DATA_S):
			PTB->PSOR = MASK(SENDING_DATA_DEBUG_POS);
			break;
		default:
			break;
		}
}

void State_set_LEDs(State s)
{
		switch(s)
		{
			case(CLI_S):
				Control_RGB_LEDs(0,0,1);
				break;
			case(SAMPLING_S):
				Control_RGB_LEDs(0,1,0);
				break;
			case(WAITING_S):
				Control_RGB_LEDs(1,1,0);
				break;
			case(SENDING_DATA_S):
				Control_RGB_LEDs(1,0,1);
				break;
			default:
				Control_RGB_LEDs(0,0,0);
				break;
		}
}

State State_check_command_and_set_next_state(uint8_t command)
{
	static const uint8_t valid_commands[10] = { 'R', 'r', 'S', 's', 'C', 'c', 'P', 'p', 'N', 'n'};
	
	if(command == valid_commands[0] || command == valid_commands[1])
		return SAMPLING_S;
	else if(command == valid_commands[2] || command == valid_commands[3])
		return SENDING_DATA_S;
	else if(command == valid_commands[4] || command == valid_commands[5])
		return CLI_S;
	else if(command == valid_commands[6] || command == valid_commands[7])
		return CLI_S;
	else if(command == valid_commands[8] || command == valid_commands[9])
		return CLI_S;
	else
		return INVALID_S;
}

int Parse_Command(uint8_t * c, State * next_state)
{
	uint16_t value = 0;
	*next_state = State_check_command_and_set_next_state( *c );
	if(*next_state == INVALID_S)
		return -1;
	*c++;
	while(*c != '\0')
	{
		if ( *c >= '0' && *c <= '9')
		{
			value = value*10 + *c - '0';
			*c++;
		}
		else
			return -1;
	}
	return value;
}
