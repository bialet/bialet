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

void bialetInit(struct BialetConfig* config);
void bialetCleanup();

const char* bialetGetFullRootDir();

struct BialetResponse bialetRun(struct BialetWrenCode* code, struct HttpMessage* hm);

char* readFile(const char* path);
char* bialetReadFile(const char* path);

struct BialetWrenCode* bialetLoadWrenCode(const char* filePath);
void bialetFreeWrenCode(struct BialetWrenCode* code);
void bialetSaveBytecodeIfNeeded(const char* wrenPath, const char* source);

int bialetRunCli(char* source);
int bialetValidateSyntax(const char* filePath);
int bialetRunTests(const char* testDir, const char* rootDir);
int bialetTestBytecode(const char* wrenFile, const char* rootDir);

#define BIALET_INDEX_FILE "/index" BIALET_EXTENSION

#endif
