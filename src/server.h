/*
 * This file is part of Bialet, which is licensed under the
 * GNU General Public License, version 2 (GPL-2.0).
 *
 * Copyright (c) 2023 Rodrigo Arce
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * For full license text, see LICENSE.md.
 */
#ifndef SERVER_H
#define SERVER_H

#include "bialet.h"

int  start_server(struct BialetConfig* config);
int  server_poll(int delay);
void stop_server();

struct String {
  char* str;
  size_t len;
};

struct HttpMessage {
  struct String uri;
  struct String headers;
  struct String method;
  struct String routes;
  struct String message;
};

#endif
