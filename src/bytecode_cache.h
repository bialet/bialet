#ifndef BIALET_BYTECODE_CACHE_H
#define BIALET_BYTECODE_CACHE_H

#include "wren_vm.h"
#include <stdbool.h>

// Process-global, in-memory cache of compiled Wren bytecode.
//
// Every WrenVM compiles the embedded framework module (bialet.wren, ~1900
// lines) and every user module it loads from scratch: lexing, parsing and
// emitting bytecode for the same files on every request. When enabled with
// --cache, this module keeps the resulting ObjFn graph for each file, keyed by
// the file's realpath, and re-materialises it into each new VM instead of
// recompiling.
//
// It is OFF by default. When disabled, the helpers below are thin wrappers
// around the normal compile-and-run path, so behaviour is byte-for-byte the
// same as a build without the cache.
//
// v1 is deliberately narrow:
//   * blobs live in memory only (no disk persistence),
//   * the cache is invalidated by comparing the source bytes stored with each
//     entry (see cache_entry_matches()), not by stat()/mtime,
//   * at most one entry per path, replaced when the source changes,
//   * failed compiles are never cached.

// Key used for the embedded framework module, which has no file of its own.
#define BYTECODE_CACHE_FRAMEWORK_KEY "<bialet-framework>"

// Enables or disables the cache. Must be called before any VM is created
// (main() does it right after parsing the command line).
void bytecode_cache_enable(bool enabled);
bool bytecode_cache_is_enabled(void);

// Returns a malloc'd canonical key (realpath) for a module source file, or
// NULL when the file cannot be resolved. Caller frees. Only meaningful while
// the cache is enabled.
char* bytecode_cache_path_key(const char* path);

// Compiles [source] in the module named [module_name] (NULL means the core
// module, which is where bialet.wren is compiled) and returns the module body
// closure, exactly like compileInModule() in wren_vm.c. Returns NULL on a
// compile error.
//
// When the cache is enabled and [cache_key] is non-NULL, a matching entry is
// deserialised instead of compiled; a miss (or any invalid/corrupt entry)
// falls back to a normal compile and stores the result.
ObjClosure* bytecode_cache_compile(WrenVM* vm, const char* module_name,
                                   const char* source, const char* cache_key);

// bytecode_cache_compile() plus the execution half of wrenInterpret().
WrenInterpretResult bytecode_cache_interpret(WrenVM* vm, const char* module_name,
                                             const char* source,
                                             const char* cache_key);

#endif
