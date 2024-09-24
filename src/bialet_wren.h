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
#ifndef BIALET_WREN_H
#define BIALET_WREN_H

#include "bialet.h"
#include "mongoose.h"

struct BialetResponse {
  int   status;
  char* header;
  char* body;
  int   length;
};

void bialet_init(struct BialetConfig* config);

struct BialetResponse bialet_run(char* module, char* code,
                                 struct mg_http_message* hm);

char* bialet_read_file(const char* path);

int bialet_run_cli(char* code);

#define BIALET_EXTENSION ".wren"
#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
