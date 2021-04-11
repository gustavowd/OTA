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

/* use static allocation to keep the heap size as low as possible */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
uint8_t memory_buf[MAX_MEM_SIZE];
#endif

volatile int reconnection_trigger = 0;
/* Private functions - prototypes ------------------------------------------- */
// Functions that will be used just internally, in this library
uint32_t uint8_t2uint32_t(uint8_t * vector);
void uint32_t2uint8_t(uint8_t * vector, uint32_t data);
error_ota_t read_file_info(uint32_t * info, const TCHAR *path);
error_ota_t write_file_info(uint32_t info, const TCHAR* path);
/* Private functions - implementation --------------------------------------- */
// Functions that will be used just internally, in this library
void OTA_Error_Handler(char *msg)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
	UARTPutString(msg,strlen(msg));
  /* USER CODE END Error_Handler_Debug */
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

error_ota_t read_file_info(uint32_t * info, const TCHAR *path){
	//variables
	FRESULT error_fat;
	error_ota_t error_control = error_ota_none;
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
		error_control = error_ota_general;
		* info = 0;
	}
	return (error_control);
}

error_ota_t write_file_info(uint32_t info, const TCHAR* path){
	//variables
	FRESULT error_fat;
	error_ota_t error_control = error_ota_none;
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
		error_control = error_ota_general;
	}
	return (error_control);
}

void Get_Hash(const TCHAR* path, char * Saida){
	FRESULT res;
	FIL Arq;
	int BR=0;


	res=f_open(&Arq, path, FA_CREATE_ALWAYS | FA_WRITE);
	if(res!=FR_OK)
		Fat_Error_Handler(res);
	res=f_write(&Arq,&Saida,32,&BR);
	if(res!=FR_OK)
			Fat_Error_Handler(res);
	res=f_close(&Arq);
	if(res!=FR_OK)
			Fat_Error_Handler(res);
}
void sha256sum(const TCHAR* path, unsigned char* saida){
	mbedtls_sha256_context Contexto;
	uint8_t buffer[512];
	FIL Arq;
	FRESULT rec;
	int BW,lido=0;
	rec=f_open(&Arq, path, FA_READ);//abre o arquivo pra leitura
	if(rec!=FR_OK)
		Fat_Error_Handler(rec);
	mbedtls_sha256_init(&Contexto);//inicia o contexto
	mbedtls_sha256_starts_ret(&Contexto, 0);//inicia o sha256
	while (lido <f_size(&Arq)){
		rec=f_read(&Arq, buffer,512,&BW);//adiciona ao buffer 512 bits do arquivo
		if(rec!=FR_OK)
				Fat_Error_Handler(rec);
		mbedtls_sha256_update_ret(&Contexto,buffer,BW); //adiciona o buffer ao sha256
		lido+=BW;
	}

	mbedtls_sha256_finish_ret(&Contexto, saida);//grava em saida o hash
	f_close(&Arq);
	mbedtls_sha256_free(&Contexto);
	UARTPutString("SHA-256 Gerado: ",4);
	UARTPutString(saida, 32);
	UARTPutString("\n\r>>",4);
}

error_ota_t Integrity(const TCHAR* Firmware_path, const TCHAR* Versao_path){
	//Variables
	error_ota_t error_control = error_ota_none;
	unsigned char Saida_Firmware[32], Saida_Versao[32];
	FIL Versao;
	unsigned int i, BW;

	//Getting SHA-256 from firmware
	sha256sum(Firmware_path, Saida_Firmware);//obtem o hash do firmware

	//Getting SHA-256 from file
	f_open(&Versao, Versao_path, FA_READ);
	f_read(&Versao, Saida_Versao, 32, &BW);//Obtem o hash do firmware do arquivo do servidor
	f_close(&Versao);

	//Comparing hash
	for(i = 0; i < 32; i++){// só testando depois melhoro
		if(Saida_Firmware[i] != Saida_Versao[i]){
			error_control = error_ota_general;
		}
	}
#ifdef DEBUG_MODE
	UARTPutString("Hash de A = ",12);
	UARTPutString(Saida_Firmware, 32);
	UARTPutString("\n\r>>",4);
	UARTPutString("Hash de B = ",12);
	UARTPutString(Saida_Versao, 32);
	UARTPutString("\n\r>>",4);
#endif

	//error control
	if(error_control == error_ota_general){
		UARTPutString("Falha no teste de integridade\n\r>>", 33);
	}
	else{
		UARTPutString("Sucesso no teste de integridade\n\r>>", 35);
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

int http_get_response(struct tls_connection *con, char *resp, size_t resp_size)
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




int Get_File(char *Request, const TCHAR* Path)
{
	int retries, ret = pdFALSE;
	int i=0;
	UARTPutString("Obtendo versão de novo firmware\n\r>>",35);
	struct hostent *server_addr = NULL;
	if(tls_client_init(&cli)) {
		OTA_Error_Handler("Falha na inicializacao do cliente\n\r>>");
		ret = pdFALSE;
		goto exit;
	}
	if(tls_cert_load(&cli.tls, NULL, SSL_CA_PEM, NULL, NULL)) {
		OTA_Error_Handler("Falha no Certificado\n\r>>");
		ret = pdFALSE;
		goto deallocate;
	}
	retries = AUTH_SERVER_LOOKUP_RETRIES;
	do {
		server_addr = gethostbyname(AUTH_SERVER);
		if(server_addr == NULL) {
			OTA_Error_Handler("Falha em obter o host\n\r>>");
			retries--;
			vTaskDelay(2000);
		}
	} while(server_addr == NULL && retries);
	if(!retries) {
		OTA_Error_Handler("Maximo alcancadas\n\r>>");
		ret = pdTRUE;
		goto deallocate;
	}
   char server_ip_s[16];
   unsigned long server_ip = *((unsigned long*) server_addr->h_addr);
   // Convert the IP Address into a string.
   sprintf(server_ip_s, "%d.%d.%d.%d", (int)(server_ip & 0xff), (int)((server_ip >> 8) & 0xff),
		  (int)((server_ip >> 16) & 0xff), (int)((server_ip >> 24) & 0xff));
   ret = tls_client_connect(&cli, server_ip_s, AUTH_PORT);
	if(ret) {
		OTA_Error_Handler("Falha na conexao TLS\n\r>>");
		ret = pdFALSE;
		goto deallocate;
	}
	//UARTPutString("Conexão Estabelecida\n\r>>",24);

	int len = strlen((char *) Request);
	if(http_send_request(&cli.con, Request, len) < 0) {
		OTA_Error_Handler("Falha na requisicao\n\r>>");
		ret = pdFALSE;
		goto deallocate;
	}
	//UARTPutString("Requisição feita\n\r>>",20);

	FRESULT res;
	FIL Arquivo;
	uint32_t BW=0;
	int j;
	//UARTPutString("Gravando dados\n\r>>",sizeof("Gravando dados\n\r>>"));

	res=f_open(&Arquivo, Path, FA_CREATE_ALWAYS | FA_WRITE);//ABRE ARQUIVO
	if(res==FR_OK){
		//UARTPutString("iniciou a transmissao\n\r>>",sizeof("iniciou a transmissao\n\r>>"));
		while(http_get_response(&cli.con, buf, sizeof(buf))) {//OBTEM UM OS DADOS
			/*-----TIRANDO CABEÇALHO HTTP------*/
			if(BW==0){//BW só vai ser 0 quando for a primeira vez q ele entrar aqui, ou quando houver alguma falha
				for (i=0;
				!((buf[i]=='\r')&&(buf[i+1]=='\n')&&(buf[i+2]=='\r')&&(buf[i+3]=='\n'));
				i++);
				i+=4;
				for (j=0;(j+i)<BUFFER_SIZE;j++){
						buf[j]=buf[j+i];
					}
				if(buf[BUFFER_SIZE-1]!='\0'){//Verifica se o arquivo é menor que o buffer
					res=f_write(&Arquivo, buf,BUFFER_SIZE-(i),(void*) &BW);
					//res=f_write(&Arquivo, buf,sizeof(buf),&BW);//GRAVA NO ARQUIVO
					if(res!=FR_OK)
						Fat_Error_Handler(res);
				}
				else{
					for (j=0;buf[j]!='\0';j++);
					f_write(&Arquivo, buf,j,(void*) &BW);
				}
			}
			/*-----FIM TIRANDO CABEÇALHO HTTP------*/
			else{
				if(buf[BUFFER_SIZE-1]!='\0'){
					res=f_write(&Arquivo, buf,sizeof(buf),&BW);//GRAVA NO ARQUIVO
					if(res!=FR_OK)
						Fat_Error_Handler(res);
				}
				else{
					for (j=0;buf[j]!='\0';j++);//achar uma função que acha o fim do arquivo pra isso ficar mais rapido
					f_write(&Arquivo, buf,j,&BW);
				}
			}
		}
		res=f_close(&Arquivo);
		if(res!=FR_OK)
			Fat_Error_Handler(res);
	}
	else Fat_Error_Handler(res);

	UARTPutString("Encerrou a Get_File\n\r>>",23);
	//integridade("C1.TXT","V_D.TXT");

deallocate:
	tls_connection_free(&cli.con);
	tls_context_free(&cli.tls);
exit:
	return ret;
}

/* Public functions --------------------------------------------------------- */
// Implementation of functions that are available to the upper layer

void OTA(void *argument){
	//Variables
	uint32_t current_version;
	uint32_t new_version;
	uint8_t hash_arquivo[32];
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
	UARTPutString("OTA Server iniciou!\n\r>>", 23);

	printf_install_putchar(UARTPutChar);//isso é o driver uart instalado no freertos?
	while(1){
		xSemaphoreTake(sem_connected, portMAX_DELAY);

		//Get the current version of the firmware from file
		read_file_info(&current_version, FIRMWARE_CURRENT_VERSION_PATH);
		//Get the new firmware version file from server
		UARTPutString("Downloading new firmware version from server...", 47);
		UARTPutString(" \n\r>>", 4);
		Get_File(AUTH_REQUEST_VERSION, FIRMWARE_NEW_VERSION_PATH);
		//Get the new firmware version from file
		read_file_info(&new_version, FIRMWARE_NEW_VERSION_PATH);

		//show current version on uart
		UARTPutString("Current firmware version: ", 26);
		UARTPutString(current_version + 48, 3);
		UARTPutString(" \n\r>>", 4);
		//show current version on uart
		UARTPutString("new firmware version: ", 22);
		UARTPutString(new_version + 48, 3);
		UARTPutString(" \n\r>>", 4);

		if(new_version > current_version){//compara versão atual e a do servidor

			UARTPutString("Downloading new firmware...", 27);
			UARTPutString(" \n\r>>", 4);
			Get_File(AUTH_REQUEST_FIRMWARE, FIRMWARE_PATH);//obtem o firmware

			//UARTPutString("Generating SHA-256...", 21);
			//UARTPutString(" \n\r>>", 4);
			//sha256sum(FIRMWARE_PATH, &hash_arquivo);//teste

			UARTPutString("Downloading SHA-256 file...", 27);
			UARTPutString(" \n\r>>", 4);
			Get_File(AUTH_REQUEST_HASH,FIRMWARE_NEW_VERSION_HASH_PATH);//obtem arquivo contendo o hash

			UARTPutString("Checking integrity of firmware on SD card...", 44);
			UARTPutString(" \n\r>>", 4);
			if(Integrity(FIRMWARE_PATH, FIRMWARE_NEW_VERSION_HASH_PATH)==0){//verifica integridade
				UARTPutString("Restarting to Bootloader...",27);
				UARTPutString(" \n\r>>", 4);
				Reset_Handler(); //Pode ser que isso faça o programa só reiniciar, não ir ao bootloader
			}
		}
		xSemaphoreGive(sem_connected); //ler pra ter certeza

		vTaskDelay(10000);
	}
}




