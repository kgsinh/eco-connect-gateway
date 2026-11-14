/**
 * @file app_http.h
 * @brief Static web page on HTTP port 80.
 */
#ifndef APP_HTTP_H
#define APP_HTTP_H

/**
 * @brief Start Zephyr's HTTP server thread.
 * @retval 0 Server started.
 * @retval -EALREADY Already running.
 */
int app_http_start(void);

#endif /* APP_HTTP_H */
