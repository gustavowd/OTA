#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#define AUTH_SERVER "192.168.0.121"//moodle.pb.utfpr.edu.br
#define AUTH_PORT 443
#define AUTH_SERVER_LOOKUP_RETRIES 5
//#define AUTH_USER ""
//#define AUTH_PASS ""
//#define AUTH_DATA "POSTDATA=dst=&popup=true&username=" AUTH_USER "&password=" AUTH_PASS "\r\n"
//#define AUTH_DATA_LEN "65"	// sizeof(AUTH_DATA)
//#define AUTH_REQUEST "POST /login/index.php HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: OMG/rainbows!!!\r\nAccept: */*\r\nContent-Length: " "\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n"

#define AUTH_REQUEST_VERSION "GET /files/Version.TXT HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
#define AUTH_REQUEST_FIRMWARE "GET /files/Gustavo.pdf HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"
#define AUTH_REQUEST_HASH "GET /Hash.txt HTTP/1.1\r\nHost: " AUTH_SERVER "\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:74.0) Gecko/20100101 Firefox/74.0\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n"

#define AUTH_REQUEST_BUFFER_SIZE 512
#define BUFFER_SIZE 512
#define VERSION 1

#endif /* OTA_SERVER_H */
