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
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))
#define MAX_URL_LEN 200
#define MEGABYTE (1024 * 1024)

/* Configs */
int port = 8080;
char *host = "localhost";
int output = 1;
int debug = 0;
char *root_dir = ".";
char *db_path = ".db.sqlite3";

static void http_handler(struct mg_connection *c, int ev, void *ev_data,
                        void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_http_serve_opts opts = {.root_dir = root_dir,
                                      .ssi_pattern = "#.wren"};
    mg_http_serve_dir(c, hm, &opts);
  }
}

/* Reload files */
static void trigger_reload_files() {
    // Migration
    char *code;
    if ((code = bialet_read_file("_migration.wren"))) {
        message(yellow("Migration"));
        bialet_run("migration", code, 0);
    }
}

static void *file_watcher(void *arg) {
  int length, i = 0;
  char buffer[BUF_LEN];
  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
  }
  // TODO Add watcher for new folders
  int wd = inotify_add_watch(fd, root_dir, IN_MODIFY);
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
            if (event->name[0] != '.')
              trigger_reload_files();
        }
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }
}

char *server_url() {
  char *url = malloc(MAX_URL_LEN);
  sprintf(url, "http://%s:%d", host, port);
  return url;
}

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue(server_url()));
}

int main(int argc, char *argv[]) {
  pid_t pid;
  int status;
  struct rlimit mem_limit;
  mem_limit.rlim_cur = 50 * MEGABYTE;  // 50 MB soft limit
  mem_limit.rlim_max = 100 * MEGABYTE; // 100 MB hard limit
  struct rlimit cpu_limit;
  cpu_limit.rlim_cur = 15;
  cpu_limit.rlim_max = 30;

  struct mg_mgr mgr;

  bialet_init(db_path);
  mg_mgr_init(&mgr);

  welcome();
  trigger_reload_files();

  mg_http_listen(&mgr, server_url(), http_handler, NULL);

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, file_watcher, NULL);

  for (;;) {
    pid = fork();
    if (pid == 0) {
      // Set cpu time and memory limit
      if (setrlimit(RLIMIT_AS, &mem_limit) == -1 ||
          setrlimit(RLIMIT_CPU, &cpu_limit) == -1) {
        perror("setrlimit");
        exit(1);
      }
      for (;;) {
        mg_mgr_poll(&mgr, 1000);
      }
    } else if (pid > 0) {
      // Parent: Wait for child to exit
      wait(&status);
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        break;
      } else {
        message(red("Error"), "Restarting");
      }
    } else {
      perror("fork");
      exit(1);
    }
  }

  return 0;
}
