#define MG_ENABLE_DIRLIST 0

#include "bialet.h"
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
#define MAX_PATH_LEN 100

struct BialetConfig bialet_config;

static void http_handler(struct mg_connection *c, int ev, void *ev_data,
                         void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_http_serve_opts opts = {.root_dir = bialet_config.root_dir,
                                      .ssi_pattern = "#.wren"};
    mg_http_serve_dir(c, hm, &opts);
  }
}

#define WAIT_FOR_RELOAD 3
time_t last_reload = 0;

/* Reload files */
static void trigger_reload_files() {
  time_t current_time = time(NULL);
  if (current_time - last_reload > WAIT_FOR_RELOAD) {
    last_reload = current_time;
    char *code;
    char path[MAX_PATH_LEN];
    strcpy(path, bialet_config.root_dir);
    strcat(path, "/_migration.wren");
    if ((code = bialet_read_file(path))) {
      struct BialetResponse r = bialet_run("migration", code, 0);
      message(yellow("Running migration"), r.body);
    }
  }
}

static void *file_watcher(void *arg) {
  pthread_detach(pthread_self());
  int length, i = 0;
  char buffer[BUF_LEN];
  int fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init");
  }
  // TODO Add watcher for new folders
  int wd = inotify_add_watch(fd, bialet_config.root_dir, IN_MODIFY);
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
          trigger_reload_files();
        }
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }
  pthread_exit(NULL);
}

char *server_url() {
  char *url = malloc(MAX_URL_LEN);
  sprintf(url, "http://%s:%d", bialet_config.host, bialet_config.port);
  return url;
}

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue(server_url()));
}

int main(int argc, char *argv[]) {
  pid_t pid;
  int status, opt;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;
  struct mg_mgr mgr;
  pthread_t thread_id;

  /* Default config values */
  /* Arg config values */
  bialet_config.root_dir = ".";
  bialet_config.host = "localhost";
  bialet_config.port = 7000;
  bialet_config.log_file = stdout;
  bialet_config.mem_soft_limit = 50;
  bialet_config.mem_hard_limit = 100;
  bialet_config.cpu_soft_limit = 15;
  bialet_config.cpu_hard_limit = 30;
  /* Env config values */
  bialet_config.debug = 0;
  bialet_config.db_path = ".db.sqlite3";

  /* Parse args */
  while ((opt = getopt(argc, argv, "h:p:l:m:M:c:C:")) != -1) {
    switch (opt) {
    case 'h':
      bialet_config.host = optarg;
      break;
    case 'p':
      bialet_config.port = atoi(optarg);
      break;
    case 'l':
      bialet_config.log_file = fopen(optarg, "rwb");
      break;
    case 'm':
      bialet_config.mem_soft_limit = atoi(optarg);
      break;
    case 'M':
      bialet_config.mem_hard_limit = atoi(optarg);
      break;
    case 'c':
      bialet_config.cpu_soft_limit = atoi(optarg);
      break;
    case 'C':
      bialet_config.cpu_hard_limit = atoi(optarg);
      break;
    default:
      fprintf(stderr,
              "🚲 bialet\nUsage: %s [-h host] [-p port] [-l log] root_dir\n",
              argv[0]);
      exit(EXIT_FAILURE);
    }
  }
  if (optind < argc) {
    bialet_config.root_dir = argv[optind];
  }

  bialet_init(&bialet_config);
  mg_mgr_init(&mgr);

  welcome();
  trigger_reload_files();

  if (mg_http_listen(&mgr, server_url(), http_handler, NULL) == NULL) {
    perror("Could not start server");
    exit(1);
  }

  pthread_create(&thread_id, NULL, file_watcher, NULL);

  mem_limit.rlim_cur = bialet_config.mem_soft_limit * MEGABYTE;
  mem_limit.rlim_max = bialet_config.mem_hard_limit * MEGABYTE;
  cpu_limit.rlim_cur = bialet_config.cpu_soft_limit;
  cpu_limit.rlim_max = bialet_config.cpu_hard_limit;

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
