#include "bialet_wren.h"
#include "bialet.wren.inc"
#include "messages.h"
#include "wren.h"
#include "wren_vm.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE_ERROR_LEN 100

WrenConfiguration wren_config;

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
/*
 * WrenVM
 */
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

static void exampleMethod(WrenVM *vm) {
  const char *buffer = wrenGetSlotString(vm, 1);
  // TODO This will be needed later!
}

WrenForeignMethodFn wren_bind_foreign_method(WrenVM *vm, const char *module,
                                      const char *className, bool isStatic,
                                      const char *signature) {
  if (strcmp(module, "bialet") == 0) {
    if (strcmp(className, "Example") == 0) {
      if (isStatic && strcmp(signature, "someMethod(_)") == 0) {
        return exampleMethod;
      }
    }
  }
}

struct BialetResponse bialet_run(char *module, char *code) {
  struct BialetResponse r;
  WrenVM *vm = 0;

  vm = wrenNewVM(&wren_config);
  wrenInterpret(vm, "bialet", bialetModuleSource);
  WrenInterpretResult result = wrenInterpret(vm, module, code);

  wrenEnsureSlots(vm, 4);
  wrenGetVariable(vm, "bialet", "Response", 0);
  WrenHandle *responseClass = wrenGetSlotHandle(vm, 0);

  /* Get body from response */
  WrenHandle *outMethod = wrenMakeCallHandle(vm, "out()");
  wrenSetSlotHandle(vm, 0, responseClass);
  if (wrenCall(vm, outMethod) == WREN_RESULT_SUCCESS) {
    const char *body = wrenGetSlotString(vm, 0);
    r.body = string_safe_copy(body);
  } else {
    r.body = "";
  }
  /* Get headers from response */
  WrenHandle *headersMethod = wrenMakeCallHandle(vm, "headers()");
  wrenSetSlotHandle(vm, 0, responseClass);
  if (wrenCall(vm, headersMethod) == WREN_RESULT_SUCCESS) {
    const char *headersString = wrenGetSlotString(vm, 0);
    r.header = string_safe_copy(headersString);
  } else {
    r.header = "Content-type: text/html\r\n";
  }
  /* Get status from response */
  WrenHandle *statusMethod = wrenMakeCallHandle(vm, "status()");
  wrenSetSlotHandle(vm, 0, responseClass);
  if (wrenCall(vm, statusMethod) == WREN_RESULT_SUCCESS) {
    const double status = wrenGetSlotDouble(vm, 0);
    r.status = (int)status;
  } else {
    r.status = 200;
  }
  /* Clean Wren vm */
  wrenReleaseHandle(vm, responseClass);
  wrenReleaseHandle(vm, outMethod);
  wrenReleaseHandle(vm, headersMethod);
  wrenReleaseHandle(vm, statusMethod);
  wrenFreeVM(vm);

  switch (result) {
  case WREN_RESULT_SUCCESS: {
  } break;
  case WREN_RESULT_COMPILE_ERROR:
  case WREN_RESULT_RUNTIME_ERROR:
  default: {
    r.status = 500;
    r.body = "Internal Server Error";
  } break;
  }
  return r;
}

void bialet_init() {
  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &wren_write;
  wren_config.errorFn = &wren_error;
  wren_config.loadModuleFn = &wren_load_module;
  wren_config.bindForeignMethodFn = &wren_bind_foreign_method;
}
