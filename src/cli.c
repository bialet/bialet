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
#include "cli.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Options are identified by enum so short and long spellings dispatch to the
 * same handler. takes_arg: 0 = flag, 1 = required value, 2 = optional value
 * (only -T/--tests). */
typedef enum {
  CLI_OPT_PORT,
  CLI_OPT_HOST,
  CLI_OPT_HELP,
  CLI_OPT_RUN,
  CLI_OPT_VALIDATE,
  CLI_OPT_TESTS,
  CLI_OPT_VERSION,
  CLI_OPT_LOG,
  CLI_OPT_DB,
  CLI_OPT_WAL,
  CLI_OPT_IGNORE,
  CLI_OPT_MEM_SOFT,
  CLI_OPT_MEM_HARD,
  CLI_OPT_CPU_SOFT,
  CLI_OPT_CPU_HARD,
  CLI_OPT_MAX_POST,
  CLI_OPT_QUIET,
  CLI_OPT_COUNT
} CliOptId;

typedef struct {
  const char* long_name;
  char        short_name;
  int         takes_arg;
} CliOptSpec;

static const CliOptSpec cli_opts[] = {
    {"port", 'p', 1},     {"host", 'h', 1},     {"help", 'H', 0},
    {"run", 'r', 1},      {"validate", 't', 1}, {"tests", 'T', 2},
    {"version", 'v', 0},  {"log", 'l', 1},      {"db", 'd', 1},
    {"wal", 'w', 0},      {"ignore", 'i', 1},   {"mem-soft", 'm', 1},
    {"mem-hard", 'M', 1}, {"cpu-soft", 'c', 1}, {"cpu-hard", 'C', 1},
    {"max-post", 'b', 1}, {"quiet", 'q', 0},
};

static CliOptId short_to_id(char c) {
  for(int i = 0; i < CLI_OPT_COUNT; i++) {
    if(cli_opts[i].short_name == c)
      return (CliOptId)i;
  }
  return CLI_OPT_COUNT;
}

/* Variadic so the format attribute applies: GCC only accepts it on a variadic
 * function, and with it every caller's format string is checked. */
BIALET_PRINTF_FORMAT(2, 3)
static void cli_error(BialetCliOptions* opts, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(opts->error, sizeof(opts->error), fmt, ap);
  va_end(ap);
  opts->action = BIALET_CLI_INVALID;
}

static int path_exists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static void set_option(CliOptId id, const char* value, struct BialetConfig* config,
                       BialetCliOptions* opts) {
  char* endptr;
  long  num;
  switch(id) {
    case CLI_OPT_PORT:
      num = strtol(value, &endptr, 10);
      if(*endptr != '\0' || num < 0 || num > 65535) {
        cli_error(opts, "Invalid port number: %s", value);
        return;
      }
      config->port = (int)num;
      break;
    case CLI_OPT_HOST:
      config->host = (char*)value;
      break;
    case CLI_OPT_HELP:
      opts->action = BIALET_CLI_HELP;
      break;
    case CLI_OPT_RUN:
      opts->run_code = value;
      break;
    case CLI_OPT_VALIDATE:
      opts->validate_file = value;
      break;
    case CLI_OPT_TESTS:
      opts->run_tests = 1;
      if(value != NULL)
        opts->test_dir = value;
      break;
    case CLI_OPT_VERSION:
      opts->action = BIALET_CLI_VERSION;
      break;
    case CLI_OPT_LOG: {
      FILE* opened = fopen(value, "a");
      if(opened == NULL) {
        snprintf(opts->error, sizeof(opts->error), "Error opening log file %s: %s",
                 value, strerror(errno));
        opts->action = BIALET_CLI_INVALID;
        return;
      }
      /* Repeating -l leaked the previously opened stream. */
      if(config->log_file != NULL && config->log_file != stdout &&
         config->log_file != stderr) {
        fclose(config->log_file);
      }
      config->log_file = opened;
      config->output_color = 0;
    } break;
    case CLI_OPT_DB:
      config->db_path = (char*)value;
      break;
    case CLI_OPT_WAL:
      config->wal_mode = 1;
      break;
    case CLI_OPT_IGNORE:
      config->ignored_files = (char*)value;
      break;
    case CLI_OPT_MEM_SOFT:
    case CLI_OPT_MEM_HARD:
      num = strtol(value, &endptr, 10);
      if(*endptr != '\0' || num < 0) {
        cli_error(opts, "Invalid memory limit: %s", value);
        return;
      }
      if(id == CLI_OPT_MEM_SOFT)
        config->mem_soft_limit = (int)num;
      else
        config->mem_hard_limit = (int)num;
      break;
    case CLI_OPT_CPU_SOFT:
    case CLI_OPT_CPU_HARD:
      num = strtol(value, &endptr, 10);
      if(*endptr != '\0' || num < 0) {
        cli_error(opts, "Invalid CPU limit: %s", value);
        return;
      }
      if(id == CLI_OPT_CPU_SOFT)
        config->cpu_soft_limit = (int)num;
      else
        config->cpu_hard_limit = (int)num;
      break;
    case CLI_OPT_MAX_POST:
      num = strtol(value, &endptr, 10);
      if(*endptr != '\0' || num <= 0) {
        cli_error(opts, "Invalid max post size: %s (use kilobytes, e.g. 128)",
                  value);
        return;
      }
      config->max_post_size = (size_t)num * 1024;
      break;
    case CLI_OPT_QUIET:
      config->quiet = 1;
      config->output_color = 0;
      break;
    case CLI_OPT_COUNT:
      break;
  }
}

/* A bare `dev` or the first positional argument. */
static void handle_positional(const char* arg, struct BialetConfig* config,
                              BialetCliOptions* opts, int* root_set) {
  if(strcmp(arg, "dev") == 0) {
    opts->dev_mode = 1;
    return;
  }
  if(!*root_set) {
    config->root_dir = (char*)arg;
    *root_set = 1;
  }
}

static void parse_short_options(int* i, const char* arg, int argc, char* argv[],
                                struct BialetConfig* config,
                                BialetCliOptions*    opts) {
  for(size_t k = 1; arg[k] != '\0'; k++) {
    CliOptId id = short_to_id(arg[k]);
    if(id == CLI_OPT_COUNT) {
      char buf[3] = {'-', arg[k], '\0'};
      cli_error(opts, "Invalid parameter: %s", buf);
      return;
    }
    if(cli_opts[id].takes_arg) {
      const char* value;
      if(arg[k + 1] != '\0') {
        value = &arg[k + 1];
      } else if(cli_opts[id].takes_arg == 1) {
        if(*i + 1 < argc) {
          value = argv[++*i];
        } else {
          cli_error(opts, "Option requires an argument: %s", arg);
          return;
        }
      } else {
        /* optional value: consume the next argument only if it is not an
         * option, mirroring the historical -T behavior */
        if(*i + 1 < argc && argv[*i + 1][0] != '-')
          value = argv[++*i];
        else
          value = NULL;
      }
      set_option(id, value, config, opts);
      return;
    }
    set_option(id, NULL, config, opts);
    if(opts->action != BIALET_CLI_SERVE)
      return;
  }
}

static void parse_long_option(int* i, const char* arg, int argc, char* argv[],
                              struct BialetConfig* config, BialetCliOptions* opts,
                              int* root_set) {
  const char* name = arg + 2;
  const char* eq = strchr(name, '=');
  size_t      len = eq ? (size_t)(eq - name) : strlen(name);

  CliOptId id = CLI_OPT_COUNT;
  for(int k = 0; k < CLI_OPT_COUNT; k++) {
    if(strlen(cli_opts[k].long_name) == len &&
       strncmp(cli_opts[k].long_name, name, len) == 0) {
      id = (CliOptId)k;
      break;
    }
  }
  if(id == CLI_OPT_COUNT) {
    /* An unknown long option that names an existing path is served like a
     * positional path; anything else is an invalid parameter. */
    if(path_exists(arg)) {
      if(!*root_set) {
        config->root_dir = (char*)arg;
        *root_set = 1;
      }
      return;
    }
    cli_error(opts, "Invalid parameter: %s", arg);
    return;
  }

  const char* value = NULL;
  if(eq != NULL) {
    if(cli_opts[id].takes_arg == 0) {
      cli_error(opts, "Option does not take an argument: %s", arg);
      return;
    }
    value = eq + 1;
  } else if(cli_opts[id].takes_arg == 1) {
    if(*i + 1 < argc) {
      value = argv[++*i];
    } else {
      char buf[64];
      snprintf(buf, sizeof(buf), "--%s", cli_opts[id].long_name);
      cli_error(opts, "Option requires an argument: %s", buf);
      return;
    }
  } else if(cli_opts[id].takes_arg == 2) {
    if(*i + 1 < argc && argv[*i + 1][0] != '-')
      value = argv[++*i];
  }
  set_option(id, value, config, opts);
}

void cli_parse(int argc, char* argv[], struct BialetConfig* config,
               BialetCliOptions* opts) {
  memset(opts, 0, sizeof(*opts));
  opts->action = BIALET_CLI_SERVE;

  int positional_only = 0;
  int root_set = 0;

  for(int i = 1; i < argc; i++) {
    const char* arg = argv[i];

    if(positional_only || arg[0] != '-' || arg[1] == '\0') {
      handle_positional(arg, config, opts, &root_set);
      continue;
    }
    if(arg[1] == '-') {
      if(arg[2] == '\0') { /* `--` ends option parsing */
        positional_only = 1;
        continue;
      }
      parse_long_option(&i, arg, argc, argv, config, opts, &root_set);
    } else {
      parse_short_options(&i, arg, argc, argv, config, opts);
    }
    if(opts->action != BIALET_CLI_SERVE)
      return;
  }
}

void cli_print_help(const char* prog, FILE* out) {
  fprintf(out, BIALET_USAGE, prog);
}

void cli_print_version(void) {
  printf("bialet %s\n", BIALET_VERSION);
}
