#include "bialet.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define GREEN_COLOR 32
#define RED_COLOR 31
#define YELLOW_COLOR 33
#define BLUE_COLOR 34
#define MAGENTA_COLOR 35
#define CYAN_COLOR 36

FILE* log_file;
int   apply_color = 0;

void message_init(struct BialetConfig* config) {
  log_file = config->log_file;
  apply_color = config->output_color;
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD  mode = 0;
  if(GetConsoleMode(hOut, &mode)) {
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#endif
  if(!isatty(1))
    apply_color = 0;
}

// Lets callers outside message()/colorize() (e.g. the -T test runner, which
// prints with plain printf instead of message()) decide whether to emit ANSI
// codes, using the same isatty + -q + output_color determination as the rest
// of the app instead of re-deriving it.
int message_color_enabled(void) {
  return apply_color;
}

// Lets a caller temporarily redirect message() output (e.g. the -T test
// runner silencing System.log/migration noise during -q) without a second
// global. Returns the previous destination so it can be restored.
FILE* message_set_log_file(FILE* f) {
  FILE* old = log_file;
  log_file = f;
  return old;
}

/* colorize() hands back either a fresh heap string or its argument unchanged,
 * and message_internal() has to know which one it got. The old code guessed by
 * testing str[0] == '\033', so any *caller-supplied* string beginning with ESC
 * was passed to free(). Wren log output (bialet_wren_write) and Wren error text
 * (bialet_wren_error) both reach message() verbatim, making that a non-heap
 * free on remotely-influenced data.
 *
 * Ownership is now recorded rather than inferred: colorize() registers what it
 * allocated on a small list that message_internal() drains. The list is
 * thread-local because the cron thread, the dmon thread and the request path
 * all log. Every color helper is only ever called as an argument to message(),
 * so the list is always drained by the matching message_internal() call. */
#define MSG_MAX_ARGS 9
#define MSG_MAX_PENDING MSG_MAX_ARGS
/* Longest log line written without touching the heap. */
#define MSG_STACK_LINE 256

static _Thread_local char*  msg_pending[MSG_MAX_PENDING];
static _Thread_local size_t msg_pending_count;

char* colorize(char* str, int color) {
  if(str == NULL || !apply_color || !color) {
    return str;
  }
  /* Out of slots: return uncolored rather than allocate something that nobody
   * is going to free. */
  if(msg_pending_count >= MSG_MAX_PENDING) {
    return str;
  }
  size_t len = strlen(str);
  size_t size = len + 20;
  char*  output = malloc(size);
  if(output == NULL)
    return str;
  snprintf(output, size, "\033[%dm%s\033[0m", color, str);
  msg_pending[msg_pending_count++] = output;
  return output;
}

char* green(char* str) {
  return colorize(str, GREEN_COLOR);
}
char* red(char* str) {
  return colorize(str, RED_COLOR);
}
char* blue(char* str) {
  return colorize(str, BLUE_COLOR);
}
char* yellow(char* str) {
  return colorize(str, YELLOW_COLOR);
}
char* magenta(char* str) {
  return colorize(str, MAGENTA_COLOR);
}
char* cyan(char* str) {
  return colorize(str, CYAN_COLOR);
}

/* The leading "YYYY-MM-DD HH:MM:SS " stamp, cached per thread for the second it
 * was computed for. Every log line used to call time(), localtime_r() (which
 * lands in tzload()/tzset internals on a cold cache) and six fprintf
 * conversions, which is most of the cost of logging a request line. */
static _Thread_local struct {
  time_t second;
  int    valid;
  /* Large enough for the worst case the compiler can prove for the %d fields,
   * so -Wformat-truncation (glibc) does not reject the snprintf. A real
   * timestamp is 20 bytes. */
  char text[80];
} msg_stamp;

static const char* message_stamp(void) {
  time_t now = time(NULL);
  if(msg_stamp.valid && msg_stamp.second == now)
    return msg_stamp.text;

  struct tm  tmbuf;
  struct tm* tm;
#ifdef _WIN32
  tm = localtime_s(&tmbuf, &now) == 0 ? &tmbuf : NULL;
#else
  tm = localtime_r(&now, &tmbuf);
#endif
  if(tm == NULL) {
    msg_stamp.text[0] = '\0';
    msg_stamp.valid = 0;
    return msg_stamp.text;
  }
  snprintf(msg_stamp.text, sizeof(msg_stamp.text), "%d-%02d-%02d %02d:%02d:%02d ",
           (tm->tm_year + 1900), (tm->tm_mon + 1), tm->tm_mday, tm->tm_hour,
           tm->tm_min, tm->tm_sec);
  msg_stamp.second = now;
  msg_stamp.valid = 1;
  return msg_stamp.text;
}

void message_internal(int num, ...) {
  /* Survive a message() emitted before message_init() set log_file. */
  FILE* out = log_file ? log_file : stderr;

  /* Clamped instead of using a VLA sized by a varargs count: VLAs are optional
   * in C11/C17 and absent from MSVC. */
  if(num > MSG_MAX_ARGS)
    num = MSG_MAX_ARGS;

  /* The line is assembled into one buffer and written with a single fwrite:
   * the old version issued a fprintf plus an fputc per argument, each taking
   * the stream lock. */
  char        stack_line[MSG_STACK_LINE];
  char*       line = stack_line;
  size_t      cap = sizeof(stack_line);
  size_t      len = 0;
  const char* stamp = message_stamp();

  va_list measure;
  va_start(measure, num);
  size_t needed = strlen(stamp);
  for(int i = 0; i < num; ++i) {
    const char* str = va_arg(measure, const char*);
    /* fprintf("%s", NULL) is undefined behavior, not a printed "(null)". */
    needed += strlen(str != NULL ? str : "(null)");
    if(i < num - 1)
      needed++;
  }
  va_end(measure);

  if(needed + 1 > cap) {
    line = (char*)malloc(needed + 1);
    if(line == NULL) {
      line = stack_line;
    } else {
      cap = needed + 1;
    }
  }

  {
    size_t n = strlen(stamp);
    if(n > cap - 1)
      n = cap - 1;
    memcpy(line, stamp, n);
    len = n;
  }

  va_list args;
  va_start(args, num);
  for(int i = 0; i < num; ++i) {
    const char* str = va_arg(args, const char*);
    if(str == NULL)
      str = "(null)";
    size_t n = strlen(str);
    if(len + n > cap - 1)
      n = cap - 1 - len;
    memcpy(line + len, str, n);
    len += n;
    if(i < num - 1 && len < cap - 1)
      line[len++] = ' ';
  }
  va_end(args);

  fwrite(line, 1, len, out);
  fputc('\n', out);
  fflush(out);
  if(line != stack_line)
    free(line);

  /* Free exactly what colorize() allocated for this message -- no guessing. */
  for(size_t i = 0; i < msg_pending_count; ++i) {
    free(msg_pending[i]);
  }
  msg_pending_count = 0;
}

#define message_1(x) message_internal(1, x)
#define message_2(x, y) message_internal(2, x, y)
#define message_3(x, y, z) message_internal(3, x, y, z)
#define message_4(w, x, y, z) message_internal(4, w, x, y, z)
#define message_5(v, w, x, y, z) message_internal(5, v, w, x, y, z)
#define message_6(u, v, w, x, y, z) message_internal(6, u, v, w, x, y, z)
#define message_7(t, u, v, w, x, y, z) message_internal(7, t, u, v, w, x, y, z)
#define message_8(s, t, u, v, w, x, y, z) message_internal(8, s, t, u, v, w, x, y, z)
#define message_9(r, s, t, u, v, w, x, y, z)                                        \
  message_internal(9, r, s, t, u, v, w, x, y, z)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define message(...)                                                                \
  GET_MACRO(__VA_ARGS__, message_9, message_8, message_7, message_6, message_5,     \
            message_4, message_3, message_2, message_1)                             \
  (__VA_ARGS__)
