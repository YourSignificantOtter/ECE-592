/*
 *  File: sd_io.c
 *  Author: Nelson Lombardo
 *  Year: 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */
 
// Modified 2017 by Alex Dean (agdean@ncsu.edu) for teaching FSMs
// - Removed support for PC development (_M_IX86)
// - Split single-line loops & conditionals in source code for readability
// - Fused loops in SD_Read 
// _ Inlined __SD_Write_Block into SD_Write

#include "sd_io.h"
#include <MKL25Z4.h>
#include "debug.h"


extern volatile uint32_t idle_counter;
extern volatile osMessageQueueId_t SPI_Message_Queue;

/******************************************************************************
 Private Methods Prototypes - Direct work with SD card
******************************************************************************/

/**
    \brief Simple function to calculate power of two.
    \param e Exponent.
    \return Math function result.
*/
DWORD __SD_Power_Of_Two(BYTE e);

/**
     \brief Assert the SD card (SPI CS low).
 */
inline void __SD_Assert (void);

/**
    \brief Deassert the SD (SPI CS high).
 */
inline void __SD_Deassert (void);

/**
    \brief Change to max the speed transfer.
    \param throttle
 */
void __SD_Speed_Transfer (BYTE throttle);

/**
    \brief Send SPI commands.
    \param cmd Command to send.
    \param arg Argument to send.
    \return R1 response.
 */
BYTE __SD_Send_Cmd(BYTE cmd, DWORD arg);

/**
    \brief Get the total numbers of sectors in SD card.
    \param dev Device descriptor.
    \return Quantity of sectors. Zero if fail.
 */
DWORD __SD_Sectors (SD_DEV *dev);

/******************************************************************************
 Private Methods - Direct work with SD card
******************************************************************************/

DWORD __SD_Power_Of_Two(BYTE e)
{
    DWORD partial = 1;
    BYTE idx;
    for(idx=0; idx!=e; idx++) partial *= 2;
    return(partial);
}

inline void __SD_Assert(void){
    SPI_CS_Low();
}

inline void __SD_Deassert(void){
    SPI_CS_High();
}

void __SD_Speed_Transfer(BYTE throttle) {
    if(throttle == HIGH) SPI_Freq_High();
    else SPI_Freq_Low();
}

BYTE __SD_Send_Cmd(BYTE cmd, DWORD arg)
{
		PTB->PSOR = MASK(DBG_SD_SEND_CMD);
    BYTE crc, res;

	// ACMD«n» is the command sequense of CMD55-CMD«n»
    if(cmd & 0x80) {
        cmd &= 0x7F;
        res = __SD_Send_Cmd(CMD55, 0);
        if (res > 1)
				{
					PTB->PCOR = MASK(DBG_SD_SEND_CMD);
					return (res);
				}
    }

    // Select the card
    __SD_Deassert();
    SPI_RW(0xFF);
    __SD_Assert();
    SPI_RW(0xFF);

    // Send complete command set
    SPI_RW(cmd);                        // Start and command index
    SPI_RW((BYTE)(arg >> 24));          // Arg[31-24]
    SPI_RW((BYTE)(arg >> 16));          // Arg[23-16]
    SPI_RW((BYTE)(arg >> 8 ));          // Arg[15-08]
    SPI_RW((BYTE)(arg >> 0 ));          // Arg[07-00]

    // CRC?
    crc = 0x01;                         // Dummy CRC and stop
    if(cmd == CMD0) 
			crc = 0x95;         // Valid CRC for CMD0(0)
    if(cmd == CMD8) 
			crc = 0x87;         // Valid CRC for CMD8(0x1AA)
    SPI_RW(crc);

    // Receive command response
    // Wait for a valid response in timeout of 5 milliseconds
    SPI_Timer_On(5);
    do {
				PTB->PTOR = MASK(DBG_SD_SEND_CMD);
        res = SPI_RW(0xFF);
    } while((res & 0x80)&&(SPI_Timer_Status()==TRUE));
    PTB->PCOR = MASK(DBG_SD_SEND_CMD);
		SPI_Timer_Off();
		
    // Return with the response value
    return(res);
}

DWORD __SD_Sectors (SD_DEV *dev)
{
    BYTE csd[16];
    BYTE idx;
    DWORD ss = 0;
    WORD C_SIZE = 0;
    BYTE C_SIZE_MULT = 0;
    BYTE READ_BL_LEN = 0;
    if(__SD_Send_Cmd(CMD9, 0)==0) 
    {
        // Wait for response
        while (SPI_RW(0xFF) == 0xFF);
        for (idx=0; idx!=16; idx++) 
					csd[idx] = SPI_RW(0xFF);
        // Dummy CRC
        SPI_RW(0xFF);
        SPI_RW(0xFF);
        SPI_Release();
        if(dev->cardtype & SDCT_SD1)
        {
            ss = csd[0];
            // READ_BL_LEN[83:80]: max. read data block length
            READ_BL_LEN = (csd[5] & 0x0F);
            // C_SIZE [73:62]
            C_SIZE = (csd[6] & 0x03);
            C_SIZE <<= 8;
            C_SIZE |= (csd[7]);
            C_SIZE <<= 2;
            C_SIZE |= ((csd[8] >> 6) & 0x03);
            // C_SIZE_MULT [49:47]
            C_SIZE_MULT = (csd[9] & 0x03);
            C_SIZE_MULT <<= 1;
            C_SIZE_MULT |= ((csd[10] >> 7) & 0x01);
        }
        else if(dev->cardtype & SDCT_SD2)
        {
            // C_SIZE [69:48]
            C_SIZE = (csd[7] & 0x3F);
            C_SIZE <<= 8;
            C_SIZE |= (csd[8] & 0xFF);
            C_SIZE <<= 8;
            C_SIZE |= (csd[9] & 0xFF);
            // C_SIZE_MULT [--]. don't exits
            C_SIZE_MULT = 0;
        }
        ss = (C_SIZE + 1);
        ss *= __SD_Power_Of_Two(C_SIZE_MULT + 2);
        ss *= __SD_Power_Of_Two(READ_BL_LEN);
        ss /= SD_BLK_SIZE;

        return (ss);
    } else return (0); // Error
}

/******************************************************************************
 Public Methods - Direct work with SD card
******************************************************************************/

BOOL osTimerDelay(uint32_t ms)
{
	static uint32_t start = 0; // get initial tick count
	if(start == 0)
		start = osKernelGetTickCount(); //reset the start time
	
	if(osKernelGetTickCount() - start >= ms) //check the run time since first call
	{
		start = 0; //be able to reset time for next use
		return TRUE;
	}
	else
		return FALSE;
}

SDRESULTS SD_Init(SD_DEV *dev)
{
		PTB->PSOR = MASK(DBG_SD_INIT);
    BYTE n, cmd, ct, ocr[4];
    BYTE idx;
    BYTE init_trys;
		volatile uint32_t before, after, diff; // idle time counts
	
		before = idle_counter;
	
    ct = 0;
    for(init_trys=0; ((init_trys!=SD_INIT_TRYS)&&(!ct)); init_trys++)
    {
        // Initialize SPI for use with the memory card
        SPI_Init();

        SPI_CS_High();
        SPI_Freq_Low();

        // 80 dummy clocks
        for(idx = 0; idx != 10; idx++) 
					SPI_RW(0xFF);

				/* Replaced with the osDelay() call
        SPI_Timer_On(500);
        while(SPI_Timer_Status()==TRUE) {
					PTB->PTOR = MASK(DBG_SD_INIT);
				}
				PTB->PSOR = MASK(DBG_SD_INIT);
        SPI_Timer_Off();
				*/
				
				PTB->PCOR = MASK(DBG_SD_INIT);
				uint32_t tick_freq = osKernelGetTickFreq();
				osDelay(0.5/(1.0/tick_freq));
				PTB->PSOR = MASK(DBG_SD_INIT);
				
        dev->mount = FALSE;
        //SPI_Timer_On(500);
        while ((__SD_Send_Cmd(CMD0, 0) != 1)&&(osTimerDelay(500) == FALSE)){//(SPI_Timer_Status()==TRUE)) {
					PTB->PTOR = MASK(DBG_SD_INIT);
				}
				PTB->PSOR = MASK(DBG_SD_INIT);
	      SPI_Timer_Off();
        // Idle state
        if (__SD_Send_Cmd(CMD0, 0) == 1) {                      
            // SD version 2?
            if (__SD_Send_Cmd(CMD8, 0x1AA) == 1) {
                // Get trailing return value of R7 resp
                for (n = 0; n < 4; n++) 
									ocr[n] = SPI_RW(0xFF);
                // VDD range of 2.7-3.6V is OK?  
                if ((ocr[2] == 0x01)&&(ocr[3] == 0xAA))
                {
                    // Wait for leaving idle state (ACMD41 with HCS bit)...
                    //SPI_Timer_On(1000);
                    while ((osTimerDelay(1000) == FALSE)&&(__SD_Send_Cmd(ACMD41, 1UL << 30))) {//((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(ACMD41, 1UL << 30))) {
											PTB->PTOR = MASK(DBG_SD_INIT);
										}
										PTB->PSOR = MASK(DBG_SD_INIT);
                    //SPI_Timer_Off(); 
                    // CCS in the OCR? 
										// AGD: Delete SPI_Timer_Status call?
                    if ((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(CMD58, 0) == 0))
                    {
                        for (n = 0; n < 4; n++) 
													ocr[n] = SPI_RW(0xFF);
                        // SD version 2?
                        ct = (ocr[0] & 0x40) ? SDCT_SD2 | SDCT_BLOCK : SDCT_SD2;
                    }
                }
            } else {
                // SD version 1 or MMC?
                if (__SD_Send_Cmd(ACMD41, 0) <= 1)
                {
                    // SD version 1
                    ct = SDCT_SD1; 
                    cmd = ACMD41;
                } else {
                    // MMC version 3
                    ct = SDCT_MMC; 
                    cmd = CMD1;
                }
                // Wait for leaving idle state
                SPI_Timer_On(250);
                while ((osTimerDelay(250) == FALSE)&&(__SD_Send_Cmd(cmd, 0))) {//((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(cmd, 0))) {
									PTB->PTOR = MASK(DBG_SD_INIT);
								}
								PTB->PSOR = MASK(DBG_SD_INIT);
                SPI_Timer_Off();
                if(SPI_Timer_Status()==FALSE) 
									ct = 0;
                if(__SD_Send_Cmd(CMD59, 0))   
									ct = 0;   // Deactivate CRC check (default)
                if(__SD_Send_Cmd(CMD16, 512)) 
									ct = 0;   // Set R/W block length to 512 bytes
            }
        }
    }
    if(ct) {
        dev->cardtype = ct;
        dev->mount = TRUE;
        dev->last_sector = __SD_Sectors(dev) - 1;
        dev->debug.read = 0;
        dev->debug.write = 0;
        __SD_Speed_Transfer(HIGH); // High speed transfer
    }
    SPI_Release();
		PTB->PCOR = MASK(DBG_SD_INIT);
		
		after = idle_counter;
		diff = after - before;
		
    return (ct ? SD_OK : SD_NOINIT);
}

SDRESULTS SD_Read(SD_DEV *dev, void *dat, DWORD sector, WORD ofs, WORD cnt)
{
		PTB->PSOR = MASK(DBG_SD_READ);
    SDRESULTS res;
    BYTE tkn, data;
    WORD byte_num;
		volatile uint32_t before, after, diff; // idle time counts
	
		before = idle_counter;
		
    res = SD_ERROR;
    if ((sector > dev->last_sector)||(cnt == 0))
		{
			PTB->PCOR = MASK(DBG_SD_READ);
			return(SD_PARERR);
		}
    // Convert sector number to byte address (sector * SD_BLK_SIZE)
    if (__SD_Send_Cmd(CMD17, sector * SD_BLK_SIZE) == 0) {
				
				tkn = 0xFF;
				while(tkn == 0xFF)
				{
					tkn = SPI_RW(0xFF);
					if(tkn != 0xFF)
						break;
					
					PTB->PTOR = MASK(DBG_SD_READ);
					osDelay(1);
					PTB->PTOR = MASK(DBG_SD_READ);
				}
			
				/*
				SPI_Timer_On(100);  // Wait for data packet (timeout of 100ms)
        do {
            tkn = SPI_RW(0xFF);
						PTB->PTOR = MASK(DBG_SD_READ);
        } while((tkn==0xFF)&&(SPI_Timer_Status()==TRUE));
				PTB->PSOR = MASK(DBG_SD_READ);
        SPI_Timer_Off();
				*/
			
        // Token of single block?
        if(tkn==0xFE) { 
					// AGD: Loop fusion to simplify FSM formation
					byte_num = 0;
          do {
						data = SPI_RW(0xff);
						if ((byte_num >= ofs) && (byte_num < ofs+cnt)) {
               *(BYTE*)dat = data;
               ((BYTE *) dat)++;
						} // else discard bytes before and after data
          } while(++byte_num < SD_BLK_SIZE + 2 ); // 512 byte block + 2 byte CRC
          res = SD_OK;
        }
    }
    SPI_Release();
    dev->debug.read++;
		PTB->PCOR = MASK(DBG_SD_READ);
		
		after = idle_counter;
		diff = after - before;
		
		SPI_RW(0xFF); // This is here so that I can actually read the value of Diff
		
    return(res);
}

SDRESULTS SD_Write(SD_DEV *dev, void *dat, DWORD sector)
{
		PTB->PSOR = MASK(DBG_SD_WRITE);
    WORD idx;
    BYTE line;
		volatile uint32_t before, after, diff; // idle time counts
	
		before = idle_counter;

		// Query invalid?
    if(sector > dev->last_sector) {
			PTB->PCOR = MASK(DBG_SD_WRITE);
			return(SD_PARERR);
		}

    // Convert sector number to bytes address (sector * SD_BLK_SIZE)
    if(__SD_Send_Cmd(CMD24, sector * SD_BLK_SIZE)==0) {
			// Send token (single block write)
			SPI_RW(0xFE);
			// Send block data
			for(idx=0; idx!=SD_BLK_SIZE; idx++) 
				SPI_RW(*((BYTE*)dat + idx));
			/* Dummy CRC */
			SPI_RW(0xFF);
			SPI_RW(0xFF);
			// If not accepted, returns the reject error
			if((SPI_RW(0xFF) & 0x1F) != 0x05) {
				PTB->PCOR = MASK(DBG_SD_WRITE);
				return(SD_REJECT);
			}
			
			/* replaced with osDelay
			// Waits until finish of data programming with a timeout
			SPI_Timer_On(SD_IO_WRITE_TIMEOUT_WAIT);
			do {
					PTB->PTOR = MASK(DBG_SD_WRITE);
					line = SPI_RW(0xFF);
			} while((line==0)&&(SPI_Timer_Status()==TRUE));
			PTB->PSOR = MASK(DBG_SD_WRITE);
			SPI_Timer_Off();
			*/
			line = 0x00;
			while(line == 0x00)
			{
				line = SPI_RW(0xFF);
				if(line != 0x00)
					break;
				
				PTB->PTOR = MASK(DBG_SD_WRITE);
				osDelay(1);
				PTB->PTOR = MASK(DBG_SD_WRITE);
			}
			
			dev->debug.write++;
			
			after = idle_counter;
			diff = after - before;
			
			PTB->PCOR = MASK(DBG_SD_WRITE);
			if(line==0)
				return(SD_BUSY);
			else 
				return(SD_OK);	
		}
    else {
			PTB->PCOR = MASK(DBG_SD_WRITE);
			return(SD_ERROR);
		}
}

SDRESULTS SD_Status(SD_DEV *dev)
{
    return(__SD_Send_Cmd(CMD0, 0) ? SD_OK : SD_NORESPONSE);
}

// «sd_io.c» is part of:
/*----------------------------------------------------------------------------/
/  ulibSD - Library for SD cards semantics            (C)Nelson Lombardo, 2015
/-----------------------------------------------------------------------------/
/ ulibSD library is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/

// Derived from Mister Chan works on FatFs code (http://elm-chan.org/fsw/ff/00index_e.html):
/*----------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.11                 (C)ChaN, 2015
/-----------------------------------------------------------------------------/
/ FatFs module is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/
