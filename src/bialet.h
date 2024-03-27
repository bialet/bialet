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
#ifndef BIALET_CONFIG_H
#define BIALET_CONFIG_H

#ifdef _WIN32
#define IS_WIN 1
#else
#define IS_UNIX 1
#endif

#ifdef _WIN64

#include <stdio.h>
#include <winsock2.h>
#include <windows.h>

#else

#include <stdio.h>

#endif

#define MAX_NUMBER_LENGTH 100

struct BialetConfig {
  char *root_dir;
  char *host;
  int port;

  FILE *log_file;
  int debug;
  int output_color;

  int mem_soft_limit, mem_hard_limit, cpu_soft_limit, cpu_hard_limit;

  char *db_path;
};

typedef enum {
  BIALETQUERYTYPE_NULL,
  BIALETQUERYTYPE_NUMBER,
  BIALETQUERYTYPE_STRING,
  BIALETQUERYTYPE_BOOLEAN
} BialetQueryType;

typedef struct {
  char *name;
  char *value;
  BialetQueryType type;
} BialetQueryRow;

typedef struct {
  BialetQueryRow *rows;
  int rowCount;
} BialetQueryResult;

typedef struct {
  char *value;
  BialetQueryType type;
} BialetQueryParameter;

typedef struct {
  BialetQueryResult *results;
  int resultsCount;
  BialetQueryParameter *parameters;
  int parametersCount;
  char *queryString;
  char *lastInsertId;
} BialetQuery;

BialetQuery *createBialetQuery();
void addResultRow(BialetQuery *query, int resultIndex, const char *name,
                  const char *value, BialetQueryType type);
void addResult(BialetQuery *query);
void addParameter(BialetQuery *query, const char *value, BialetQueryType type);
void freeBialetQuery(BialetQuery *query);

#define BIALET_USAGE                                                           \
  "🚲 bialet\n\nUsage: %s [-h host] [-p port] [-l log] [-d database] "       \
  "root_dir\n"

/* Welcome, not found and error pages */
#define BIALET_HEADERS "Content-Type: text/html; charset=UTF-8\r\n"
#define BIALET_HEADER_PAGE                                                     \
  "<!DOCTYPE html><body style=\"font:2.3rem "                                  \
  "system-ui;text-align:center;margin:2em;color:#024\"><h1>"
#define BIALET_FOOTER_PAGE                                                     \
  "</p><p style=\"font-size:.8em;margin-top:2em\">Powered by 🚲 <b><a "      \
  "href=\"https://bialet.dev\" style=\"color:#007FAD\" >Bialet"
#define BIALET_WELCOME_PAGE                                                      \
  BIALET_HEADER_PAGE                                                             \
  "👋 Welcome to Bialet</h1><p>You're in! What's next?<p>Check out our <b><a " \
  "href=\"https://bialet.dev/quickstart.html\" "                                 \
  "style=\"color:#007FAD\">Quickstart "                                          \
  "guide</a></b>." BIALET_FOOTER_PAGE
#define BIALET_NOT_FOUND_PAGE                                                  \
  BIALET_HEADER_PAGE                                                           \
  "⚠️ Not found</h1><p>Uh-oh! No route found." BIALET_FOOTER_PAGE
#define BIALET_ERROR_PAGE                                                      \
  BIALET_HEADER_PAGE                                                           \
  "🚨 Internal Server Error</h1><p>Oops! Something broke." BIALET_FOOTER_PAGE

#endif
