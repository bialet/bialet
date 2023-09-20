#include "bialet_wren.h"
#include "bialet.wren.inc"
#include "messages.h"
#include "mongoose.h"
#include "wren.h"
#include "wren_vm.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_ERROR_LEN 100

WrenConfiguration wren_config;
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

  char module[100];
  // TODO Read path from config
  strcpy(module, ".");
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
  wrenEnsureSlots(vm, 1000);
  const char *query = wrenGetSlotString(vm, 1);
  if (wrenGetSlotType(vm, 2) != WREN_TYPE_LIST) {
    message(red("Runtime Error"),
            "Argument for parameters in Db should be type list");
  } else {
    sqlite3_stmt *stmt;
    int result, i_bind, map_slot;

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
      // TODO Bind null values
      default:
        message(red("Error"), "Uknown type on binding");
        break;
      }
    }

    char *columns[100];
    const char *col_name;
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
        const char *col_data = (const char *)sqlite3_column_text(stmt, i);
        wrenSetSlotString(vm, ++map_slot, string_safe_copy(columns[i]));
        // TODO Fix error when data is empty
        wrenSetSlotString(vm, ++map_slot, string_safe_copy(col_data));
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
  }
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
    /* Get headers from response */
    WrenHandle *headers_method = wrenMakeCallHandle(vm, "headers()");
    wrenSetSlotHandle(vm, 0, response_class);
    if (wrenCall(vm, headers_method) == WREN_RESULT_SUCCESS) {
      const char *headersString = wrenGetSlotString(vm, 0);
      r.header = string_safe_copy(headersString);
    } else {
      message(red("Runtime Error"), "Failed to get headers");
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
    /* Clean Wren vm */
    wrenReleaseHandle(vm, response_class);
    wrenReleaseHandle(vm, out_method);
    wrenReleaseHandle(vm, headers_method);
    wrenReleaseHandle(vm, status_method);
  }
  wrenFreeVM(vm);

  if (error) {
    message(red("Error"), "Internal Server Error");
    r.status = 500;
    r.header = "Content-type: text/html\r\n";
    r.body = "Internal Server Error";
  }

  return r;
}

void bialet_init(char *db_path) {
  // TODO Move to config
  if (!sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                       NULL)) {
    message(red("SQL Error"), "Can't open database in", db_path);
  }

  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &wren_write;
  wren_config.errorFn = &wren_error;
  wren_config.loadModuleFn = &wren_load_module;
  wren_config.bindForeignMethodFn = &wren_bind_foreign_method;
}
