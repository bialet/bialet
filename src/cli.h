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
#ifndef BIALET_CLI_H
#define BIALET_CLI_H

#include "bialet.h"
#include <stdio.h>

/* Command-line parsing. Every short option has a long equivalent; both map to
 * the same setting. cli_parse() applies settings directly to `config` (which
 * must already hold its defaults) and reports what main() should do next via
 * opts->action. */

typedef enum {
  BIALET_CLI_SERVE,   /* start the server (or run the -r/-t/-T mode) */
  BIALET_CLI_HELP,    /* print help to stdout and exit 0 */
  BIALET_CLI_VERSION, /* print the version and exit 0 */
  BIALET_CLI_INVALID  /* print opts->error + help to stderr and exit 1 */
} BialetCliAction;

typedef struct {
  BialetCliAction action;
  const char*     run_code;      /* -r / --run */
  const char*     validate_file; /* -t / --validate */
  const char*     test_dir;      /* optional arg of -T / --tests */
  int             run_tests;     /* -T / --tests */
  int             dev_mode;      /* `dev` subcommand */
  char            error[256];    /* set when action == BIALET_CLI_INVALID */
} BialetCliOptions;

void cli_parse(int argc, char* argv[], struct BialetConfig* config,
               BialetCliOptions* opts);
void cli_print_help(const char* prog, FILE* out);
void cli_print_version(void);

#define BIALET_USAGE                                                                \
  "🚲 bialet\n\n"                                                                 \
  "Usage: %s [options] [dev] [root_dir]\n\n"                                        \
  "Starts a server serving the given root_dir (default: the current "               \
  "directory). Add `dev` to enable live reload, in-browser error display, "         \
  "and auto-open the browser.\n\n"                                                  \
  "Examples:\n"                                                                     \
  "  bialet                   Serve the current directory on 127.0.0.1:7001\n"      \
  "  bialet dev               Dev server with live reload and error display\n"      \
  "  bialet [dev] /path/to/app\n"                                                   \
  "                           Serve a specific directory, optionally in dev "       \
  "mode\n\n"                                                                        \
  "Options:\n"                                                                      \
  "  -p, --port PORT       Port number                            (default: "       \
  "7001)\n"                                                                         \
  "  -h, --host HOST       Host name                              (default: "       \
  "127.0.0.1)\n"                                                                    \
  "  -H, --help            Show this help and exit\n"                               \
  "  -r, --run CODE        Run the code passed as argument, then exit\n"            \
  "  -t, --validate FILE   Validate the syntax of a Wren file, then exit\n"         \
  "  -T, --tests [DIR]     Run tests in the _tests/ folder\n"                       \
  "  -v, --version         Print the version and exit\n"                            \
  "  -l, --log FILE        Log file location                      (default: "       \
  "stdout)\n"                                                                       \
  "  -d, --db FILE         SQLite database file location          (default: "       \
  "_db.sqlite3)\n"                                                                  \
  "  -w, --wal             Enable SQLite Write-Ahead logging mode\n"                \
  "  -i, --ignore LIST     Ignored files: comma-separated list of glob "            \
  "expressions\n"                                                                   \
  "                        (default: README*,AGENTS*,LICENSE*,*.json,*.yml,"        \
  "*.yaml)\n"                                                                       \
  "  -m, --mem-soft MB     Memory soft limit                      (default: 50)\n"  \
  "  -M, --mem-hard MB     Memory hard limit                      (default: 100)\n" \
  "  -c, --cpu-soft PERC   CPU soft limit                         (default: 15)\n"  \
  "  -C, --cpu-hard PERC   CPU hard limit                         (default: 30)\n"  \
  "  -b, --max-post KB     Max request body                       (default: 128)\n" \
  "  -q, --quiet           Quiet: suppress the browser auto-open and colored "      \
  "output\n\n"                                                                      \
  "Long options take a value as `--port 8080` or `--port=8080`.\n\n"                \
  "Any other long-form option is rejected as an invalid parameter.\n\n"             \
  "Full documentation: https://bialet.dev/usage.html\n"

#endif
