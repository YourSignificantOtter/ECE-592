#include "integer.h"
#include <MKL25Z4.h>
#include "spi_io.h"
#include "sd_io.h"
#include "LEDs.h"
#include "debug.h"

SD_DEV dev[1];          // Create device descriptor
uint8_t buffer[512];    // Example of your buffer data
volatile uint32_t sum = 0;

void test_write(void) {
	// Write test data to given block (sector_num) in flash. 
	// Read it back, compute simple checksum to confirm it is correct.
	
	PTB->PSOR = MASK(DBG_7);
	
	int i;
	DWORD sector_num = 0x23; // Manual wear leveling
  SDRESULTS res = SD_ERROR;
	FSM_SD_RETURN_TYPE fsm_res;
	fsm_res.result = SD_ERROR;
	fsm_res.state = FSM_BUSY;
	
	// Erase buffer
	for (i=0; i<SD_BLK_SIZE; i++)
		buffer[i] = 0;
	
	// Load sample data into buffer
	buffer[0] = 0xDE;
	buffer[1] = 0xAD;
	buffer[2] = 0xC0;
	buffer[3] = 0xDE;
	
	buffer[508] = 0xFE;
	buffer[509] = 0xED;
	buffer[510] = 0xCA;
	buffer[511] = 0xFE;

  if(SD_Init(dev)==SD_OK) {
		// Change the data in this sector
		//res = SD_Write(dev, (void*)buffer, sector_num);
		do {
			fsm_res = SD_Write_FSM(dev, (void*)buffer, sector_num);
	}while(fsm_res.state == FSM_BUSY);
		if(fsm_res.state == SD_OK) {
			Control_RGB_LEDs(0,0,1);	// Blue - written ok
			// erase buffer
			for (i=0; i<SD_BLK_SIZE; i++)
				buffer[i] = 0;
			// read block again
			//res = SD_Read(dev, (void*)buffer, sector_num, 0, 512);
			do
			{
				fsm_res = SD_Read_FSM(dev, (void*)buffer, sector_num, 0, 512); 
			}	while(fsm_res.state == FSM_BUSY);
			res = fsm_res.result;
			
			if(res==SD_OK) {
				for (i = 0, sum = 0; i < SD_BLK_SIZE; i++)
					sum += buffer[i];
			
				if (sum == 0x06DC)
					Control_RGB_LEDs(0,1,0); // Green - read was OK
				else
					Control_RGB_LEDs(1,0,0); // Red - checksum failed
			} else {
				Control_RGB_LEDs(1,0,0); // Red - read failed
			}
		} else {
				Control_RGB_LEDs(1,0,1); // Magenta - write failed
		}
	} else {
		while (1) {
			Control_RGB_LEDs(1,0,0); // Red - init failed
			SPI_Timer_On(250);
			while (SPI_Timer_Status() == TRUE)
				;
			SPI_Timer_Off();
			Control_RGB_LEDs(1,1,0); // Yellow - init failed
			SPI_Timer_On(250);
			while (SPI_Timer_Status() == TRUE)
				;
			SPI_Timer_Off();
		}
	}
	while (1)
		;
}

int main(void)
{
	Init_Debug_Signals();
	Init_RGB_LEDs();
	Control_RGB_LEDs(1,1,0);	// Yellow - starting up
	
 	// Test function to write a block and then read back, verify
	test_write();
  
	while (1)
		;
}
