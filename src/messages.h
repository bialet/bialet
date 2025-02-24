#ifndef MESSAGES_H
#define MESSAGES_H

#include "bialet.h"

void messageInit(struct BialetConfig* config);

char* colorize(char* str, int color);
char* green(char* str);
char* red(char* str);
char* blue(char* str);
char* yellow(char* str);
char* magenta(char* str);
char* cyan(char* str);

void messageInternal(int num, ...);

#define message_1(x) messageInternal(1, x)
#define message_2(x, y) messageInternal(2, x, y)
#define message_3(x, y, z) messageInternal(3, x, y, z)
#define message_4(w, x, y, z) messageInternal(4, w, x, y, z)
#define message_5(v, w, x, y, z) messageInternal(5, v, w, x, y, z)
#define message_6(u, v, w, x, y, z) messageInternal(6, u, v, w, x, y, z)
#define message_7(t, u, v, w, x, y, z) messageInternal(7, t, u, v, w, x, y, z)
#define message_8(s, t, u, v, w, x, y, z) messageInternal(8, s, t, u, v, w, x, y, z)
#define message_9(r, s, t, u, v, w, x, y, z)                                        \
  messageInternal(9, r, s, t, u, v, w, x, y, z)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define message(...)                                                                \
  GET_MACRO(__VA_ARGS__, message_9, message_8, message_7, message_6, message_5,     \
            message_4, message_3, message_2, message_1)                             \
  (__VA_ARGS__)

#endif
