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

/* Network management retains this callback pointer */
static struct net_mgmt_event_callback ipv4_cb;

/**
 * @brief Log the DHCP address, netmask, gateway and lease once a lease is bound.
 * @param cb Registered callback (unused).
 * @param mgmt_event Event that fired; only NET_EVENT_IPV4_ADDR_ADD is handled.
 * @param iface Interface the address was added to.
 */
static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	if (iface->config.ip.ipv4 == NULL) {
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		const struct net_if_addr_ipv4 *addr = &iface->config.ip.ipv4->unicast[i];

		if (addr->ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("DHCP address: %s",
			net_addr_ntop(NET_AF_INET, &addr->ipv4.address.in_addr, buf, sizeof(buf)));
		LOG_INF("Netmask:    %s",
			net_addr_ntop(NET_AF_INET, &addr->netmask, buf, sizeof(buf)));
		LOG_INF("Gateway:    %s",
			net_addr_ntop(NET_AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
		LOG_INF("Lease:      %u s", iface->config.dhcpv4.lease_time);

		break;
	}
}

/**
 * @brief Add APP_NET_STATIC_IPV4_ADDR/_NETMASK to the interface.
 * @param iface Interface to configure.
 * @retval 0 Address and netmask set.
 * @retval -EINVAL Address or netmask string does not parse, or the netmask
 *         could not be set (the address is removed again).
 * @retval -ENOMEM No free unicast address slot.
 */
static int add_static_ipv4(struct net_if *iface)
{
	struct net_in_addr addr;
	struct net_in_addr netmask;

	/* Convert IPv4 address string to binary. */
	if (net_addr_pton(NET_AF_INET, CONFIG_APP_NET_STATIC_IPV4_ADDR, &addr) < 0) {

		LOG_ERR("Invalid static IPv4 address");
		return -EINVAL;
	}

	/* Convert netmask string to binary. */
	if (net_addr_pton(NET_AF_INET, CONFIG_APP_NET_STATIC_IPV4_NETMASK, &netmask) < 0) {

		LOG_ERR("Invalid IPv4 netmask");
		return -EINVAL;
	}

	/* Add the manual IPv4 address. */
	if (net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0U) == NULL) {

		LOG_ERR("Failed to add static IPv4 address");
		return -ENOMEM;
	}

	/* Configure the netmask. */
	if (!net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask)) {

		LOG_ERR("Failed to set IPv4 netmask");

		/* Roll back the previously added address. */
		if (!net_if_ipv4_addr_rm(iface, &addr)) {
			LOG_ERR("Failed to remove static IPv4 address");
		}

		return -EINVAL;
	}

	LOG_INF("Static IPv4 configured");
	LOG_INF("IP: %s", CONFIG_APP_NET_STATIC_IPV4_ADDR);
	LOG_INF("Netmask: %s", CONFIG_APP_NET_STATIC_IPV4_NETMASK);

	return 0;
}

int app_net_start(void)
{
	struct net_if *iface = net_if_get_default();
	int ret;

	if (iface == NULL) {
		LOG_ERR("No network interface found");
		return -ENODEV;
	}

	LOG_INF("Network device: %s", net_if_get_device(iface)->name);

	/* Static IPv4 mode */
	if (IS_ENABLED(CONFIG_APP_NET_STATIC)) {

		LOG_INF("Network mode: STATIC IPv4");

		ret = add_static_ipv4(iface);

		if (ret < 0) {
			LOG_ERR("Static IPv4 setup failed: %d", ret);
			return ret;
		}

		return 0;
	}

	/* DHCPv4 mode */
	if (IS_ENABLED(CONFIG_APP_NET_DHCP)) {

		LOG_INF("Network mode: DHCPv4");

		net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);

		net_mgmt_add_event_callback(&ipv4_cb);

		net_dhcpv4_start(iface);

		LOG_INF("DHCPv4 client started");

		return 0;
	}

	LOG_ERR("No network mode selected");
	return -EINVAL;
}
