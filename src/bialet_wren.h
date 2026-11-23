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

/* Like bialet_run(), but [code] may be NULL when [stat] names the served file:
 * the bytecode cache is tried on that signature first and the file is only
 * read when the cache misses. A zeroed status with no body means the file
 * vanished between the router's lstat and the read. */
struct BialetResponse bialet_run_file(char* module, char* code,
                                      const struct BialetFileStat* stat,
                                      struct HttpMessage*          hm);

char* read_file(const char* path);
char* bialet_read_file(const char* path);

int bialet_run_cli(char* code);
/* Validates a single .wren file, or every .wren file under a directory, and
 * their local imports. [checked] and [failed] receive how many files were
 * compiled and how many failed; either may be NULL. Returns 0 when all pass. */
int bialet_validate_syntax(const char* path, int* checked, int* failed);
int bialet_run_tests(const char* testDir, const char* rootDir);
/* 1 when a trivial query succeeds on the process database, 0 otherwise. */
int bialet_db_healthy(void);

#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
