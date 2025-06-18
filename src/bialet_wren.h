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
#ifndef BIALET_WREN_H
#define BIALET_WREN_H

#include "bialet.h"
#include "server.h"

void bialetInit(struct BialetConfig* config);

struct BialetResponse bialetRun(char* module, char* code, struct HttpMessage* hm);

char* readFile(const char* path);

int bialetRunCli(char* code);

#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
