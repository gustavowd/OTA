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

#define AUTH_SERVER "intranet.pb.utfpr.edu.br"
#define AUTH_PORT 443
#define AUTH_SERVER_LOOKUP_RETRIES 5
#define AUTH_USER ""
#define AUTH_PASS ""
#define AUTH_DATA "POSTDATA=dst=&popup=true&username=" AUTH_USER "&password=" AUTH_PASS "\r\n"
#define AUTH_DATA_LEN "65"	// sizeof(AUTH_DATA)
#define AUTH_REQUEST "POST /login HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: OMG/rainbows!!!\r\nAccept: */*\r\nContent-Length: " AUTH_DATA_LEN "\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n" AUTH_DATA "\r\n"
#define AUTH_REQUEST_BUFFER_SIZE 512

struct tls_client cli;
static uint8_t buf[1024];
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

	struct hostent *server_addr = NULL;

	if(tls_client_init(&cli)) {
		ret = pdFALSE;
		goto exit;
	}

	if(tls_cert_load(&cli.tls, NULL, SSL_CA_PEM, NULL, NULL)) {
		ret = pdFALSE;
		goto deallocate;
	}

	retries = AUTH_SERVER_LOOKUP_RETRIES;

	do {
		server_addr = gethostbyname(AUTH_SERVER);
		if(server_addr == NULL) {
			retries--;
			vTaskDelay(2000);
		}
	} while(server_addr == NULL && retries);

	if(!retries) {
		ret = pdTRUE;
		goto deallocate;
	}

	snprintf(buf, sizeof(buf), "%hhu.%hhu.%hhu.%hhu", server_addr->h_addr[0], server_addr->h_addr[1], server_addr->h_addr[2], server_addr->h_addr[3]);

	if(tls_client_connect(&cli, buf, AUTH_PORT)) {
		ret = pdFALSE;
		goto deallocate;
	}

	if(http_send_request(&cli.con, AUTH_REQUEST, sizeof(AUTH_REQUEST)) < 0) {
		ret = pdFALSE;
		goto deallocate;
	}

	while(http_get_response(&cli.con, buf, sizeof(buf))) {
		if(strstr(buf, "\n\r\n\r  Voc� est� logado!\n\r")) {
			ret = pdTRUE;
			break;
		}
	}

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
