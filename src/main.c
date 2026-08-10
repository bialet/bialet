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
#include "livereload.h"
#include "messages.h"
#include "server.h"
#include "show_errors.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

#include <ftw.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>

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

// Bialet logo is a bycicle however there is no emoji support on Windows terminal.
// We will use a dash instead, empty logo looks bad as well.
#if IS_WIN
#define BIALET_LOGO "-"
#else
#define BIALET_LOGO "🚲"
#endif

struct BialetConfig    bialet_config;
time_t                 last_reload = 0;
static volatile int8_t keep_running = 1;
static int             cron_installed = 0;
static char*           cron_code = 0;

// run_mutex serializes every background bialet_run against the shared SQLite
// handle: cron ticks, migrations, and file-watch-triggered runs all go through
// the single parent-process connection. The cron thread and the dmon
// (file-watch) thread must not drive the global sqlite3* concurrently, and the
// pthread_atfork prepare handler takes the same lock so a fork() cannot land
// while a background thread is inside SQLite/Wren. cron_code/cron_installed are
// also written by the dmon thread and read by the cron thread; the mutex
// prevents torn reads and use-after-free when a cron file is replaced while a
// cron tick is running.
#if !IS_WIN
static pthread_mutex_t run_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void migrate() {
  char* code;
  char  path[MAX_PATH_LEN];
  char  altPath[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s%s", bialet_config.root_dir, MIGRATION_FILE);
  snprintf(altPath, sizeof(altPath), "%s%s", bialet_config.root_dir,
           MIGRATION_FILE_ALT);
#if !IS_WIN
  pthread_mutex_lock(&run_mutex);
#endif
  if((code = read_file(path)) || (code = read_file(altPath))) {
    struct BialetResponse r = bialet_run("migration", code, 0);
    message(yellow("Migration start"), r.body);
  } else {
    bialet_run("migration", "Db.init", 0);
  }
#if !IS_WIN
  pthread_mutex_unlock(&run_mutex);
#endif
}

static void install_cron() {
  char  path[MAX_PATH_LEN];
  char  altPath[MAX_PATH_LEN];
  char* new_code = 0;
  snprintf(path, sizeof(path), "%s%s", bialet_config.root_dir, CRON_FILE);
  snprintf(altPath, sizeof(altPath), "%s%s", bialet_config.root_dir, CRON_FILE_ALT);
  if((new_code = read_file(path)) == 0)
    new_code = read_file(altPath);
  if(new_code != 0) {
    message(yellow("Installing cron"));
  }
#if !IS_WIN
  pthread_mutex_lock(&run_mutex);
#endif
  free(cron_code);
  cron_code = new_code;
  cron_installed = (cron_code != 0);
#if !IS_WIN
  pthread_mutex_unlock(&run_mutex);
#endif
}

static void cron_run() {
#if !IS_WIN
  pthread_mutex_lock(&run_mutex);
#endif
  if(cron_installed && cron_code) {
    bialet_run("cron", cron_code, 0);
  }
#if !IS_WIN
  pthread_mutex_unlock(&run_mutex);
#endif
}

void* cron_thread(void* arg) {
  (void)arg;
  while(1) {
    cron_run();
    sleep(60);
  }
  return NULL;
}

#if !IS_WIN
// The Linux parent forks the HTTP child while the cron and dmon threads may be
// inside SQLite/Wren. The child inherits copies of whatever mutexes those
// threads hold (locked forever, since the owner thread does not exist in the
// child) and torn heap. Taking run_mutex before fork and releasing it in both
// parent and child guarantees no background bialet_run is in flight at fork
// time, so the child's bialet_reopen_db() and later request handling never
// touch a locked SQLite handle.
static void atfork_prepare(void) {
  pthread_mutex_lock(&run_mutex);
}

static void atfork_parent(void) {
  pthread_mutex_unlock(&run_mutex);
}

static void atfork_child(void) {
  pthread_mutex_unlock(&run_mutex);
}
#endif

/* Reload files */
static void trigger_reload_files(const char* filepath) {
  time_t current_time = time(NULL);
  if(current_time - last_reload > WAIT_FOR_RELOAD) {
    last_reload = current_time;
    if(filepath == NULL) {
      migrate();
      install_cron();
      return;
    }
    if(!strcmp(filepath, "_migration" BIALET_EXTENSION) ||
       !strcmp(filepath, "_app/migration" BIALET_EXTENSION)) {
      migrate();
    }
    if(!strcmp(filepath, "_cron" BIALET_EXTENSION) ||
       !strcmp(filepath, "_app/cron" BIALET_EXTENSION)) {
      install_cron();
    }
  }
}

#define DMON_IMPL
#include "dmon.h"

static void dmon_callback(dmon_watch_id watch_id, dmon_action action,
                          const char* rootdir, const char* filepath,
                          const char* oldfilepath, void* user) {
  (void)watch_id;
  (void)action;
  (void)rootdir;
  (void)oldfilepath;
  (void)user;
  if(filepath) {
    livereload_notify();
    const char* ext = strrchr(filepath, '.');
    if(ext && !strcmp(ext, BIALET_EXTENSION)) {
      trigger_reload_files(filepath);
    }
  }
}

char* server_url(int port) {
  static char url[MAX_URL];
  snprintf(url, MAX_URL, "http://%s:%d", bialet_config.host, port);
  return url;
}

void welcome(int port) {
  message(BIALET_LOGO, green("bialet"), "is riding on", blue(server_url(port)));
}

static void open_browser(const char* url) {
#if IS_WIN
  char cmd[MAX_URL + 20];
  snprintf(cmd, sizeof(cmd), "start %s", url);
  system(cmd);
#else
  pid_t pid = fork();
  if(pid == 0) {
#if IS_MAC
    execlp("open", "open", url, (char*)NULL);
#else
    execlp("xdg-open", "xdg-open", url, (char*)NULL);
#endif
    _exit(1);
  }
#endif
}

void sigint_handler(int signum) {
  (void)signum;
  keep_running = 0;
  stop_server();
}

int main(int argc, char* argv[]) {
  char* code = "";
  char* validate_file = NULL;
  char* test_dir = NULL;
  int   run_tests = 0;
  int   dev_mode = 0;
#if !IS_WIN
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
#else
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);
  signal(SIGABRT, sigint_handler);
#endif

#if !IS_WIN
  // Register before any threads are created so every fork (including
  // open_browser) is bracketed by the run_mutex prepare/parent/child handlers.
  if(pthread_atfork(atfork_prepare, atfork_parent, atfork_child) != 0) {
    fprintf(stderr, "pthread_atfork failed\n");
    exit(EXIT_FAILURE);
  }
#endif

#if IS_LINUX
  pid_t         pid;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;

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
  bialet_config.max_post_size = 128 * 1024;        // Default 128KB
  /* SQLite pragma defaults */
  bialet_config.sqlite_foreign_keys = 1; // ON
  bialet_config.sqlite_synchronous = 1;  // NORMAL

  /* Parse args */

  // `dev` subcommand. The bundled getopt() is BSD-style: it stops at the first
  // non-option argument, so `dev -p 8080` would leave -p unparsed. Shift a
  // leading `dev` out of argv so trailing options still work; `-p 8080 dev`
  // (dev after options) is handled after the getopt loop below.
  if(argc > 1 && strcmp(argv[1], "dev") == 0) {
    dev_mode = 1;
    for(int i = 1; i < argc - 1; i++)
      argv[i] = argv[i + 1];
    argc--;
  }

  int opt;
  while((opt = getopt(argc, argv, "h:p:l:d:b:m:M:c:C:r:i:t:Tvw")) != -1) {
    switch(opt) {
      case 'h':
        bialet_config.host = optarg;
        break;
      case 'b': {
        char* endptr;
        long  post_kb = strtol(optarg, &endptr, 10);
        if(*endptr != '\0' || post_kb <= 0) {
          fprintf(stderr, "Invalid max post size: %s (use kilobytes, e.g. 128)\n",
                  optarg);
          exit(EXIT_FAILURE);
        }
        bialet_config.max_post_size = (size_t)post_kb * 1024;
      } break;
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
    if(strcmp(argv[optind], "dev") == 0) {
      dev_mode = 1;
      optind++;
    }
  }
  if(optind < argc) {
    bialet_config.root_dir = argv[optind];
  }

  // Set up temporary database for tests
  char temp_db_path[PATH_MAX];
  if(run_tests) {
    bialet_config.enable_tests = 1;
#if !IS_WIN
    // mkstemp() creates the file atomically with O_EXCL, so a local attacker
    // cannot pre-place a symlink at a predictable PID-based path and redirect
    // the test DB writes onto an arbitrary victim file.
    snprintf(temp_db_path, sizeof(temp_db_path), "/tmp/bialet_test_XXXXXX");
    int temp_db_fd = mkstemp(temp_db_path);
    if(temp_db_fd < 0) {
      perror("mkstemp");
      exit(EXIT_FAILURE);
    }
    close(temp_db_fd);
#else
    // GetTempFileNameA creates the file with exclusive (CREATE_NEW) semantics
    // in the system temp directory and retries on collisions, the Windows
    // equivalent of mkstemp/O_EXCL. The old PID-based path was predictable and
    // let a local attacker pre-place a junction at it.
    char win_temp[MAX_PATH];
    if(GetTempPathA(sizeof(win_temp), win_temp) == 0) {
      perror("GetTempPathA");
      exit(EXIT_FAILURE);
    }
    if(GetTempFileNameA(win_temp, "bialet", 0, temp_db_path) == 0) {
      perror("GetTempFileNameA");
      exit(EXIT_FAILURE);
    }
#endif
    bialet_config.db_path = temp_db_path;

    // If test_dir was specified, set it as root_dir for resolution
    if(test_dir != NULL) {
      bialet_config.root_dir = test_dir;
    }
  }

  // Cap the request-body size at the smaller of the configured max post size
  // and a memory-safe ceiling (soft limit / 512). Wren body parsing allocates
  // ~160 bytes of address space per body line (one ObjString each), so without
  // this ceiling a body allowed by -b could push the child past the enforced
  // RLIMIT_AS budget. At the default 50MB soft limit the ceiling is 100KB, so
  // -b 128 is effective once the soft limit reaches 64MB.
  {
    size_t mem_safe_post =
        ((size_t)bialet_config.mem_soft_limit * 1024 * 1024) / 512;
    if(bialet_config.max_post_size > mem_safe_post) {
      bialet_config.max_post_size = mem_safe_post;
    }
  }

  char resolved_root[MAX_PATH_LEN];
  if(realpath_n(bialet_config.root_dir, resolved_root, sizeof(resolved_root)) ==
     NULL) {
    fprintf(stderr, "Error: app directory not found: %s\n", bialet_config.root_dir);
    fprintf(stderr,
            "Run bialet from inside your app folder, or pass the folder as an "
            "argument.\n");
    if(!dev_mode) {
      fprintf(stderr, "\nDid you mean: bialet dev\n");
      fprintf(stderr, "  Starts from the current directory with live reload, "
                      "error display, and your browser.\n");
    }
    exit(EXIT_FAILURE);
  }
  struct stat root_stat;
  if(stat(resolved_root, &root_stat) != 0 || !S_ISDIR(root_stat.st_mode)) {
    fprintf(stderr, "Error: not a directory: %s\n", bialet_config.root_dir);
    exit(EXIT_FAILURE);
  }
  bialet_config.full_root_dir = resolved_root;

  message_init(&bialet_config);
  bialet_init(&bialet_config);
  if(strcmp(code, "") != 0) {
    exit(bialet_run_cli(code));
  }

  if(validate_file != NULL) {
    int result = bialet_validate_syntax(validate_file);
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
    int result = bialet_run_tests(test_dir, bialet_config.root_dir);

    // Clean up temp database
    bialet_cleanup();
    unlink(temp_db_path);

    exit(result);
  }

  int port = start_server(&bialet_config);
  if(port < 0) {
    perror("Error starting bialet");
    exit(1);
  }

  welcome(port);
  trigger_reload_files(NULL);
  if(dev_mode)
    bialet_enable_dev_flags();
  livereload_init();
  show_errors_init();
  if(dev_mode)
    open_browser(server_url(port));

#if IS_LINUX
  int       status;
  pthread_t cron_tid;
  pthread_create(&cron_tid, NULL, cron_thread, NULL);

  dmon_init();
  dmon_watch(bialet_config.full_root_dir, dmon_callback, DMON_WATCHFLAGS_RECURSIVE,
             NULL);

  mem_limit.rlim_cur = bialet_config.mem_soft_limit * MEGABYTE;
  mem_limit.rlim_max = bialet_config.mem_hard_limit * MEGABYTE;
  cpu_limit.rlim_cur = bialet_config.cpu_soft_limit;
  cpu_limit.rlim_max = bialet_config.cpu_hard_limit;

  for(;;) {
    pid = fork();
    if(pid == 0) {
      // The sqlite connection was opened in the parent before fork() and is
      // still used by the parent's cron/dmon threads. SQLite forbids sharing
      // a connection across fork(); open our own fresh connection so the HTTP
      // child never touches the shared pre-fork handle.
      bialet_reopen_db();
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
      // Parent: wait for the HTTP child specifically. `wait()` would also
      // reap the browser child forked by open_browser() in dev mode and, if
      // that one exited cleanly first, tear down dmon and exit while the
      // HTTP child still serves.
      waitpid(pid, &status, 0);
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

  dmon_deinit();
#endif

#if !IS_LINUX
  dmon_init();
  dmon_watch(bialet_config.full_root_dir, dmon_callback, DMON_WATCHFLAGS_RECURSIVE,
             NULL);

  time_t last_cron = time(NULL);
  while(keep_running) {
    server_poll(SERVER_POLL_DELAY);
    time_t now = time(NULL);
    if(difftime(now, last_cron) >= 60) {
      cron_run();
      last_cron = now;
    }
  }

  dmon_deinit();
#endif

  return 0;
}
