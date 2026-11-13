/**
 * @file app_tcp.c
 * @brief TCP echo server on port 4242.
 *
 * Runs in its own thread so main() keeps the LED chase; one client at a time.
 */
#include "app_tcp.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(app_tcp, LOG_LEVEL_INF);

#define APP_TCP_PORT          4242
#define TCP_SERVER_STACK_SIZE 2048
#define TCP_SERVER_PRIORITY   8

K_THREAD_STACK_DEFINE(tcp_server_stack, TCP_SERVER_STACK_SIZE);

static struct k_thread tcp_server_thread_data;
static K_SEM_DEFINE(net_up_sem, 0, 1);
static struct net_mgmt_event_callback if_cb;

/**
 * @brief Give net_up_sem when the interface becomes operational.
 *
 * Runs in the net_mgmt thread: signal and return, never block here.
 */
static void if_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			     struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_IF_UP:
		k_sem_give(&net_up_sem);
		break;
	default:
		break;
	}
}

/**
 * @brief Create a TCP socket bound to APP_TCP_PORT on every local address and listen on it.
 * @return Listening socket descriptor, or negative errno on failure.
 */
static int open_listener(void)
{
	struct net_sockaddr addr = {0};
	struct net_sockaddr_in *sin = net_sin(&addr);
	int sock;
	int err;

	sin->sin_family = NET_AF_INET;
	sin->sin_port = net_htons(APP_TCP_PORT);
	/* sin_addr stays 0 = INADDR_ANY: accept on whatever address app_net set */

	sock = zsock_socket(NET_AF_INET, NET_SOCK_STREAM, NET_IPPROTO_TCP);
	if (sock < 0) {
		err = errno;
		LOG_ERR("socket: %d", err);
		return -err;
	}

	if (zsock_bind(sock, &addr, sizeof(addr)) < 0) {
		/* Saved first: the close below may overwrite errno */
		err = errno;
		LOG_ERR("bind: %d", err);
		(void)zsock_close(sock);
		return -err;
	}

	/* Backlog 1: one client is served at a time */
	if (zsock_listen(sock, 1) < 0) {
		err = errno;
		LOG_ERR("listen: %d", err);
		(void)zsock_close(sock);
		return -err;
	}

	return sock;
}

/**
 * @brief Echo everything received from a client until it closes or an error occurs.
 * @param client Connected socket; the caller closes it.
 */
static void serve_client(int client)
{
	/* Static: off the thread stack; only this thread uses it */
	static uint8_t buf[256];

	for (;;) {
		ssize_t len = zsock_recv(client, buf, sizeof(buf), 0);

		if (len <= 0) {
			if (len == 0) {
				LOG_INF("Client closed");
			} else {
				LOG_ERR("recv: %d", errno);
			}
			return;
		}

		/* send() may queue fewer bytes than asked: loop until all are queued */
		size_t off = 0U;

		while (off < (size_t)len) {
			ssize_t sent = zsock_send(client, &buf[off], (size_t)len - off, 0);

			if (sent < 0) {
				LOG_ERR("send: %d", errno);
				return;
			}
			off += (size_t)sent;
		}
	}
}

/**
 * @brief Wait for the network, then serve clients on APP_TCP_PORT.
 * @param p1 Unused.
 * @param p2 Unused.
 * @param p3 Unused.
 */
static void tcp_server_thread(void *p1, void *p2, void *p3)
{
	int listener;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Waiting for network interface to come up...");
	(void)k_sem_take(&net_up_sem, K_FOREVER);
	LOG_INF("Network interface is up. Starting TCP server on port %d.", APP_TCP_PORT);

	listener = open_listener();
	if (listener < 0) {
		LOG_ERR("TCP server not started: %d", listener);
		return;
	}

	for (;;) {
		int client = zsock_accept(listener, NULL, NULL);

		if (client < 0) {
			LOG_ERR("accept: %d", errno);
			/* Keep a persistent error from spinning */
			(void)k_sleep(K_SECONDS(1));
			continue;
		}

		LOG_INF("Client connected");
		serve_client(client);
		(void)zsock_close(client);
	}
}

int app_tcp_start(void)
{
	net_mgmt_init_event_callback(&if_cb, if_event_handler, NET_EVENT_IF_UP);
	net_mgmt_add_event_callback(&if_cb);

	/* IF_UP may have fired before the callback was registered */
	if (net_if_is_up(net_if_get_default())) {
		k_sem_give(&net_up_sem);
	}

	(void)k_thread_create(&tcp_server_thread_data, tcp_server_stack,
			      K_THREAD_STACK_SIZEOF(tcp_server_stack), tcp_server_thread, NULL,
			      NULL, NULL, TCP_SERVER_PRIORITY, 0, K_NO_WAIT);
	(void)k_thread_name_set(&tcp_server_thread_data, "tcp");

	return 0;
}
