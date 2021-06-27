/*
 * bootloader.h
 *
 *  Created on: 20/02/2021
 *      Author: Gustavo Correa
 */

#ifndef BOOTLOADER_HEADER_BOOTLOADER_H_
#define BOOTLOADER_HEADER_BOOTLOADER_H_
/* Includes ----------------------------------------------------------------- */
// All dependencies

/* Public definitions ------------------------------------------------------- */
// Constants and macros
#define FIRMWARE_VERSION_ADDRESS				0x80BFFFC
#define FIRMWARE_PATH 							"Firmware.bin"
#define FIRMWARE_CURRENT_VERSION_PATH 			"O_VER.BIN"
#define FIRMWARE_INTEGRITY_PATH 				"INT.BIN"
#define FIRMWARE_NEW_VERSION_PATH 				"Nova_versao.bin"
#define MAX_RETRIES 							300
#define BUFFER_SIZE								512
#define APP_START_ADDRESS   					0x08008000 //Inicio do setor 1 (FLASH_SECTOR_1)
#define FLASH_TYPEPROGRAM						FLASH_TYPEPROGRAM_WORD

#define get_version(x) *((const uint32_t *)(x))
/* Public types ------------------------------------------------------------- */
// Unions, structs, enumerations and other type definitions

//#define BOOTLOADER_DEBUG_MODE
#define PROTECT_SECTION_0
typedef enum{
	error_bootloader_none,
	error_bootloader_general,
	error_bootloader_new_version,
	error_bootloader_app_not_valid
}error_bootloader_t;

/* Public variables --------------------------------------------------------- */
// Variables available to upper layers (strongly recommend never using this)


/* Public functions --------------------------------------------------------- */
// Prototypes of functions that are available to the upper layer
void bootloader();




#endif /* BOOTLOADER_HEADER_BOOTLOADER_H_ */
