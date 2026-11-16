#ifndef BIALET_BYTECODE_CACHE_H
#define BIALET_BYTECODE_CACHE_H

#include "bialet.h"
#include "wren_vm.h"
#include <sys/stat.h>

// Process-global, in-memory cache of compiled Wren bytecode.
//
// Every WrenVM compiles the embedded framework module (bialet.wren, ~1900
// lines), and every request also compiles the served file and its local
// imports. This module keeps the compiled ObjFn graph of a file and
// re-materialises it into each new VM instead of recompiling.
//
// Caching policy:
//
//   wren_core.wren (bootstrap)              never (compiled before the core
//                                           classes exist; untouched here)
//   bialet.wren (embedded, core module)     always, with or without --cache
//   served .wren file (route/error page)    only with --cache / -x
//   locally imported .wren file             only with --cache / -x
//
// The framework entry is built from a string compiled into the binary, so it
// cannot go stale at runtime: it needs no source copy and no content check,
// only the format/build stamp in its header. User files are invalidated by a
// 64-bit xxHash64 of their bytes: a collision would mean serving bytecode
// compiled from different source, and 2^-64 per comparison is acceptable
// (32-bit CRC is not, for a 4-byte saving).
//
// One entry per path, replaced when the source changes; a failed compile is
// never cached. Blobs live only in memory. Every failure -- miss, mismatch,
// corrupt or incompatible blob -- falls back to the normal compile path.

// Key used for the embedded framework module, which has no file of its own.
#define BYTECODE_CACHE_FRAMEWORK_KEY "<bialet-framework>"

// Enables or disables caching of user files (served routes and local imports).
// The framework module is cached regardless. Must be called before any VM is
// created (main() does it right after parsing the command line).
void bytecode_cache_set_user_files(bool enabled);
bool bytecode_cache_user_files_enabled(void);

// Returns a malloc'd canonical key (realpath) for a module source file, or
// NULL when the file cannot be resolved. Caller frees. Only meaningful while
// user-file caching is enabled.
char* bytecode_cache_path_key(const char* path);

// Fills [out] with the identity of one version of a file. Returns false when
// the platform cannot be trusted for a stat fast hit (see struct
// BialetFileStat); the caller then takes the read+hash path.
bool bytecode_cache_stat_from_native(const struct stat*     st,
                                     struct BialetFileStat* out);

// Serves [cache_key] straight from the cache when the entry's file signature
// matches [stat]: no open, no read, no source hash. Returns NULL on a miss
// (including an entry with no recorded signature), and the caller falls back
// to reading the file. Only served route/error files use this; imported
// modules are read by the loader before the VM sees them.
ObjClosure* bytecode_cache_load_by_stat(WrenVM* vm, const char* module_name,
                                        const char*                  cache_key,
                                        const struct BialetFileStat* stat);

// Compiles and runs the embedded framework module, always through the cache.
WrenInterpretResult bytecode_cache_interpret_framework(WrenVM*     vm,
                                                       const char* source);

// Compiles [source] in the module named [module_name] and returns the module
// body closure, exactly like compileInModule() in wren_vm.c. Returns NULL on a
// compile error.
//
// When user-file caching is enabled and [cache_key] is non-NULL, a matching
// entry is deserialised instead of compiled; a miss (or any invalid/corrupt
// entry) falls back to a normal compile and stores the result.
ObjClosure* bytecode_cache_compile_user(WrenVM* vm, const char* module_name,
                                        const char* source, const char* cache_key,
                                        const struct BialetFileStat* stat);

// bytecode_cache_compile_user() plus the execution half of wrenInterpret().
WrenInterpretResult bytecode_cache_interpret_user(WrenVM*     vm,
                                                  const char* module_name,
                                                  const char* source,
                                                  const char* cache_key);

#endif
