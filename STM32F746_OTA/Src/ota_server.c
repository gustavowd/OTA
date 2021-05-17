/*
 * 		ota_server.c
 *
 *  	Created on: 5/05/2021
 *      Author: Gustavo Correa
 */

/* Includes ----------------------------------------------------------------- */
// All dependencies
// The respective header of the library must be always included
#include "ota_server.h"

/* Private definitions ------------------------------------------------------ */
// Constants and macros

/* Private types ------------------------------------------------------------ */
// Unions, structs, enumerations and other type definitions

/* Private variables -------------------------------------------------------- */
// Variables used by this library in more than one function
struct tls_client cli;
static uint8_t buf[BUFFER_SIZE];
SemaphoreHandle_t sem_connected = NULL;
unsigned long file_size = 0;

/* use static allocation to keep the heap size as low as possible */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
uint8_t memory_buf[MAX_MEM_SIZE];
#endif

volatile int reconnection_trigger = 0;
/* Private functions - prototypes ------------------------------------------- */
// Functions that will be used just internally, in this library
void text2hex(unsigned char * string);
error_ota_t read_file_info(uint32_t * info, const TCHAR *path);
error_ota_t sha256sum(const TCHAR* path, unsigned char* saida);
error_ota_t integrity(const TCHAR* Firmware_path, const TCHAR* Versao_path);
int http_send_request(struct tls_connection *con, char *req, size_t req_size);
int http_get_response(struct tls_connection *con, unsigned char *resp, size_t resp_size);
error_ota_t get_file_from_server(char *Request, const TCHAR* Path);

/* Private functions - implementation --------------------------------------- */
// Functions that will be used just internally, in this library
void OTA_Error_Handler(char *msg)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
	UARTPutString(msg,strlen(msg));
  /* USER CODE END Error_Handler_Debug */
}

void text2hex(unsigned char * string){
	int i;
	char aux[2];
	for(i = 0; i < 64; i += 2){
		aux[0] = string[i];
		aux[1] = string[i + 1];
		string[i / 2] = strtol(aux, NULL, 16);
	}
}

error_ota_t read_file_info(uint32_t * info, const TCHAR *path){
	//variables
	FRESULT error_fat;
	error_ota_t error_control = error_ota_none;
	FIL Arq;
	uint8_t buffer[] = {0, 0, 0, 0};
	unsigned int BR = 0;

	//read operation
	error_fat = f_open(&Arq, path, FA_READ);
	if(error_fat == FR_OK){
		error_fat = f_read(&Arq, buffer, f_size(&Arq), &BR);
		if(error_fat == FR_OK){
			* info  = (uint32_t)atoi(buffer);
		}
	}
	f_close(&Arq);

	//error control
	if(error_fat != FR_OK){
		error_control = error_ota_general;
		* info = 0;
	}
	return (error_control);
}

error_ota_t sha256sum(const TCHAR* path, unsigned char* saida){
	//Variables
	error_ota_t error_control = error_ota_none;
	mbedtls_sha256_context Contexto;
	uint8_t buffer[BUFFER_SIZE];
	FIL Arq;
	FRESULT rec;
	unsigned int BW,lido=0;

	//Generating the hash
	rec=f_open(&Arq, path, FA_READ);//abre o arquivo pra leitura
	if(rec!=FR_OK){
		error_control = error_ota_general;
	}
	mbedtls_sha256_init(&Contexto);//inicia o contexto
	mbedtls_sha256_starts_ret(&Contexto, 0);//inicia o sha256
	while (lido < f_size(&Arq)){
		rec = f_read(&Arq, buffer, BUFFER_SIZE, &BW);//adiciona ao buffer 512 bits do arquivo
		if(rec != FR_OK){
			error_control = error_ota_general;
		}
		mbedtls_sha256_update_ret(&Contexto, buffer, BW); //adiciona o buffer ao sha256
		lido += BW;
	}
	mbedtls_sha256_finish_ret(&Contexto, saida);//grava em saida o hash
	f_close(&Arq);
	mbedtls_sha256_free(&Contexto);

	//error control
	return(error_control);
}

error_ota_t integrity(const TCHAR* Firmware_path, const TCHAR* Versao_path){
	//Variables
	error_ota_t error_control = error_ota_none;
	unsigned char Saida_Firmware[32], Saida_Versao[64];
	FIL Versao;
	unsigned int BW;

	//Getting SHA-256 from firmware
	error_control = sha256sum(Firmware_path, Saida_Firmware);//obtem o hash do firmware

	//Getting SHA-256 from file
	f_open(&Versao, Versao_path, FA_READ);
	f_read(&Versao, Saida_Versao, 64, &BW);//Obtem o hash do firmware do arquivo do servidor
	f_close(&Versao);
	text2hex(Saida_Versao);
	//Comparing hash
	if(strncmp((char *)Saida_Firmware, (char *)Saida_Versao, 32) != 0){
		error_control = error_ota_general;
	}
#ifdef DEBUG_MODE_OTA
	UARTPutString("Firmware hash on sd card: ",26);
	UARTPutString("\n\r>>",4);
	UARTPutString((char *)(Saida_Firmware), 32);
	UARTPutString("\n\r>>",4);
	UARTPutString("Hash of the server firmware: ",29);
	UARTPutString("\n\r>>",4);
	UARTPutString((char *)(Saida_Versao), 32);
	UARTPutString("\n\r>>",4);
#endif

	//error control
	if(error_control == error_ota_general){
		UARTPutString("Integrity check fail!\n\r>>", 25);
	}
	else{
		UARTPutString("Integrity check Successful!\n\r>>", 31);
	}
	return(error_control);
}

int http_send_request(struct tls_connection *con, char *req, size_t req_size)
{
	int ret = 0;

	do {
		ret = tls_write(con, req, req_size);
		if(ret > 0) {
			req += ret;
			req_size -= ret;
		}
	} while(req_size > 0 && (ret > 0 || ret == TLS_WANT_READ || ret == TLS_WANT_WRITE));

	return ret;
}

int http_get_response(struct tls_connection *con, unsigned char *resp, size_t resp_size)
{
	int ret = 0;

	memset(resp, 0, resp_size);

	do {
		ret = tls_read(con, resp, resp_size);
		if(ret > 0) {
			resp += ret;
			resp_size -= ret;
		}
	} while(resp_size > 0 && (ret > 0 || ret == TLS_WANT_READ || ret == TLS_WANT_WRITE));

	return ret;
}

error_ota_t write_file_sd(const TCHAR* Path){
	//Variables
	error_ota_t error_control = error_ota_none;
	unsigned long i, j;
	FRESULT res;
	FIL Arquivo;
	unsigned int BW = 0;
	char file_size_buffer[] = {0,0,0,0,0,0,0,0,0,0};

	//Abrindo arquivo
	res = f_open(&Arquivo, Path, (file_size == 0)? FA_CREATE_ALWAYS | FA_WRITE : FA_OPEN_APPEND | FA_WRITE);

	if(res == FR_OK){
		/*-----CONSOME CABEÇALHO HTTP------*/
		if(file_size == 0){//Entra somente na primeira vez para obter o tamanho do arquivo
			for (i = 0;
				((strncmp((char *)&buf[i], "\r\n\r\n", 4) != 0) && (i < BUFFER_SIZE));
				i++)
			{
				if((strncmp((char *)&buf[i], "Content-Length:", 15) == 0) && (file_size == 0)){
					for (j = i + 16; strncmp((char *)&buf[j], "\r\n", 2) != 0 ;j++);
					strncpy(file_size_buffer, (char *)&buf[i + 16], (j - (i + 16)));
					file_size = atoi(file_size_buffer);
				}
			}
			i += 4;
			itoa(file_size, file_size_buffer, 10);
			UARTPutString("File size: ", 11);
			UARTPutString(file_size_buffer, strlen(file_size_buffer));
			UARTPutString(" bytes\n\r>>", 10);
			/*-----FIM DO CONSUMO DO CABEÇALHO HTTP------*/
			//Escreve o restante do buffer no arquivo.
			res = f_write(&Arquivo, (char *)&buf[i], (file_size > (BUFFER_SIZE - i)) ? (BUFFER_SIZE - i) : file_size, &BW);//GRAVA NO ARQUIVO
			if(res != FR_OK){
				error_control = error_ota_general;
			}
			file_size -= (file_size > (BUFFER_SIZE - i)) ? (BUFFER_SIZE - i) : file_size;
		}


		else{
			//Escreve o buffer no arquivo
			res = f_write(&Arquivo, buf, (file_size > BUFFER_SIZE) ? BUFFER_SIZE : file_size, &BW);//GRAVA NO ARQUIVO
			if(res != FR_OK){
				error_control = error_ota_general;
			}
			file_size -= (file_size > BUFFER_SIZE) ? BUFFER_SIZE : file_size;
		}
	}
	//error control
	else{
		error_control = error_ota_general;
	}
	f_close(&Arquivo);
	return(error_control);
}


error_ota_t get_file_from_server(char *Request, const TCHAR* Path)
{
	//variables
	error_ota_t error_control = error_ota_none;
	int retries, ret = pdFALSE;
	struct hostent *server_addr = NULL;
	char server_ip_s[16];
	unsigned long server_ip;
	int len = strlen((char *) Request);

	//Establishing conection
	UARTPutString("Connecting to server...\n\r>>", 27);
	if(tls_client_init(&cli)) {
		OTA_Error_Handler("Client initialization fail!\n\r>>");
		ret = pdFALSE;
		error_control = error_ota_general;
		goto exit;

	}
	if(tls_cert_load(&cli.tls, NULL, SSL_CA_PEM, NULL, NULL)) {
		OTA_Error_Handler("Certification fail!\n\r>>");
		ret = pdFALSE;
		error_control = error_ota_general;
		goto deallocate;
	}
	retries = AUTH_SERVER_LOOKUP_RETRIES;
	do {
		server_addr = gethostbyname(AUTH_SERVER);
		if(server_addr == NULL) {
			OTA_Error_Handler("Host fail!\n\r>>");
			retries--;
			vTaskDelay(2000);
		}
	} while(server_addr == NULL && retries);
	server_ip = *((unsigned long*) server_addr->h_addr);
	if(!retries) {
		OTA_Error_Handler("Max retries\n\r>>");
		ret = pdTRUE;
		error_control = error_ota_general;
		goto deallocate;
	}
   // Convert the IP Address into a string.
   sprintf(server_ip_s, "%d.%d.%d.%d", (int)(server_ip & 0xff), (int)((server_ip >> 8) & 0xff),
		  (int)((server_ip >> 16) & 0xff), (int)((server_ip >> 24) & 0xff));
   ret = tls_client_connect(&cli, server_ip_s, AUTH_PORT);
	if(ret) {
		OTA_Error_Handler("TLS conection fail!\n\r>>");
		ret = pdFALSE;
		error_control = error_ota_general;
		goto deallocate;
	}

	UARTPutString("Connection established!\n\r>>",27);
	if(http_send_request(&cli.con, Request, len) < 0) {
		OTA_Error_Handler("Requisition fail!\n\r>>");
		ret = pdFALSE;
		error_control = error_ota_general;
		goto deallocate;
	}

	UARTPutString("Downloading file...\n\r>>",sizeof("Downloading file...\n\r>>"));
	while(http_get_response(&cli.con, buf, sizeof(buf))) {
		//Rotina de armazenamento do buffer no arquivo.
		error_control = write_file_sd(Path);
		if(error_control != error_ota_none){
			OTA_Error_Handler("Saving file error!\n\r>>");
			f_unlink(Path);
			goto deallocate;
		}
	}
	//error_control
	if(error_control == error_ota_none){
		OTA_Error_Handler("Successful download!	\n\r>>");
	}
deallocate:
	tls_connection_free(&cli.con);
	tls_context_free(&cli.tls);
exit:
	//Garante que o file_size seja zerado para as proximas operações.
	file_size = 0;
	return (error_control);
}

/* Public functions --------------------------------------------------------- */
// Implementation of functions that are available to the upper layer

void OTA(void *argument){
	//Variables
	error_ota_t error_control = error_ota_general;
	uint32_t current_version = 0;
	uint32_t new_version = 0;
	char buf_version[10];
	/*
	* 0. Initialize the RNG and the session data
	*/
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
  mbedtls_memory_buffer_alloc_init(memory_buf, sizeof(memory_buf));
#endif

	//create semaphore
	sem_connected = xSemaphoreCreateBinary();

	//Initialize UART driver
	InitUART();
	// clean terminal
	UARTPutString("\033[2J\033[H", 0);

	// print welcome message
	UARTPutString("Started OTA!\n\r>>", 16);

	printf_install_putchar(UARTPutChar);
	//Aguarda a inicialização do fatfs
	vTaskDelay(1000);
	while(1){
		xSemaphoreTake(sem_connected, portMAX_DELAY);
		UARTPutString("\n\n\n\r>>", 6);
		UARTPutString("Looking for new firmware...", 27);
		UARTPutString("\n\r>>", 4);
		//Get the current version of the firmware from file
		current_version = get_flash_info(FIRMWARE_VERSION_ADDRESS);
#ifdef DEBUG_MODE_OTA
		current_version = 0;
#endif

		UARTPutString("Downloading new firmware version from server...", 47);
		UARTPutString("\n\r>>", 4);
		error_control = get_file_from_server(AUTH_REQUEST_VERSION, FIRMWARE_NEW_VERSION_PATH);
		if(error_control == error_ota_none){
			//Get the new firmware version from file
			error_control = read_file_info(&new_version, FIRMWARE_NEW_VERSION_PATH);
			//show current version on uart
			itoa(current_version, buf_version, 10);
			UARTPutString("Current firmware version: ", 26);
			UARTPutString(buf_version, strlen(buf_version));
			UARTPutString("\n\r>>", 4);
			//show current version on uart
			itoa(new_version, buf_version, 10);
			UARTPutString("New firmware version: ", 22);
			UARTPutString(buf_version, strlen(buf_version));
			UARTPutString("\n\r>>", 4);
		}
		else{
			error_control = error_ota_general;
			UARTPutString("Error getting version file!", 27);
			UARTPutString("\n\r>>", 4);
			//Remove files from SD card
			f_unlink(FIRMWARE_NEW_VERSION_PATH);
		}
		if((new_version > current_version) && (error_control == error_ota_none)){//compara versão atual e a do servidor

			UARTPutString("Downloading new firmware...", 27);
			UARTPutString("\n\r>>", 4);
			if(get_file_from_server(AUTH_REQUEST_FIRMWARE, FIRMWARE_PATH) == error_ota_none){//obtem o firmware
				UARTPutString("Downloading SHA-256 file...", 27);
				UARTPutString("\n\r>>", 4);
				if(get_file_from_server(AUTH_REQUEST_HASH,FIRMWARE_NEW_VERSION_HASH_PATH) == error_ota_none){//obtem arquivo contendo o hash

					UARTPutString("Checking integrity of firmware on SD card...", 44);
					UARTPutString(" \n\r>>", 4);
					if(integrity(FIRMWARE_PATH, FIRMWARE_NEW_VERSION_HASH_PATH) == error_ota_none){//verifica integridade
						f_unlink(FIRMWARE_NEW_VERSION_HASH_PATH);
						UARTPutString("Restarting to Bootloader...",27);
						UARTPutString("\n\r>>", 4);
						//MX_GPIO_Deinit(); //Puts GPIOs in default state
						SysTick->CTRL = 0x0; //Disables SysTick timer and its related interrupt
						HAL_DeInit();

						RCC->CIR = 0x00000000; //Disable all interrupts related to clock
						__set_MSP(*((volatile uint32_t*) BOOTLOADER_START_ADDRESS)); //Set the MSP

						__DMB(); //ARM says to use a DMB instruction before relocating VTOR */
						SCB->VTOR = BOOTLOADER_START_ADDRESS; //We relocate vector table to the sector 1
						 __DSB(); //ARM says to use a DSB instruction just after relocating VTOR */
						/* We are now ready to jump to the main firmware */
						uint32_t JumpAddress = *((volatile uint32_t*) (BOOTLOADER_START_ADDRESS + 4));
						void (*reset_handler)(void) = (void*)JumpAddress;
						reset_handler(); //We start the execution from he Reset_Handler of the main firmware
					}
					else{
						UARTPutString("Atualization fail!",18);
						UARTPutString("\n\r>>", 4);
						f_unlink(FIRMWARE_PATH);
						f_unlink(FIRMWARE_NEW_VERSION_HASH_PATH);
					}
				}
				else{
					f_unlink(FIRMWARE_PATH);
					UARTPutString("Hash download fail!", 19);
					UARTPutString("\n\r>>", 4);
				}
			}
			else{
				UARTPutString("Firmware download fail!", 23);
				UARTPutString("\n\r>>", 4);
			}
		}
		//Remove files from SD card
		f_unlink(FIRMWARE_NEW_VERSION_PATH);
		xSemaphoreGive(sem_connected);
		vTaskDelay(10000);
	}
}




