/**
 * @file app_tcp.h
 * @brief TCP echo server on port 4242.
 */
#ifndef APP_TCP_H
#define APP_TCP_H

/**
 * @brief Start the server thread; it waits for the interface, then serves.
 * @retval 0 Thread started.
 */
int app_tcp_start(void);

#endif /* APP_TCP_H */
