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

void bialetInit(struct BialetConfig* config);

struct BialetResponse bialetRun(char* module, char* code,
                                 struct mg_http_message* hm);

char* readFile(const char* path);

int bialetRunCli(char* code);

#define BIALET_EXTENSION ".wren"
#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
