/**
 * @file app_http.c
 * @brief Static web page on HTTP port 80.
 *
 * The server runs in Zephyr's own thread (subsys/net/lib/http); this file only
 * describes the site. Its resource table needs the linker section in
 * http_sections.ld. The page is src/web/index.html, gzipped at build time.
 */
#include "app_http.h"
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

/*
 * Generated from src/web/index.html by CMakeLists.txt (file2hex.py --gzip).
 * The #include inside the initializer is Zephyr's usual way to embed a file
 * (MISRA Rule 20.1 is advisory).
 */
static const uint8_t index_html_gz[] = {
#include "index.html.gz.inc"
};

/* Not const: HTTP_RESOURCE_DEFINE casts it to void * (MISRA 11.8); the server only reads it */
static struct http_resource_detail_static index_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			/* Sent unconditionally: the server doesn't check Accept-Encoding */
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};

/* Passed by pointer: port 0 would request an ephemeral port, written back here */
static uint16_t http_port = 80U;

/* All local addresses, up to MAX_CLIENTS clients, backlog 2, no fallback page (404) */
HTTP_SERVICE_DEFINE(gw_http_service, NULL, &http_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 2, NULL,
		    NULL, NULL);

HTTP_RESOURCE_DEFINE(index_resource, gw_http_service, "/", &index_detail);

int app_http_start(void)
{
	return http_server_start();
}
