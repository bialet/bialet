#ifndef wren_core_h
#define wren_core_h

#include "wren_vm.h"

// This module defines the built-in classes and their primitives methods that
// are implemented directly in C code. Some languages try to implement as much
// of the core module itself in the primary language instead of in the host
// language.
//
// With Wren, we try to do as much of it in C as possible. Primitive methods
// are always faster than code written in Wren, and it minimizes startup time
// since we don't have to parse, compile, and execute Wren code.
//
// There is one limitation, though. Methods written in C cannot call Wren ones.
// They can only be the top of the callstack, and immediately return. This
// makes it difficult to have primitive methods that rely on polymorphic
// behavior. For example, `System.print` should call `toString` on its argument,
// including user-defined `toString` methods on user-defined classes.

void wrenInitializeCore(WrenVM* vm);

// The application-wide default zone for Date values, taken from the
// BIALET_TIMEZONE row in BIALET_CONFIG and installed once at startup. The Date
// primitives use it when the Wren-level zone is absent or empty, so every VM
// (migrations, cron, one per request) sees the same zone without consulting the
// database. A zone the allow-list refuses is ignored with a log line.
// date_get_default_zone() returns NULL when no zone is configured, meaning "use
// the process zone".
void        date_set_default_zone(const char* zone);
const char* date_get_default_zone(void);

#endif
