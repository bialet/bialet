#include "bialet_wren.h"
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

/* Configs */
int color = 1;
int output = 1;
int debug = 0;
char *rootDir = ".";

char *colorize(char *str, int color) {
  if (!color) {
    return str;
  }
  char *output = malloc(100);
  sprintf(output, "\033[%dm%s\033[0m", color, str);
  return output;
}

char *green(char *str) { return colorize(str, 32); }
char *red(char *str) { return colorize(str, 31); }
char *blue(char *str) { return colorize(str, 34); }
char *yellow(char *str) { return colorize(str, 33); }

void message_internal(int num, ...) {
  va_list args;
  va_start(args, num);

  for (int i = 0; i < num; ++i) {
    char *str = va_arg(args, char *);
    printf("%s", str);
    if (i < num - 1) {
      printf(" ");
    }
  }

  printf("\n");
  va_end(args);
}

#define message_1(x) message_internal(1, x)
#define message_2(x, y) message_internal(2, x, y)
#define message_3(x, y, z) message_internal(3, x, y, z)
#define message_4(w, x, y, z) message_internal(4, w, x, y, z)
#define message_5(v, w, x, y, z) message_internal(5, v, w, x, y, z)
#define message_6(u, v, w, x, y, z) message_internal(6, u, v, w, x, y, z)
#define message_7(t, u, v, w, x, y, z) message_internal(7, t, u, v, w, x, y, z)
#define message_8(s, t, u, v, w, x, y, z)                                      \
  message_internal(8, s, t, u, v, w, x, y, z)
#define message_9(r, s, t, u, v, w, x, y, z)                                   \
  message_internal(9, r, s, t, u, v, w, x, y, z)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define message(...)                                                           \
  GET_MACRO(__VA_ARGS__, message_9, message_8, message_7, message_6,           \
            message_5, message_4, message_3, message_2, message_1)             \
  (__VA_ARGS__)


static void httpHandler(struct mg_connection *c, int ev, void *ev_data,
                        void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    // TODO Add routing
    if (false) {
      /* if (mg_http_match_uri(hm, "/*.wren")) { */
      // TODO Read code from routing
    } else {
      struct mg_http_serve_opts opts = {.root_dir = ".",
                                        .ssi_pattern = "#.wren"};
      mg_http_serve_dir(c, hm, &opts);
    }
  }
}

static void initConfig() { message(green("Reload config")); }

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

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue("http://localhost:8080"));
}

int main() {

  struct mg_mgr mgr;

  bialetWrenInit();
  welcome();
  initConfig();

  mg_log_set(MG_LL_DEBUG);
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
