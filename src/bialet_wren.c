#include "bialet_wren.h"
#include "wren_vm.h"
#include <stdio.h>
#include <stdlib.h>

char *wrenBuffer = 0;
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
  wrenBuffer = StrAppend(wrenBuffer, "\n", text);
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
  /* strcpy(module, zDir); */
  /* strcat(module, "/"); */
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
  switch (errorType) {
  case WREN_ERROR_COMPILE: {
    printf("[%s line %d] [Error] %s\n", module, line, msg);
  } break;
  case WREN_ERROR_STACK_TRACE: {
    printf("[%s line %d] in %s\n", module, line, msg);
  } break;
  case WREN_ERROR_RUNTIME: {
    printf("[Runtime Error] %s\n", msg);
  } break;
  }
}

struct BialetResponse runCode(char *code) {
  const char *module = "main";
  WrenVM *vm = 0;
  vm = wrenNewVM(&wrenConfig);
  WrenInterpretResult result = wrenInterpret(vm, module, code);
  wrenFreeVM(vm);

  struct BialetResponse r;
  r.header = "Content-type: text/html\r\n";
  switch (result) {
  case WREN_RESULT_SUCCESS: {
    r.status = 200;
    r.body = wrenBuffer;
  } break;
  case WREN_RESULT_COMPILE_ERROR:
  case WREN_RESULT_RUNTIME_ERROR:
  default: {
    r.status = 500;
    r.body = "Internal Server Error";
  } break;
  }
  wrenBuffer = 0;
  return r;
}

void bialetWrenInit() {
  wrenInitConfiguration(&wrenConfig);
  wrenConfig.writeFn = &writeFn;
  wrenConfig.errorFn = &errorFn;
  wrenConfig.loadModuleFn = &loadModuleFn;
}
