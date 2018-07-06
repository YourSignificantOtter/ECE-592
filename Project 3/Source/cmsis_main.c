/*----------------------------------------------------------------------------
 * CMSIS-RTOS 'main' function template
 *---------------------------------------------------------------------------*/
 
#include "RTE_Components.h"
#include  CMSIS_device_header
#include "cmsis_os2.h"
 
#ifdef RTE_Compiler_EventRecorder
#include "EventRecorder.h"
#endif

#include "integer.h"
#include <MKL25Z4.h>
#include "spi_io.h"
#include "sd_io.h"
#include "LEDs.h"
#include "debug.h"

SD_DEV dev[1];          // Create device descriptor
uint8_t buffer[512];    // Example of your buffer data
volatile uint32_t sum = 0;
extern volatile uint32_t idle_counter;
volatile osMessageQueueId_t SPI_Message_Queue;
 
/*----------------------------------------------------------------------------
 * Application main thread
 *---------------------------------------------------------------------------*/
void app_main (void *argument) {
 
  // ...
  for (;;) {}
}

void SD_Manager_thread(void * argument) {
	// On first run, init card and write test data to given block (sector_num) in flash. 
	// Then repeatedly read the sector and confirm its contents	

	/*
	//1 second of idle time, read the idle_counter
	volatile uint32_t before, after, diff, sysTick; // idle time counts
	before = idle_counter;
	sysTick = osKernelGetTickFreq();
	osDelay(1.0/(1.0/sysTick));
	after = idle_counter;
	diff = after - before;
	*/
	
	int i;
	DWORD sector_num = 0x23; // Manual wear leveling
  SDRESULTS res;
	
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

  if(SD_Init(dev)!=SD_OK) {
			Control_RGB_LEDs(1,0,0); // Red - init failed
			while (1)
				;
	}
	// Change the data in this sector
	res = SD_Write(dev, (void*)buffer, sector_num);
	if(res!=SD_OK) {
		Control_RGB_LEDs(1,0,1); // Magenta - write failed
		while (1)
			;
	}
	Control_RGB_LEDs(0,0,1);	// Blue - written ok
	
	while (1) { // Repeat this loop forever, ten times per second
		// Need to insert an osDelay call here 
		
		// erase buffer
		for (i=0; i<SD_BLK_SIZE; i++)
			buffer[i] = 0;
		// read block again
		res = SD_Read(dev, (void*)buffer, sector_num, 0, 512);
		
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
	}
}
 
int main (void) {
 
  // System Initialization
  SystemCoreClockUpdate();
#ifdef RTE_Compiler_EventRecorder
  // Initialize and start Event Recorder
  EventRecorderInitialize(EventRecordError, 1U);
#endif
  // ...
 
	Init_Debug_Signals();
	Init_RGB_LEDs();
	SPI_Message_Queue = osMessageQueueNew(8, sizeof(unsigned char), NULL);
	Control_RGB_LEDs(1,1,0);	// Yellow - starting up
	
  osKernelInitialize();                 // Initialize CMSIS-RTOS
  //osThreadNew(app_main, NULL, NULL);    // Create application main thread
	osThreadNew(SD_Manager_thread, NULL, NULL);
  osKernelStart();                      // Start thread execution
  for (;;) {}
}
