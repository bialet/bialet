#include "mongoose.h"
#include "wren_vm.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/types.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

struct BialetResponse {
  int status;
  char *header;
  char *body;
};

char *wrenBuffer = 0;
WrenConfiguration wrenConfig;

/*
 ** Allocate memory safely
 */
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

static WrenLoadModuleResult loadModuleFn(WrenVM *vm, const char *name) {

  char module[100];
  /* strcpy(module, zDir); */
  /* strcat(module, "/"); */
  strcat(module, name);
  strcat(module, ".wren");
  char *buffer = 0;
  long length;
  FILE *f = fopen(module, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length + 1);
    if (buffer) {
      fread(buffer, 1, length, f);
    }
    fclose(f);
  }

  WrenLoadModuleResult result = {0};
  result.source = NULL;

  if (buffer) {
    buffer[length] = '\0';
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
    wrenBuffer = 0;
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

static void httpHandler(struct mg_connection *c, int ev, void *ev_data,
                        void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    // TODO Add routing
    if (mg_http_match_uri(hm, "/api/hello")) {
      // TODO Read code from routing
      char *code = "System.print(\"Test code\")";
      struct BialetResponse r = runCode(code);
      mg_http_reply(c, r.status, r.header, r.body, MG_ESC("status"));
    } else {
      struct mg_http_serve_opts opts = {.root_dir = "."};
      mg_http_serve_dir(c, hm, &opts);
    }
  }
}

static void initConfig() { printf("Reload config\n"); }

static void *fileWatcher(void *arg) {
  int length, i = 0;
  char buffer[BUF_LEN];
  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
  }
  // TODO Remove hardcoded path
  // TODO Add watcher for new folders
  int wd =
      inotify_add_watch(fd, "/home/albo/hobby/bialet/bialet/build", IN_MODIFY);
  if (wd < 0) {
    perror("inotify_add_watch");
  }
  for (;;) {
    length = read(fd, buffer, BUF_LEN);

    if (length < 0) {
      perror("read");
    }

    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];
      if (event->len) {
        if (event->mask & IN_MODIFY) {
          initConfig();
          printf("File %s was modified.\n", event->name);
        }
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }
}

int main() {

  struct mg_mgr mgr;

  wrenInitConfiguration(&wrenConfig);
  wrenConfig.writeFn = &writeFn;
  wrenConfig.errorFn = &errorFn;
  wrenConfig.loadModuleFn = &loadModuleFn;

  initConfig();
  mg_mgr_init(&mgr);
  // TODO Add host and port from params
  mg_http_listen(&mgr, "http://0.0.0.0:8080", httpHandler, NULL);

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, fileWatcher, NULL);

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  return 0;
}
