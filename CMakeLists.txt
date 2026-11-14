# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(eco_connect_iot_gateway)

target_sources(app PRIVATE
  src/main.c
  src/led/hal/led_hal.c
  src/led/cmsis/led_cmsis.c
  src/net/app_net.c
  src/net/enc28j60_revid.c
  src/net/tcp/app_tcp.c
  src/net/http/app_http.c
  src/sensor/app_sensor.c
)

# Single include root: sources include "led/hal/led_hal.h", "net/app_net.h"
target_include_directories(app PRIVATE src)

# Linker section holding the HTTP server's static resource table
zephyr_linker_sources(SECTIONS src/net/http/http_sections.ld)

# Web page: src/web/index.html, gzipped into a C byte list at build time
generate_inc_file_for_target(app src/web/index.html
	${ZEPHYR_BINARY_DIR}/include/generated/index.html.gz.inc --gzip)

