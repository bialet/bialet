#include "wren_core.h"

#include "bialet.h"
#include "bialet.wren.inc"
#include "bialet_test.wren.inc"
#include "bialet_wren.h"
#include "hash.h"
#include "http_call.h"
#include "json.h"
#include "markdown.h"
#include "wren_core.wren.inc"
#include "wren_math.h"
#include "wren_primitive.h"
#include "wren_utils.h"
#include "wren_value.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

DEF_PRIMITIVE(bool_not) {
  RETURN_BOOL(!AS_BOOL(args[0]));
}

DEF_PRIMITIVE(bool_toString) {
  if(AS_BOOL(args[0])) {
    RETURN_VAL(CONST_STRING(vm, "true"));
  } else {
    RETURN_VAL(CONST_STRING(vm, "false"));
  }
}

DEF_PRIMITIVE(class_name) {
  RETURN_OBJ(AS_CLASS(args[0])->name);
}

DEF_PRIMITIVE(class_supertype) {
  ObjClass* classObj = AS_CLASS(args[0]);

  // Object has no superclass.
  if(classObj->superclass == NULL)
    RETURN_NULL;

  RETURN_OBJ(classObj->superclass);
}

DEF_PRIMITIVE(class_toString) {
  RETURN_OBJ(AS_CLASS(args[0])->name);
}

DEF_PRIMITIVE(class_attributes) {
  RETURN_VAL(AS_CLASS(args[0])->attributes);
}

DEF_PRIMITIVE(fiber_new) {
  if(!validateFn(vm, args[1], "Argument"))
    return false;

  ObjClosure* closure = AS_CLOSURE(args[1]);
  if(closure->fn->arity > 1) {
    RETURN_ERROR("Function cannot take more than one parameter.");
  }

  RETURN_OBJ(wrenNewFiber(vm, closure));
}

DEF_PRIMITIVE(fiber_abort) {
  vm->fiber->error = args[1];

  // If the error is explicitly null, it's not really an abort.
  return IS_NULL(args[1]);
}

// Transfer execution to [fiber] coming from the current fiber whose stack has
// [args].
//
// [isCall] is true if [fiber] is being called and not transferred.
//
// [hasValue] is true if a value in [args] is being passed to the new fiber.
// Otherwise, `null` is implicitly being passed.
static bool runFiber(WrenVM* vm, ObjFiber* fiber, Value* args, bool isCall,
                     bool hasValue, const char* verb) {

  if(wrenHasError(fiber)) {
    RETURN_ERROR_FMT("Cannot $ an aborted fiber.", verb);
  }

  if(isCall) {
    // You can't call a called fiber, but you can transfer directly to it,
    // which is why this check is gated on `isCall`. This way, after resuming a
    // suspended fiber, it will run and then return to the fiber that called it
    // and so on.
    if(fiber->caller != NULL)
      RETURN_ERROR("Fiber has already been called.");

    if(fiber->state == FIBER_ROOT)
      RETURN_ERROR("Cannot call root fiber.");

    // Remember who ran it.
    fiber->caller = vm->fiber;
  }

  if(fiber->numFrames == 0) {
    RETURN_ERROR_FMT("Cannot $ a finished fiber.", verb);
  }

  // When the calling fiber resumes, we'll store the result of the call in its
  // stack. If the call has two arguments (the fiber and the value), we only
  // need one slot for the result, so discard the other slot now.
  if(hasValue)
    vm->fiber->stackTop--;

  if(fiber->numFrames == 1 &&
     fiber->frames[0].ip == fiber->frames[0].closure->fn->code.data) {
    // The fiber is being started for the first time. If its function takes a
    // parameter, bind an argument to it.
    if(fiber->frames[0].closure->fn->arity == 1) {
      fiber->stackTop[0] = hasValue ? args[1] : NULL_VAL;
      fiber->stackTop++;
    }
  } else {
    // The fiber is being resumed, make yield() or transfer() return the result.
    fiber->stackTop[-1] = hasValue ? args[1] : NULL_VAL;
  }

  vm->fiber = fiber;
  return false;
}

DEF_PRIMITIVE(fiber_call) {
  return runFiber(vm, AS_FIBER(args[0]), args, true, false, "call");
}

DEF_PRIMITIVE(fiber_call1) {
  return runFiber(vm, AS_FIBER(args[0]), args, true, true, "call");
}

DEF_PRIMITIVE(fiber_current) {
  RETURN_OBJ(vm->fiber);
}

DEF_PRIMITIVE(fiber_error) {
  RETURN_VAL(AS_FIBER(args[0])->error);
}

DEF_PRIMITIVE(fiber_isDone) {
  ObjFiber* runFiber = AS_FIBER(args[0]);
  RETURN_BOOL(runFiber->numFrames == 0 || wrenHasError(runFiber));
}

DEF_PRIMITIVE(fiber_suspend) {
  // Switching to a null fiber tells the interpreter to stop and exit.
  vm->fiber = NULL;
  vm->apiStack = NULL;
  return false;
}

DEF_PRIMITIVE(fiber_transfer) {
  return runFiber(vm, AS_FIBER(args[0]), args, false, false, "transfer to");
}

DEF_PRIMITIVE(fiber_transfer1) {
  return runFiber(vm, AS_FIBER(args[0]), args, false, true, "transfer to");
}

DEF_PRIMITIVE(fiber_transferError) {
  runFiber(vm, AS_FIBER(args[0]), args, false, true, "transfer to");
  vm->fiber->error = args[1];
  return false;
}

DEF_PRIMITIVE(fiber_try) {
  runFiber(vm, AS_FIBER(args[0]), args, true, false, "try");

  // If we're switching to a valid fiber to try, remember that we're trying it.
  if(!wrenHasError(vm->fiber))
    vm->fiber->state = FIBER_TRY;
  return false;
}

DEF_PRIMITIVE(fiber_try1) {
  runFiber(vm, AS_FIBER(args[0]), args, true, true, "try");

  // If we're switching to a valid fiber to try, remember that we're trying it.
  if(!wrenHasError(vm->fiber))
    vm->fiber->state = FIBER_TRY;
  return false;
}

DEF_PRIMITIVE(fiber_yield) {
  ObjFiber* current = vm->fiber;
  vm->fiber = current->caller;

  // Unhook this fiber from the one that called it.
  current->caller = NULL;
  current->state = FIBER_OTHER;

  if(vm->fiber != NULL) {
    // Make the caller's run method return null.
    vm->fiber->stackTop[-1] = NULL_VAL;
  }

  return false;
}

DEF_PRIMITIVE(fiber_yield1) {
  ObjFiber* current = vm->fiber;
  vm->fiber = current->caller;

  // Unhook this fiber from the one that called it.
  current->caller = NULL;
  current->state = FIBER_OTHER;

  if(vm->fiber != NULL) {
    // Make the caller's run method return the argument passed to yield.
    vm->fiber->stackTop[-1] = args[1];

    // When the yielding fiber resumes, we'll store the result of the yield
    // call in its stack. Since Fiber.yield(value) has two arguments (the Fiber
    // class and the value) and we only need one slot for the result, discard
    // the other slot now.
    current->stackTop--;
  }

  return false;
}

DEF_PRIMITIVE(fn_new) {
  if(!validateFn(vm, args[1], "Argument"))
    return false;

  // The block argument is already a function, so just return it.
  RETURN_VAL(args[1]);
}

DEF_PRIMITIVE(fn_arity) {
  RETURN_NUM(AS_CLOSURE(args[0])->fn->arity);
}

static void call_fn(WrenVM* vm, Value* args, int numArgs) {
  // +1 to include the function itself.
  wrenCallFunction(vm, vm->fiber, AS_CLOSURE(args[0]), numArgs + 1);
}

#define DEF_FN_CALL(numArgs)                                                        \
  DEF_PRIMITIVE(fn_call##numArgs) {                                                 \
    call_fn(vm, args, numArgs);                                                     \
    return false;                                                                   \
  }

DEF_FN_CALL(0)
DEF_FN_CALL(1)
DEF_FN_CALL(2)
DEF_FN_CALL(3)
DEF_FN_CALL(4)
DEF_FN_CALL(5)
DEF_FN_CALL(6)
DEF_FN_CALL(7)
DEF_FN_CALL(8)
DEF_FN_CALL(9)
DEF_FN_CALL(10)
DEF_FN_CALL(11)
DEF_FN_CALL(12)
DEF_FN_CALL(13)
DEF_FN_CALL(14)
DEF_FN_CALL(15)
DEF_FN_CALL(16)

DEF_PRIMITIVE(fn_toString) {
  RETURN_VAL(CONST_STRING(vm, "<fn>"));
}

// Creates a new list of size args[1], with all elements initialized to args[2].
DEF_PRIMITIVE(list_filled) {
  if(!validateInt(vm, args[1], "Size"))
    return false;
  if(AS_NUM(args[1]) < 0)
    RETURN_ERROR("Size cannot be negative.");

  uint32_t size = (uint32_t)AS_NUM(args[1]);
  ObjList* list = wrenNewList(vm, size);

  for(uint32_t i = 0; i < size; i++) {
    list->elements.data[i] = args[2];
  }

  RETURN_OBJ(list);
}

DEF_PRIMITIVE(list_new) {
  RETURN_OBJ(wrenNewList(vm, 0));
}

DEF_PRIMITIVE(list_add) {
  wrenValueBufferWrite(vm, &AS_LIST(args[0])->elements, args[1]);
  RETURN_VAL(args[1]);
}

// Adds an element to the list and then returns the list itself. This is called
// by the compiler when compiling list literals instead of using add() to
// minimize stack churn.
DEF_PRIMITIVE(list_addCore) {
  wrenValueBufferWrite(vm, &AS_LIST(args[0])->elements, args[1]);

  // Return the list.
  RETURN_VAL(args[0]);
}

DEF_PRIMITIVE(list_clear) {
  wrenValueBufferClear(vm, &AS_LIST(args[0])->elements);
  RETURN_NULL;
}

DEF_PRIMITIVE(list_count) {
  RETURN_NUM(AS_LIST(args[0])->elements.count);
}

DEF_PRIMITIVE(list_insert) {
  ObjList* list = AS_LIST(args[0]);

  // count + 1 here so you can "insert" at the very end.
  uint32_t index = validateIndex(vm, args[1], list->elements.count + 1, "Index");
  if(index == UINT32_MAX)
    return false;

  wrenListInsert(vm, list, args[2], index);
  RETURN_VAL(args[2]);
}

DEF_PRIMITIVE(list_iterate) {
  ObjList* list = AS_LIST(args[0]);

  // If we're starting the iteration, return the first index.
  if(IS_NULL(args[1])) {
    if(list->elements.count == 0)
      RETURN_FALSE;
    RETURN_NUM(0);
  }

  if(!validateInt(vm, args[1], "Iterator"))
    return false;

  // Stop if we're out of bounds.
  double index = AS_NUM(args[1]);
  if(index < 0 || index >= list->elements.count - 1)
    RETURN_FALSE;

  // Otherwise, move to the next index.
  RETURN_NUM(index + 1);
}

DEF_PRIMITIVE(list_iteratorValue) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(vm, args[1], list->elements.count, "Iterator");
  if(index == UINT32_MAX)
    return false;

  RETURN_VAL(list->elements.data[index]);
}

DEF_PRIMITIVE(list_removeAt) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(vm, args[1], list->elements.count, "Index");
  if(index == UINT32_MAX)
    return false;

  RETURN_VAL(wrenListRemoveAt(vm, list, index));
}

DEF_PRIMITIVE(list_removeValue) {
  ObjList* list = AS_LIST(args[0]);
  int      index = wrenListIndexOf(vm, list, args[1]);
  if(index == -1)
    RETURN_NULL;
  RETURN_VAL(wrenListRemoveAt(vm, list, index));
}

DEF_PRIMITIVE(list_indexOf) {
  ObjList* list = AS_LIST(args[0]);
  RETURN_NUM(wrenListIndexOf(vm, list, args[1]));
}

DEF_PRIMITIVE(list_swap) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t indexA = validateIndex(vm, args[1], list->elements.count, "Index 0");
  if(indexA == UINT32_MAX)
    return false;
  uint32_t indexB = validateIndex(vm, args[2], list->elements.count, "Index 1");
  if(indexB == UINT32_MAX)
    return false;

  Value a = list->elements.data[indexA];
  list->elements.data[indexA] = list->elements.data[indexB];
  list->elements.data[indexB] = a;

  RETURN_NULL;
}

// Concatenates a list of strings into a single string with [sep] between
// elements. This is the native backend for List.join(sep): it measures the
// total size once and performs a single allocation, keeping joins linear in
// the input size. The interpreted join() in the core module pre-converts
// every element with toString, so every element here is guaranteed to be a
// string.
DEF_PRIMITIVE(list_join) {
  ObjList*   list = AS_LIST(args[0]);
  ObjString* sep = AS_STRING(args[1]);
  size_t     sepLength = sep->length;
  size_t     total = 0;

  uint32_t count = list->elements.count;
  for(uint32_t i = 0; i < count; i++) {
    Value element = list->elements.data[i];
    if(!IS_STRING(element))
      RETURN_ERROR("List elements must be strings.");
    size_t length = AS_STRING(element)->length;
    if(length > SIZE_MAX - total)
      RETURN_ERROR("Joined string is too large.");
    total += length;
  }

  // The separator is copied between each pair of elements. Only when there are
  // two or more elements does a separator contribute to the total; with an
  // empty list, count - 1 would underflow to a huge value.
  if(count > 1) {
    if(sepLength > (SIZE_MAX - total) / (count - 1)) {
      RETURN_ERROR("Joined string is too large.");
    }
    total += sepLength * (count - 1);
  }

  // total + 1 keeps the buffer at least one byte so the NULL check below is
  // meaningful even for an empty result.
  char* buffer = malloc(total + 1);
  if(buffer == NULL)
    RETURN_ERROR("Out of memory joining strings.");

  char* out = buffer;
  for(uint32_t i = 0; i < count; i++) {
    if(i > 0 && sepLength > 0) {
      memcpy(out, sep->value, sepLength);
      out += sepLength;
    }
    ObjString* part = AS_STRING(list->elements.data[i]);
    if(part->length > 0) {
      memcpy(out, part->value, part->length);
      out += part->length;
    }
  }

  Value result = wrenNewStringLength(vm, buffer, total);
  free(buffer);
  RETURN_VAL(result);
}

DEF_PRIMITIVE(list_subscript) {
  ObjList* list = AS_LIST(args[0]);

  if(IS_NUM(args[1])) {
    uint32_t index = validateIndex(vm, args[1], list->elements.count, "Subscript");
    if(index == UINT32_MAX)
      return false;

    RETURN_VAL(list->elements.data[index]);
  }

  if(!IS_RANGE(args[1])) {
    RETURN_ERROR("Subscript must be a number or a range.");
  }

  int      step;
  uint32_t count = list->elements.count;
  uint32_t start = calculateRange(vm, AS_RANGE(args[1]), &count, &step);
  if(start == UINT32_MAX)
    return false;

  ObjList* result = wrenNewList(vm, count);
  for(uint32_t i = 0; i < count; i++) {
    result->elements.data[i] = list->elements.data[start + i * step];
  }

  RETURN_OBJ(result);
}

DEF_PRIMITIVE(list_subscriptSetter) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(vm, args[1], list->elements.count, "Subscript");
  if(index == UINT32_MAX)
    return false;

  list->elements.data[index] = args[2];
  RETURN_VAL(args[2]);
}

DEF_PRIMITIVE(map_new) {
  RETURN_OBJ(wrenNewMap(vm));
}

DEF_PRIMITIVE(map_subscript) {
  if(!validateKey(vm, args[1]))
    return false;

  ObjMap* map = AS_MAP(args[0]);
  Value   value = wrenMapGet(map, args[1]);
  if(IS_UNDEFINED(value))
    RETURN_NULL;

  RETURN_VAL(value);
}

DEF_PRIMITIVE(map_subscriptSetter) {
  if(!validateKey(vm, args[1]))
    return false;

  wrenMapSet(vm, AS_MAP(args[0]), args[1], args[2]);
  RETURN_VAL(args[2]);
}

// Adds an entry to the map and then returns the map itself. This is called by
// the compiler when compiling map literals instead of using [_]=(_) to
// minimize stack churn.
DEF_PRIMITIVE(map_addCore) {
  if(!validateKey(vm, args[1]))
    return false;

  wrenMapSet(vm, AS_MAP(args[0]), args[1], args[2]);

  // Return the map itself.
  RETURN_VAL(args[0]);
}

DEF_PRIMITIVE(map_clear) {
  wrenMapClear(vm, AS_MAP(args[0]));
  RETURN_NULL;
}

DEF_PRIMITIVE(map_containsKey) {
  if(!validateKey(vm, args[1]))
    return false;

  RETURN_BOOL(!IS_UNDEFINED(wrenMapGet(AS_MAP(args[0]), args[1])));
}

DEF_PRIMITIVE(map_count) {
  RETURN_NUM(AS_MAP(args[0])->count);
}

DEF_PRIMITIVE(map_iterate) {
  ObjMap* map = AS_MAP(args[0]);

  if(map->count == 0)
    RETURN_FALSE;

  // If we're starting the iteration, start at the first used entry.
  uint32_t index = 0;

  // Otherwise, start one past the last entry we stopped at.
  if(!IS_NULL(args[1])) {
    if(!validateInt(vm, args[1], "Iterator"))
      return false;

    if(AS_NUM(args[1]) < 0)
      RETURN_FALSE;
    index = (uint32_t)AS_NUM(args[1]);

    if(index >= map->capacity)
      RETURN_FALSE;

    // Advance the iterator.
    index++;
  }

  // Find a used entry, if any.
  for(; index < map->capacity; index++) {
    if(!IS_UNDEFINED(map->entries[index].key))
      RETURN_NUM(index);
  }

  // If we get here, walked all of the entries.
  RETURN_FALSE;
}

DEF_PRIMITIVE(map_remove) {
  if(!validateKey(vm, args[1]))
    return false;

  RETURN_VAL(wrenMapRemoveKey(vm, AS_MAP(args[0]), args[1]));
}

DEF_PRIMITIVE(map_keyIteratorValue) {
  ObjMap*  map = AS_MAP(args[0]);
  uint32_t index = validateIndex(vm, args[1], map->capacity, "Iterator");
  if(index == UINT32_MAX)
    return false;

  MapEntry* entry = &map->entries[index];
  if(IS_UNDEFINED(entry->key)) {
    RETURN_ERROR("Invalid map iterator.");
  }

  RETURN_VAL(entry->key);
}

DEF_PRIMITIVE(map_valueIteratorValue) {
  ObjMap*  map = AS_MAP(args[0]);
  uint32_t index = validateIndex(vm, args[1], map->capacity, "Iterator");
  if(index == UINT32_MAX)
    return false;

  MapEntry* entry = &map->entries[index];
  if(IS_UNDEFINED(entry->key)) {
    RETURN_ERROR("Invalid map iterator.");
  }

  RETURN_VAL(entry->value);
}

DEF_PRIMITIVE(null_not) {
  RETURN_VAL(TRUE_VAL);
}

// Returns empty string for null values. This is used in string interpolation
// and when explicitly calling .toString on null. For debugging purposes,
// consider using System.print(value == null) to explicitly check for null
// rather than relying on toString representation.
DEF_PRIMITIVE(null_toString) {
  RETURN_VAL(CONST_STRING(vm, ""));
}

DEF_PRIMITIVE(num_fromString) {
  // A missing form field or a NULL column arrives as null. Treat it like an
  // unparseable string (null) instead of raising "Argument must be a string"
  // and turning every POST into a 500.
  if(IS_NULL(args[1]))
    RETURN_NULL;

  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[1]);

  // Corner case: Can't parse an empty string.
  if(string->length == 0)
    RETURN_NULL;

  errno = 0;
  char*  end;
  double number = strtod(string->value, &end);

  // Skip past any trailing whitespace.
  while(*end != '\0' && isspace((unsigned char)*end))
    end++;

  if(errno == ERANGE)
    RETURN_ERROR("Number literal is too large.");

  // We must have consumed the entire string. Otherwise, it contains non-number
  // characters and we can't parse it.
  if(end < string->value + string->length)
    RETURN_NULL;

  RETURN_NUM(number);
}

// Defines a primitive on Num that calls infix [op] and returns [type].
#define DEF_NUM_CONSTANT(name, value)                                               \
  DEF_PRIMITIVE(num_##name) {                                                       \
    RETURN_NUM(value);                                                              \
  }

DEF_NUM_CONSTANT(infinity, INFINITY)
DEF_NUM_CONSTANT(nan, WREN_DOUBLE_NAN)
DEF_NUM_CONSTANT(pi, 3.14159265358979323846264338327950288)
DEF_NUM_CONSTANT(tau, 6.28318530717958647692528676655900577)

DEF_NUM_CONSTANT(largest, DBL_MAX)
DEF_NUM_CONSTANT(smallest, DBL_MIN)

DEF_NUM_CONSTANT(maxSafeInteger, 9007199254740991.0)
DEF_NUM_CONSTANT(minSafeInteger, -9007199254740991.0)

// Defines a primitive on Num that calls infix [op] and returns [type].
#define DEF_NUM_INFIX(name, op, type)                                               \
  DEF_PRIMITIVE(num_##name) {                                                       \
    if(!validateNum(vm, args[1], "Right operand"))                                  \
      return false;                                                                 \
    RETURN_##type(AS_NUM(args[0]) op AS_NUM(args[1]));                              \
  }

DEF_NUM_INFIX(minus, -, NUM)
DEF_NUM_INFIX(plus, +, NUM)
DEF_NUM_INFIX(multiply, *, NUM)
DEF_NUM_INFIX(divide, /, NUM)
DEF_NUM_INFIX(lt, <, BOOL)
DEF_NUM_INFIX(gt, >, BOOL)
DEF_NUM_INFIX(lte, <=, BOOL)
DEF_NUM_INFIX(gte, >=, BOOL)

// Defines a primitive on Num that call infix bitwise [op].
#define DEF_NUM_BITWISE(name, op)                                                   \
  DEF_PRIMITIVE(num_bitwise##name) {                                                \
    if(!validateNum(vm, args[1], "Right operand"))                                  \
      return false;                                                                 \
    uint32_t left = (uint32_t)AS_NUM(args[0]);                                      \
    uint32_t right = (uint32_t)AS_NUM(args[1]);                                     \
    RETURN_NUM(left op right);                                                      \
  }

DEF_NUM_BITWISE(And, &)
DEF_NUM_BITWISE(Or, |)
DEF_NUM_BITWISE(Xor, ^)
DEF_NUM_BITWISE(LeftShift, <<)
DEF_NUM_BITWISE(RightShift, >>)

// Defines a primitive method on Num that returns the result of [fn].
#define DEF_NUM_FN(name, fn)                                                        \
  DEF_PRIMITIVE(num_##name) {                                                       \
    RETURN_NUM(fn(AS_NUM(args[0])));                                                \
  }

DEF_NUM_FN(abs, fabs)
DEF_NUM_FN(acos, acos)
DEF_NUM_FN(asin, asin)
DEF_NUM_FN(atan, atan)
DEF_NUM_FN(cbrt, cbrt)
DEF_NUM_FN(ceil, ceil)
DEF_NUM_FN(cos, cos)
DEF_NUM_FN(floor, floor)
DEF_NUM_FN(negate, -)
DEF_NUM_FN(round, round)
DEF_NUM_FN(sin, sin)
DEF_NUM_FN(sqrt, sqrt)
DEF_NUM_FN(tan, tan)
DEF_NUM_FN(log, log)
DEF_NUM_FN(log2, log2)
DEF_NUM_FN(exp, exp)

DEF_PRIMITIVE(num_mod) {
  if(!validateNum(vm, args[1], "Right operand"))
    return false;
  RETURN_NUM(fmod(AS_NUM(args[0]), AS_NUM(args[1])));
}

DEF_PRIMITIVE(num_eqeq) {
  if(!IS_NUM(args[1]))
    RETURN_FALSE;
  RETURN_BOOL(AS_NUM(args[0]) == AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_bangeq) {
  if(!IS_NUM(args[1]))
    RETURN_TRUE;
  RETURN_BOOL(AS_NUM(args[0]) != AS_NUM(args[1]));
}

DEF_PRIMITIVE(num_bitwiseNot) {
  // Bitwise operators always work on 32-bit unsigned ints.
  RETURN_NUM(~(uint32_t)AS_NUM(args[0]));
}

DEF_PRIMITIVE(num_dotDot) {
  if(!validateNum(vm, args[1], "Right hand side of range"))
    return false;

  double from = AS_NUM(args[0]);
  double to = AS_NUM(args[1]);
  RETURN_VAL(wrenNewRange(vm, from, to, true));
}

DEF_PRIMITIVE(num_dotDotDot) {
  if(!validateNum(vm, args[1], "Right hand side of range"))
    return false;

  double from = AS_NUM(args[0]);
  double to = AS_NUM(args[1]);
  RETURN_VAL(wrenNewRange(vm, from, to, false));
}

DEF_PRIMITIVE(num_atan2) {
  if(!validateNum(vm, args[1], "x value"))
    return false;

  RETURN_NUM(atan2(AS_NUM(args[0]), AS_NUM(args[1])));
}

DEF_PRIMITIVE(num_min) {
  if(!validateNum(vm, args[1], "Other value"))
    return false;

  double value = AS_NUM(args[0]);
  double other = AS_NUM(args[1]);
  RETURN_NUM(value <= other ? value : other);
}

DEF_PRIMITIVE(num_max) {
  if(!validateNum(vm, args[1], "Other value"))
    return false;

  double value = AS_NUM(args[0]);
  double other = AS_NUM(args[1]);
  RETURN_NUM(value > other ? value : other);
}

DEF_PRIMITIVE(num_clamp) {
  if(!validateNum(vm, args[1], "Min value"))
    return false;
  if(!validateNum(vm, args[2], "Max value"))
    return false;

  double value = AS_NUM(args[0]);
  double min = AS_NUM(args[1]);
  double max = AS_NUM(args[2]);
  double result = (value < min) ? min : ((value > max) ? max : value);
  RETURN_NUM(result);
}

DEF_PRIMITIVE(num_pow) {
  if(!validateNum(vm, args[1], "Power value"))
    return false;

  RETURN_NUM(pow(AS_NUM(args[0]), AS_NUM(args[1])));
}

DEF_PRIMITIVE(num_fraction) {
  double unused;
  RETURN_NUM(modf(AS_NUM(args[0]), &unused));
}

DEF_PRIMITIVE(num_isInfinity) {
  RETURN_BOOL(isinf(AS_NUM(args[0])));
}

DEF_PRIMITIVE(num_isInteger) {
  double value = AS_NUM(args[0]);
  if(isnan(value) || isinf(value))
    RETURN_FALSE;
  RETURN_BOOL(trunc(value) == value);
}

DEF_PRIMITIVE(num_isNan) {
  RETURN_BOOL(isnan(AS_NUM(args[0])));
}

DEF_PRIMITIVE(num_sign) {
  double value = AS_NUM(args[0]);
  if(value > 0) {
    RETURN_NUM(1);
  } else if(value < 0) {
    RETURN_NUM(-1);
  } else {
    RETURN_NUM(0);
  }
}

DEF_PRIMITIVE(num_toString) {
  RETURN_VAL(wrenNumToString(vm, AS_NUM(args[0])));
}

DEF_PRIMITIVE(num_truncate) {
  double integer;
  modf(AS_NUM(args[0]), &integer);
  RETURN_NUM(integer);
}

DEF_PRIMITIVE(object_same) {
  RETURN_BOOL(wrenValuesEqual(args[1], args[2]));
}

DEF_PRIMITIVE(object_not) {
  RETURN_VAL(FALSE_VAL);
}

DEF_PRIMITIVE(object_eqeq) {
  RETURN_BOOL(wrenValuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_bangeq) {
  RETURN_BOOL(!wrenValuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_is) {
  if(!IS_CLASS(args[1])) {
    RETURN_ERROR("Right operand must be a class.");
  }

  ObjClass* classObj = wrenGetClass(vm, args[0]);
  ObjClass* baseClassObj = AS_CLASS(args[1]);

  // Walk the superclass chain looking for the class.
  do {
    if(baseClassObj == classObj)
      RETURN_BOOL(true);

    classObj = classObj->superclass;
  } while(classObj != NULL);

  RETURN_BOOL(false);
}

DEF_PRIMITIVE(object_toString) {
  Obj*  obj = AS_OBJ(args[0]);
  Value name = OBJ_VAL(obj->classObj->name);
  RETURN_VAL(wrenStringFormat(vm, "instance of @", name));
}

DEF_PRIMITIVE(object_type) {
  RETURN_OBJ(wrenGetClass(vm, args[0]));
}

DEF_PRIMITIVE(object_iterate) {
  // Both Object primitives are bound on the root class, so every Wren value is
  // offered to the for-in protocol. Reading the receiver as an Obj/ObjInstance
  // without checking its object kind turns a Bool/Num/Null (a JSON scalar, a
  // missing request parameter) into a wild pointer read instead of a catchable
  // error, which killed the serving child.
  if(!IS_INSTANCE(args[0])) {
    vm->fiber->error = CONST_STRING(vm, "Value is not iterable.");
    return false;
  }
  if(IS_NULL(args[1]))
    RETURN_NUM(0);
  if(!validateNum(vm, args[1], "Iterator"))
    return false;
  double iterator = AS_NUM(args[1]);
  ++iterator;
  if(iterator < AS_INSTANCE(args[0])->obj.classObj->numFields)
    RETURN_NUM(iterator);
  RETURN_FALSE;
}

DEF_PRIMITIVE(object_iteratorValue) {
  if(!IS_INSTANCE(args[0])) {
    vm->fiber->error = CONST_STRING(vm, "Value is not iterable.");
    return false;
  }
  ObjInstance* instance = AS_INSTANCE(args[0]);
  uint32_t     index =
      validateIndex(vm, args[1], instance->obj.classObj->numFields, "Field");
  if(index == UINT32_MAX)
    return false;
  RETURN_VAL(instance->fields[index]);
}

DEF_PRIMITIVE(range_from) {
  RETURN_NUM(AS_RANGE(args[0])->from);
}

DEF_PRIMITIVE(range_to) {
  RETURN_NUM(AS_RANGE(args[0])->to);
}

DEF_PRIMITIVE(range_min) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUM(fmin(range->from, range->to));
}

DEF_PRIMITIVE(range_max) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUM(fmax(range->from, range->to));
}

DEF_PRIMITIVE(range_isInclusive) {
  RETURN_BOOL(AS_RANGE(args[0])->isInclusive);
}

DEF_PRIMITIVE(range_iterate) {
  ObjRange* range = AS_RANGE(args[0]);

  // Special case: empty range.
  if(range->from == range->to && !range->isInclusive)
    RETURN_FALSE;

  // Start the iteration.
  if(IS_NULL(args[1]))
    RETURN_NUM(range->from);

  if(!validateNum(vm, args[1], "Iterator"))
    return false;

  double iterator = AS_NUM(args[1]);

  // Iterate towards [to] from [from].
  if(range->from < range->to) {
    iterator++;
    if(iterator > range->to)
      RETURN_FALSE;
  } else {
    iterator--;
    if(iterator < range->to)
      RETURN_FALSE;
  }

  if(!range->isInclusive && iterator == range->to)
    RETURN_FALSE;

  RETURN_NUM(iterator);
}

DEF_PRIMITIVE(range_iteratorValue) {
  // Assume the iterator is a number so that is the value of the range.
  RETURN_VAL(args[1]);
}

DEF_PRIMITIVE(range_toString) {
  ObjRange* range = AS_RANGE(args[0]);

  Value from = wrenNumToString(vm, range->from);
  wrenPushRoot(vm, AS_OBJ(from));

  Value to = wrenNumToString(vm, range->to);
  wrenPushRoot(vm, AS_OBJ(to));

  Value result =
      wrenStringFormat(vm, "@$@", from, range->isInclusive ? ".." : "...", to);

  wrenPopRoot(vm);
  wrenPopRoot(vm);
  RETURN_VAL(result);
}

DEF_PRIMITIVE(string_fromCodePoint) {
  if(!validateInt(vm, args[1], "Code point"))
    return false;

  int codePoint = (int)AS_NUM(args[1]);
  if(codePoint < 0) {
    RETURN_ERROR("Code point cannot be negative.");
  } else if(codePoint > 0x10ffff) {
    RETURN_ERROR("Code point cannot be greater than 0x10ffff.");
  }

  RETURN_VAL(wrenStringFromCodePoint(vm, codePoint));
}

DEF_PRIMITIVE(string_fromByte) {
  if(!validateInt(vm, args[1], "Byte"))
    return false;
  int byte = (int)AS_NUM(args[1]);
  if(byte < 0) {
    RETURN_ERROR("Byte cannot be negative.");
  } else if(byte > 0xff) {
    RETURN_ERROR("Byte cannot be greater than 0xff.");
  }
  RETURN_VAL(wrenStringFromByte(vm, (uint8_t)byte));
}

DEF_PRIMITIVE(string_byteAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(vm, args[1], string->length, "Index");
  if(index == UINT32_MAX)
    return false;

  RETURN_NUM((uint8_t)string->value[index]);
}

DEF_PRIMITIVE(string_byteCount) {
  RETURN_NUM(AS_STRING(args[0])->length);
}

DEF_PRIMITIVE(string_codePointAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(vm, args[1], string->length, "Index");
  if(index == UINT32_MAX)
    return false;

  // If we are in the middle of a UTF-8 sequence, indicate that.
  const uint8_t* bytes = (uint8_t*)string->value;
  if((bytes[index] & 0xc0) == 0x80)
    RETURN_NUM(-1);

  // Decode the UTF-8 sequence.
  RETURN_NUM(
      wrenUtf8Decode((uint8_t*)string->value + index, string->length - index));
}

DEF_PRIMITIVE(string_contains) {
  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  RETURN_BOOL(wrenStringFind(string, search, 0) != UINT32_MAX);
}

DEF_PRIMITIVE(string_endsWith) {
  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  // Edge case: If the search string is longer then return false right away.
  if(search->length > string->length)
    RETURN_FALSE;

  RETURN_BOOL(memcmp(string->value + string->length - search->length, search->value,
                     search->length) == 0);
}

DEF_PRIMITIVE(string_indexOf1) {
  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  uint32_t index = wrenStringFind(string, search, 0);
  RETURN_NUM(index == UINT32_MAX ? -1 : (int)index);
}

DEF_PRIMITIVE(string_indexOf2) {
  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);
  uint32_t   start = validateIndex(vm, args[2], string->length, "Start");
  if(start == UINT32_MAX)
    return false;

  uint32_t index = wrenStringFind(string, search, start);
  RETURN_NUM(index == UINT32_MAX ? -1 : (int)index);
}

DEF_PRIMITIVE(string_iterate) {
  ObjString* string = AS_STRING(args[0]);

  // If we're starting the iteration, return the first index.
  if(IS_NULL(args[1])) {
    if(string->length == 0)
      RETURN_FALSE;
    RETURN_NUM(0);
  }

  if(!validateInt(vm, args[1], "Iterator"))
    return false;

  if(AS_NUM(args[1]) < 0)
    RETURN_FALSE;
  uint32_t index = (uint32_t)AS_NUM(args[1]);

  // Advance to the beginning of the next UTF-8 sequence.
  do {
    index++;
    if(index >= string->length)
      RETURN_FALSE;
  } while((string->value[index] & 0xc0) == 0x80);

  RETURN_NUM(index);
}

DEF_PRIMITIVE(string_iterateByte) {
  ObjString* string = AS_STRING(args[0]);

  // If we're starting the iteration, return the first index.
  if(IS_NULL(args[1])) {
    if(string->length == 0)
      RETURN_FALSE;
    RETURN_NUM(0);
  }

  if(!validateInt(vm, args[1], "Iterator"))
    return false;

  if(AS_NUM(args[1]) < 0)
    RETURN_FALSE;
  uint32_t index = (uint32_t)AS_NUM(args[1]);

  // Advance to the next byte.
  index++;
  if(index >= string->length)
    RETURN_FALSE;

  RETURN_NUM(index);
}

DEF_PRIMITIVE(string_iteratorValue) {
  ObjString* string = AS_STRING(args[0]);
  uint32_t   index = validateIndex(vm, args[1], string->length, "Iterator");
  if(index == UINT32_MAX)
    return false;

  RETURN_VAL(wrenStringCodePointAt(vm, string, index));
}

DEF_PRIMITIVE(string_startsWith) {
  if(!validateString(vm, args[1], "Argument"))
    return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  // Edge case: If the search string is longer then return false right away.
  if(search->length > string->length)
    RETURN_FALSE;

  RETURN_BOOL(memcmp(string->value, search->value, search->length) == 0);
}

DEF_PRIMITIVE(string_plus) {
  if(!validateString(vm, args[1], "Right operand"))
    return false;
  RETURN_VAL(wrenStringFormat(vm, "@@", args[0], args[1]));
}

DEF_PRIMITIVE(string_subscript) {
  ObjString* string = AS_STRING(args[0]);

  if(IS_NUM(args[1])) {
    int index = validateIndex(vm, args[1], string->length, "Subscript");
    if(index == -1)
      return false;

    RETURN_VAL(wrenStringCodePointAt(vm, string, index));
  }

  if(!IS_RANGE(args[1])) {
    RETURN_ERROR("Subscript must be a number or a range.");
  }

  int      step;
  uint32_t count = string->length;
  int      start = calculateRange(vm, AS_RANGE(args[1]), &count, &step);
  if(start == -1)
    return false;

  RETURN_VAL(wrenNewStringFromRange(vm, string, start, count, step));
}

DEF_PRIMITIVE(string_toString) {
  RETURN_VAL(args[0]);
}

// Bounded byte search used by String.split. memchr() finds candidate first
// bytes, so a delimiter that does not occur costs one vectorized scan.
static const char* string_find(const char* hay, size_t hay_len, const char* needle,
                               size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len)
    return NULL;
  size_t      remaining = hay_len - needle_len + 1;
  const char* p = hay;
  while(remaining > 0) {
    const char* hit = (const char*)memchr(p, needle[0], remaining);
    if(hit == NULL)
      return NULL;
    if(memcmp(hit, needle, needle_len) == 0)
      return hit;
    remaining -= (size_t)(hit - p) + 1;
    p = hit + 1;
  }
  return NULL;
}

// Escapes the five HTML specials for template output. A string with nothing to
// escape is returned as-is, which is the common case for an interpolation.
DEF_PRIMITIVE(string_safe) {
  ObjString* in = AS_STRING(args[0]);
  size_t     length = (size_t)in->length;
  size_t     out_len = 0;
  bool       special = false;

  for(size_t i = 0; i < length; i++) {
    size_t add;
    switch((unsigned char)in->value[i]) {
      case '&':
        add = 5;
        break; // &amp;
      case '<':
        add = 4;
        break; // &lt;
      case '>':
        add = 4;
        break; // &gt;
      case '"':
        add = 6;
        break; // &quot;
      case '\'':
        add = 6;
        break; // &apos;
      default:
        add = 1;
        break;
    }
    if(add > 1)
      special = true;
    if(out_len > SIZE_MAX - add)
      RETURN_ERROR("Escaped string is too large.");
    out_len += add;
  }

  if(!special)
    RETURN_VAL(args[0]);
  if(out_len == SIZE_MAX)
    RETURN_ERROR("Escaped string is too large.");

  char* out = (char*)malloc(out_len + 1);
  if(out == NULL)
    RETURN_ERROR("Out of memory escaping string.");

  size_t j = 0;
  for(size_t i = 0; i < length; i++) {
    char c = in->value[i];
    switch(c) {
      case '&':
        memcpy(out + j, "&amp;", 5);
        j += 5;
        break;
      case '<':
        memcpy(out + j, "&lt;", 4);
        j += 4;
        break;
      case '>':
        memcpy(out + j, "&gt;", 4);
        j += 4;
        break;
      case '"':
        memcpy(out + j, "&quot;", 6);
        j += 6;
        break;
      case '\'':
        memcpy(out + j, "&apos;", 6);
        j += 6;
        break;
      default:
        out[j++] = c;
        break;
    }
  }
  out[j] = '\0';

  Value result = wrenNewStringLength(vm, out, out_len);
  free(out);
  RETURN_VAL(result);
}

static bool string_is_trim_byte(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Trims the default whitespace set. ASCII bytes can be compared directly: a
// UTF-8 continuation byte is always >= 0x80, so it can never be one of these.
static Value string_trim_impl(WrenVM* vm, Value receiver, bool trimStart,
                              bool trimEnd) {
  ObjString* in = AS_STRING(receiver);
  size_t     begin = 0;
  size_t     finish = (size_t)in->length;

  if(trimStart) {
    while(begin < finish && string_is_trim_byte((unsigned char)in->value[begin]))
      begin++;
  }
  if(trimEnd) {
    while(finish > begin &&
          string_is_trim_byte((unsigned char)in->value[finish - 1]))
      finish--;
  }
  return wrenNewStringLength(vm, in->value + begin, finish - begin);
}

DEF_PRIMITIVE(string_trim) {
  RETURN_VAL(string_trim_impl(vm, args[0], true, true));
}

DEF_PRIMITIVE(string_trimStart) {
  RETURN_VAL(string_trim_impl(vm, args[0], true, false));
}

DEF_PRIMITIVE(string_trimEnd) {
  RETURN_VAL(string_trim_impl(vm, args[0], false, true));
}

// split(_) is byte-based: the delimiter is matched as bytes, so multi-byte
// text and embedded NULs are preserved in the pieces.
DEF_PRIMITIVE(string_split) {
  if(!validateString(vm, args[1], "Delimiter"))
    return false;
  ObjString* in = AS_STRING(args[0]);
  ObjString* delimiter = AS_STRING(args[1]);
  if(delimiter->length == 0)
    RETURN_ERROR("Delimiter must be a non-empty string.");

  ObjList* result = wrenNewList(vm, 0);
  wrenPushRoot(vm, (Obj*)result);

  size_t size = (size_t)in->length;
  size_t delim_len = (size_t)delimiter->length;
  size_t last = 0;

  while(last < size) {
    const char* hit =
        string_find(in->value + last, size - last, delimiter->value, delim_len);
    if(hit == NULL)
      break;
    size_t index = (size_t)(hit - in->value);
    // Root the piece across the append: wrenValueBufferWrite may grow the
    // list, and a collection there must not free the string before it is
    // stored.
    Value part = wrenNewStringLength(vm, in->value + last, index - last);
    wrenPushRoot(vm, AS_OBJ(part));
    wrenValueBufferWrite(vm, &result->elements, part);
    wrenPopRoot(vm);
    last = index + delim_len;
  }

  Value tail = last < size ? wrenNewStringLength(vm, in->value + last, size - last)
                           : wrenNewStringLength(vm, "", 0);
  wrenPushRoot(vm, AS_OBJ(tail));
  wrenValueBufferWrite(vm, &result->elements, tail);
  wrenPopRoot(vm);

  wrenPopRoot(vm); // result
  RETURN_OBJ(result);
}

DEF_PRIMITIVE(system_clock) {
  RETURN_NUM((double)clock() / CLOCKS_PER_SEC);
}

DEF_PRIMITIVE(system_gc) {
  wrenCollectGarbage(vm);
  RETURN_NULL;
}

DEF_PRIMITIVE(system_writeString) {
  if(vm->config.writeFn != NULL) {
    vm->config.writeFn(vm, AS_CSTRING(args[1]));
  }

  RETURN_VAL(args[1]);
}

// Monotonic wall-clock milliseconds for call-level deadlines. System.clock is
// CPU time and does not advance while a socket waits, so a redirect chain could
// not be bounded by it.
static double monotonic_milliseconds(void) {
#ifdef _WIN32
  return (double)GetTickCount64();
#elif defined(__APPLE__)
  static mach_timebase_info_data_t timebase;
  if(timebase.denom == 0)
    mach_timebase_info(&timebase);
  return (double)mach_absolute_time() * timebase.numer / timebase.denom / 1000000.0;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

DEF_PRIMITIVE(util_monotonicMs) {
  RETURN_NUM(monotonic_milliseconds());
};

DEF_PRIMITIVE(util_hash) {
  if(!validateString(vm, args[1], "Password"))
    return false;
  // Length-delimited like Util.sha256: a Wren string can hold NUL, and every
  // stored byte must reach the KDF.
  ObjString* input = AS_STRING(args[1]);
  char       hash[HASH_AND_SALT_LENGTH] = {0};
  hash_password(input->value, (size_t)input->length, hash);
  RETURN_VAL(wrenNewString(vm, hash));
}

DEF_PRIMITIVE(util_verify) {
  if(!validateString(vm, args[1], "Password") ||
     !validateString(vm, args[2], "Hash"))
    return false;
  ObjString* input = AS_STRING(args[1]);
  ObjString* hash_and_salt = AS_STRING(args[2]);
  int        result =
      verify_password(input->value, (size_t)input->length, hash_and_salt->value);
  RETURN_BOOL(result);
}

// Constant-time byte comparison for tokens and passwords. Lengths are folded
// into the result instead of returning early, so two equal-length values take
// the same path regardless of where they differ.
DEF_PRIMITIVE(util_secureEquals) {
  if(!validateString(vm, args[1], "Argument") ||
     !validateString(vm, args[2], "Argument"))
    return false;

  ObjString* a = AS_STRING(args[1]);
  ObjString* b = AS_STRING(args[2]);
  size_t     limit = (size_t)(a->length > b->length ? a->length : b->length);
  uint32_t   diff = a->length == b->length ? 0 : 1;

  for(size_t i = 0; i < limit; i++) {
    uint8_t ca = i < (size_t)a->length ? (uint8_t)a->value[i] : 0;
    uint8_t cb = i < (size_t)b->length ? (uint8_t)b->value[i] : 0;
    diff |= (uint32_t)(ca ^ cb);
  }
  RETURN_BOOL(diff == 0);
}

DEF_PRIMITIVE(util_randomString) {
  double len_value = AS_NUM(args[1]);
  if(len_value < 0) {
    RETURN_ERROR("Length cannot be negative.");
  }
  // Cap the length so a huge value cannot exhaust the heap; the result is a
  // heap buffer sized from the double after validation, never a stack VLA.
  if(len_value > 1000000.0) {
    RETURN_ERROR("Length too large.");
  }
  const int len = (int)len_value;

  char* random_str = (char*)malloc((size_t)len + 1);
  if(random_str == NULL) {
    RETURN_ERROR("Out of memory generating random string.");
  }
  const char charset[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const int charset_len = (int)(sizeof(charset) - 1);
  int       written = 0;

  // Draw from the OS CSPRNG (same source as password salts), not SQLite's
  // documented non-cryptographic PRNG: session IDs and CSRF tokens built on
  // Util.randomString must not be guessable. Bytes are batched into a small
  // pool and the same rejection sampling as before removes modulo bias.
  unsigned char pool[256];
  size_t        pool_used = sizeof(pool);
  while(written < len) {
    if(pool_used >= sizeof(pool)) {
      random_bytes_fill(pool, sizeof(pool));
      pool_used = 0;
    }
    unsigned char random_byte = pool[pool_used++];
    if(random_byte >= (unsigned char)(charset_len * 4)) {
      continue;
    }
    random_str[written++] = charset[random_byte % charset_len];
  }
  random_str[len] = '\0';
  Value result = wrenNewString(vm, random_str);
  free(random_str);
  RETURN_VAL(result);
}

// Returns the hex value of [c], or -1 if it is not a hex digit.
static int urlHexDigit(char c) {
  if(c >= '0' && c <= '9')
    return c - '0';
  if(c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if(c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

// Decodes a URL-encoded string in a single allocation. The previous Wren
// implementation iterated str.count (an O(n) Sequence walk) once per
// character, making decoding quadratic on attacker-controlled query/body
// input. A decoded byte is never longer than its encoded form ("%XX" -> one
// byte, "+" -> one byte, everything else passes through), so the input
// length is a safe upper bound for the output buffer.
DEF_PRIMITIVE(util_urlDecode) {
  ObjString*  string = AS_STRING(args[1]);
  uint32_t    length = string->length;
  const char* input = string->value;

  char* buffer = malloc((size_t)length + 1);
  if(buffer == NULL)
    RETURN_ERROR("Out of memory decoding URL string.");

  size_t out = 0;
  for(uint32_t i = 0; i < length; i++) {
    char c = input[i];
    if(c == '%' && i + 2 < length) {
      int high = urlHexDigit(input[i + 1]);
      int low = urlHexDigit(input[i + 2]);
      if(high >= 0 && low >= 0) {
        buffer[out++] = (char)((high << 4) | low);
        i += 2;
        continue;
      }
    }

    // A trailing '%' or one without two hex digits is passed through as-is.
    if(c == '+') {
      buffer[out++] = ' ';
    } else {
      buffer[out++] = c;
    }
  }

  Value result = wrenNewStringLength(vm, buffer, out);
  free(buffer);
  RETURN_VAL(result);
}

// Shared Base64 encoder behind Util.encodeBase64 (standard alphabet, '='
// padding) and Util.base64UrlEncode (RFC 4648 §5 alphabet, no padding). Both
// are byte-oriented over the ObjString's stored length, so they stay consistent
// with each other and with decodeBase64. Returns a heap buffer the caller owns,
// or NULL on allocation failure.
static const char BASE64_STD_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char BASE64_URL_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static char* base64_encode_alloc(const unsigned char* data, size_t len,
                                 const char* alphabet, int pad) {
  size_t groups = (len + 2) / 3;
  size_t out_len = groups * 4;
  if(!pad) {
    if(len % 3 == 1)
      out_len -= 2;
    else if(len % 3 == 2)
      out_len -= 1;
  }

  char* out = (char*)malloc(out_len + 1);
  if(out == NULL)
    return NULL;

  size_t o = 0;
  for(size_t i = 0; i < len; i += 3) {
    size_t   rem = len - i;
    unsigned b0 = data[i];
    unsigned b1 = rem > 1 ? data[i + 1] : 0;
    unsigned b2 = rem > 2 ? data[i + 2] : 0;

    out[o++] = alphabet[b0 >> 2];
    out[o++] = alphabet[((b0 & 0x3) << 4) | (b1 >> 4)];
    if(rem > 1)
      out[o++] = alphabet[((b1 & 0xF) << 2) | (b2 >> 6)];
    else if(pad)
      out[o++] = '=';
    if(rem > 2)
      out[o++] = alphabet[b2 & 0x3F];
    else if(pad)
      out[o++] = '=';
  }
  out[o] = '\0';
  return out;
}

DEF_PRIMITIVE(util_encodeBase64) {
  ObjString* input = AS_STRING(args[1]);
  char*      encoded = base64_encode_alloc((const unsigned char*)input->value,
                                           input->length, BASE64_STD_CHARS, 1);
  if(encoded == NULL)
    RETURN_ERROR("Out of memory encoding Base64.");
  Value result = wrenNewString(vm, encoded);
  free(encoded);
  RETURN_VAL(result);
}

DEF_PRIMITIVE(util_base64UrlEncode) {
  ObjString* input = AS_STRING(args[1]);
  char*      encoded = base64_encode_alloc((const unsigned char*)input->value,
                                           input->length, BASE64_URL_CHARS, 0);
  if(encoded == NULL)
    RETURN_ERROR("Out of memory encoding Base64url.");
  Value result = wrenNewString(vm, encoded);
  free(encoded);
  RETURN_VAL(result);
}

// Hashes the string's stored bytes (input->length), not strlen(input->value),
// so embedded NULs are part of the digest.
DEF_PRIMITIVE(util_sha256) {
  ObjString* input = AS_STRING(args[1]);
  char       hex[65];
  sha256_hex(input->value, input->length, hex);
  RETURN_VAL(wrenNewString(vm, hex));
}

// BASE64URL(SHA256(input)) without padding: exactly PKCE's code_challenge for
// the S256 method (RFC 7636).
DEF_PRIMITIVE(util_sha256Base64Url) {
  ObjString*    input = AS_STRING(args[1]);
  unsigned char digest[32];
  sha256_raw(input->value, input->length, digest);
  char* encoded = base64_encode_alloc(digest, sizeof(digest), BASE64_URL_CHARS, 0);
  if(encoded == NULL)
    RETURN_ERROR("Out of memory encoding SHA-256 digest.");
  Value result = wrenNewString(vm, encoded);
  free(encoded);
  RETURN_VAL(result);
}

// Builds a default page from the shared C-side template (BIALET_HEADER_PAGE /
// BIALET_FOOTER_PAGE). This is the single source of truth for the default page
// chrome; Wren's Response.page/pageHtml call this instead of redefining the
// template in Wren, so both the C-side fallbacks and the Wren API render the
// exact same page. [args[1]] and [args[2]] must already be HTML-escaped.
DEF_PRIMITIVE(response_default_page) {
  if(!validateString(vm, args[1], "title") ||
     !validateString(vm, args[2], "message"))
    return false;

  ObjString* title = AS_STRING(args[1]);
  ObjString* message = AS_STRING(args[2]);

  static const char kMessageWrap[] = "</h1><p>";

  size_t head_len = strlen(BIALET_HEADER_PAGE);
  size_t foot_len = strlen(BIALET_FOOTER_PAGE);
  size_t needed = head_len + title->length + sizeof(kMessageWrap) - 1 +
                  message->length + foot_len + 1;
  char*  buffer = (char*)malloc(needed);
  if(buffer == NULL)
    RETURN_ERROR("Out of memory building page.");

  char* p = buffer;
  memcpy(p, BIALET_HEADER_PAGE, head_len);
  p += head_len;
  memcpy(p, title->value, title->length);
  p += title->length;
  memcpy(p, kMessageWrap, sizeof(kMessageWrap) - 1);
  p += sizeof(kMessageWrap) - 1;
  memcpy(p, message->value, message->length);
  p += message->length;
  memcpy(p, BIALET_FOOTER_PAGE, foot_len);
  p += foot_len;
  *p = '\0';

  Value result = wrenNewStringLength(vm, buffer, (uint32_t)(p - buffer));
  free(buffer);
  RETURN_VAL(result);
}

DEF_PRIMITIVE(http_call) {
  struct HttpRequest request;
  request.url = AS_CSTRING(args[1]);
  request.method = AS_CSTRING(args[2]);
  request.raw_headers = AS_CSTRING(args[3]);
  request.postData = AS_CSTRING(args[4]);
  request.basicAuth = AS_CSTRING(args[5]);
  request.timeout = IS_NUM(args[6]) ? (long)AS_NUM(args[6]) : 0;
  request.connectTimeout = IS_NUM(args[7]) ? (long)AS_NUM(args[7]) : 0;

  struct HttpResponse response;
  response.error = 0;
  response.status = 200;
  response.headers = NULL;
  response.body = NULL;
  response.error_message = NULL;

  http_call_perform(&request, &response);

  ObjList* res = wrenNewList(vm, 5);
  res->elements.data[0] = NUM_VAL(response.status);
  res->elements.data[1] =
      wrenNewString(vm, response.headers ? response.headers : "");
  res->elements.data[2] = wrenNewString(vm, response.body ? response.body : "");
  res->elements.data[3] = NUM_VAL(response.error);
  res->elements.data[4] =
      wrenNewString(vm, response.error_message ? response.error_message : "");

  // http_call_perform heap-allocates the body/headers (curl chunk copies,
  // or the Windows parse_http_response buffers); release them now that the
  // values live in Wren strings. free(NULL) is safe for early-error paths.
  free(response.headers);
  free(response.body);
  free(response.error_message);

  RETURN_OBJ(res);
}

DEF_PRIMITIVE(test_runRequest) {
  const char* route = AS_CSTRING(args[1]);
  const char* message = AS_CSTRING(args[2]);

  // Get root directory from bialet config
  extern const char* bialet_get_full_root_dir();
  const char*        rootDir = bialet_get_full_root_dir();

  // Strip query string from route for file lookup
  const char* qmark = strchr(route, '?');
  int         routePathLen = (int)(qmark ? (size_t)(qmark - route) : strlen(route));

  // Resolve route to a .wren file path, mirroring the real server: probe the
  // exact ".wren" file, then the directory index, then walk up the path
  // looking for a <folder>.wren route handler (deepest match wins). routesLen
  // is the length of the URL prefix of the matched file, so Request.route(n)
  // reads the segments that follow it (null at the bare folder URL) exactly
  // as it does over HTTP.
  char path[4096];
  char filePath[4096];
  int  routesLen = 0;
  {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s%.*s.wren", rootDir, routePathLen, route);
    strncpy(path, tmp, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
  }

  // Check if file exists, otherwise try index.wren
  extern char* read_file(const char* path);
  char*        code = read_file(path);
  if(code == NULL) {
    char tmp2[4096];
    snprintf(tmp2, sizeof(tmp2), "%s%.*s/index.wren", rootDir, routePathLen, route);
    strncpy(path, tmp2, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    code = read_file(path);
    if(code != NULL)
      routesLen = routePathLen;
  } else {
    routesLen = routePathLen;
  }

  if(code == NULL) {
    // No exact file or directory index: walk up the path probing
    // <folder>.wren route handlers, deepest first, matching the server.
    int prefixLen = routePathLen;
    while(prefixLen > 0) {
      int slash = prefixLen - 1;
      while(slash >= 0 && route[slash] != '/')
        slash--;
      if(slash <= 0)
        break; // reached the root: no folder name to name a route file after
      prefixLen = slash;
      char tmp3[4096];
      snprintf(tmp3, sizeof(tmp3), "%s%.*s.wren", rootDir, prefixLen, route);
      strncpy(path, tmp3, sizeof(path) - 1);
      path[sizeof(path) - 1] = '\0';
      code = read_file(path);
      if(code != NULL) {
        routesLen = prefixLen;
        break;
      }
    }
  }

  if(code == NULL) {
    // Return 404 response
    ObjList* res = wrenNewList(vm, 3);
    res->elements.data[0] = NUM_VAL(404);
    res->elements.data[1] = OBJ_VAL(wrenNewString(vm, "Not Found"));
    res->elements.data[2] = OBJ_VAL(wrenNewString(vm, ""));
    RETURN_OBJ(res);
  }

  // Build HttpMessage struct
  extern struct String create_string(const char* str, size_t len);
  struct HttpMessage*  hm = (struct HttpMessage*)malloc(sizeof(struct HttpMessage));
  memset(hm, 0, sizeof(struct HttpMessage));

  // Store full message
  hm->message = create_string(message, strlen(message));

  // Parse method from message
  const char* methodEnd = strchr(message, ' ');
  if(methodEnd) {
    size_t methodLen = methodEnd - message;
    char   methodBuf[32];
    // Bound the copy so a hostile test request cannot overflow the stack
    // buffer; the truncated value is enough to drive the route handler.
    size_t methodCopy =
        methodLen < sizeof(methodBuf) - 1 ? methodLen : sizeof(methodBuf) - 1;
    memcpy(methodBuf, message, methodCopy);
    methodBuf[methodCopy] = '\0';
    hm->method = create_string(methodBuf, methodCopy);

    // Parse URI
    const char* uriStart = methodEnd + 1;
    const char* uriEnd = strchr(uriStart, ' ');
    if(uriEnd) {
      size_t uriLen = uriEnd - uriStart;
      char   uriBuf[1024];
      size_t uriCopy = uriLen < sizeof(uriBuf) - 1 ? uriLen : sizeof(uriBuf) - 1;
      memcpy(uriBuf, uriStart, uriCopy);
      uriBuf[uriCopy] = '\0';
      hm->uri = create_string(uriBuf, uriCopy);
    } else {
      hm->uri = create_string("/", 1);
    }
  } else {
    hm->method = create_string("GET", 3);
    hm->uri = create_string("/", 1);
  }

  // Extract headers
  const char* headersStart = strchr(message, '\n');
  if(headersStart) {
    headersStart++; // skip \n
    const char* bodyStart = strstr(headersStart, "\r\n\r\n");
    if(bodyStart) {
      size_t headersLen = bodyStart - headersStart;
      char   headersBuf[4096];
      if(headersLen < sizeof(headersBuf)) {
        strncpy(headersBuf, headersStart, headersLen);
        headersBuf[headersLen] = '\0';
        hm->headers = create_string(headersBuf, headersLen);
      } else {
        hm->headers = create_string("", 0);
      }
    } else {
      hm->headers = create_string(headersStart, strlen(headersStart));
    }
  } else {
    hm->headers = create_string("", 0);
  }

  hm->routes = create_string(route, routesLen);

  // Call bialetRun to execute the route handler
  extern struct BialetResponse bialet_run(char* module, char* code,
                                          struct HttpMessage* hm);
  snprintf(filePath, sizeof(filePath), "%s%s", rootDir, route);
  struct BialetResponse response = bialet_run(filePath, code, hm);

  // Free HttpMessage fields
  free(hm->method.str);
  free(hm->uri.str);
  free(hm->headers.str);
  free(hm->routes.str);
  free(hm->message.str);
  free(hm);
  free(code);

  // Return [status, body, headers_string]
  ObjList* res = wrenNewList(vm, 3);
  res->elements.data[0] = NUM_VAL(response.status);
  res->elements.data[1] =
      OBJ_VAL(wrenNewString(vm, response.body ? response.body : ""));
  res->elements.data[2] =
      OBJ_VAL(wrenNewString(vm, response.header ? response.header : ""));

  // Free response data (respect ownership flags: error fallback pages are
  // static strings and must not be freed).
  if(response.body_owned)
    free(response.body);
  if(response.header_owned)
    free(response.header);

  RETURN_OBJ(res);
}

DEF_PRIMITIVE(tests_skip) {
  extern void bialet_test_mark_skip(void);
  bialet_test_mark_skip();
  RETURN_NULL;
}

void setTimezone(const char* tz) {
  if(tz == NULL || tz[0] == '\0')
    return;
#if defined(_WIN32)
  // putenv() stores the pointer it is given, so it must outlive the call.
  // Allocate a fresh string, publish it, then release the previous one (the
  // Windows CRT never frees the replaced value itself).
  static char* tz_buf = NULL;
  int          needed = snprintf(NULL, 0, "TZ=%s", tz);
  if(needed < 0)
    return;
  char* new_buf = malloc((size_t)needed + 1);
  if(new_buf == NULL)
    return;
  snprintf(new_buf, (size_t)needed + 1, "TZ=%s", tz);
  putenv(new_buf);
  free(tz_buf);
  tz_buf = new_buf;
#else
  setenv("TZ", tz, 1);
#endif
  tzset();
}

// Accepts the Wren-level timezone argument for the date primitives: a string,
// or null/absent (which means "keep the current zone"). AS_CSTRING on a Wren
// null yields the bogus pointer 0x21, not a C NULL, so the guard setTimezone()
// already has was unreachable from Wren and a null Date.tz (Db.init installs
// one when BIALET_TIMEZONE is unset) crashed the process.
static int date_timezone_arg(WrenVM* vm, Value arg, const char** out) {
  if(IS_NULL(arg) || IS_UNDEFINED(arg)) {
    *out = NULL;
    return 1;
  }
  if(!IS_STRING(arg)) {
    vm->fiber->error = wrenStringFormat(vm, "Timezone must be a string.");
    return 0;
  }
  *out = AS_CSTRING(arg);
  return 1;
}

DEF_PRIMITIVE(date_current) {
  const char* tz;
  if(!date_timezone_arg(vm, args[1], &tz))
    return false;
  setTimezone(tz);
  char       fullDate[20];
  time_t     now = time(NULL);
  struct tm* local = localtime(&now);
  strftime(fullDate, 20, "%Y-%m-%d %H:%M:%S", local);
  RETURN_VAL(wrenNewString(vm, fullDate));
}

// Formats an epoch value as a local date string. Date.new(number) is a year,
// not a timestamp, so this is the constructor for unix seconds.
DEF_PRIMITIVE(date_fromUnix) {
  if(!validateNum(vm, args[1], "Unix time"))
    return false;
  const char* tz;
  if(!date_timezone_arg(vm, args[2], &tz))
    return false;
  setTimezone(tz);
  time_t     seconds = (time_t)AS_NUM(args[1]);
  struct tm* local = localtime(&seconds);
  if(local == NULL)
    RETURN_ERROR("Unix time is out of range.");
  char fullDate[20];
  if(strftime(fullDate, sizeof(fullDate), "%Y-%m-%d %H:%M:%S", local) == 0)
    RETURN_ERROR("Unix time is out of range.");
  RETURN_VAL(wrenNewString(vm, fullDate));
}

/* Whether year [y] has 53 ISO 8601 weeks: it does exactly when its January 1
 * falls on a Thursday, or on a Wednesday in a leap year. */
static int year_has_53_iso_weeks(int y) {
  struct tm jan1 = {0};
  jan1.tm_year = y - 1900;
  jan1.tm_mday = 1;
  mktime(&jan1);
  int iso_wday = jan1.tm_wday == 0 ? 7 : jan1.tm_wday; /* Mon=1 .. Sun=7 */
  int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return iso_wday == 4 || (leap && iso_wday == 3);
}

/* ISO 8601 week number (1-53) of [y]-[m]-[d]. strftime()'s %V produces this
 * on glibc but is unsupported (empty output) on the msvcrt the Windows build
 * links, so it is computed here for every platform. */
static int iso_week_number(int y, int m, int d) {
  struct tm t = {0};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  mktime(&t);
  int iso_wday = t.tm_wday == 0 ? 7 : t.tm_wday; /* Mon=1 .. Sun=7 */
  int week = (t.tm_yday + 1 - iso_wday + 10) / 7;
  if(week < 1)
    return iso_week_number(y - 1, 12, 28); /* last week of the previous year */
  if(week == 53 && !year_has_53_iso_weeks(y))
    return 1; /* the week of Dec 29-31 belongs to next year */
  return week;
}

/* ISO 8601 week-numbering year for [y]-[m]-[d]: differs from [y] only for the
 * first or last few days of the year. */
static int iso_week_year(int y, int m, int d) {
  struct tm t = {0};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  mktime(&t);
  int iso_wday = t.tm_wday == 0 ? 7 : t.tm_wday;
  int week = (t.tm_yday + 1 - iso_wday + 10) / 7;
  if(week < 1)
    return y - 1;
  if(week == 53 && !year_has_53_iso_weeks(y))
    return y + 1;
  return y;
}

/* Appends the strftime expansion of [fmt] to out[*pos], growing nothing beyond
 * [cap]. */
static void date_format_append(char* out, size_t cap, size_t* pos,
                               const struct tm* t, const char* fmt) {
  char tmp[64];
  if(strftime(tmp, sizeof(tmp), fmt, t) == 0)
    return;
  for(const char* p = tmp; *p && *pos + 1 < cap; p++)
    out[(*pos)++] = *p;
  out[*pos] = '\0';
}

DEF_PRIMITIVE(date_format) {
  if(!validateString(vm, args[1], "Format"))
    return false;
  const char* format = AS_CSTRING(args[1]);
  int         year = AS_NUM(args[2]);
  int         month = AS_NUM(args[3]);
  int         day = AS_NUM(args[4]);
  int         hour = AS_NUM(args[5]);
  int         minute = AS_NUM(args[6]);
  int         second = AS_NUM(args[7]);
  const char* tz;
  if(!date_timezone_arg(vm, args[8], &tz))
    return false;
  setTimezone(tz);
  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  // mktime fills in tm_wday and tm_yday, which strftime() needs for %w, %j
  // and %V. Without it, those directives always read the zero-initialized
  // struct fields.
  mktime(&t);

  // The C library's strftime() is used for the C89 directives, but the POSIX
  // extras are expanded here so the output matches on Windows too: the msvcrt
  // that the MinGW build links against does not implement %F %T %V %G %g %e
  // %u %R %r %h %n %t %s and returns empty text for them.
  char   out[128];
  size_t pos = 0;
  out[0] = '\0';
  for(const char* p = format; *p && pos + 16 < sizeof(out); p++) {
    if(*p != '%') {
      out[pos++] = *p;
      continue;
    }
    char d = *++p;
    if(d == '\0')
      break;
    switch(d) {
      case '%':
        out[pos++] = '%';
        break;
      case 'F':
        date_format_append(out, sizeof(out), &pos, &t, "%Y-%m-%d");
        break;
      case 'T':
        date_format_append(out, sizeof(out), &pos, &t, "%H:%M:%S");
        break;
      case 'R':
        date_format_append(out, sizeof(out), &pos, &t, "%H:%M");
        break;
      case 'r':
        date_format_append(out, sizeof(out), &pos, &t, "%I:%M:%S %p");
        break;
      case 'h':
        date_format_append(out, sizeof(out), &pos, &t, "%b");
        break;
      case 'n':
        out[pos++] = '\n';
        break;
      case 't':
        out[pos++] = '\t';
        break;
      case 'V':
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "%02d",
                                iso_week_number(year, month, day));
        break;
      case 'G':
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "%04d",
                                iso_week_year(year, month, day));
        break;
      case 'g':
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "%02d",
                                iso_week_year(year, month, day) % 100);
        break;
      case 'u':
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "%d",
                                t.tm_wday == 0 ? 7 : t.tm_wday);
        break;
      case 'e':
        /* %e is space-padded, %d is zero-padded. */
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "%2d", t.tm_mday);
        break;
      case 's': {
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%ld", (long)mktime(&t));
        for(char* c = tmp; *c && pos + 1 < sizeof(out); c++)
          out[pos++] = *c;
        break;
      }
      default: {
        char fmt[3] = {'%', d, '\0'};
        date_format_append(out, sizeof(out), &pos, &t, fmt);
        break;
      }
    }
  }
  out[pos] = '\0';
  RETURN_VAL(wrenNewString(vm, out));
}

DEF_PRIMITIVE(date_unix) {
  int         year = AS_NUM(args[1]);
  int         month = AS_NUM(args[2]);
  int         day = AS_NUM(args[3]);
  int         hour = AS_NUM(args[4]);
  int         minute = AS_NUM(args[5]);
  int         second = AS_NUM(args[6]);
  const char* tz;
  if(!date_timezone_arg(vm, args[7], &tz))
    return false;
  setTimezone(tz);

  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t result = mktime(&t);
  RETURN_NUM((double)result);
};

DEF_PRIMITIVE(markdown_html) {
  if(!validateString(vm, args[1], "Markdown"))
    return false;
  char* html = markdown_to_html(AS_CSTRING(args[1]));
  if(html == NULL)
    RETURN_ERROR("Markdown output too large");
  Value result = wrenNewString(vm, html);
  free(html);
  RETURN_VAL(result);
}

DEF_PRIMITIVE(markdown_file) {
  if(!validateString(vm, args[1], "Markdown file name"))
    return false;
  char* content = bialet_read_file(AS_CSTRING(args[1]));
  if(content == NULL)
    RETURN_FALSE;
  char* html = markdown_to_html(content);
  free(content);
  if(html == NULL)
    RETURN_ERROR("Markdown output too large");
  Value result = wrenNewString(vm, html);
  free(html);
  RETURN_VAL(result);
};

// A query argument is either a plain string (Query.fromString, Query.new) or a
// backtick literal, which is an OBJ_QUERY instance -- an ObjString with a
// different object kind, so IS_STRING/validateString rejects it.
static bool is_query_text(Value v) {
  if(IS_STRING(v))
    return true;
  return IS_OBJ(v) && AS_OBJ(v)->type == OBJ_QUERY;
}

// True when the text carries a NUL inside its recorded length. Query text and
// string parameters are stored, prepared and bound as C strings (strdup,
// sqlite3_prepare_v2 and sqlite3_bind_text with a length of -1), and results
// are read back with strlen, so an embedded NUL would silently make SQLite see
// a prefix of the value the application validated. Rejecting it keeps the
// bound value byte-identical to the Wren string.
static bool has_embedded_nul(Value v) {
  ObjString* text = AS_STRING(v);
  return (size_t)text->length != strlen(text->value);
}

// Binds each parameter in [params] to [query]. Returns false (and sets a
// fiber error) when a parameter has a type the native layer cannot bind, so a
// parameter is never dropped silently: that would shift every later `?`
// placeholder left and corrupt the row. The Wren layer stringifies
// non-primitives (HtmlNode, Date, ...) via toString before calling in, so a
// failure here means a caller bypassed that and passed an unsupported type.
static bool queryPrepare(WrenVM* vm, BialetQuery* query, ObjList* params) {
  Value val;
  if(vm->config.writeFn != NULL) {
    for(int i = 0; i < params->elements.count; i++) {
      val = params->elements.data[i];
      int ok = 0;
      if(IS_NULL(val)) {
        ok = add_parameter(query, 0, BIALETQUERYTYPE_NULL);
      } else if(IS_BOOL(val)) {
        ok = add_parameter(query, AS_BOOL(val) ? "1" : "0", BIALETQUERYTYPE_BOOLEAN);
      } else if(IS_NUM(val)) {
        char num[MAX_NUMBER_LENGTH];
        // %.17g keeps full double precision and renders compactly (DBL_MAX
        // is ~24 chars); %f expands to hundreds of chars and overflowed
        // this fixed stack buffer.
        int written = snprintf(num, sizeof(num), "%.17g", AS_NUM(val));
        if(written < 0 || written >= (int)sizeof(num)) {
          vm->fiber->error = wrenStringFormat(
              vm, "Query parameter %d does not fit in a number buffer", i + 1);
          return false;
        }
        ok = add_parameter(query, num, BIALETQUERYTYPE_NUMBER);
      } else if(IS_STRING(val)) {
        if(has_embedded_nul(val)) {
          vm->fiber->error =
              wrenStringFormat(vm,
                               "Query parameter %d contains a NUL byte, which "
                               "cannot be bound as text",
                               i + 1);
          return false;
        }
        ok = add_parameter(query, AS_CSTRING(val), BIALETQUERYTYPE_STRING);
      } else {
        vm->fiber->error = wrenStringFormat(
            vm,
            "Unsupported query parameter type @; only null, bool, num and "
            "string are bindable. Stringify values with toString() first.",
            OBJ_VAL(wrenGetClass(vm, val)->name));
        return false;
      }
      // A parameter that cannot be stored must fail the query: binding the
      // remaining values would shift every later `?` left and corrupt the row.
      if(ok != 0) {
        vm->fiber->error =
            wrenStringFormat(vm, "Out of memory binding query parameter %d", i + 1);
        return false;
      }
    }
    vm->config.queryFn(vm, query);
    // queryFn has no return value; it reports failure by setting the fiber's
    // error. Without this check the primitive still succeeded, so Db.migrate
    // recorded a version whose SQL never ran.
    if(wrenHasError(vm->fiber))
      return false;
  }
  return true;
}

DEF_PRIMITIVE(query_fetch) {
  if(!is_query_text(args[1])) {
    vm->fiber->error = wrenStringFormat(vm, "Query must be a string.");
    return false;
  }
  if(has_embedded_nul(args[1])) {
    vm->fiber->error =
        wrenStringFormat(vm, "Query text must not contain NUL bytes.");
    return false;
  }
  if(!IS_LIST(args[2])) {
    vm->fiber->error = wrenStringFormat(vm, "Query parameters must be a list.");
    return false;
  }
  BialetQuery* query = create_bialet_query();
  if(query == NULL)
    RETURN_ERROR("Out of memory allocating query");
  char* qs = strdup(AS_CSTRING(args[1]));
  if(qs == NULL) {
    free_bialet_query(query);
    RETURN_ERROR("Out of memory allocating query string");
  }
  query->queryString = qs;
  if(!queryPrepare(vm, query, AS_LIST(args[2]))) {
    free_bialet_query(query);
    return false;
  }

  // The rows, their keys and their values below are reachable only from C
  // locals until the finished list is returned, so a collection landing inside
  // wrenNew*/wrenMapSet would free a half-built row: a 20000-row SELECT
  // reliably killed the HTTP child by handing the collector a partially built
  // list. Rooting every temporary works but makes the collector re-mark the
  // whole growing result set on each collection; pausing it instead is bounded
  // work proportional to the result set (which the query itself bounds), and is
  // the same choice prim_json_parse_primitive makes for the same reason.
  size_t saved_next_gc = vm->nextGC;
  vm->nextGC = SIZE_MAX;

  // wrenNewList(n) leaves its n slots uninitialized, so both lists grow by
  // appending: a slot only ever holds a real Value.
  ObjList* list = wrenNewList(vm, 0);

  // Column names are identical on every row of a result set, so build them
  // once instead of allocating a fresh list and a fresh string per column for
  // each row. A 2000-row SELECT used to intern thousands of duplicate names.
  ObjList* names = wrenNewList(vm, 0);
  if(query->resultsCount > 0) {
    BialetQueryResult first = query->results[0];
    for(int j = 0; j < first.rowCount; j++) {
      wrenValueBufferWrite(vm, &names->elements,
                           wrenNewString(vm, first.rows[j].name));
    }
  }

  for(int i = 0; i < query->resultsCount; i++) {
    BialetQueryResult res = query->results[i];
    ObjMap*           row = wrenNewMap(vm);

    for(int j = 0; j < res.rowCount; j++) {
      Value colValue =
          res.rows[j].type == BIALETQUERYTYPE_NULL
              ? NULL_VAL
              : wrenNewStringLength(vm, res.rows[j].value, res.rows[j].size);
      wrenMapSet(vm, row, names->elements.data[j], colValue);
    }
    wrenValueBufferWrite(vm, &list->elements, OBJ_VAL(row));
  }
  vm->nextGC = saved_next_gc;
  free_bialet_query(query);
  RETURN_OBJ(list);
}

DEF_PRIMITIVE(query_execute) {
  if(!is_query_text(args[1])) {
    vm->fiber->error = wrenStringFormat(vm, "Query must be a string.");
    return false;
  }
  if(has_embedded_nul(args[1])) {
    vm->fiber->error =
        wrenStringFormat(vm, "Query text must not contain NUL bytes.");
    return false;
  }
  if(!IS_LIST(args[2])) {
    vm->fiber->error = wrenStringFormat(vm, "Query parameters must be a list.");
    return false;
  }
  BialetQuery* query = create_bialet_query();
  if(query == NULL)
    RETURN_ERROR("Out of memory allocating query");
  char* qs = strdup(AS_CSTRING(args[1]));
  if(qs == NULL) {
    free_bialet_query(query);
    RETURN_ERROR("Out of memory allocating query string");
  }
  query->queryString = qs;
  if(!queryPrepare(vm, query, AS_LIST(args[2]))) {
    free_bialet_query(query);
    return false;
  }
  if(query->lastInsertId) {
    Value lastInsertId = wrenNewString(vm, query->lastInsertId);
    free_bialet_query(query);
    RETURN_VAL(lastInsertId);
  } else {
    free_bialet_query(query);
    RETURN_NULL;
  }
}

DEF_PRIMITIVE(query_toString) {
  if(!is_query_text(args[0])) {
    vm->fiber->error = wrenStringFormat(vm, "Query must be a string.");
    return false;
  }
  const char* queryString = AS_CSTRING(args[0]);
  RETURN_VAL(wrenNewString(vm, queryString));
}

DEF_PRIMITIVE(query_new) {
  // Query.new() (used by Query.fromString) supplies no text.
  if(IS_NULL(args[1]))
    RETURN_VAL(wrenNewQuery(vm, ""));
  if(!is_query_text(args[1])) {
    vm->fiber->error = wrenStringFormat(vm, "Query must be a string.");
    return false;
  }
  if(has_embedded_nul(args[1])) {
    vm->fiber->error =
        wrenStringFormat(vm, "Query text must not contain NUL bytes.");
    return false;
  }
  const char* queryString = AS_CSTRING(args[1]);
  RETURN_VAL(wrenNewQuery(vm, queryString));
}

// Creates either the Object or Class class in the core module with [name].
static ObjClass* defineClass(WrenVM* vm, ObjModule* module, const char* name) {
  ObjString* nameString = AS_STRING(wrenNewString(vm, name));
  wrenPushRoot(vm, (Obj*)nameString);

  ObjClass* classObj = wrenNewSingleClass(vm, 0, nameString);

  wrenDefineVariable(vm, module, name, nameString->length, OBJ_VAL(classObj), NULL);

  wrenPopRoot(vm);
  return classObj;
}

void wrenInitializeCore(WrenVM* vm) {
  ObjModule* coreModule = wrenNewModule(vm, NULL);
  wrenPushRoot(vm, (Obj*)coreModule);

  // The core module's key is null in the module map.
  wrenMapSet(vm, vm->modules, NULL_VAL, OBJ_VAL(coreModule));
  wrenPopRoot(vm); // coreModule.

  // Define the root Object class. This has to be done a little specially
  // because it has no superclass.
  vm->objectClass = defineClass(vm, coreModule, "Object");
  PRIMITIVE(vm->objectClass, "!", object_not);
  PRIMITIVE(vm->objectClass, "==(_)", object_eqeq);
  PRIMITIVE(vm->objectClass, "!=(_)", object_bangeq);
  PRIMITIVE(vm->objectClass, "is(_)", object_is);
  PRIMITIVE(vm->objectClass, "toString", object_toString);
  PRIMITIVE(vm->objectClass, "type", object_type);
  PRIMITIVE(vm->objectClass, "iterate(_)", object_iterate);
  PRIMITIVE(vm->objectClass, "iteratorValue(_)", object_iteratorValue);

  // Now we can define Class, which is a subclass of Object.
  vm->classClass = defineClass(vm, coreModule, "Class");
  wrenBindSuperclass(vm, vm->classClass, vm->objectClass);
  PRIMITIVE(vm->classClass, "name", class_name);
  PRIMITIVE(vm->classClass, "supertype", class_supertype);
  PRIMITIVE(vm->classClass, "toString", class_toString);
  PRIMITIVE(vm->classClass, "attributes", class_attributes);

  // Finally, we can define Object's metaclass which is a subclass of Class.
  ObjClass* objectMetaclass = defineClass(vm, coreModule, "Object metaclass");

  // Wire up the metaclass relationships now that all three classes are built.
  vm->objectClass->obj.classObj = objectMetaclass;
  objectMetaclass->obj.classObj = vm->classClass;
  vm->classClass->obj.classObj = vm->classClass;

  // Do this after wiring up the metaclasses so objectMetaclass doesn't get
  // collected.
  wrenBindSuperclass(vm, objectMetaclass, vm->classClass);

  PRIMITIVE(objectMetaclass, "same(_,_)", object_same);

  // The core class diagram ends up looking like this, where single lines point
  // to a class's superclass, and double lines point to its metaclass:
  //
  //        .------------------------------------. .====.
  //        |                  .---------------. | #    #
  //        v                  |               v | v    #
  //   .---------.   .-------------------.   .-------.  #
  //   | Object  |==>| Object metaclass  |==>| Class |=="
  //   '---------'   '-------------------'   '-------'
  //        ^                                 ^ ^ ^ ^
  //        |                  .--------------' # | #
  //        |                  |                # | #
  //   .---------.   .-------------------.      # | # -.
  //   |  Base   |==>|  Base metaclass   |======" | #  |
  //   '---------'   '-------------------'        | #  |
  //        ^                                     | #  |
  //        |                  .------------------' #  | Example classes
  //        |                  |                    #  |
  //   .---------.   .-------------------.          #  |
  //   | Derived |==>| Derived metaclass |=========="  |
  //   '---------'   '-------------------'            -'

  // The rest of the classes can now be defined normally.
  wrenInterpret(vm, NULL, coreModuleSource);

  vm->boolClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Bool"));
  PRIMITIVE(vm->boolClass, "toString", bool_toString);
  PRIMITIVE(vm->boolClass, "!", bool_not);

  vm->fiberClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Fiber"));
  PRIMITIVE(vm->fiberClass->obj.classObj, "new(_)", fiber_new);
  PRIMITIVE(vm->fiberClass->obj.classObj, "abort(_)", fiber_abort);
  PRIMITIVE(vm->fiberClass->obj.classObj, "current", fiber_current);
  PRIMITIVE(vm->fiberClass->obj.classObj, "suspend()", fiber_suspend);
  PRIMITIVE(vm->fiberClass->obj.classObj, "yield()", fiber_yield);
  PRIMITIVE(vm->fiberClass->obj.classObj, "yield(_)", fiber_yield1);
  PRIMITIVE(vm->fiberClass, "call()", fiber_call);
  PRIMITIVE(vm->fiberClass, "call(_)", fiber_call1);
  PRIMITIVE(vm->fiberClass, "error", fiber_error);
  PRIMITIVE(vm->fiberClass, "isDone", fiber_isDone);
  PRIMITIVE(vm->fiberClass, "transfer()", fiber_transfer);
  PRIMITIVE(vm->fiberClass, "transfer(_)", fiber_transfer1);
  PRIMITIVE(vm->fiberClass, "transferError(_)", fiber_transferError);
  PRIMITIVE(vm->fiberClass, "try()", fiber_try);
  PRIMITIVE(vm->fiberClass, "try(_)", fiber_try1);

  vm->fnClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Fn"));
  PRIMITIVE(vm->fnClass->obj.classObj, "new(_)", fn_new);

  PRIMITIVE(vm->fnClass, "arity", fn_arity);

  FUNCTION_CALL(vm->fnClass, "call()", fn_call0);
  FUNCTION_CALL(vm->fnClass, "call(_)", fn_call1);
  FUNCTION_CALL(vm->fnClass, "call(_,_)", fn_call2);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_)", fn_call3);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_)", fn_call4);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_)", fn_call5);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_)", fn_call6);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_)", fn_call7);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_)", fn_call8);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_)", fn_call9);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_)", fn_call10);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_)", fn_call11);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_,_)", fn_call12);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_,_,_)", fn_call13);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_)", fn_call14);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)", fn_call15);
  FUNCTION_CALL(vm->fnClass, "call(_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_)", fn_call16);

  PRIMITIVE(vm->fnClass, "toString", fn_toString);

  vm->nullClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Null"));
  PRIMITIVE(vm->nullClass, "!", null_not);
  PRIMITIVE(vm->nullClass, "toString", null_toString);

  vm->numClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Num"));
  PRIMITIVE(vm->numClass->obj.classObj, "fromString(_)", num_fromString);
  PRIMITIVE(vm->numClass->obj.classObj, "infinity", num_infinity);
  PRIMITIVE(vm->numClass->obj.classObj, "nan", num_nan);
  PRIMITIVE(vm->numClass->obj.classObj, "pi", num_pi);
  PRIMITIVE(vm->numClass->obj.classObj, "tau", num_tau);
  PRIMITIVE(vm->numClass->obj.classObj, "largest", num_largest);
  PRIMITIVE(vm->numClass->obj.classObj, "smallest", num_smallest);
  PRIMITIVE(vm->numClass->obj.classObj, "maxSafeInteger", num_maxSafeInteger);
  PRIMITIVE(vm->numClass->obj.classObj, "minSafeInteger", num_minSafeInteger);
  PRIMITIVE(vm->numClass, "-(_)", num_minus);
  PRIMITIVE(vm->numClass, "+(_)", num_plus);
  PRIMITIVE(vm->numClass, "*(_)", num_multiply);
  PRIMITIVE(vm->numClass, "/(_)", num_divide);
  PRIMITIVE(vm->numClass, "<(_)", num_lt);
  PRIMITIVE(vm->numClass, ">(_)", num_gt);
  PRIMITIVE(vm->numClass, "<=(_)", num_lte);
  PRIMITIVE(vm->numClass, ">=(_)", num_gte);
  PRIMITIVE(vm->numClass, "&(_)", num_bitwiseAnd);
  PRIMITIVE(vm->numClass, "|(_)", num_bitwiseOr);
  PRIMITIVE(vm->numClass, "^(_)", num_bitwiseXor);
  PRIMITIVE(vm->numClass, "<<(_)", num_bitwiseLeftShift);
  PRIMITIVE(vm->numClass, ">>(_)", num_bitwiseRightShift);
  PRIMITIVE(vm->numClass, "abs", num_abs);
  PRIMITIVE(vm->numClass, "acos", num_acos);
  PRIMITIVE(vm->numClass, "asin", num_asin);
  PRIMITIVE(vm->numClass, "atan", num_atan);
  PRIMITIVE(vm->numClass, "cbrt", num_cbrt);
  PRIMITIVE(vm->numClass, "ceil", num_ceil);
  PRIMITIVE(vm->numClass, "cos", num_cos);
  PRIMITIVE(vm->numClass, "floor", num_floor);
  PRIMITIVE(vm->numClass, "-", num_negate);
  PRIMITIVE(vm->numClass, "round", num_round);
  PRIMITIVE(vm->numClass, "min(_)", num_min);
  PRIMITIVE(vm->numClass, "max(_)", num_max);
  PRIMITIVE(vm->numClass, "clamp(_,_)", num_clamp);
  PRIMITIVE(vm->numClass, "sin", num_sin);
  PRIMITIVE(vm->numClass, "sqrt", num_sqrt);
  PRIMITIVE(vm->numClass, "tan", num_tan);
  PRIMITIVE(vm->numClass, "log", num_log);
  PRIMITIVE(vm->numClass, "log2", num_log2);
  PRIMITIVE(vm->numClass, "exp", num_exp);
  PRIMITIVE(vm->numClass, "%(_)", num_mod);
  PRIMITIVE(vm->numClass, "~", num_bitwiseNot);
  PRIMITIVE(vm->numClass, "..(_)", num_dotDot);
  PRIMITIVE(vm->numClass, "...(_)", num_dotDotDot);
  PRIMITIVE(vm->numClass, "atan(_)", num_atan2);
  PRIMITIVE(vm->numClass, "pow(_)", num_pow);
  PRIMITIVE(vm->numClass, "fraction", num_fraction);
  PRIMITIVE(vm->numClass, "isInfinity", num_isInfinity);
  PRIMITIVE(vm->numClass, "isInteger", num_isInteger);
  PRIMITIVE(vm->numClass, "isNan", num_isNan);
  PRIMITIVE(vm->numClass, "sign", num_sign);
  PRIMITIVE(vm->numClass, "toString", num_toString);
  PRIMITIVE(vm->numClass, "truncate", num_truncate);

  // These are defined just so that 0 and -0 are equal, which is specified by
  // IEEE 754 even though they have different bit representations.
  PRIMITIVE(vm->numClass, "==(_)", num_eqeq);
  PRIMITIVE(vm->numClass, "!=(_)", num_bangeq);

  vm->stringClass = AS_CLASS(wrenFindVariable(vm, coreModule, "String"));
  PRIMITIVE(vm->stringClass->obj.classObj, "fromCodePoint(_)", string_fromCodePoint);
  PRIMITIVE(vm->stringClass->obj.classObj, "fromByte(_)", string_fromByte);
  PRIMITIVE(vm->stringClass, "+(_)", string_plus);
  PRIMITIVE(vm->stringClass, "[_]", string_subscript);
  PRIMITIVE(vm->stringClass, "byteAt_(_)", string_byteAt);
  PRIMITIVE(vm->stringClass, "byteCount_", string_byteCount);
  PRIMITIVE(vm->stringClass, "codePointAt_(_)", string_codePointAt);
  PRIMITIVE(vm->stringClass, "contains(_)", string_contains);
  PRIMITIVE(vm->stringClass, "endsWith(_)", string_endsWith);
  PRIMITIVE(vm->stringClass, "indexOf(_)", string_indexOf1);
  PRIMITIVE(vm->stringClass, "indexOf(_,_)", string_indexOf2);
  PRIMITIVE(vm->stringClass, "iterate(_)", string_iterate);
  PRIMITIVE(vm->stringClass, "iterateByte_(_)", string_iterateByte);
  PRIMITIVE(vm->stringClass, "iteratorValue(_)", string_iteratorValue);
  PRIMITIVE(vm->stringClass, "startsWith(_)", string_startsWith);
  PRIMITIVE(vm->stringClass, "toString", string_toString);
  PRIMITIVE(vm->stringClass, "safe", string_safe);
  PRIMITIVE(vm->stringClass, "split(_)", string_split);
  PRIMITIVE(vm->stringClass, "trim()", string_trim);
  PRIMITIVE(vm->stringClass, "trimStart()", string_trimStart);
  PRIMITIVE(vm->stringClass, "trimEnd()", string_trimEnd);

  vm->queryClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Query"));
  PRIMITIVE(vm->queryClass->obj.classObj, "new(_)", query_new);
  PRIMITIVE(vm->queryClass, "toString", query_toString);
  PRIMITIVE(vm->queryClass, "queryRaw_(_,_)", query_execute);
  PRIMITIVE(vm->queryClass, "fetchRaw_(_,_)", query_fetch);

  vm->listClass = AS_CLASS(wrenFindVariable(vm, coreModule, "List"));
  PRIMITIVE(vm->listClass->obj.classObj, "filled(_,_)", list_filled);
  PRIMITIVE(vm->listClass->obj.classObj, "new()", list_new);
  PRIMITIVE(vm->listClass, "[_]", list_subscript);
  PRIMITIVE(vm->listClass, "[_]=(_)", list_subscriptSetter);
  PRIMITIVE(vm->listClass, "add(_)", list_add);
  PRIMITIVE(vm->listClass, "addCore_(_)", list_addCore);
  PRIMITIVE(vm->listClass, "clear()", list_clear);
  PRIMITIVE(vm->listClass, "count", list_count);
  PRIMITIVE(vm->listClass, "insert(_,_)", list_insert);
  PRIMITIVE(vm->listClass, "iterate(_)", list_iterate);
  PRIMITIVE(vm->listClass, "iteratorValue(_)", list_iteratorValue);
  PRIMITIVE(vm->listClass, "removeAt(_)", list_removeAt);
  PRIMITIVE(vm->listClass, "remove(_)", list_removeValue);
  PRIMITIVE(vm->listClass, "indexOf(_)", list_indexOf);
  PRIMITIVE(vm->listClass, "swap(_,_)", list_swap);
  PRIMITIVE(vm->listClass, "joinNative(_)", list_join);

  vm->mapClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Map"));
  PRIMITIVE(vm->mapClass->obj.classObj, "new()", map_new);
  PRIMITIVE(vm->mapClass, "[_]", map_subscript);
  PRIMITIVE(vm->mapClass, "[_]=(_)", map_subscriptSetter);
  PRIMITIVE(vm->mapClass, "addCore_(_,_)", map_addCore);
  PRIMITIVE(vm->mapClass, "clear()", map_clear);
  PRIMITIVE(vm->mapClass, "containsKey(_)", map_containsKey);
  PRIMITIVE(vm->mapClass, "count", map_count);
  PRIMITIVE(vm->mapClass, "remove(_)", map_remove);
  PRIMITIVE(vm->mapClass, "iterate(_)", map_iterate);
  PRIMITIVE(vm->mapClass, "keyIteratorValue_(_)", map_keyIteratorValue);
  PRIMITIVE(vm->mapClass, "valueIteratorValue_(_)", map_valueIteratorValue);

  vm->rangeClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Range"));
  PRIMITIVE(vm->rangeClass, "from", range_from);
  PRIMITIVE(vm->rangeClass, "to", range_to);
  PRIMITIVE(vm->rangeClass, "min", range_min);
  PRIMITIVE(vm->rangeClass, "max", range_max);
  PRIMITIVE(vm->rangeClass, "isInclusive", range_isInclusive);
  PRIMITIVE(vm->rangeClass, "iterate(_)", range_iterate);
  PRIMITIVE(vm->rangeClass, "iteratorValue(_)", range_iteratorValue);
  PRIMITIVE(vm->rangeClass, "toString", range_toString);

  ObjClass* systemClass = AS_CLASS(wrenFindVariable(vm, coreModule, "System"));
  PRIMITIVE(systemClass->obj.classObj, "clock", system_clock);
  PRIMITIVE(systemClass->obj.classObj, "gc()", system_gc);
  PRIMITIVE(systemClass->obj.classObj, "writeString_(_)", system_writeString);

  ObjClass* dateClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Date"));
  PRIMITIVE(dateClass->obj.classObj, "current_(_)", date_current);
  PRIMITIVE(dateClass->obj.classObj, "fromUnix_(_,_)", date_fromUnix);
  PRIMITIVE(dateClass->obj.classObj, "unix_(_,_,_,_,_,_,_)", date_unix);
  PRIMITIVE(dateClass->obj.classObj, "format_(_,_,_,_,_,_,_,_)", date_format);

  // While bootstrapping the core types and running the core module, a number
  // of string objects have been created, many of which were instantiated
  // before stringClass was stored in the VM. Some of them *must* be created
  // first -- the ObjClass for string itself has a reference to the ObjString
  // for its name.
  //
  // These all currently have a NULL classObj pointer, so go back and assign
  // them now that the string class is known.
  for(Obj* obj = vm->first; obj != NULL; obj = obj->next) {
    if(obj->type == OBJ_STRING)
      obj->classObj = vm->stringClass;
  }

  // Bialet classes
  WrenInterpretResult bialetResult = wrenInterpret(vm, NULL, bialetModuleSource);
  if(bialetResult != WREN_RESULT_SUCCESS) {
    fprintf(stderr, "ERROR: Failed to load bialet module: %d\n", bialetResult);
  }

  ObjClass* utilClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Util"));
  PRIMITIVE(utilClass->obj.classObj, "hash_(_)", util_hash);
  PRIMITIVE(utilClass->obj.classObj, "verify_(_,_)", util_verify);
  PRIMITIVE(utilClass->obj.classObj, "secureEquals_(_,_)", util_secureEquals);
  PRIMITIVE(utilClass->obj.classObj, "randomString_(_)", util_randomString);
  PRIMITIVE(utilClass->obj.classObj, "urlDecode_(_)", util_urlDecode);
  PRIMITIVE(utilClass->obj.classObj, "monotonicMs_()", util_monotonicMs);
  PRIMITIVE(utilClass->obj.classObj, "sha256_(_)", util_sha256);
  PRIMITIVE(utilClass->obj.classObj, "sha256Base64Url_(_)", util_sha256Base64Url);
  PRIMITIVE(utilClass->obj.classObj, "base64UrlEncode_(_)", util_base64UrlEncode);
  PRIMITIVE(utilClass->obj.classObj, "encodeBase64_(_)", util_encodeBase64);

  ObjClass* responseClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Response"));
  PRIMITIVE(responseClass->obj.classObj, "defaultPage_(_,_)", response_default_page);

  ObjClass* httpClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Http"));
  PRIMITIVE(httpClass, "call_(_,_,_,_,_,_,_)", http_call);

  ObjClass* markdownClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Markdown"));
  PRIMITIVE(markdownClass->obj.classObj, "html_(_)", markdown_html);
  PRIMITIVE(markdownClass->obj.classObj, "file_(_)", markdown_file);

  ObjClass* jsonClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Json"));
  PRIMITIVE(jsonClass->obj.classObj, "parse_(_)", json_parse_primitive);

  // Conditionally load test classes
  if(vm->config.enableTests) {
    WrenInterpretResult testResult =
        wrenInterpret(vm, NULL, bialet_testModuleSource);
    if(testResult != WREN_RESULT_SUCCESS) {
      fprintf(stderr, "ERROR: Failed to load test module: %d\n", testResult);
    }

    ObjClass* testClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Test"));
    PRIMITIVE(testClass, "runTestRequest_(_,_)", test_runRequest);

    ObjClass* testsClass = AS_CLASS(wrenFindVariable(vm, coreModule, "Tests"));
    PRIMITIVE(testsClass->obj.classObj, "skip_()", tests_skip);
  }
}
