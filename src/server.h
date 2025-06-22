/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2025 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#ifndef SERVER_H
#define SERVER_H

#include "bialet.h"

int  start_server(struct BialetConfig* config);
int  server_poll(int delay);
void stop_server();
void custom_error(int status, struct BialetResponse* response);
void sse_broadcast_reload();

struct String {
  char*  str;
  size_t len;
};

struct HttpMessage {
  struct String uri;
  struct String headers;
  struct String method;
  struct String routes;
  struct String message;
};

#define SSE_SCRIPT                                                                  \
  "<script>/* bialet live reload */"                                                \
  "(new EventSource('/__bialet_reload'))."                                          \
  "addEventListener('reload',()=>location.reload())</script>"

#define SSE_SCRIPT_LEN 127
#define SSE_KEEP_ALIVE                                                                 \
  "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: "           \
  "no-cache\r\nConnection: keep-alive\r\n\r\n"
#define SSE_KEEP_ALIVE_LEN 66

#endif
