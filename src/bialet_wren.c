#include "bialet_wren.h"
#include "bialet.h"
#include "bialet.wren.inc"
#include "messages.h"
#include "mongoose.h"
#include "wren.h"
#include "wren_vm.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LINE_ERROR_LEN 100
#define MAX_COLUMNS 100
#define MAX_MODULE_LEN 50
#define HTTP_ERROR 500
#define QUERY_INITIAL_SLOT 4

WrenConfiguration wren_config;
static struct BialetConfig bialet_config;
sqlite3 *db;

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
  // TODO Security: prevent load modules from parent directories
  strcpy(module, bialet_config.root_dir);
  strcat(module, "/");
  strcat(module, name);
  strcat(module, ".wren");
  char *buffer = bialet_read_file(module);

  WrenLoadModuleResult result = {0};
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

static void db_query(WrenVM *vm) {
  const char *query = wrenGetSlotString(vm, 1);
  sqlite3_stmt *stmt;
  int result, i_bind, map_slot = 0;

  char *columns[100];
  const char *col_name;
  const char *value;
  int col_type;

  wrenEnsureSlots(vm, QUERY_INITIAL_SLOT);
  if (wrenGetSlotType(vm, 2) != WREN_TYPE_LIST) {
    message(red("Runtime Error"),
            "Argument for parameters in Db should be type list");
  } else {

    result = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (result != SQLITE_OK) {
      message(red("SQL Error"), sqlite3_errmsg(db));
      return;
    }

    /* Binding values */
    for (int i = 0; i < wrenGetListCount(vm, 2); ++i) {
      i_bind = i + 1;
      wrenGetListElement(vm, 2, i, 3);
      int type = wrenGetSlotType(vm, 3);
      switch (type) {
      case WREN_TYPE_STRING:
        sqlite3_bind_text(stmt, i_bind, wrenGetSlotString(vm, 3), -1,
                          SQLITE_STATIC);
        break;
      case WREN_TYPE_NUM:
        sqlite3_bind_double(stmt, i_bind, wrenGetSlotDouble(vm, 3));
        break;
      case WREN_TYPE_BOOL:
        sqlite3_bind_int(stmt, i_bind, wrenGetSlotBool(vm, 3));
        break;
      case WREN_TYPE_NULL:
        sqlite3_bind_null(stmt, i_bind);
        break;
      default:
        message(red("Error"), "Uknown type on binding");
        break;
      }
    }

    int col_count = sqlite3_column_count(stmt);
    for (int i = 0; i < col_count; i++) {
      col_name = sqlite3_column_name(stmt, i);
      columns[i] = string_safe_copy(col_name);
    }
    wrenSetSlotNewList(vm, 0);
    /* Execute statement and fetch results */
    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
      map_slot = 1;
      wrenSetSlotNewMap(vm, map_slot);
      for (int i = 0; i < col_count; i++) {
        wrenSetSlotString(vm, ++map_slot, string_safe_copy(columns[i]));
        col_type = sqlite3_column_type(stmt, i);
        switch (col_type) {
        case SQLITE_TEXT:
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
          value = (const char *)sqlite3_column_text(stmt, i);
          wrenSetSlotString(vm, ++map_slot, string_safe_copy(value));
          break;
        case SQLITE_NULL:
          wrenSetSlotNull(vm, ++map_slot);
          break;
        default:
          message(red("Error"), "Uknown type on binding");
          break;
        }
        wrenEnsureSlots(vm, QUERY_INITIAL_SLOT + col_count * i);
        wrenSetMapValue(vm, 1, map_slot - 1, map_slot);
      }
      wrenInsertInList(vm, 0, -1, 1);
    }
    if (result != SQLITE_DONE) {
      message(red("SQL Error"), sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
  }
}
static void db_last_insert_id(WrenVM *vm) {
  wrenSetSlotDouble(vm, 0, (int)sqlite3_last_insert_rowid(db));
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
  const char *password = wrenGetSlotString(vm, 1);
  unsigned char salt[16];
  if (!RAND_bytes(salt, sizeof(salt))) {
    // TODO Error!
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
}

static void verify_password(WrenVM *vm) {
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
}

WrenForeignMethodFn wren_bind_foreign_method(WrenVM *vm, const char *module,
                                             const char *className,
                                             bool isStatic,
                                             const char *signature) {
  if (strcmp(module, "bialet") == 0) {
    if (strcmp(className, "Db") == 0) {
      if (strcmp(signature, "intQuery(_,_)") == 0) {
        return db_query;
      }
      if (strcmp(signature, "intLastInsertId()") == 0) {
        return db_last_insert_id;
      }
    }
    if (strcmp(className, "Util") == 0) {
      if (strcmp(signature, "randomString(_)") == 0) {
        return random_string;
      }
    }
    if (strcmp(className, "User") == 0) {
      if (strcmp(signature, "hash(_)") == 0) {
        return hash_password;
      }
      if (strcmp(signature, "verify(_,_)") == 0) {
        return verify_password;
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
    message("Request", get_mg_str(hm->method), get_mg_str(hm->uri),
            get_mg_str(hm->query), get_mg_str(hm->body));
  }

  vm = wrenNewVM(&wren_config);
  /* Load bialet framework with Request appended */
  char *bialetCompleteCode;
  bialetCompleteCode = string_safe_copy(bialetModuleSource);
  if (hm) {
    char *message = escape_special_chars(get_mg_str(hm->message));
    bialetCompleteCode = string_append(bialetCompleteCode, "\nRequest.init(\"",
                                       string_append(message, "\")", "\n"));
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
    WrenHandle *out_method = wrenMakeCallHandle(vm, "out()");
    wrenSetSlotHandle(vm, 0, response_class);
    if (wrenCall(vm, out_method) == WREN_RESULT_SUCCESS) {
      const char *body = wrenGetSlotString(vm, 0);
      r.body = string_safe_copy(body);
    } else {
      message(red("Runtime Error"), "Failed to get body");
      error = 1;
    }
    /* Get status from response */
    WrenHandle *status_method = wrenMakeCallHandle(vm, "status()");
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
      WrenHandle *headers_method = wrenMakeCallHandle(vm, "headers()");
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
    r.header = "Content-type: text/html\r\n";
    r.body = "Internal Server Error";
  }

  if (!hm)
    r.header = NULL;

  return r;
}

void bialet_init(struct BialetConfig *config) {
  char db_path[MAX_MODULE_LEN];
  // TODO Security: prevent load modules from parent directories
  strcpy(db_path, config->root_dir);
  strcat(db_path, "/");
  strcat(db_path, config->db_path);
  if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't open database in", config->db_path);
  }

  bialet_config = *config;
  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &wren_write;
  wren_config.errorFn = &wren_error;
  wren_config.loadModuleFn = &wren_load_module;
  wren_config.bindForeignMethodFn = &wren_bind_foreign_method;
}
