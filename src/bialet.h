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

#include <stdio.h>

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

/* Welcome, not found and error pages */
#define BIALET_HEADERS "Content-Type: text/html; charset=UTF-8\r\n"
#define BIALET_HEADER_PAGE                                                     \
  "<!DOCTYPE html><body style=\"font:2em "                                     \
  "system-ui;text-align:center;margin:2em;color:#024\"><h1>"
#define BIALET_FOOTER_PAGE                                                     \
  "</p><p style=\"font-size:.8em;margin-top:2em\">Powered by 🚲 <b><a "      \
  "href=\"https://bialet.org\" style=\"color:#0BF;text-decoration:0\">bialet"
#define BIALET_WELCOME_PAGE                                                    \
  BIALET_HEADER_PAGE                                                           \
  "👋 Welcome to Bialet</h1><p>You're in! What's next? Check the <a "        \
  "href=\"https://bialet.org/getting-started\">documentation</"                \
  "a>" BIALET_FOOTER_PAGE
#define BIALET_NOT_FOUND_PAGE                                                  \
  BIALET_HEADER_PAGE                                                           \
  "⚠️ Not found</h1><p>Uh-oh! No route found." BIALET_FOOTER_PAGE
#define BIALET_ERROR_PAGE                                                      \
  BIALET_HEADER_PAGE                                                           \
  "🚨 Internal Server Error</h1><p>Oops! Something broke." BIALET_FOOTER_PAGE

#endif
