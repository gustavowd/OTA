#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#include "mbedtls/memory_buffer_alloc.h"

#include "main.h"
#include "cmsis_os.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"

#include <string.h>
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_gpio.h"
//#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "tls_client_utils.h"

#include "driver_uart.h"
#include "fatfs.h"
#include "sha256.h"

#define AUTH_SERVER "192.168.0.121"//moodle.pb.utfpr.edu.br
#define AUTH_PORT 443
#define AUTH_SERVER_LOOKUP_RETRIES 5
#define AUTH_USER ""
#define AUTH_PASS ""
#define AUTH_DATA "POSTDATA=dst=&popup=true&username=" AUTH_USER "&password=" AUTH_PASS "\r\n"
#define AUTH_DATA_LEN "65"	// sizeof(AUTH_DATA)
//#define AUTH_REQUEST "POST /login/index.php HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: OMG/rainbows!!!\r\nAccept: */*\r\nContent-Length: " "\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n"

#define AUTH_REQUEST "GET /files/TesteTcc.txt HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
//#define AUTH_REQUEST "GET /files/revisao-de-literatura.tex HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
//#define AUTH_REQUEST "GET /index.html HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"

#define AUTH_REQUEST_BUFFER_SIZE 512
#define BUFFER_SIZE 512
struct tls_client cli;
static uint8_t buf[BUFFER_SIZE];
//static uint8_t buf_HTTP[366];
SemaphoreHandle_t sem_connected = NULL;



/* use static allocation to keep the heap size as low as possible */
#ifdef MBEDTLS_MEMORY_BUFFER_ALLOC_C
uint8_t memory_buf[MAX_MEM_SIZE];
#endif

/* List of trusted root CA certificates
 * Currently this is just GlobalSign as it's the root used by developer.mbed.org
 * If you want to trust more that one root, just concatenate them.
 */
const char SSL_CA_PEM[] =												\
"-----BEGIN CERTIFICATE-----\r\n"										\
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\r\n"	\
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\r\n"	\
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\r\n"	\
"MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\r\n"	\
"YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\r\n"	\
"aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\r\n"	\
"jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\r\n"	\
"xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\r\n"	\
"1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\r\n"	\
"snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\r\n"	\
"U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\r\n"	\
"9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\r\n"	\
"BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\r\n"	\
"AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\r\n"	\
"yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\r\n"	\
"38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\r\n"	\
"AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\r\n"	\
"DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\r\n"	\
"HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\r\n"	\
"-----END CERTIFICATE-----\r\n";

void OTA_Error_Handler(char *msg)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
	UARTPutString(msg,strlen(msg));
  /* USER CODE END Error_Handler_Debug */
}
void sha256sum_teste(const TCHAR* path, unsigned char * saida){//NÃO DEIXAR NENHUM ARQUIVO ABERTO ANTES DE CHAMAR ESSA FUNÇÃO
	mbedtls_sha256_context Contexto;
	FIL Arq;
	FRESULT rec;
	rec=f_open(&Arq, path, FA_READ);
	if(rec!=FR_OK)
		Fat_Error_Handler(rec);
	mbedtls_sha256_init(&Contexto);//inicia
	if(mbedtls_sha256_update_ret(&Contexto,&Arq, f_size(&Arq))==0){//adiciona o arquivo
		if(mbedtls_sha256_starts_ret(&Contexto, 0)==0){ //inicia o calculo
			if(mbedtls_sha256_finish_ret(&Contexto, saida)){//termina e grava o resultado na saida
			}else mbedtls_sha256_free(&Contexto);
		}else mbedtls_sha256_free(&Contexto);
	}else mbedtls_sha256_free(&Contexto);
	f_close(&Arq);
	UARTPutString(saida, sizeof(saida));
	UARTPutString("\n\r\n\r>>",6);

	//mbedtls_sha256_ret(Entrada,sizeof(Entrada), saida,0 );
}
void integridade(unsigned char X, unsigned char Y){

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




int utfpr_auth(void)
{
	int retries, ret = pdFALSE;
	int i=0;

	struct hostent *server_addr = NULL;
	OTA_Error_Handler("Iniciou a função UTFPR_Auth\n\r\n\r>>");
	if(tls_client_init(&cli)) {
		OTA_Error_Handler("Falha na inicializacao do cliente\n\r\n\r>>");
		ret = pdFALSE;
		goto exit;
	}

	if(tls_cert_load(&cli.tls, NULL, SSL_CA_PEM, NULL, NULL)) {
		OTA_Error_Handler("Falha no Certificado\n\r\n\r>>");
		ret = pdFALSE;
		goto deallocate;
	}

	retries = AUTH_SERVER_LOOKUP_RETRIES;

	do {
		server_addr = gethostbyname(AUTH_SERVER);
		if(server_addr == NULL) {
			OTA_Error_Handler("Falha em obter o host\n\r\n\r>>");
			retries--;
			vTaskDelay(2000);
		}
	} while(server_addr == NULL && retries);

	if(!retries) {
		OTA_Error_Handler("Maximo alcancadas\n\r\n\r>>");
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
		OTA_Error_Handler("Falha na conexao TLS\n\r\n\r>>");
		ret = pdFALSE;
		goto deallocate;

	}

	int len = strlen((char *) AUTH_REQUEST);
	if(http_send_request(&cli.con, AUTH_REQUEST, len) < 0) {
		OTA_Error_Handler("Falha na requisicao\n\r\n\r>>");
		ret = pdFALSE;
		goto deallocate;
	}
	/*
	http_get_response(&cli.con, buf, sizeof(buf));
	//read_buffer=buf;
	UARTPutString(buf,sizeof(buf));
	UARTPutString("\n\r\n\r>>",6);
	http_get_response(&cli.con, buf, sizeof(buf));
	UARTPutString(buf,sizeof(buf));
	UARTPutString("\n\r\n\r>>Encerrou a transmicao\n\r\n\r>>",33);
	*/
	//FATFS fatfs;
	//FIL file;
	//uint8_t read_buffer[512];
	//BYTE work[512];

	FRESULT res=FR_OK;
	FIL Arquivo;
	FIL Arquivo1,Arquivo2;
	//uint32_t file_size = f_size(&Arquivo);
	uint32_t BW;
	UARTPutString("Tentando gravar dados\n\r\n\r>>",27);
	//res = f_mount(&fatfs, SDPath, 1);
	//http_get_response(&cli.con, buf_HTTP, sizeof(buf_HTTP));
	//UARTPutString(buf_HTTP,sizeof(buf_HTTP));
	//UARTPutString("\n\r\n\r>>",6);

	/*
	 * TODO
	 * 	Falta tirar o cabeçario HTTP do arquivo.
	 */

	if(res==FR_OK){
		res=f_open(&Arquivo, "novo.txt", FA_CREATE_ALWAYS | FA_WRITE);//ABRE ARQUIVO
		if(res==FR_OK){
			UARTPutString("iniciou a transmissao\n\r\n\r>>",27);
			while(http_get_response(&cli.con, buf, sizeof(buf))) {//OBTEM UM OS DADOS
				if(buf[BUFFER_SIZE-1]!=0){
					res=f_write(&Arquivo, buf,sizeof(buf)-1,(void *)&BW);//GRAVA NO ARQUIVO
					if(res!=FR_OK)
						Fat_Error_Handler(res);
				}
				else{

					while(buf[i]!='\0'){// DEVE HAVER UMA FORMA MAIS EFICIENTE DE SE FAZER ISSO.
						f_write(&Arquivo, &buf[i],1,(void *)&BW);
						i++;
					}
				}
			}
			res=f_close(&Arquivo);
			if(res!=FR_OK)
				Fat_Error_Handler(res);
			else UARTPutString("Fechou arquivo\n\r\n\r>>",20);
		}
		else Fat_Error_Handler(res);
	}
	else Fat_Error_Handler(res);

	UARTPutString("Encerrou a transmissao\n\r\n\r>>",28);

	unsigned char x[32],y[32];
	//f_open(&Arquivo1, "TesteTcc.txt", FA_READ);
	sha256sum_teste("TesteTcc.txt",&x);
	sha256sum_teste("novo.txt",&y);
	int flag=0;
	for(i=0;i<32;i++){// só testando depois melhoro
		if(x[i]!=y[i])
			flag=1;
	}
	if(flag==0){
		UARTPutString("Os arquivos sao identicos\n\r\n\r>>",31);
	}
	else	UARTPutString("sao diferentes\n\r\n\r>>",20);

	//sha256sum_teste("Copia.txt");



	//f_close(&Arquivo1);



deallocate:
	tls_connection_free(&cli.con);
	tls_context_free(&cli.tls);
exit:
	return ret;
}

volatile int reconnection_trigger = 0;
void SSL_Client(void *argument)
{
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
  UARTPutString("Tarefa TLS de login iniciou!\n\r\n\r>>",34);

  printf_install_putchar(UARTPutChar);

  /*
  int abc = 10;
  printf_lib("Teste de printf %d", abc);

  char data;
  while(1){
	  UARTGetChar(&data);
	  UARTPutChar(data);
	  if (data == 'q') break;
  }
  */

  xSemaphoreTake(sem_connected, portMAX_DELAY);
  utfpr_auth();

  while(1){
	  vTaskDelay(10000);
  }
}





