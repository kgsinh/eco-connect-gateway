/**
 * @file app_net.h
 * @brief IPv4 bring-up on the default network interface.
 */
#ifndef APP_NET_H
#define APP_NET_H

/**
 * @brief Add the static IPv4 address from Kconfig and start DHCPv4.
 *
 * Returns without waiting for a lease. The DHCP address, netmask, gateway
 * and lease time are logged from a net_mgmt event when the lease arrives.
 * An invalid or unset static address is logged and skipped; DHCP still runs.
 *
 * @retval 0 DHCPv4 started.
 * @retval -ENODEV No network interface.
 */
int app_net_start(void);

#endif /* APP_NET_H */
