/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2026 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#ifndef BIALET_WREN_H
#define BIALET_WREN_H

#include "bialet.h"
#include "server.h"

void bialet_init(struct BialetConfig* config);
void bialet_cleanup();
void bialet_reopen_db();

const char* bialet_get_full_root_dir();

struct BialetResponse bialet_run(char* module, char* code, struct HttpMessage* hm);

char* read_file(const char* path);
char* bialet_read_file(const char* path);

int bialet_run_cli(char* code);
int bialet_validate_syntax(const char* filePath);
int bialet_run_tests(const char* testDir, const char* rootDir);

#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
