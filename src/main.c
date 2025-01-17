/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2025 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
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
#include <sys/resource.h>
#include <sys/wait.h>

#endif

#define BIALET_VERSION "0.7-beta"
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

struct BialetConfig  bialet_config;
time_t               last_reload = 0;
static volatile bool keep_running = true;

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

char* serverUrl() {
  static char url[MAX_URL];
  snprintf(url, MAX_URL, "http://%s:%d", bialet_config.host, bialet_config.port);
  return url;
}

void welcome() {
  message("🚲", green("bialet"), "is riding on", blue(serverUrl()));
}

void sigintHandler(int signum) {
  keep_running = false;
  stop_server();
}

int main(int argc, char* argv[]) {
  char*            code = "";
  struct sigaction sa;
  sa.sa_handler = sigintHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);

#ifdef IS_UNIX
  pid_t         pid;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;

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

  welcome();
  triggerReloadFiles();

#ifdef IS_UNIX
  int status;
  struct rlimit sys_limit;

  mem_limit.rlim_cur = bialet_config.mem_soft_limit * MEGABYTE;
  mem_limit.rlim_max = bialet_config.mem_hard_limit * MEGABYTE;
  cpu_limit.rlim_cur = bialet_config.cpu_soft_limit;
  cpu_limit.rlim_max = bialet_config.cpu_hard_limit;

  printf("Memory limit: %d MB\n", bialet_config.mem_soft_limit);
  printf("CPU limit: %d%%\n", bialet_config.cpu_soft_limit);

    if (getrlimit(RLIMIT_AS, &sys_limit) == 0) {
         printf("System RLIMIT_AS: soft=%lu, hard=%lu\n", sys_limit.rlim_cur, sys_limit.rlim_max);
        if (mem_limit.rlim_max > sys_limit.rlim_max) {
            mem_limit.rlim_max = sys_limit.rlim_max;
        }
        if (mem_limit.rlim_cur > mem_limit.rlim_max) {
            mem_limit.rlim_cur = mem_limit.rlim_max;
        }
    }

    if (getrlimit(RLIMIT_CPU, &sys_limit) == 0) {
        if (cpu_limit.rlim_max > sys_limit.rlim_max) {
            cpu_limit.rlim_max = sys_limit.rlim_max;
        }
        if (cpu_limit.rlim_cur > cpu_limit.rlim_max) {
            cpu_limit.rlim_cur = cpu_limit.rlim_max;
        }
    }

    for (int i = 0; i < 5; i++) {  // Limit the number of forks
        pid = fork();
        if (pid == 0) {
            // Simulate child process workload
                  while(keep_running) {
                              server_poll(SERVER_POLL_DELAY);
                                    }
                  exit(0);

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
