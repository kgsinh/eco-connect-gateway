/**
 * @file app_net.c
 * @brief IPv4 bring-up: static address for a direct PC link, DHCPv4 for a router.
 *
 * The Ethernet driver is started by Zephyr from the devicetree. Ping replies
 * come from the IPv4 stack.
 */
#include "app_net.h"
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>

LOG_MODULE_REGISTER(app_net, LOG_LEVEL_INF);

/**
 * @brief Log the DHCP address, netmask, gateway and lease once a lease is bound.
 * @param cb Registered callback (unused).
 * @param mgmt_event Event that fired; only NET_EVENT_IPV4_ADDR_ADD is handled.
 * @param iface Interface the address was added to.
 */
static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
			       uint64_t mgmt_event, struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		const struct net_if_addr_ipv4 *addr = &iface->config.ip.ipv4->unicast[i];

		if (addr->ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("IP address: %s",
			net_addr_ntop(NET_AF_INET, &addr->ipv4.address.in_addr, buf, sizeof(buf)));
		LOG_INF("Netmask:    %s",
			net_addr_ntop(NET_AF_INET, &addr->netmask, buf, sizeof(buf)));
		LOG_INF("Gateway:    %s",
			net_addr_ntop(NET_AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
		LOG_INF("Lease:      %u s", iface->config.dhcpv4.lease_time);
	}
}

/**
 * @brief Add APP_NET_STATIC_IPV4_ADDR/_NETMASK to the interface.
 * @param iface Interface to configure.
 * @retval 0 Address added, or none configured (empty Kconfig string).
 * @retval -EINVAL Address or netmask string does not parse.
 * @retval -ENOMEM No free unicast address slot.
 */
static int add_static_ipv4(struct net_if *iface)
{
	struct net_in_addr addr;
	struct net_in_addr netmask;

	if (CONFIG_APP_NET_STATIC_IPV4_ADDR[0] == '\0') {
		return 0;
	}

	if ((net_addr_pton(NET_AF_INET, CONFIG_APP_NET_STATIC_IPV4_ADDR, &addr) < 0) ||
	    (net_addr_pton(NET_AF_INET, CONFIG_APP_NET_STATIC_IPV4_NETMASK, &netmask) < 0)) {
		LOG_ERR("Invalid static IPv4 address or netmask in Kconfig");
		return -EINVAL;
	}

	if (net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0U) == NULL) {
		LOG_ERR("Could not add static IPv4 address");
		return -ENOMEM;
	}

	(void)net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);
	LOG_INF("Static IP:  %s/%s", CONFIG_APP_NET_STATIC_IPV4_ADDR,
		CONFIG_APP_NET_STATIC_IPV4_NETMASK);

	return 0;
}

int app_net_start(void)
{
	/* net_mgmt keeps a pointer to the callback after registration */
	static struct net_mgmt_event_callback ipv4_cb;
	struct net_if *iface = net_if_get_default();

	if (iface == NULL) {
		LOG_ERR("No network interface");
		return -ENODEV;
	}

	(void)add_static_ipv4(iface);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);

	LOG_INF("Starting DHCPv4 on %s", net_if_get_device(iface)->name);
	net_dhcpv4_start(iface);

	return 0;
}
