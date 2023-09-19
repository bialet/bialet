#include "bialet_wren.h"
#include "messages.h"
#include "wren.h"
#include "wren_vm.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE_ERROR_LEN 100

WrenConfiguration wrenConfig;

static char *SafeMalloc(size_t size) {
  char *p;

  p = (char *)malloc(size);
  if (p == 0) {
    exit(1);
  }
  return p;
}

static char *StrDup(const char *zSrc) {
  char *zDest;
  size_t size;

  if (zSrc == 0)
    return 0;
  size = strlen(zSrc) + 1;
  zDest = (char *)SafeMalloc(size);
  strcpy(zDest, zSrc);
  return zDest;
}

static char *StrAppend(char *zPrior, const char *zSep, const char *zSrc) {
  char *zDest;
  size_t size;
  size_t n0, n1, n2;

  if (zSrc == 0)
    return 0;
  if (zPrior == 0)
    return StrDup(zSrc);
  n0 = strlen(zPrior);
  n1 = strlen(zSep);
  n2 = strlen(zSrc);
  size = n0 + n1 + n2 + 1;
  zDest = (char *)SafeMalloc(size);
  memcpy(zDest, zPrior, n0);
  free(zPrior);
  memcpy(&zDest[n0], zSep, n1);
  memcpy(&zDest[n0 + n1], zSrc, n2 + 1);
  return zDest;
}
/*
 * WrenVM
 */
static void writeFn(WrenVM *vm, const char *text) {
  message(yellow("Log"), text);
}

char *readFile(const char *path) {
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

static WrenLoadModuleResult loadModuleFn(WrenVM *vm, const char *name) {

  char module[100];
  // TODO Read path from config
  strcpy(module, ".");
  strcat(module, "/");
  strcat(module, name);
  strcat(module, ".wren");
  char *buffer = readFile(module);

  WrenLoadModuleResult result = {0};
  result.source = NULL;

  if (buffer) {
    result.source = buffer;
  }
  return result;
}

void errorFn(WrenVM *vm, WrenErrorType errorType, const char *module,
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

WrenForeignMethodFn bindForeignMethod(WrenVM *vm, const char *module,
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

struct BialetResponse runCode(char *module, char *code) {
  struct BialetResponse r;
  WrenVM *vm = 0;

  vm = wrenNewVM(&wrenConfig);
  WrenInterpretResult result = wrenInterpret(vm, module, code);

  wrenEnsureSlots(vm, 4);
  wrenGetVariable(vm, "bialet", "Response", 0);
  WrenHandle *responseClass = wrenGetSlotHandle(vm, 0);

  /* Get body from response */
  WrenHandle *outMethod = wrenMakeCallHandle(vm, "out()");
  wrenSetSlotHandle(vm, 0, responseClass);
  if (wrenCall(vm, outMethod) == WREN_RESULT_SUCCESS) {
    const char *body = wrenGetSlotString(vm, 0);
    r.body = StrDup(body);
  } else {
    r.body = "";
  }
  /* Get headers from response */
  WrenHandle *headersMethod = wrenMakeCallHandle(vm, "headers()");
  wrenSetSlotHandle(vm, 0, responseClass);
  if (wrenCall(vm, headersMethod) == WREN_RESULT_SUCCESS) {
    const char *headersString = wrenGetSlotString(vm, 0);
    r.header = StrDup(headersString);
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

void bialetWrenInit() {
  wrenInitConfiguration(&wrenConfig);
  wrenConfig.writeFn = &writeFn;
  wrenConfig.errorFn = &errorFn;
  wrenConfig.loadModuleFn = &loadModuleFn;
  wrenConfig.bindForeignMethodFn = &bindForeignMethod;
}
