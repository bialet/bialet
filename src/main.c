#ifdef _WIN32
#define IS_WIN 1
#else
#define IS_UNIX 1
#endif

#ifdef IS_WIN

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define DIV 1048576 
#define WIDTH 7
#define BUF_LEN 1024
#define FTW_F 1

#else

#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/wat.h>
#include <unistd.h>
#include <pthread.h>
#include <ftw.h>


#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#endif

#include <errno.h>
#include <sys/types.h>


#include "bialet.h"
#include "bialet_wren.h"
#include "messages.h"
#include "mongoose.h"
#include "wren_vm.h"

#define BIALET_VERSION "0.4"
#define MAX_URL_LEN 200
#define MEGABYTE (1024 * 1024)
#define MAX_PATH_LEN 100
#define EXTENSION ".wren"
#define MIGRATION_FILE "/_migration" EXTENSION
#define MIGRATION_FILE_ALT "/_app/migration" EXTENSION
#define DB_FILE "_db.sqlite3"
#define ROUTE_FILE "_route" EXTENSION
#define MAX_ROUTES 100
#define MAX_IGNORED_FILES 20
#define IGNORED_FILES "README*,LICENSE*,*.json,*.yml,*.yaml"
#define WAIT_FOR_RELOAD 3

struct BialetConfig bialet_config;
char *routes_list[MAX_ROUTES];
char *routes_files[MAX_ROUTES];
char *ignored_list[MAX_IGNORED_FILES];
int routes_index = 0;
int ignored_files_index = 0;
time_t last_reload = 0;

static void http_handler(struct mg_connection *c, int ev, void *ev_data,
                         void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_http_serve_opts opts = {.root_dir = bialet_config.root_dir,
                                      .ssi_pattern = "#" EXTENSION};
    for (int i = 0; i < routes_index; i++) {
      if (mg_http_match_uri(hm, routes_list[i])) {
        hm->bialet_routes = strdup(routes_list[i]);
        mg_http_serve_ssi(c, hm, bialet_config.root_dir, routes_files[i]);
        return;
      }
    }
    for (int i = 0; i < ignored_files_index; i++) {
      if (mg_http_match_uri(hm, ignored_list[i])) {
        mg_http_reply(c, 404, BIALET_HEADERS, BIALET_NOT_FOUND_PAGE);
        return;
      }
    }
    hm->bialet_routes = "";
    mg_http_serve_dir(c, hm, &opts);
  }
}

static void migrate() {
  char *code;
  char path[MAX_PATH_LEN];
  char altPath[MAX_PATH_LEN];
  strcpy(path, bialet_config.root_dir);
  strcat(path, MIGRATION_FILE);
  strcpy(altPath, bialet_config.root_dir);
  strcat(altPath, MIGRATION_FILE_ALT);
  if ((code = bialet_read_file(path)) || (code = bialet_read_file(altPath))) {
    struct BialetResponse r = bialet_run("migration", code, 0);
    message(yellow("Running migration"), r.body);
  } else {
    bialet_run("migration", "import \"bialet\" for Db\nDb.init", 0);
  }
}

static int parse_routes_callback(const char *fpath, const struct stat *sb,
                                 int typeflag) {
  if (typeflag == FTW_F && strstr(fpath, ROUTE_FILE)) {
    routes_files[routes_index] = strdup(fpath);
    char *relative_path = strstr(fpath, bialet_config.root_dir) +
                          strlen(bialet_config.root_dir) + 1;
    char *last_slash = strrchr(relative_path, '/');
    *last_slash = '\0';

    char *route_with_hash = malloc(strlen(relative_path) + 3);
    sprintf(route_with_hash, "/%s#", relative_path);
    routes_list[routes_index] = route_with_hash;
    routes_index++;
  }
  return 0;
}

static void parse_routes() {
  routes_index = 0;
  #ifdef IS_UNIX
  ftw(bialet_config.root_dir, parse_routes_callback, 16);
  #endif
}

static void parse_ignore(char *ignored_files_str) {
  char *token;
  ignored_files_index = 0;
  char *str = strdup(ignored_files_str);
  char file[MAX_PATH_LEN];
  token = strtok(str, ",");
  while (token != NULL) {
    // Append / to ignore files
    strcpy(file, "/");
    strcat(file, token);
    ignored_list[ignored_files_index] = strdup(file);
    ignored_files_index++;
    token = strtok(NULL, ",");
  }
}

/* Reload files */
static void trigger_reload_files() {
  time_t current_time = time(NULL);
  if (current_time - last_reload > WAIT_FOR_RELOAD) {
    last_reload = current_time;
    migrate();
    parse_routes();
  }
}

static void *file_watcher(void *arg) {
  #ifdef IS_UNIX
  pthread_detach(pthread_self());
  int length, i = 0;
  char buffer[BUF_LEN];
  int fd = inotify_init();
  char *ext;
  if (fd < 0) {
    perror("inotify_init");
  }
  /* @TODO File watchers not work when a new folder is created */
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
        ext = strrchr(event->name, '.');
        // Only reload .wren files
        if (ext && !strcmp(ext, EXTENSION)) {
          trigger_reload_files();
        }
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }
  pthread_exit(NULL);
  #endif
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
  int status, opt;
  char *code = "";
  char *ignored_files_str = IGNORED_FILES;
 
  #ifdef IS_UNIX
  pid_t pid;
  struct rlimit mem_limit;
  struct rlimit cpu_limit;
  pthread_t thread_id;

  #endif
  struct mg_mgr mgr;
  
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
  bialet_config.output_color = 1;
  bialet_config.db_path = DB_FILE;

  /* Parse args */
  
  #ifdef IS_UNIX
  while ((opt = getopt(argc, argv, "h:p:l:d:m:M:c:C:r:i:v")) != -1) {
    switch (opt) {
    case 'h':
      bialet_config.host = optarg;
      break;
    case 'p':
      bialet_config.port = atoi(optarg);
      break;
    case 'l':
      if ((bialet_config.log_file = fopen(optarg, "a")) == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
      }
      bialet_config.output_color = 0;
      break;
    case 'd':
      bialet_config.db_path = optarg;
      break;
    case 'i':
      ignored_files_str = optarg;
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
  if (optind < argc) {
    bialet_config.root_dir = argv[optind];
    /* @TODO Error handling root dir exists */
  }
  #endif

  message_init(&bialet_config);
  bialet_init(&bialet_config);
  if (strcmp(code, "") != 0) {
    migrate();
    exit(bialet_run_cli(code));
  }
  mg_mgr_init(&mgr);

  if (mg_http_listen(&mgr, server_url(), http_handler, NULL) == NULL) {
    perror("Error starting bialet");
    exit(1);
  }

  welcome();
  parse_ignore(ignored_files_str);
  trigger_reload_files();

  #ifdef IS_UNIX
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
  #endif

  #ifdef IS_WIN
      for (;;) {
        mg_mgr_poll(&mgr, 1000);
      }
  #endif

  return 0;
}
