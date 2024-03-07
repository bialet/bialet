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
#include "bialet_wren.h"
#include "bialet.h"
#include "bialet.wren.inc"
#include "bialet_extra.wren.inc"
#include "messages.h"
#include "mongoose.h"
#include "wren.h"
#include "wren_vm.h"
#include <curl/curl.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BIALET_SQLITE_ERROR 11
#define MAX_URL_LEN 200
#define MAX_LINE_ERROR_LEN 100
#define MAX_COLUMNS 100
#define MAX_MODULE_LEN 50
#define HTTP_ERROR 500

WrenConfiguration wren_config;
static struct BialetConfig bialet_config;
sqlite3 *db;
char response_buffer[2048];

static char *safe_malloc(size_t size) {
  char *p;

  p = (char *)malloc(size);
  if (p == 0) {
    exit(1);
  }
  return p;
}

static char *string_safe_copy(const char *zSrc) {
  char *zDest;
  size_t size;

  if (zSrc == 0)
    return 0;
  size = strlen(zSrc) + 1;
  zDest = (char *)safe_malloc(size);
  strcpy(zDest, zSrc);
  return zDest;
}

static char *string_append(char *zPrior, const char *zSep, const char *zSrc) {
  char *zDest;
  size_t size;
  size_t n0, n1, n2;

  if (zSrc == 0)
    return 0;
  if (zPrior == 0)
    return string_safe_copy(zSrc);
  n0 = strlen(zPrior);
  n1 = strlen(zSep);
  n2 = strlen(zSrc);
  size = n0 + n1 + n2 + 1;
  zDest = (char *)safe_malloc(size);
  memcpy(zDest, zPrior, n0);
  free(zPrior);
  memcpy(&zDest[n0], zSep, n1);
  memcpy(&zDest[n0 + n1], zSrc, n2 + 1);
  return zDest;
}

static void wren_write(WrenVM *vm, const char *text) {
  message(yellow("Log"), text);
}

char *bialet_read_file(const char *path) {
  char *buffer = 0;
  long length;
  FILE *f = fopen(path, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length + 1);
    if (buffer) {
      fread(buffer, 1, length, f);
      buffer[length] = '\0';
    }
    fclose(f);
  }
  return buffer;
}

static WrenLoadModuleResult wren_load_module(WrenVM *vm, const char *name) {

  char module[MAX_MODULE_LEN];
  WrenLoadModuleResult result = {0};

  /* Load Bialet modules */
  if (strcmp(name, "bialet/extra") == 0) {
    result.source = bialet_extraModuleSource;
    return result;
  }

  if (name[0] == '/') {
    strcpy(module, bialet_config.root_dir);
  } else {
    /* @TODO Security: prevent load modules from parent directories */
    /* @TODO Create an object for the user data */
    char *user_data = wrenGetUserData(vm);
    char *called_module = string_safe_copy(user_data);
    char *last_slash = strrchr(called_module, '/');
    if (last_slash)
      *last_slash = '\0';
    strcpy(module, called_module);
    strcat(module, "/");
  }
  strcat(module, name);
  strcat(module, BIALET_EXTENSION);
  char *buffer = bialet_read_file(module);
  result.source = NULL;

  if (buffer) {
    result.source = buffer;
  }
  return result;
}

void wren_error(WrenVM *vm, WrenErrorType errorType, const char *module,
                const int line, const char *msg) {
  char lineMessage[MAX_LINE_ERROR_LEN];
  sprintf(lineMessage, "%s line %d", module, line);
  switch (errorType) {
  case WREN_ERROR_COMPILE: {
    message(red("Compilation Error"), lineMessage, (char *)msg);
  } break;
  case WREN_ERROR_STACK_TRACE: {
    message(red("Stack Error"), lineMessage, (char *)msg);
  } break;
  case WREN_ERROR_RUNTIME: {
    message(red("Runtime Error"), (char *)msg);
  } break;
  }
}

static char *sqlite3Int64ToString(sqlite3_int64 value) {
  char *str = (char *)malloc(21 * sizeof(char));
  if (str == NULL)
    return NULL;
  sprintf(str, "%lld", value);
  return str;
}

static void query_sqlite_execute(WrenVM *vm, BialetQuery *query) {
  sqlite3_stmt *stmt;
  char *columns[MAX_COLUMNS];
  const char *value;
  int colType, colCount = 0, type, rowCount = 0, bindCounter = 0;

  /* Prepare the query */
  int result = sqlite3_prepare_v2(db, query->queryString, -1, &stmt, 0);
  if (result != SQLITE_OK) {
    message(red("Query Error"), sqlite3_errmsg(db));
    return;
  }

  /* Bind parameters */
  for (int i = 0; i < query->parametersCount; i++) {
    bindCounter = i + 1;
    switch (query->parameters[i].type) {
    case BIALETQUERYTYPE_STRING:
      sqlite3_bind_text(stmt, bindCounter, query->parameters[i].value, -1,
                        SQLITE_STATIC);
      break;
    case BIALETQUERYTYPE_NUMBER:
      sqlite3_bind_double(stmt, bindCounter, atof(query->parameters[i].value));
      break;
    case BIALETQUERYTYPE_BOOLEAN:
      sqlite3_bind_int(stmt, bindCounter, atoi(query->parameters[i].value));
      break;
    case BIALETQUERYTYPE_NULL:
      sqlite3_bind_null(stmt, bindCounter);
      break;
    default:
      message(red("Query Error"), "Uknown type on binding parameters");
      break;
    }
  }

  /* Execute statement and fetch results */
  while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    if (!colCount) {
      /* Get column names */
      colCount = sqlite3_column_count(stmt);
      for (int i = 0; i < colCount; i++) {
        columns[i] = string_safe_copy(sqlite3_column_name(stmt, i));
      }
    }

    addResult(query);
    for (int i = 0; i < colCount; i++) {
      colType = sqlite3_column_type(stmt, i);
      switch (colType) {
      case SQLITE_INTEGER:
      case SQLITE_FLOAT:
        type = BIALETQUERYTYPE_NUMBER;
        value = (const char *)sqlite3_column_text(stmt, i);
        break;
      case SQLITE_TEXT:
      case SQLITE_BLOB:
        type = BIALETQUERYTYPE_STRING;
        value = (const char *)sqlite3_column_text(stmt, i);
        break;
      case SQLITE_NULL:
        type = BIALETQUERYTYPE_NULL;
        value = NULL;
        break;
      default:
        message(red("Query Error"), "Uknown type on binding result");
        break;
      }
      addResultRow(query, rowCount, columns[i], value, type);
    }
    rowCount++;
  }

  if (result != SQLITE_DONE) {
    message(red("SQL Error"), sqlite3_errmsg(db));
  }
  query->lastInsertId = sqlite3Int64ToString(sqlite3_last_insert_rowid(db));
  sqlite3_finalize(stmt);
}

static void random_string(WrenVM *vm) {
  const int len = wrenGetSlotDouble(vm, 1);
  char random_str[len + 1];
  char charset[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  srand(time(0));
  for (int i = 0; i < len; i++) {
    int random_index = rand() % (sizeof(charset) - 1);
    random_str[i] = charset[random_index];
  }

  random_str[len] = '\0';
  wrenSetSlotString(vm, 0, random_str);
}

static void hash_password(WrenVM *vm) {
/* @TODO OpenSSL 3 only */
#if OPENSSL_VERSION_NUMBER > 0x102031af
  const char *password = wrenGetSlotString(vm, 1);
  unsigned char salt[16];
  if (!RAND_bytes(salt, sizeof(salt))) {
    /* @TODO Handle error for password hashing */
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, hash, &hash_len);

  EVP_MD_CTX_free(ctx);

  static char result[128];
  for (unsigned int i = 0; i < hash_len; i++) {
    sprintf(result + (i * 2), "%02x", hash[i]);
  }

  strcat(result, "/");
  for (int i = 0; i < sizeof(salt); i++) {
    sprintf(result + strlen(result), "%02x", salt[i]);
  }

  wrenEnsureSlots(vm, 2);
  wrenSetSlotString(vm, 0, result);
#endif
}

static void verify_password(WrenVM *vm) {
#if OPENSSL_VERSION_NUMBER > 0x102031af
  int result = 0;
  const char *password = wrenGetSlotString(vm, 1);
  const char *hash_and_salt = wrenGetSlotString(vm, 2);

  char stored_hash[65], stored_salt[33];
  strncpy(stored_hash, hash_and_salt, 64);
  stored_hash[64] = 0;
  strcpy(stored_salt, hash_and_salt + 65);

  unsigned char salt[16];
  for (int i = 0; i < 16; i++) {
    sscanf(stored_salt + i * 2, "%2hhx", &salt[i]);
  }

  unsigned char new_hash[EVP_MAX_MD_SIZE];
  unsigned int new_hash_len;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, new_hash, &new_hash_len);

  EVP_MD_CTX_free(ctx);

  char new_hash_str[65];
  for (unsigned int i = 0; i < new_hash_len; i++) {
    sprintf(new_hash_str + (i * 2), "%02x", new_hash[i]);
  }
  new_hash_str[64] = 0;
  result = strcmp(new_hash_str, stored_hash) == 0;
  wrenEnsureSlots(vm, 2);
  wrenSetSlotBool(vm, 0, result);
#endif
}

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total_size = size * nmemb;
  strncat((char *)userp, contents, total_size);
  return total_size;
}

static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  /* received header is nitems * size long in 'buffer' NOT ZERO TERMINATED */
  /* 'userdata' is set with CURLOPT_HEADERDATA */
  /* @TODO Get response headers and HTTP status */
  printf("CURL Response Header: %s\n", buffer);
  return nitems * size;
}

static void http_call(WrenVM *vm) {
  CURL *handle;
  CURLcode res;
  struct curl_slist *headers = NULL;

  const char *url = wrenGetSlotString(vm, 1);
  const char *method = wrenGetSlotString(vm, 2);
  const char *raw_headers = wrenGetSlotString(vm, 3);
  const char *postData = wrenGetSlotString(vm, 4);
  const char *basicAuth = wrenGetSlotString(vm, 5);
  printf("URL: %s - Method: %s - Headers: %s - PostData: %s\n", url, method,
         raw_headers, postData);

  response_buffer[0] = '\0';

  handle = curl_easy_init();
  if (handle) {
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    /* Headers */
    char *header_string = strdup(raw_headers);
    char *header_line = strtok(header_string, "\n");
    while (header_line != NULL) {
      // Add each header line to the slist
      headers = curl_slist_append(headers, header_line);
      header_line = strtok(NULL, "\n");
    }
    free(header_string);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    if (basicAuth) {
      curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      curl_easy_setopt(handle, CURLOPT_USERPWD, basicAuth);
    }

    if (strlen(postData) > 0) {
      curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postData);
    }
    /* For completeness */
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* only allow redirects to HTTP and HTTPS URLs */
    curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    /* each transfer needs to be done within 20 seconds! */
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L);
    /* connect fast or fail */
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    /* Speed up the connection using IPv4 only */
    curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);

    res = curl_easy_perform(handle);
    /* Check for errors */
    if (res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(handle);
    curl_slist_free_all(headers);
  }
  printf("Response: %s\n", response_buffer);
  /* @TODO Handle curl errors */
  wrenEnsureSlots(vm, 4);
  wrenSetSlotNewList(vm, 0);
  wrenSetSlotDouble(vm, 5, 200);
  wrenSetSlotString(vm, 6, "Content-Type: application/json");
  wrenSetSlotString(vm, 7, string_safe_copy(response_buffer));
  wrenInsertInList(vm, 0, 0, 5);
  wrenInsertInList(vm, 0, 1, 6);
  wrenInsertInList(vm, 0, 2, 7);
}

WrenForeignMethodFn wren_bind_foreign_method(WrenVM *vm, const char *module,
                                             const char *className,
                                             bool isStatic,
                                             const char *signature) {
  if (strcmp(module, "bialet") == 0) {
    if (strcmp(className, "Util") == 0) {
      if (strcmp(signature, "randomString_(_)") == 0) {
        return random_string;
      }
      if (strcmp(signature, "hash_(_)") == 0) {
        return hash_password;
      }
      if (strcmp(signature, "verify_(_,_)") == 0) {
        return verify_password;
      }
    }
    if (strcmp(className, "Http") == 0) {
      if (strcmp(signature, "call_(_,_,_,_,_)") == 0) {
        return http_call;
      }
    }
  }
  return NULL;
}
char *escape_special_chars(const char *input) {
  int i, j = 0, len = strlen(input);
  char *output =
      malloc(len * 2 + 1); // Worst case: all characters need escaping
  if (output == NULL)
    return NULL;

  for (i = 0; i < len; i++) {
    if (input[i] == '"' || input[i] == '\\' || input[i] == '%') {
      output[j++] = '\\';
    }
    output[j++] = input[i];
  }
  output[j] = '\0';
  return output;
}

char *get_mg_str(struct mg_str str) {
  char *val = NULL;
  int method_len = (int)(str.len);
  val = malloc(method_len + 1);
  if (val) {
    strncpy(val, str.ptr, method_len);
    val[method_len] = '\0';
  }
  return val;
}

struct BialetResponse bialet_run(char *module, char *code,
                                 struct mg_http_message *hm) {
  struct BialetResponse r;
  int error = 0;
  WrenVM *vm = 0;

  if (hm) {
    char url[MAX_URL_LEN];
    strcpy(url, get_mg_str(hm->uri));
    if (hm->query.len > 0) {
      strcat(url, "?");
      strcat(url, get_mg_str(hm->query));
    }
    message("Request", get_mg_str(hm->method), url);
  }

  vm = wrenNewVM(&wren_config);
  wrenSetUserData(vm, module);
  /* Load bialet framework with Request appended */
  char *bialetCompleteCode;
  bialetCompleteCode = string_safe_copy(bialetModuleSource);
  if (hm) {
    char *message = escape_special_chars(get_mg_str(hm->message));
    bialetCompleteCode =
        string_append(bialetCompleteCode, "\nRequest.init(\"",
                      string_append(message, "\",\"", hm->bialet_routes));
    bialetCompleteCode = string_append(bialetCompleteCode, "\")", "\n");
  }
  wrenInterpret(vm, "bialet", bialetCompleteCode);
  /* Run user code */
  if (!error) {
    WrenInterpretResult result = wrenInterpret(vm, module, code);
    error = result != WREN_RESULT_SUCCESS;
  }
  wrenEnsureSlots(vm, 4);
  if (!error) {
    wrenGetVariable(vm, "bialet", "Response", 0);
    WrenHandle *response_class = wrenGetSlotHandle(vm, 0);
    /* Get body from response */
    WrenHandle *out_method = wrenMakeCallHandle(vm, "out");
    wrenSetSlotHandle(vm, 0, response_class);
    if (wrenCall(vm, out_method) == WREN_RESULT_SUCCESS) {
      const char *body = wrenGetSlotString(vm, 0);
      r.body = string_safe_copy(body);
    } else {
      message(red("Runtime Error"), "Failed to get body");
      error = 1;
    }
    /* Get status from response */
    WrenHandle *status_method = wrenMakeCallHandle(vm, "status");
    wrenSetSlotHandle(vm, 0, response_class);
    if (wrenCall(vm, status_method) == WREN_RESULT_SUCCESS) {
      const double status = wrenGetSlotDouble(vm, 0);
      r.status = (int)status;
    } else {
      message(red("Runtime Error"), "Failed to get status");
      error = 1;
    }
    /* Get headers from response */
    if (hm) {
      WrenHandle *headers_method = wrenMakeCallHandle(vm, "headers");
      wrenSetSlotHandle(vm, 0, response_class);
      if (wrenCall(vm, headers_method) == WREN_RESULT_SUCCESS) {
        const char *headersString = wrenGetSlotString(vm, 0);
        r.header = string_safe_copy(headersString);
      } else {
        message(red("Runtime Error"), "Failed to get headers");
        error = 1;
      }
      wrenReleaseHandle(vm, headers_method);
    }
    /* Clean Wren vm */
    wrenReleaseHandle(vm, response_class);
    wrenReleaseHandle(vm, out_method);
    wrenReleaseHandle(vm, status_method);
  }
  wrenFreeVM(vm);

  if (error) {
    message(red("Error"), "Internal Server Error");
    r.status = HTTP_ERROR;
    r.header = BIALET_HEADERS;
    r.body = BIALET_ERROR_PAGE;
  }

  if (!hm)
    r.header = NULL;

  return r;
}

void bialet_init(struct BialetConfig *config) {
  char db_path[MAX_MODULE_LEN];
  int lastChar = (int)strlen(config->db_path) - 1;
  if (config->db_path[0] == '/') {
    strcpy(db_path, config->db_path);
  } else {
      if (config->db_path[lastChar] == '/') {
        config->db_path[lastChar] = '\0';
      }
    strcpy(db_path, config->root_dir);
    strcat(db_path, "/");
    strcat(db_path, config->db_path);
  }
  if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't open database in", config->db_path);
    exit(BIALET_SQLITE_ERROR);
  }

  bialet_config = *config;
  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &wren_write;
  wren_config.errorFn = &wren_error;
  wren_config.queryFn = &query_sqlite_execute;
  wren_config.loadModuleFn = &wren_load_module;
  wren_config.bindForeignMethodFn = &wren_bind_foreign_method;

  curl_global_init(CURL_GLOBAL_DEFAULT);
}

BialetQuery *createBialetQuery() {
  BialetQuery *query = (BialetQuery *)malloc(sizeof(BialetQuery));
  query->results = NULL;
  query->resultsCount = 0;
  query->parameters = NULL;
  query->parametersCount = 0;
  return query;
}

void addResult(BialetQuery *query) {
  query->resultsCount++;
  query->results = (BialetQueryResult *)realloc(
      query->results, query->resultsCount * sizeof(BialetQueryResult));
  BialetQueryResult *newResult = &query->results[query->resultsCount - 1];
  newResult->rows = NULL;
  newResult->rowCount = 0;
}

void addResultRow(BialetQuery *query, int resultIndex, const char *name,
                  const char *value, BialetQueryType type) {
  if (resultIndex < 0 || resultIndex >= query->resultsCount)
    return;

  BialetQueryResult *result = &query->results[resultIndex];
  result->rowCount++;
  result->rows = (BialetQueryRow *)realloc(
      result->rows, result->rowCount * sizeof(BialetQueryRow));
  BialetQueryRow *newRow = &result->rows[result->rowCount - 1];
  newRow->name = name != NULL ? strdup(name) : "";
  newRow->value = value != NULL ? strdup(value) : "";
  newRow->type = type;
}

void addParameter(BialetQuery *query, const char *value, BialetQueryType type) {
  query->parametersCount++;
  query->parameters = (BialetQueryParameter *)realloc(
      query->parameters, query->parametersCount * sizeof(BialetQueryParameter));
  BialetQueryParameter *newParameter =
      &query->parameters[query->parametersCount - 1];
  newParameter->value = value != NULL ? strdup(value) : NULL;
  newParameter->type = type;
}

void freeBialetQuery(BialetQuery *query) {
  if (!query)
    return; // Guard clause to prevent dereferencing a NULL pointer

  // Free each row in each result
  for (int i = 0; i < query->resultsCount; i++) {
    for (int j = 0; j < query->results[i].rowCount; j++) {
      free(query->results[i].rows[j].name); // Free row name
      query->results[i].rows[j].name = NULL;

      free(query->results[i].rows[j].value); // Free row value
      query->results[i].rows[j].value = NULL;
    }
    free(query->results[i].rows); // Free the rows array itself
    query->results[i].rows = NULL;
  }
  free(query->results); // Free the results array
  query->results = NULL;

  // Free each parameter value
  for (int i = 0; i < query->parametersCount; i++) {
    free(query->parameters[i].value);
    query->parameters[i].value = NULL;
  }
  free(query->parameters); // Free the parameters array
  query->parameters = NULL;

  // Free queryString and lastInsertId if they exist
  if (query->queryString) {
    free(query->queryString);
    query->queryString = NULL;
  }
  if (query->lastInsertId) {
    free(query->lastInsertId);
    query->lastInsertId = NULL;
  }

  // Finally, free the BialetQuery structure itself
  free(query);
}
