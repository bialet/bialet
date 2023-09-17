#define MG_ENABLE_DIRLIST 0

#include "bialet_wren.h"
#include "messages.h"
#include "mongoose.h"
#include "wren_vm.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define MAX_URL_LEN 200

/* Configs */
int port = 8080;
char *host = "localhost";
int output = 1;
int debug = 0;
char *rootDir = ".";

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

char *serverUrl() {
  char *url = malloc(MAX_URL_LEN);
  sprintf(url, "http://%s:%d", host, port);
  return url;
}

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue(serverUrl()));
}

int main() {

  struct mg_mgr mgr;

  bialetWrenInit();
  welcome();
  initConfig();

  mg_mgr_init(&mgr);
  // TODO Add host and port from params
  mg_http_listen(&mgr, serverUrl(), httpHandler, NULL);

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, fileWatcher, NULL);

  for (;;) {
    mg_mgr_poll(&mgr, 1000);
  }

  return 0;
}
