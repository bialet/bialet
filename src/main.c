/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2026 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#include "bialet.h"
#include "bialet_wren.h"
#include "messages.h"
#include "server.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#if IS_WIN

#include <winsock2.h>

#include <windows.h>

#include "getopt.h"
#include <signal.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

#define DIV 1048576
#define WIDTH 7
#define BUF_LEN 1024
#define FTW_F 1

#else

#if !IS_MAC
#include <sys/inotify.h>
#endif

#include <ftw.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#endif

#define MEGABYTE (1024 * 1024)
#define MAX_URL 256
#define MAX_PATH_LEN 100
#define MIGRATION_FILE "/_migration" BIALET_EXTENSION
#define MIGRATION_FILE_ALT "/_app/migration" BIALET_EXTENSION
#define CRON_FILE "/_cron" BIALET_EXTENSION
#define CRON_FILE_ALT "/_app/cron" BIALET_EXTENSION
#define DB_FILE "_db.sqlite3"
#define ROUTE_FILE "_route" BIALET_EXTENSION
#define MAX_ROUTES 100
#define IGNORED_FILES "README*,AGENTS*,LICENSE*,*.json,*.yml,*.yaml"
#define WAIT_FOR_RELOAD 3
#define SERVER_POLL_DELAY 200

struct BialetConfig    bialet_config;
time_t                 last_reload = 0;
static volatile int8_t keep_running = 1;
static int             cron_installed = 0;
static char*           cron_code = 0;

static void migrate() {
  char* code;
  char  path[MAX_PATH_LEN];
  char  altPath[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s%s", bialet_config.root_dir, MIGRATION_FILE);
  snprintf(altPath, sizeof(altPath), "%s%s", bialet_config.root_dir,
           MIGRATION_FILE_ALT);
  if((code = readFile(path)) || (code = readFile(altPath))) {
    struct BialetResponse r = bialetRun("migration", code, 0);
    message(yellow("Running migration"), r.body);
  } else {
    bialetRun("migration", "Db.init", 0);
  }
}

static void cron_install() {
  char path[MAX_PATH_LEN];
  char altPath[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s%s", bialet_config.root_dir, CRON_FILE);
  snprintf(altPath, sizeof(altPath), "%s%s", bialet_config.root_dir,
           CRON_FILE_ALT);
  if((cron_code = readFile(path)) || (cron_code = readFile(altPath))) {
    message(yellow("Installing cron"));
    cron_installed = 1;
  } else {
    cron_installed = 0;
  }
}

static void cron_run() {
  if(cron_installed) {
    bialetRun("cron", cron_code, 0);
  }
}

void* cron_thread(void* arg) {
  (void)arg;
  while(1) {
    cron_run();
    sleep(60);
  }
  return NULL;
}

/* Reload files */
static void triggerReloadFiles() {
  time_t current_time = time(NULL);
  if(current_time - last_reload > WAIT_FOR_RELOAD) {
    last_reload = current_time;
    migrate();
    cron_install();
  }
}

#if IS_LINUX
static void* fileWatcher(void* arg) {
  (void)arg;
  pthread_detach(pthread_self());
  int   length, i = 0;
  char  buffer[BUF_LEN];
  int   fd = inotify_init();
  char* ext;
  if(fd < 0) {
    perror("inotify_init");
  }
  /* Note: This only watches the root directory. To properly watch subdirectories,
   * we would need to either:
   * 1. Recursively add inotify watches for all subdirectories at startup
   * 2. Listen for IN_CREATE events and dynamically add watches for new directories
   * For now, only files directly in root_dir will trigger auto-reload. */
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
        if(ext && !strcmp(ext, BIALET_EXTENSION)) {
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

char* serverUrl(int port) {
  static char url[MAX_URL];
  snprintf(url, MAX_URL, "http://%s:%d", bialet_config.host, port);
  return url;
}

void welcome(int port) {
  message("🚲", green("bialet"), "is riding on", blue(serverUrl(port)));
}

void sigintHandler(int signum) {
  (void)signum;
  keep_running = 0;
  stop_server();
}

int main(int argc, char* argv[]) {
  char*            code = "";
  char*            validate_file = NULL;
  char*            test_dir = NULL;
  int              run_tests = 0;
#if !IS_WIN
  struct sigaction sa;
  sa.sa_handler = sigintHandler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
#else
  signal(SIGINT, sigintHandler);
  signal(SIGTERM, sigintHandler);
  signal(SIGABRT, sigintHandler);
#endif

#if IS_LINUX
  pid_t         pid;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;
  pthread_t     thread_id;

#endif
  /* Default config values */
  /* Arg config values */
  bialet_config.root_dir = ".";
  bialet_config.host = BIALET_DEFAULT_HOST;
  bialet_config.port = -1;
  bialet_config.log_file = stdout;
  bialet_config.mem_soft_limit = 50;
  bialet_config.mem_hard_limit = 100;
  bialet_config.cpu_soft_limit = 15;
  bialet_config.cpu_hard_limit = 30;
  /* Env config values */
  bialet_config.debug = 0;
  bialet_config.output_color = 1;
  bialet_config.db_path = DB_FILE;
  bialet_config.wal_mode = 0;
  bialet_config.ignored_files = IGNORED_FILES;
  bialet_config.max_upload_size = 2 * 1024 * 1024; // Default 2MB
  /* SQLite pragma defaults */
  bialet_config.sqlite_foreign_keys = 1; // ON
  bialet_config.sqlite_synchronous = 1;  // NORMAL

  /* Parse args */

  int opt;
  while((opt = getopt(argc, argv, "h:p:l:d:m:M:c:C:r:i:t:Tvw")) != -1) {
    switch(opt) {
      case 'h':
        bialet_config.host = optarg;
        break;
      case 'p': {
        char* endptr;
        long  port_val = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || port_val < 0 || port_val > 65535) {
          fprintf(stderr, "Invalid port number: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.port = (int)port_val;
      } break;
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
      case 'w':
        bialet_config.wal_mode = 1;
        break;
      case 'i':
        bialet_config.ignored_files = optarg;
        break;
      case 'm': {
        char* endptr;
        long  mem = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || mem < 0) {
          fprintf(stderr, "Invalid memory limit: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.mem_soft_limit = (int)mem;
      } break;
      case 'M': {
        char* endptr;
        long  mem = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || mem < 0) {
          fprintf(stderr, "Invalid memory limit: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.mem_hard_limit = (int)mem;
      } break;
      case 'c': {
        char* endptr;
        long  cpu = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || cpu < 0) {
          fprintf(stderr, "Invalid CPU limit: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.cpu_soft_limit = (int)cpu;
      } break;
      case 'C': {
        char* endptr;
        long  cpu = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || cpu < 0) {
          fprintf(stderr, "Invalid CPU limit: %s\n", optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.cpu_hard_limit = (int)cpu;
      } break;
      case 'r':
        code = optarg;
        break;
      case 't':
        validate_file = optarg;
        break;
      case 'T':
        run_tests = 1;
        if(optind < argc && argv[optind][0] != '-') {
          test_dir = argv[optind];
          optind++;
        }
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
  }

  // Set up temporary database for tests
  char temp_db_path[MAX_PATH_LEN];
  if(run_tests) {
    bialet_config.enable_tests = 1;
    snprintf(temp_db_path, sizeof(temp_db_path), "/tmp/bialet_test_%d.sqlite3",
             getpid());
    bialet_config.db_path = temp_db_path;

    // If test_dir was specified, set it as root_dir for resolution
    if(test_dir != NULL) {
      bialet_config.root_dir = test_dir;
    }
  }

  char resolved_root[MAX_PATH_LEN];
  if(realpath(bialet_config.root_dir, resolved_root) == NULL) {
    fprintf(stderr, "Error with root directory.\n");
    exit(EXIT_FAILURE);
  }
  bialet_config.full_root_dir = resolved_root;

  messageInit(&bialet_config);
  bialetInit(&bialet_config);
  if(strcmp(code, "") != 0) {
    exit(bialetRunCli(code));
  }

  if(validate_file != NULL) {
    int result = bialetValidateSyntax(validate_file);
    if(result == 0) {
      printf("✓ Syntax OK: %s\n", validate_file);
    } else {
      fprintf(stderr, "✗ Syntax errors found in: %s\n", validate_file);
    }
    exit(result);
  }

  if(run_tests) {
    // Run migrations on temp database
    migrate();

    // Run tests
    if(test_dir == NULL) {
      test_dir = bialet_config.root_dir;
    }
    int result = bialetRunTests(test_dir, bialet_config.root_dir);

    // Clean up temp database
    bialetCleanup();
    unlink(temp_db_path);

    exit(result);
  }

  int port = start_server(&bialet_config);
  if(port < 0) {
    perror("Error starting bialet");
    exit(1);
  }

  welcome(port);
  triggerReloadFiles();

#if IS_LINUX
  int       status;
  pthread_t cron_tid;
  pthread_create(&cron_tid, NULL, cron_thread, NULL);
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

#if !IS_LINUX
  time_t last_cron = time(NULL);
  while(keep_running) {
    server_poll(SERVER_POLL_DELAY);
    time_t now = time(NULL);
    if(difftime(now, last_cron) >= 60) {
      cron_run();
      last_cron = now;
    }
  }
#endif

  return 0;
}
