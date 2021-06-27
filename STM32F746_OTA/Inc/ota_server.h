/*
 * 		ota_server.h
 *
 *  	Created on: 5/05/2021
 *      Author: Gustavo Correa
 */

#ifndef OTA_SERVER_H
#define OTA_SERVER_H

/* Includes ----------------------------------------------------------------- */
// All dependencies

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
/* Public definitions ------------------------------------------------------- */
// Constants and macros

#define FIRMWARE_VERSION_ADDRESS 0x080BFFFC
#define get_flash_info(x) (*((const uint32_t *)(x)))

#define FIRMWARE_PATH 					"Firmware.bin"
#define FIRMWARE_NEW_VERSION_PATH 		"Nova_versao.bin"
#define FIRMWARE_NEW_VERSION_HASH_PATH 	"Hash.bin"


#define AUTH_SERVER 					"192.168.0.122"
#define AUTH_PORT 						443
#define AUTH_SERVER_LOOKUP_RETRIES 		5

#define AUTH_REQUEST_VERSION 	"GET /files/" FIRMWARE_NEW_VERSION_PATH " HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
#define AUTH_REQUEST_FIRMWARE 	"GET /files/" FIRMWARE_PATH " HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
#define AUTH_REQUEST_HASH 		"GET /files/" FIRMWARE_NEW_VERSION_HASH_PATH " HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"

#define AUTH_REQUEST_BUFFER_SIZE 		512
#define BUFFER_SIZE 					512
#define VERSION 						1
#define BOOTLOADER_START_ADDRESS		0x8000000
//#define DEBUG_MODE_OTA


/* Public types ------------------------------------------------------------- */
// Unions, structs, enumerations and other type definitions
typedef enum{
	error_ota_none,
	error_ota_general

}error_ota_t;
/* Public variables --------------------------------------------------------- */
// Variables available to upper layers (strongly recommend never using this)

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


/* Public functions --------------------------------------------------------- */
// Prototypes of functions that are available to the upper layer

void OTA(void *argument);


#endif /* OTA_SERVER_H */
