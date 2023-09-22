#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define COLORIZE_MAX 100
#define GREEN_COLOR 32
#define RED_COLOR 31
#define YELLOW_COLOR 33
#define BLUE_COLOR 34

char *colorize(char *str, int color) {
  if (!color) {
    return str;
  }
  char *output = malloc(COLORIZE_MAX);
  sprintf(output, "\033[%dm%s\033[0m", color, str);
  return output;
}

char *green(char *str) { return colorize(str, GREEN_COLOR); }
char *red(char *str) { return colorize(str, RED_COLOR); }
char *blue(char *str) { return colorize(str, BLUE_COLOR); }
char *yellow(char *str) { return colorize(str, YELLOW_COLOR); }

void message_internal(int num, ...) {
  va_list args;
  va_start(args, num);

  for (int i = 0; i < num; ++i) {
    char *str = va_arg(args, char *);
    printf("%s", str);
    if (i < num - 1) {
      printf(" ");
    }
  }

  printf("\n");
  va_end(args);
}

#define message_1(x) message_internal(1, x)
#define message_2(x, y) message_internal(2, x, y)
#define message_3(x, y, z) message_internal(3, x, y, z)
#define message_4(w, x, y, z) message_internal(4, w, x, y, z)
#define message_5(v, w, x, y, z) message_internal(5, v, w, x, y, z)
#define message_6(u, v, w, x, y, z) message_internal(6, u, v, w, x, y, z)
#define message_7(t, u, v, w, x, y, z) message_internal(7, t, u, v, w, x, y, z)
#define message_8(s, t, u, v, w, x, y, z)                                      \
  message_internal(8, s, t, u, v, w, x, y, z)
#define message_9(r, s, t, u, v, w, x, y, z)                                   \
  message_internal(9, r, s, t, u, v, w, x, y, z)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define message(...)                                                           \
  GET_MACRO(__VA_ARGS__, message_9, message_8, message_7, message_6,           \
            message_5, message_4, message_3, message_2, message_1)             \
  (__VA_ARGS__)
