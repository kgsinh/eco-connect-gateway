/**
 * @file app_http.c
 * @brief Static web page on HTTP port 80.
 *
 * The server runs in Zephyr's own thread (subsys/net/lib/http); this file only
 * describes the site. Its resource table needs the linker section in
 * http_sections.ld.
 */
#include "app_http.h"
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>

static const char index_html[] = "<!DOCTYPE html>\n"
				 "<html><head><title>Eco-Connect Gateway</title></head>\n"
				 "<body><h1>Eco-Connect Gateway</h1>\n"
				 "<p>Served by Zephyr over the ENC28J60.</p></body></html>\n";

/* Not const: HTTP_RESOURCE_DEFINE casts it to void * (MISRA 11.8); the server only reads it */
static struct http_resource_detail_static index_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "text/html",
		},
	.static_data = index_html,
	/* sizeof includes the string's terminating NUL, which is not page content */
	.static_data_len = sizeof(index_html) - 1U,
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
