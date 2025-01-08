/*
 * This file is part of Bialet, which is licensed under the
 * GNU General Public License, version 2 (GPL-2.0).
 *
 * Copyright (c) 2023 Rodrigo Arce
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * For full license text, see LICENSE.md.
 */
#include "bialet.h"
#include "bialet_wren.h"
#include "messages.h"
#include "server.h"
#include "wren_vm.h"
#include <sys/types.h>
#include <unistd.h>

#ifdef IS_WIN

#include <winsock2.h>

#include <windows.h>

#include "getopt.h"
#include <stdio.h>
#include <tchar.h>

#define DIV 1048576
#define WIDTH 7
#define BUF_LEN 1024
#define FTW_F 1

#else

#include <ftw.h>
#include <pthread.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#endif

#define BIALET_VERSION "0.6-beta"
#define MEGABYTE (1024 * 1024)
#define MAX_URL 256
#define MAX_PATH_LEN 100
#define EXTENSION ".wren"
#define MIGRATION_FILE "/_migration" EXTENSION
#define MIGRATION_FILE_ALT "/_app/migration" EXTENSION
#define DB_FILE "_db.sqlite3"
#define ROUTE_FILE "_route" EXTENSION
#define MAX_ROUTES 100
#define IGNORED_FILES "README*,LICENSE*,*.json,*.yml,*.yaml"
#define WAIT_FOR_RELOAD 3
#define SERVER_POLL_DELAY 200

struct BialetConfig bialet_config;
time_t              last_reload = 0;

static void migrate() {
  char* code;
  char  path[MAX_PATH_LEN];
  char  altPath[MAX_PATH_LEN];
  strcpy(path, bialet_config.root_dir);
  strcat(path, MIGRATION_FILE);
  strcpy(altPath, bialet_config.root_dir);
  strcat(altPath, MIGRATION_FILE_ALT);
  if((code = readFile(path)) || (code = readFile(altPath))) {
    struct BialetResponse r = bialetRun("migration", code, 0);
    message(yellow("Running migration"), r.body);
  } else {
    bialetRun("migration", "import \"bialet\" for Db\nDb.init", 0);
  }
}

/* Reload files */
static void triggerReloadFiles() {
  time_t current_time = time(NULL);
  if(current_time - last_reload > WAIT_FOR_RELOAD) {
    last_reload = current_time;
    migrate();
  }
}

#ifdef IS_UNIX
static void* fileWatcher(void* arg) {
  pthread_detach(pthread_self());
  int   length, i = 0;
  char  buffer[BUF_LEN];
  int   fd = inotify_init();
  char* ext;
  if(fd < 0) {
    perror("inotify_init");
  }
  /* @TODO File watchers not work when a new folder is created */
  int wd = inotify_add_watch(fd, bialet_config.root_dir, IN_MODIFY);
  if(wd < 0) {
    perror("inotify_add_watch");
  }
  for(;;) {
    length = read(fd, buffer, BUF_LEN);

    if(length < 0) {
      perror("read");
    }

    while(i < length) {
      struct inotify_event* event = (struct inotify_event*)&buffer[i];
      if(event->len) {
        ext = strrchr(event->name, '.');
        // Only reload .wren files
        if(ext && !strcmp(ext, EXTENSION)) {
          triggerReloadFiles();
        }
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }
  pthread_exit(NULL);
}
#endif

char* serverUrl() {
  static char url[MAX_URL];
  snprintf(url, MAX_URL, "http://%s:%d", bialet_config.host, bialet_config.port);
  return url;
}

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue(serverUrl()));
}

void sigintHandler(int signum) {
  stop_server();
  _exit(0);
}

int main(int argc, char* argv[]) {
  char* code = "";

#ifdef IS_UNIX
  pid_t         pid;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;
  pthread_t     thread_id;

#endif
  /* Default config values */
  /* Arg config values */
  bialet_config.root_dir = ".";
  bialet_config.host = "127.0.0.1";
  bialet_config.port = 7000;
  bialet_config.log_file = stdout;
  bialet_config.mem_soft_limit = 50;
  bialet_config.mem_hard_limit = 100;
  bialet_config.cpu_soft_limit = 15;
  bialet_config.cpu_hard_limit = 30;
  /* Env config values */
  bialet_config.debug = 0;
  bialet_config.output_color = 1;
  bialet_config.db_path = DB_FILE;
  bialet_config.ignored_files = IGNORED_FILES;

  /* Parse args */

  int opt;
  while((opt = getopt(argc, argv, "h:p:l:d:m:M:c:C:r:i:v")) != -1) {
    switch(opt) {
      case 'h':
        bialet_config.host = optarg;
        break;
      case 'p':
        bialet_config.port = atoi(optarg);
        break;
      case 'l':
        if((bialet_config.log_file = fopen(optarg, "a")) == NULL) {
          perror("Error opening log file");
          exit(EXIT_FAILURE);
        }
        bialet_config.output_color = 0;
        break;
      case 'd':
        bialet_config.db_path = optarg;
        break;
      case 'i':
        bialet_config.ignored_files = optarg;
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
      case 'r':
        code = optarg;
        break;
      case 'v':
        printf("bialet %s\n", BIALET_VERSION);
        exit(0);
        break;
      default:
        fprintf(stderr, BIALET_USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if(optind < argc) {
    bialet_config.root_dir = argv[optind];
    /* @TODO Error handling root dir exists */
  }

  messageInit(&bialet_config);
  bialetInit(&bialet_config);
  if(strcmp(code, "") != 0) {
    migrate();
    exit(bialetRunCli(code));
  }

  if(start_server(&bialet_config) != 0) {
    perror("Error starting bialet");
    exit(1);
  }

  signal(SIGINT, sigintHandler);
  signal(SIGABRT, sigintHandler);
  signal(SIGTERM, sigintHandler);
  welcome();
  triggerReloadFiles();

#ifdef IS_UNIX
  int status;
  pthread_create(&thread_id, NULL, fileWatcher, NULL);

  mem_limit.rlim_cur = bialet_config.mem_soft_limit * MEGABYTE;
  mem_limit.rlim_max = bialet_config.mem_hard_limit * MEGABYTE;
  cpu_limit.rlim_cur = bialet_config.cpu_soft_limit;
  cpu_limit.rlim_max = bialet_config.cpu_hard_limit;

  for(;;) {
    pid = fork();
    if(pid == 0) {
      // Set cpu time and memory limit
      if(setrlimit(RLIMIT_AS, &mem_limit) == -1 ||
         setrlimit(RLIMIT_CPU, &cpu_limit) == -1) {
        perror("setrlimit");
        exit(1);
      }
      for(;;) {
        server_poll(SERVER_POLL_DELAY);
      }
    } else if(pid > 0) {
      // Parent: Wait for child to exit
      wait(&status);
      if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        break;
      } else {
        message(red("Error"), "Restarting");
      }
    } else {
      perror("fork");
      exit(1);
    }
  }
#endif

#ifdef IS_WIN
  for(;;) {
    server_poll(SERVER_POLL_DELAY);
  }
#endif

  return 0;
}
