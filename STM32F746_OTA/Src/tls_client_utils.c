/*
 * tls_client_utils.c
 *
 *  Created on: 12 de abr de 2018
 *      Author: Matheus K. Ferst
 */

#include "tls_client_utils.h"

int tls_client_init(struct tls_client *cli)
{
	int ret = 0;

	ret |= tls_context_init(&cli->tls);
	ret |= tls_connection_init(&cli->con);
	ret |= mbedtls_ssl_config_defaults(&cli->tls.cfg, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);

	mbedtls_ssl_conf_authmode(&cli->tls.cfg, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_rng(&cli->tls.cfg, mbedtls_ctr_drbg_random, &cli->tls.drbg);

	return ret;
}

int tls_client_connect(struct tls_client *cli, const char *addr, int port)
{
	int ret = 0;
	char str_port[7];

	snprintf(str_port, 7, "%d", port);

	ret |= mbedtls_ssl_set_hostname(&cli->con.ctx, addr);
	ret |= mbedtls_net_connect(&cli->con.sock, addr, str_port, MBEDTLS_NET_PROTO_TCP);
	ret |= mbedtls_ssl_setup(&cli->con.ctx, &cli->tls.cfg);

	mbedtls_ssl_set_bio(&cli->con.ctx, &cli->con.sock, mbedtls_net_send, mbedtls_net_recv, NULL);
	ret |= mbedtls_ssl_handshake(&cli->con.ctx);

	return ret;
}

void tls_client_free(struct tls_client *cli)
{
	mbedtls_ssl_close_notify(&cli->con.ctx);
	tls_context_free(&cli->tls);
	tls_connection_free(&cli->con);
}
