#ifndef STATES_H
#define STATES_H

#include <MKL25Z4.H>

typedef enum{
	CLI_S,
	SAMPLING_S,
	WAITING_S,
	SENDING_DATA_S,
	INVALID_S}State;

#define CLI_DEBUG_POS (0)
#define SAMPLING_DEBUG_POS (1)
#define WAITING_DEBUG_POS (2)
#define SENDING_DATA_DEBUG_POS (3)

	
void Init_state_debug_pins(void);
void State_set_debug_pins(State s);
void State_set_LEDs(State s);
	
int32_t Parse_Command(uint8_t * c, State * next_state);

#endif
/////

