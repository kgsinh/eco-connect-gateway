/*
 * Ethernet bring-up: starts DHCPv4 on the default interface and logs the
 * lease. Non-blocking; the address arrives later via a net_mgmt event.
 */
#ifndef APP_NET_H
#define APP_NET_H

int app_net_start(void);

#endif /* APP_NET_H */
