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

/* Private functions - implementation --------------------------------------- */
// Functions that will be used just internally, in this library
void OTA_Error_Handler(char *msg)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
	UARTPutString(msg,strlen(msg));
  /* USER CODE END Error_Handler_Debug */
}
void Get_Version(const TCHAR* path, char * Saida){
	FRESULT res;
	FIL Arq;
	int BR=0;
	res=f_open(&Arq, "Versao.txt",FA_READ);
	if(res!=FR_OK)
		Fat_Error_Handler(res);
	res=f_read(&Arq,&Saida,1,&BR);
	if(res!=FR_OK)
		Fat_Error_Handler(res);
	res=f_close(&Arq);
	if(res!=FR_OK)
		Fat_Error_Handler(res);
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
	/*
	if(mbedtls_sha256_update_ret(&Contexto,&Arq, f_size(&Arq))==0){//adiciona o arquivo
		if(mbedtls_sha256_starts_ret(&Contexto, 0)==0){ //inicia o calculo
			if(mbedtls_sha256_finish_ret(&Contexto, saida)==0){//termina e grava o resultado na saida
			}else {mbedtls_sha256_free(&Contexto);UARTPutString("Erro finish\n\r",13);}
		}else {mbedtls_sha256_free(&Contexto);UARTPutString("Erro start\n\r",12);}
	}else {mbedtls_sha256_free(&Contexto);UARTPutString("Erro update\n\r",13);}*/
	f_close(&Arq);
	mbedtls_sha256_free(&Contexto);
	UARTPutString(saida, 32);
	UARTPutString("\n\r>>",4);
	//mbedtls_sha256_ret(Entrada,sizeof(Entrada), saida,0 );
}
int Integridade(const TCHAR* Firmware_path, const TCHAR* Versao_path){
	unsigned char Saida_Firmware[32],Saida_Versao[32];
	FIL Versao;
	int i, BW;
	sha256sum(Firmware_path,Saida_Firmware);//obtem o hash do firmware
	//sha256sum_teste(Versao_path,Saida_Versao);

	f_open(&Versao, Versao_path, FA_READ);
	f_read(&Versao, Saida_Versao, 32,(void *) &BW);//Obtem o hash do firmware do arquivo do servidor
	f_close(&Versao);
	//int flag=0;
	for(i=0;i<32;i++){// só testando depois melhoro
		if(Saida_Firmware[i]!=Saida_Versao[i]){
			UARTPutString("Hash de A = ",12);
			UARTPutString(Saida_Firmware, 32);
			UARTPutString("\n\r>>",4);
			UARTPutString("Hash de B = ",12);
			UARTPutString(Saida_Versao, 32);
			UARTPutString("\n\r>>",4);
			UARTPutString("Falha no teste de integridade\n\r>>",33);
			return(1);
		}
	}
	UARTPutString("Hash de A = ",12);
	UARTPutString(Saida_Firmware, 32);
	UARTPutString("\n\r>>",4);
	UARTPutString("Hash de B = ",12);
	UARTPutString(Saida_Versao, 32);
	UARTPutString("\n\r>>",4);
	UARTPutString("Sucesso no teste de integridade\n\r>>",35);
	return(0);
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
	UARTPutString("Get_File iniciada\n\r>>",21);
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

void OTA(void *argument)
{
	char Versao;
	//FIL Arq;
	//int res;
	//int BR;
	char hash_arquivo[32];
  /*
   * 0. Initialize the RNG and the session data
   */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
  mbedtls_memory_buffer_alloc_init(memory_buf, sizeof(memory_buf));
#endif

  // Não precisa essa linha pq configura no vnc player
  //mbedtls_net_init(NULL);
  sem_connected = xSemaphoreCreateBinary();


  InitUART();
  // Limpa a tela do terminal
  UARTPutString("\033[2J\033[H",0);

  // Imprime uma tela de boas-vindas
  UARTPutString("OTA Server iniciou!\n\r>>",23);

  printf_install_putchar(UARTPutChar);//isso é o driver uart instalado no freertos?

  xSemaphoreTake(sem_connected, portMAX_DELAY);
  //Obtem arquivo com a versão e hash
  Get_File(AUTH_REQUEST_VERSION,"Versao.txt");
  //UARTPutString("Saiu get file\n\r>>",17);
  //vTaskDelay(1000);

  Get_Version("Versao.txt",&Versao);// obtem arquivo de versão no servidor
  UARTPutString("Versão = ",9);
  UARTPutString(Versao+48,1);
  UARTPutString(" \n\r>>",4);
  if(Versao>VERSION){//compara versão atual e a do servidor
	  Get_File(AUTH_REQUEST_FIRMWARE,"Gustavo.pdf");//obtem o firmware
	  sha256sum("Gustavo.pdf", &hash_arquivo);//teste
	  Get_Hash("Hash.txt",hash_arquivo);//teste
	  //Get_File(AUTH_REQUEST_HASH,"Hash.txt");//obtem arquivo contendo o hash
	  if(Integridade("Gustavo.pdf", "Hash.txt")==0){//verifica integridade
		  UARTPutString("Iniciar BOOTLOADER!!!\n\r>>",25);
	  }
	  //UARTPutString(&hash_arquivo,32);
  }
  //res=integridade(Download(), Get_Hash());


  //xSemaphoreGive(sem_connected); ler pra ter certeza

  while(1){
	  vTaskDelay(10000);
  }
}




