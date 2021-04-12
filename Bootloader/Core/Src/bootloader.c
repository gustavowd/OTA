/*
 * Bootloader.c
 *
 *  Created on: 20/02/2021
 *      Author: Gustavo Correa
 */
/* Includes ----------------------------------------------------------------- */
// All dependencies
// The respective header of the library must be always included
#include "bootloader.h"
#include "fatfs.h"
#include "stm32f7xx_hal.h"

/* Private definitions ------------------------------------------------------ */
// Constants and macros

/* Private types ------------------------------------------------------------ */
// Unions, structs, enumerations and other type definitions

/* Private variables -------------------------------------------------------- */
// Variables used by this library in more than one function

/* Private functions - prototypes ------------------------------------------- */
// Functions that will be used just internally, in this library
uint64_t uint8_t2uint64_t(uint8_t * vector);
uint32_t uint8_t2uint32_t(uint8_t * vector);
void uint32_t2uint8_t(uint8_t * vector, uint32_t data);
error_bootloader_t read_file_info(uint32_t * version, const TCHAR *path);
error_bootloader_t write_file_info(uint32_t version, const TCHAR* path);
error_bootloader_t flash_erase();
error_bootloader_t flash_program();
/* Private functions - implementation --------------------------------------- */
// Functions that will be used just internally, in this library

uint64_t uint8_t2uint64_t(uint8_t * vector){
	uint8_t i;
	uint64_t result = 0;
	for(i = 0; i < 8; i++){
		result <<= 8;
		result |= (uint64_t)vector[i];
	}
	return(result);
}
uint32_t uint8_t2uint32_t(uint8_t * vector){
	uint8_t i;
	uint32_t result = 0;
	for(i = 0; i < 4; i++){
		result <<= 8;
		result |= (uint32_t)vector[i];
	}
	return(result);
}
void uint32_t2uint8_t(uint8_t * vector, uint32_t data){
	uint8_t i;
	for(i = 0; i < 4; i++){
		vector[i] = (uint8_t)(data & 0x000000FF);
		data >>= 8;
	}
}




error_bootloader_t read_file_info(uint32_t * info, const TCHAR *path){
	//variables
	FRESULT error_fat;
	error_bootloader_t error_control = error_bootloader_none;
	FIL Arq;
	uint8_t buffer[4];
	unsigned int BR = 0;

	//read operation
	error_fat = f_open(&Arq, path, FA_READ);
	if(error_fat == FR_OK){
		error_fat = f_read(&Arq, buffer, sizeof(buffer), &BR);
		if(error_fat == FR_OK){
			* info = uint8_t2uint32_t(buffer); // Verificar ponteiros aqui
		}
	}
	f_close(&Arq);

	//error control
	if(error_fat != FR_OK){
		error_control = error_bootloader_general;
		* info = 0;
	}
	return (error_control);
}

error_bootloader_t write_file_info(uint32_t info, const TCHAR* path){
	//variables
	FRESULT error_fat;
	error_bootloader_t error_control = error_bootloader_none;
	FIL Arq;
	uint8_t buffer[4];
	unsigned int Bw = 0;

	//starting buffer
	uint32_t2uint8_t(buffer, info);

	//writing operation
	error_fat = f_open(&Arq, path, FA_WRITE);
	if(error_fat == FR_OK){
		error_fat = f_write(&Arq, buffer, sizeof(buffer), &Bw);
	}
	f_close(&Arq);

	//error control
	if(error_fat != FR_OK){
		error_control = error_bootloader_general;
	}
	return (error_control);
}



error_bootloader_t flash_erase(){
	error_bootloader_t error_control = error_bootloader_none;
	FLASH_EraseInitTypeDef erase_data;
	FLASH_OBProgramInitTypeDef obConfig;
	uint32_t blocks = 0;
	/* Flash protection */
	/* Retrieves current OB */
	HAL_FLASHEx_OBGetConfig(&obConfig);

	/* If the first sector is not protected */
	if ((obConfig.WRPSector & OB_WRP_SECTOR_0) == OB_WRP_SECTOR_0) {
	HAL_FLASH_Unlock(); //Unlocks flash
	HAL_FLASH_OB_Unlock(); //Unlocks OB
	obConfig.OptionType = OPTIONBYTE_WRP;
	obConfig.WRPState = OB_WRPSTATE_ENABLE; //Enables changing of WRP settings
	obConfig.WRPSector = OB_WRP_SECTOR_0; //Enables WP on first sector
	HAL_FLASHEx_OBProgram(&obConfig); //Programs the OB
	HAL_FLASH_OB_Launch(); //Ensures that the new configuration is saved in flash
	HAL_FLASH_OB_Lock(); //Locks OB
	HAL_FLASH_Lock(); //Locks flash
	}


	erase_data.TypeErase = FLASH_TYPEERASE_MASSERASE;
	//erase_data.Banks = FLASH_BANK_1;
	erase_data.Sector = FLASH_SECTOR_1;
	erase_data.NbSectors = 0xFF; //apaga todos os setores menos o primeiro que foi protegido pelo bytes opcionais da flash
	erase_data.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_data.VoltageRange = FLASH_VOLTAGE_RANGE_3;


	HAL_FLASH_Unlock(); //Unlocks the flash memory
	HAL_FLASHEx_Erase(&erase_data, &blocks); //Deletes given sectors */
	HAL_FLASH_Lock(); //Locks again the flash memory

	if(blocks != 0xFFFFFFFFU){
		error_control = error_bootloader_general;
	}
	return(error_control);
}






error_bootloader_t flash_program(){
	//variables
	uint8_t buffer_read[8];
	uint64_t buffer_write;
	unsigned int bw = 0;
	FRESULT error_fat;
	HAL_StatusTypeDef error_flash = HAL_OK;
	error_bootloader_t error_control = error_bootloader_none;
	FIL firmware_bin;
	unsigned long firmware_size = 0;
	uint64_t i;

	//program sequence
	error_fat = f_open(&firmware_bin, FIRMWARE_PATH, FA_READ);
	if(error_fat == FR_OK){
		firmware_size = f_size(&firmware_bin);
		for(i = APP_START_ADDRESS; i < firmware_size ; i += FLASH_WRITE_SIZE){
			error_fat = f_read(&firmware_bin, buffer_read, FLASH_WRITE_SIZE, &bw);
			buffer_write = uint8_t2uint64_t(buffer_read);
			if(error_fat == FR_OK){
				HAL_FLASH_Unlock(); //Unlocks the flash memory
				error_flash = HAL_FLASH_Program(FLASH_TYPEPROGRAM, i, buffer_write); //esse buffer aqui pode dar problema
				HAL_FLASH_Lock(); //Locks again the flash memory
				if(error_flash != HAL_OK){
					error_control = error_bootloader_general;
				}
			}
			else{
				error_control = error_bootloader_general;
			}
		}

	}
	else{
		error_control = error_bootloader_general;
	}
	f_close(&firmware_bin);
	return(error_control);
}
/* Public functions --------------------------------------------------------- */
// Implementation of functions that are available to the upper layer



void bootloader(){
	error_bootloader_t error_control;
	uint32_t fw_current_version = 0;
	uint32_t fw_new_version = 0;
	uint32_t fw_integrity = 0;

	//get firmware integrity info
	read_file_info(&fw_integrity, (const TCHAR*)FIRMWARE_INTEGRITY_PATH);

	//get current firmware info
	read_file_info(&fw_current_version, (const TCHAR*)FIRMWARE_CURRENT_VERSION_PATH);

	//get new firmware info
	read_file_info(&fw_new_version, (const TCHAR*)FIRMWARE_NEW_VERSION_PATH);

	//control sequential
	if((fw_new_version > fw_current_version) || (fw_integrity !=  error_bootloader_none)){

		write_file_info(error_bootloader_app_not_valid, (const TCHAR*)FIRMWARE_INTEGRITY_PATH);
		if(flash_erase() != error_bootloader_none){
			Error_Handler();
		}

		if(flash_program() != error_bootloader_none){
			Error_Handler();
		}
		write_file_info(error_bootloader_none, (const TCHAR*)FIRMWARE_INTEGRITY_PATH);
		write_file_info(fw_new_version, (const TCHAR*)FIRMWARE_CURRENT_VERSION_PATH);
	}
	f_unlink(FIRMWARE_PATH);
	//jump to application

	//MX_GPIO_Deinit(); //Puts GPIOs in default state
	SysTick->CTRL = 0x0; //Disables SysTick timer and its related interrupt
	HAL_DeInit();

	RCC->CIR = 0x00000000; //Disable all interrupts related to clock
	__set_MSP(*((volatile uint32_t*) APP_START_ADDRESS)); //Set the MSP

	__DMB(); //ARM says to use a DMB instruction before relocating VTOR */
	SCB->VTOR = APP_START_ADDRESS; //We relocate vector table to the sector 1
	 __DSB(); //ARM says to use a DSB instruction just after relocating VTOR */
	/* We are now ready to jump to the main firmware */
	uint32_t JumpAddress = *((volatile uint32_t*) (APP_START_ADDRESS + 4));
	void (*reset_handler)(void) = (void*)JumpAddress;
	reset_handler(); //We start the execution from he Reset_Handler of the main firmware

}
