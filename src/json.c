#include "json.h"

#include "wren_primitive.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define JSON_ERROR UNDEFINED_VAL

// Maximum container nesting depth. Deeply nested JSON exhausts the C stack
// via parse_value/parse_array/parse_object recursion; reject it instead.
#define MAX_JSON_DEPTH 128

static void skip_ws(const char** p) {
  while(**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
    (*p)++;
  }
}

static int hex4(const char* s) {
  int v = 0;
  for(int i = 0; i < 4; i++) {
    char c = s[i];
    if(c == '\0')
      return -1;
    v <<= 4;
    if(c >= '0' && c <= '9')
      v |= (c - '0');
    else if(c >= 'a' && c <= 'f')
      v |= (c - 'a' + 10);
    else if(c >= 'A' && c <= 'F')
      v |= (c - 'A' + 10);
    else
      return -1;
  }
  return v;
}

static int utf8_encode(int cp, char* buf) {
  if(cp < 0)
    return -1;
  if(cp < 0x80) {
    buf[0] = (char)cp;
    return 1;
  }
  if(cp < 0x800) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if(cp < 0x10000) {
    if(cp >= 0xD800 && cp <= 0xDFFF)
      return -1;
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  if(cp < 0x110000) {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
  }
  return -1;
}

static Value parse_value(WrenVM* vm, const char** p, int depth);

static Value parse_string(WrenVM* vm, const char** p) {
  const char* s = *p;
  if(*s != '"')
    return JSON_ERROR;
  s++;

  // Measure this token before allocating. strlen(s) is the length of the whole
  // *remaining document*, so a document made of many short strings allocated
  // (and touched) quadratic memory. The decoded form is never longer than the
  // encoded form, so the span is a safe upper bound.
  const char* scan = s;
  while(*scan != '\0' && *scan != '"') {
    if(*scan == '\\' && scan[1] != '\0')
      scan += 2;
    else
      scan++;
  }
  if(*scan != '"')
    return JSON_ERROR; // unterminated string
  size_t len = (size_t)(scan - s);
  char*  buf = malloc(len + 1);
  if(buf == NULL)
    return JSON_ERROR;

  size_t out = 0;
  while(*s != '\0' && *s != '"') {
    if(*s == '\\') {
      s++;
      switch(*s) {
        case '"':
          buf[out++] = '"';
          s++;
          break;
        case '\\':
          buf[out++] = '\\';
          s++;
          break;
        case '/':
          buf[out++] = '/';
          s++;
          break;
        case 'b':
          buf[out++] = '\b';
          s++;
          break;
        case 'f':
          buf[out++] = '\f';
          s++;
          break;
        case 'n':
          buf[out++] = '\n';
          s++;
          break;
        case 'r':
          buf[out++] = '\r';
          s++;
          break;
        case 't':
          buf[out++] = '\t';
          s++;
          break;
        case 'u': {
          s++;
          int cp = hex4(s);
          if(cp < 0) {
            free(buf);
            return JSON_ERROR;
          }
          s += 4;

          if(cp >= 0xD800 && cp <= 0xDBFF) {
            if(s[0] == '\\' && s[1] == 'u') {
              s += 2;
              int low = hex4(s);
              if(low < 0 || low < 0xDC00 || low > 0xDFFF) {
                free(buf);
                return JSON_ERROR;
              }
              s += 4;
              cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            } else {
              free(buf);
              return JSON_ERROR;
            }
          } else if(cp >= 0xDC00 && cp <= 0xDFFF) {
            free(buf);
            return JSON_ERROR;
          }

          int n = utf8_encode(cp, buf + out);
          if(n < 0) {
            free(buf);
            return JSON_ERROR;
          }
          out += (size_t)n;
          break;
        }
        default:
          free(buf);
          return JSON_ERROR;
      }
    } else {
      buf[out++] = *s;
      s++;
    }
  }

  if(*s != '"') {
    free(buf);
    return JSON_ERROR;
  }
  s++;
  *p = s;

  Value result = wrenNewStringLength(vm, buf, out);
  free(buf);
  return result;
}

static Value parse_number(WrenVM* vm, const char** p) {
  /* (void) rather than __attribute__((unused)), which is a GNU extension. */
  (void)vm;
  const char* s = *p;
  errno = 0;
  char*  end;
  double num = strtod(s, &end);

  if(end == s)
    return JSON_ERROR;
  if(errno == ERANGE)
    return JSON_ERROR;
  if(isinf(num) || isnan(num))
    return JSON_ERROR;

  *p = end;
  return NUM_VAL(num);
}

static Value parse_array(WrenVM* vm, const char** p, int depth) {
  const char* s = *p;
  if(*s != '[')
    return JSON_ERROR;
  s++;

  ObjList* list = wrenNewList(vm, 0);

  skip_ws(&s);
  if(*s == ']') {
    s++;
    *p = s;
    return OBJ_VAL(list);
  }

  for(;;) {
    Value val = parse_value(vm, &s, depth + 1);
    if(IS_UNDEFINED(val))
      return JSON_ERROR;
    wrenValueBufferWrite(vm, &list->elements, val);

    skip_ws(&s);
    if(*s == ']') {
      s++;
      break;
    }
    if(*s != ',')
      return JSON_ERROR;
    s++;
    skip_ws(&s);
  }

  *p = s;
  return OBJ_VAL(list);
}

static Value parse_object(WrenVM* vm, const char** p, int depth) {
  const char* s = *p;
  if(*s != '{')
    return JSON_ERROR;
  s++;

  ObjMap* map = wrenNewMap(vm);

  skip_ws(&s);
  if(*s == '}') {
    s++;
    *p = s;
    return OBJ_VAL(map);
  }

  for(;;) {
    skip_ws(&s);
    if(*s != '"')
      return JSON_ERROR;

    Value key = parse_string(vm, &s);
    if(IS_UNDEFINED(key))
      return JSON_ERROR;

    skip_ws(&s);
    if(*s != ':')
      return JSON_ERROR;
    s++;

    Value val = parse_value(vm, &s, depth + 1);
    if(IS_UNDEFINED(val))
      return JSON_ERROR;

    wrenMapSet(vm, map, key, val);

    skip_ws(&s);
    if(*s == '}') {
      s++;
      break;
    }
    if(*s != ',')
      return JSON_ERROR;
    s++;
  }

  *p = s;
  return OBJ_VAL(map);
}

static Value parse_value(WrenVM* vm, const char** p, int depth) {
  if(depth > MAX_JSON_DEPTH)
    return JSON_ERROR;
  skip_ws(p);
  const char* s = *p;

  switch(*s) {
    case '\0':
      return JSON_ERROR;
    case '"':
      return parse_string(vm, p);
    case '{':
      return parse_object(vm, p, depth);
    case '[':
      return parse_array(vm, p, depth);
    case 't':
      if(s[1] == 'r' && s[2] == 'u' && s[3] == 'e') {
        *p = s + 4;
        return TRUE_VAL;
      }
      return JSON_ERROR;
    case 'f':
      if(s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e') {
        *p = s + 5;
        return FALSE_VAL;
      }
      return JSON_ERROR;
    case 'n':
      if(s[1] == 'u' && s[2] == 'l' && s[3] == 'l') {
        *p = s + 4;
        return NULL_VAL;
      }
      return JSON_ERROR;
    default:
      if(*s == '-' || (*s >= '0' && *s <= '9'))
        return parse_number(vm, p);
      return JSON_ERROR;
  }
}

bool prim_json_parse_primitive(WrenVM* vm, Value* args) {
  if(!IS_STRING(args[1])) {
    RETURN_ERROR("Invalid JSON: expected a string");
  }
  const char* json = AS_CSTRING(args[1]);
  if(json == NULL || *json == '\0') {
    RETURN_ERROR("Invalid JSON: empty input");
  }
  const char* p = json;

  // Pause the collector for the duration of the parse.
  //
  // The lists, maps and strings built below are held only in C locals while
  // their children are parsed: a new ObjList is not reachable from any GC root
  // until it is written into its parent, and a map key is unreachable between
  // being created and being inserted. Every wrenNew*/wrenMapSet/
  // wrenValueBufferWrite call can allocate, and wrenReallocate collects once
  // bytesAllocated passes nextGC -- so a GC landing mid-parse could free a
  // container that is still being filled. Rooting each level with
  // wrenPushRoot is not an option here: WREN_MAX_TEMP_ROOTS is 8 while nesting
  // is allowed to MAX_JSON_DEPTH (128).
  //
  // Suppressing collection for the parse is bounded work: peak growth is
  // proportional to the input, which callers already cap (max_post_size for
  // request bodies). Restoring nextGC lets the very next allocation collect.
  size_t saved_next_gc = vm->nextGC;
  vm->nextGC = SIZE_MAX;
  Value result = parse_value(vm, &p, 0);
  vm->nextGC = saved_next_gc;

  if(IS_UNDEFINED(result)) {
    RETURN_ERROR("Invalid JSON");
  }
  skip_ws(&p);
  if(*p != '\0') {
    RETURN_ERROR("Invalid JSON: unexpected trailing data");
  }
  // Assigned before any further allocation, so `result` becomes reachable from
  // the API stack before a collection can run again.
  args[0] = result;
  return true;
}
