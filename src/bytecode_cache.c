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
#include "bytecode_cache.h"

#include "bialet.h"
#include "messages.h"
#include "utils.h"
#include "wren_compiler.h"
#include "wren_value.h"
#include "wren_vm.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// Bumped when the blob layout (not the Wren bytecode) changes. Blobs never
// leave the process in v1, so this is only a self-check; a disk cache would
// need it plus an ABI stamp for the binary itself.
#define CACHE_BLOB_VERSION 1u

// Sanity ceilings for values read back from a blob. They mirror the compiler's
// own limits (MAX_CONSTANTS in wren_compiler.c, MAX_MODULE_VARS and
// MAX_PARAMETERS in wren_common.h) with headroom, so a damaged blob cannot ask
// for an enormous allocation or an unbounded recursion.
#define CACHE_MAX_SYMBOLS (1 << 20)
#define CACHE_MAX_CONSTANTS (1 << 16)
#define CACHE_MAX_SLOTS (1 << 16)
#define CACHE_MAX_UPVALUES 256
#define CACHE_MAX_ARITY 256
#define CACHE_MAX_DEBUG_NAME 4096
#define CACHE_MAX_FN_DEPTH 512

// The debug-line array is written as one little-endian u32 per byte of code, and
// it is the largest section of a blob. On a little-endian target with 32-bit int
// that is the same byte layout an int array has, so a replay copies it with one
// memcpy instead of a bounds-checked read per line. Every other target keeps the
// per-line loop.
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&                  \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && UINT_MAX == 0xffffffffU
#define CACHE_LINES_ARE_LE32 1
#else
#define CACHE_LINES_ARE_LE32 0
#endif

// Constant tags for one slot in a function's constant pool. Wren puts these
// kinds there: literals (number/string), Bialet's backtick SQL literals (an
// ObjQuery), the singletons an attribute value may hold, and nested functions.
// Anything else means the graph was already mutated at runtime (for example
// the superclass back-patching wrenBindMethodCode() does while a class body
// runs), and such a function must not be cached.
typedef enum {
  CACHE_CONST_NUM = 1,
  CACHE_CONST_NULL,
  CACHE_CONST_TRUE,
  CACHE_CONST_FALSE,
  CACHE_CONST_STRING,
  CACHE_CONST_QUERY,
  CACHE_CONST_FN
} CacheConstTag;

// Module variable value kinds at compile time. wrenCompile() only ever leaves
// NULL_VAL (an explicit declaration) or NUM_VAL(line) (an implicit forward
// reference that was never defined) in a module slot.
typedef enum { CACHE_VAR_NULL = 0, CACHE_VAR_LINE = 1 } CacheVarKind;

/* ------------------------------------------------------------------------- */
/* Process-global state                                                      */
/* ------------------------------------------------------------------------- */

// One entry per source file. A user entry is invalidated by comparing the
// length and xxHash64 of the source with what it was built from; an embedded
// entry (wren_core.wren or bialet.wren) is built from a string compiled into
// this binary, so it carries no source metadata and is never content-checked.
typedef struct {
  char*    key; // realpath (or the embedded module's synthetic key)
  uint8_t* blob;
  size_t   blob_len;
  bool     embedded;
  size_t   source_len;
  uint64_t source_hash;
  // File signature captured when the blob was stored (served files only).
  // A matching signature serves the blob without reading the file.
  bool                  has_stat;
  struct BialetFileStat stat;
} CacheEntry;

static bool        cache_user_files = false;
static CacheEntry* cache_entries = NULL;
static size_t      cache_entry_count = 0;
static size_t      cache_entry_capacity = 0;
static bool        cache_warned = false;

// Test hooks. Blobs never leave the process, so there is nothing on disk to
// damage and no way to see a replay from outside. Two environment switches cover
// that, both read once from bytecode_cache_set_user_files() before any VM or
// background thread exists:
//
//   BIALET_CACHE_TRACE=1  one log line per embedded module saying whether it was
//                         replayed from its blob or compiled from source. It is
//                         how tests/run.sh asserts that the second VM in the
//                         process really replays both embedded entries instead of
//                         silently recompiling them.
//   BIALET_CACHE_FAULT=1  store one truncated copy of the framework blob, so the
//                         next VM has to reject it, warn once and compile from
//                         source; the following store replaces the damaged entry.
//
// Off unless set, and neither touches the hit path.
static bool cache_trace = false;
static bool cache_fault = false;
static bool cache_fault_done = false;

// The cache is reached from the cron and file-watch threads of the parent
// process as well as from the HTTP child, so every access is serialised. On
// Windows this build runs no background Wren threads, but the same functions
// keep the code uniform.
//
// fork(): main.c's pthread_atfork prepare handler takes run_mutex, which
// serialises every background bialet_run() -- and every cache access happens
// inside one of those runs. The child is therefore forked with this lock
// released. Do not take it outside a bialet_run() path without revisiting that
// reasoning.
#ifdef _WIN32
static SRWLOCK cache_lock = SRWLOCK_INIT;
static void    cache_lock_acquire(void) {
  AcquireSRWLockExclusive(&cache_lock);
}
static void cache_lock_release(void) {
  ReleaseSRWLockExclusive(&cache_lock);
}
#else
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
static void            cache_lock_acquire(void) {
  pthread_mutex_lock(&cache_lock);
}
static void cache_lock_release(void) {
  pthread_mutex_unlock(&cache_lock);
}
#endif

// Reads one of the test hooks above. Only called while [cache_user_files] is
// being set, before the process has started serving, so there is no race.
static bool cache_env_flag(const char* name) {
  const char* value = getenv(name);
  return value != NULL && *value != '\0' && strcmp(value, "0") != 0;
}

void bytecode_cache_set_user_files(bool enabled) {
  cache_user_files = enabled;
  cache_trace = cache_env_flag("BIALET_CACHE_TRACE");
  cache_fault = cache_env_flag("BIALET_CACHE_FAULT");
}

bool bytecode_cache_user_files_enabled(void) {
  return cache_user_files;
}

char* bytecode_cache_path_key(const char* path, bool already_canonical) {
  if(!cache_user_files || path == NULL)
    return NULL;

  // The hot caller is a served route file, whose path is built as
  // full_root_dir + the sanitized URI, and full_root_dir was realpath()'d once
  // at startup. That path is already canonical, so realpath_n() here only
  // repeated the same walk -- one readlink() per path component, per .wren
  // request -- and the caller says so with [already_canonical].
  //
  // Everything else (a relative path, -r code) still goes through realpath_n(),
  // which is what makes the key canonical when the caller cannot promise that.
  // Two spellings of one file ("//a.wren" vs "/a.wren") get two cache entries
  // instead of one; the key is only a name, and each entry carries its own file
  // signature, so the same edit invalidates both.
  if(already_canonical)
    return string_safe_copy(path);

  char resolved[PATH_MAX];
  if(realpath_n(path, resolved, sizeof(resolved)) == NULL)
    return NULL;

  return string_safe_copy(resolved);
}

// Logs [reason] once per process: a corrupt blob is a bug, and repeating it on
// every request would bury the server log.
static void cache_warn(const char* reason) {
  cache_lock_acquire();
  bool already = cache_warned;
  cache_warned = true;
  cache_lock_release();

  if(!already)
    message(yellow("Bytecode cache"), reason);
}

/* ------------------------------------------------------------------------- */
/* Blob writer                                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
  uint8_t* data;
  size_t   len;
  size_t   capacity;
  bool     failed;
} CacheWriter;

static bool writer_reserve(CacheWriter* w, size_t extra) {
  if(w->failed)
    return false;
  if(extra > SIZE_MAX - w->len) {
    w->failed = true;
    return false;
  }
  if(w->len + extra <= w->capacity)
    return true;

  size_t capacity = w->capacity == 0 ? 4096 : w->capacity;
  while(capacity < w->len + extra) {
    if(capacity > SIZE_MAX / 2) {
      w->failed = true;
      return false;
    }
    capacity *= 2;
  }

  uint8_t* data = (uint8_t*)realloc(w->data, capacity);
  if(data == NULL) {
    w->failed = true;
    return false;
  }

  w->data = data;
  w->capacity = capacity;
  return true;
}

static void writer_bytes(CacheWriter* w, const void* data, size_t len) {
  if(len == 0 || w->failed)
    return;
  if(!writer_reserve(w, len))
    return;
  memcpy(w->data + w->len, data, len);
  w->len += len;
}

static void writer_u8(CacheWriter* w, uint8_t value) {
  writer_bytes(w, &value, 1);
}

static void writer_u32(CacheWriter* w, uint32_t value) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)(value & 0xff);
  bytes[1] = (uint8_t)((value >> 8) & 0xff);
  bytes[2] = (uint8_t)((value >> 16) & 0xff);
  bytes[3] = (uint8_t)((value >> 24) & 0xff);
  writer_bytes(w, bytes, sizeof(bytes));
}

static void writer_double(CacheWriter* w, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  uint8_t bytes[8];
  for(int i = 0; i < 8; i++)
    bytes[i] = (uint8_t)((bits >> (i * 8)) & 0xff);
  writer_bytes(w, bytes, sizeof(bytes));
}

/* ------------------------------------------------------------------------- */
/* Blob reader                                                               */
/* ------------------------------------------------------------------------- */

// A cursor that never reads past [len]. Every getter checks the remaining
// bytes before touching them and remembers the first failure in [failed], so a
// truncated blob is rejected instead of read out of bounds.
typedef struct {
  const uint8_t* data;
  size_t         len;
  size_t         pos;
  bool           failed;
} CacheReader;

// The getters are inline on purpose: the replay reads one field per byte of
// bytecode (the parallel debug-line array), and a function call per field was
// the third largest self-time symbol in the profile. Inlining moves the bounds
// check to the call site, where the compiler keeps [pos] and the remaining
// length in registers. The check itself is unchanged: a read of [count] bytes
// fails without touching memory unless all [count] bytes are still there.
static inline const uint8_t* reader_bytes(CacheReader* r, size_t count) {
  if(r->failed)
    return NULL;
  if(count > r->len - r->pos) {
    r->failed = true;
    return NULL;
  }
  const uint8_t* at = r->data + r->pos;
  r->pos += count;
  return at;
}

static inline bool reader_u8(CacheReader* r, uint8_t* out) {
  const uint8_t* at = reader_bytes(r, 1);
  if(at == NULL)
    return false;
  *out = *at;
  return true;
}

// Explicit little-endian assembly, the same byte order the writer emits. A
// compiler that knows the target is little-endian folds it into one load.
static inline uint32_t reader_le32(const uint8_t* at) {
  return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) |
         ((uint32_t)at[3] << 24);
}

static inline bool reader_u32(CacheReader* r, uint32_t* out) {
  const uint8_t* at = reader_bytes(r, 4);
  if(at == NULL)
    return false;
  *out = reader_le32(at);
  return true;
}

static inline bool reader_double(CacheReader* r, double* out) {
  const uint8_t* at = reader_bytes(r, 8);
  if(at == NULL)
    return false;
  uint64_t bits = (uint64_t)reader_le32(at) | ((uint64_t)reader_le32(at + 4) << 32);
  memcpy(out, &bits, sizeof(*out));
  return true;
}

// Reads a length-prefixed byte run. The length is bounded by what is left in
// the blob, so strings and names need no separate ceiling.
static inline bool reader_sized(CacheReader* r, const uint8_t** data, size_t* len) {
  uint32_t size;
  if(!reader_u32(r, &size))
    return false;
  const uint8_t* at = reader_bytes(r, (size_t)size);
  if(at == NULL)
    return false;
  *data = at;
  *len = (size_t)size;
  return true;
}

/* ------------------------------------------------------------------------- */
/* Serialisation                                                             */
/* ------------------------------------------------------------------------- */

static bool write_fn(CacheWriter* w, const ObjFn* fn, int depth);

// Writes the module-variable and method-symbol deltas the bytecode depends on.
// Both tables are append-only: index i in one VM names the same string at index
// i in the next only if every name before it is identical. The loader replays
// these names in order and checks that each lands at the recorded index; if
// anything shifted the entry is rejected and the source is compiled instead.
static bool write_module_state(CacheWriter* w, WrenVM* vm, ObjModule* module,
                               int symbol_base, int var_base, bool is_core) {
  if(symbol_base < 0 || symbol_base > vm->methodNames.count)
    return false;
  if(var_base < 0 || var_base > module->variables.count)
    return false;

  int symbol_count = vm->methodNames.count - symbol_base;
  int var_count = module->variables.count - var_base;

  writer_u8(w, is_core ? 1 : 0);
  writer_u32(w, (uint32_t)symbol_base);
  writer_u32(w, (uint32_t)symbol_count);
  for(int i = 0; i < symbol_count; i++) {
    ObjString* name = vm->methodNames.data[symbol_base + i];
    writer_u32(w, name->length);
    writer_bytes(w, name->value, name->length);
  }

  writer_u32(w, (uint32_t)var_base);
  writer_u32(w, (uint32_t)var_count);
  for(int i = 0; i < var_count; i++) {
    ObjString* name = module->variableNames.data[var_base + i];
    Value      value = module->variables.data[var_base + i];

    writer_u32(w, name->length);
    writer_bytes(w, name->value, name->length);

    if(IS_NULL(value)) {
      writer_u8(w, CACHE_VAR_NULL);
    } else if(IS_NUM(value)) {
      double line = AS_NUM(value);
      int    as_int = (int)line;
      if((double)as_int != line || as_int < 0)
        return false;
      writer_u8(w, CACHE_VAR_LINE);
      writer_u32(w, (uint32_t)as_int);
    } else {
      // A real value means the module had already executed: never cache it.
      return false;
    }
  }

  return !w->failed;
}

static bool write_constant(CacheWriter* w, Value value, int depth) {
  if(IS_NUM(value)) {
    writer_u8(w, CACHE_CONST_NUM);
    writer_double(w, AS_NUM(value));
    return true;
  }
  if(IS_NULL(value)) {
    writer_u8(w, CACHE_CONST_NULL);
    return true;
  }
  if(value == TRUE_VAL) {
    writer_u8(w, CACHE_CONST_TRUE);
    return true;
  }
  if(value == FALSE_VAL) {
    writer_u8(w, CACHE_CONST_FALSE);
    return true;
  }
  if(IS_STRING(value)) {
    ObjString* string = AS_STRING(value);
    writer_u8(w, CACHE_CONST_STRING);
    writer_u32(w, string->length);
    writer_bytes(w, string->value, string->length);
    return true;
  }
  // Backtick SQL literals compile to ObjQuery: same layout as an ObjString,
  // distinct ObjType and class.
  if(IS_OBJ(value) && AS_OBJ(value)->type == OBJ_QUERY) {
    ObjString* query = AS_STRING(value);
    writer_u8(w, CACHE_CONST_QUERY);
    writer_u32(w, query->length);
    writer_bytes(w, query->value, query->length);
    return true;
  }
  if(IS_FN(value)) {
    writer_u8(w, CACHE_CONST_FN);
    return write_fn(w, AS_FN(value), depth + 1);
  }
  return false;
}

static bool write_fn(CacheWriter* w, const ObjFn* fn, int depth) {
  if(depth > CACHE_MAX_FN_DEPTH)
    return false;

  // Every emitted byte also appends a debug line, so the two counts must agree
  // or the blob would not describe this function.
  if(fn->debug == NULL || fn->code.count <= 0 ||
     fn->code.count != fn->debug->sourceLines.count)
    return false;
  if(fn->maxSlots <= 0 || fn->maxSlots > CACHE_MAX_SLOTS)
    return false;
  if(fn->numUpvalues < 0 || fn->numUpvalues > CACHE_MAX_UPVALUES)
    return false;
  if(fn->arity < 0 || fn->arity > CACHE_MAX_ARITY)
    return false;
  if(fn->constants.count < 0 || fn->constants.count > CACHE_MAX_CONSTANTS)
    return false;

  writer_u32(w, (uint32_t)fn->code.count);
  writer_bytes(w, fn->code.data, (size_t)fn->code.count);
  writer_u32(w, (uint32_t)fn->maxSlots);
  writer_u32(w, (uint32_t)fn->numUpvalues);
  writer_u32(w, (uint32_t)fn->arity);

  if(fn->debug->name != NULL) {
    size_t name_len = strlen(fn->debug->name);
    if(name_len > CACHE_MAX_DEBUG_NAME)
      return false;
    writer_u8(w, 1);
    writer_u32(w, (uint32_t)name_len);
    writer_bytes(w, fn->debug->name, name_len);
  } else {
    writer_u8(w, 0);
  }

  writer_u32(w, (uint32_t)fn->debug->sourceLines.count);
  for(int i = 0; i < fn->debug->sourceLines.count; i++)
    writer_u32(w, (uint32_t)fn->debug->sourceLines.data[i]);

  writer_u32(w, (uint32_t)fn->constants.count);
  for(int i = 0; i < fn->constants.count; i++) {
    if(!write_constant(w, fn->constants.data[i], depth))
      return false;
  }

  return !w->failed;
}

/* ------------------------------------------------------------------------- */
/* Source digest (xxHash64)                                                 */
/* ------------------------------------------------------------------------- */

// xxHash64, seed 0, one-shot. Self-contained on purpose: explicit
// little-endian loads, no intrinsics, no alignment assumptions, no dependency.
// It replaces a stored copy of every source file; at 64 bits a collision would
// be needed to serve bytecode for different source, which is not a practical
// risk (see cache_entry_matches()).
#define CACHE_HASH_PRIME64_1 11400714785074694791ULL
#define CACHE_HASH_PRIME64_2 14029467366897019727ULL
#define CACHE_HASH_PRIME64_3 1609587929392839161ULL
#define CACHE_HASH_PRIME64_4 9650029242287828579ULL
#define CACHE_HASH_PRIME64_5 2870177450012600261ULL

static uint64_t hash_rotate_left(uint64_t value, int bits) {
  return (value << bits) | (value >> (64 - bits));
}

static uint64_t hash_read64(const uint8_t* p) {
  return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
         ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
         ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static uint32_t hash_read32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint64_t hash_round(uint64_t acc, uint64_t input) {
  acc += input * CACHE_HASH_PRIME64_2;
  acc = hash_rotate_left(acc, 31);
  acc *= CACHE_HASH_PRIME64_1;
  return acc;
}

static uint64_t hash_merge_round(uint64_t acc, uint64_t value) {
  acc ^= hash_round(0, value);
  acc = acc * CACHE_HASH_PRIME64_1 + CACHE_HASH_PRIME64_4;
  return acc;
}

static uint64_t bytecode_cache_hash(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  const uint8_t* end = p + len;
  uint64_t       hash;

  if(len >= 32) {
    uint64_t v1 = CACHE_HASH_PRIME64_1 + CACHE_HASH_PRIME64_2;
    uint64_t v2 = CACHE_HASH_PRIME64_2;
    uint64_t v3 = 0;
    uint64_t v4 = (uint64_t)0 - CACHE_HASH_PRIME64_1;

    while(p + 32 <= end) {
      v1 = hash_round(v1, hash_read64(p));
      p += 8;
      v2 = hash_round(v2, hash_read64(p));
      p += 8;
      v3 = hash_round(v3, hash_read64(p));
      p += 8;
      v4 = hash_round(v4, hash_read64(p));
      p += 8;
    }

    hash = hash_rotate_left(v1, 1) + hash_rotate_left(v2, 7) +
           hash_rotate_left(v3, 12) + hash_rotate_left(v4, 18);
    hash = hash_merge_round(hash, v1);
    hash = hash_merge_round(hash, v2);
    hash = hash_merge_round(hash, v3);
    hash = hash_merge_round(hash, v4);
  } else {
    hash = CACHE_HASH_PRIME64_5;
  }

  hash += (uint64_t)len;

  while(p + 8 <= end) {
    hash ^= hash_round(0, hash_read64(p));
    hash = hash_rotate_left(hash, 27) * CACHE_HASH_PRIME64_1 + CACHE_HASH_PRIME64_4;
    p += 8;
  }
  if(p + 4 <= end) {
    hash ^= (uint64_t)hash_read32(p) * CACHE_HASH_PRIME64_1;
    hash = hash_rotate_left(hash, 23) * CACHE_HASH_PRIME64_2 + CACHE_HASH_PRIME64_3;
    p += 4;
  }
  while(p < end) {
    hash ^= (uint64_t)(*p) * CACHE_HASH_PRIME64_5;
    hash = hash_rotate_left(hash, 11) * CACHE_HASH_PRIME64_1;
    p++;
  }

  hash ^= hash >> 33;
  hash *= CACHE_HASH_PRIME64_2;
  hash ^= hash >> 29;
  hash *= CACHE_HASH_PRIME64_3;
  hash ^= hash >> 32;
  return hash;
}

/* ------------------------------------------------------------------------- */
/* File signature (stat-first hit path)                                     */
/* ------------------------------------------------------------------------- */

bool bytecode_cache_stat_from_native(const struct stat*     st,
                                     struct BialetFileStat* out) {
  if(st == NULL || out == NULL)
    return false;

  memset(out, 0, sizeof(*out));
#if defined(_WIN32)
  // Mingw exposes seconds only, and st_ino is frequently 0 -- which is the
  // signal that this stamp must not be trusted for a fast hit.
  out->mtime_ns = (long long)st->st_mtime * 1000000000LL;
  out->size = (unsigned long long)st->st_size;
  out->inode = 0;
#elif defined(__APPLE__)
  out->mtime_ns = (long long)st->st_mtimespec.tv_sec * 1000000000LL +
                  (long long)st->st_mtimespec.tv_nsec;
  out->size = (unsigned long long)st->st_size;
  out->inode = (unsigned long long)st->st_ino;
#else
  out->mtime_ns =
      (long long)st->st_mtim.tv_sec * 1000000000LL + (long long)st->st_mtim.tv_nsec;
  out->size = (unsigned long long)st->st_size;
  out->inode = (unsigned long long)st->st_ino;
#endif
  out->valid = out->inode != 0;
  return out->valid != 0;
}

// Whether [stat] names the version the entry was built from. The signature is
// mtime (nanoseconds where available), size and inode, which catches edits,
// truncation and replacement. It cannot catch a preserved mtime (`cp -p`,
// `rsync --inplace`) or an in-place rewrite within the same timestamp tick of
// the same length; a dmon-driven dirty mark or periodic content revalidation
// is the intended backstop for those, not implemented here.
static bool cache_stat_matches(const CacheEntry*            entry,
                               const struct BialetFileStat* stat) {
  if(entry->embedded || !entry->has_stat || stat == NULL || !stat->valid)
    return false;
  return entry->stat.mtime_ns == stat->mtime_ns && entry->stat.size == stat->size &&
         entry->stat.inode == stat->inode;
}

/* ------------------------------------------------------------------------- */
/* Invalidation                                                             */
/* ------------------------------------------------------------------------- */

// The single invalidation point for user files: length plus xxHash64 of the
// bytes. 64 bits is deliberate. A collision means serving bytecode compiled
// from different source, and at 2^-64 per comparison that is not a practical
// risk; a 32-bit CRC would save four bytes and make collisions worth worrying
// about. A stat() fast path can sit in front of this check without changing it.
static bool cache_entry_matches(const CacheEntry* entry, const char* source,
                                size_t source_len) {
  if(entry->embedded)
    return true; // The source is part of this binary.
  return entry->source_len == source_len &&
         entry->source_hash == bytecode_cache_hash(source, source_len);
}

/* ------------------------------------------------------------------------- */
/* Blob decoding                                                            */
/* ------------------------------------------------------------------------- */

// What one replay attempt ended in. A MISMATCH is a normal miss (the blob was
// built for a different VM or module state) and costs a compile; a CORRUPT blob
// is structurally impossible and is reported once.
typedef enum {
  CACHE_REPLAY_OK,
  CACHE_REPLAY_MISMATCH,
  CACHE_REPLAY_CORRUPT,
} CacheReplay;

// Validates the module-state deltas against the VM and module the source is
// about to be loaded into. On success [r] is positioned at the function tree and
// [var_section], when given, holds where the module-variable section starts, so
// it can be replayed after the function graph exists. Does not touch [module] or
// the VM.
static bool validate_module_state(CacheReader* r, WrenVM* vm, ObjModule* module,
                                  bool is_core, size_t* var_section) {
  uint8_t core_flag;
  if(!reader_u8(r, &core_flag))
    return false;
  if((core_flag != 0) != is_core)
    return false;

  uint32_t symbol_base, symbol_count;
  if(!reader_u32(r, &symbol_base) || !reader_u32(r, &symbol_count))
    return false;
  // The VM must be in the state the module was compiled in: the same number of
  // method symbols before this module's own additions. Otherwise the recorded
  // symbol indices would name different methods.
  if(symbol_base != (uint32_t)vm->methodNames.count)
    return false;
  if(symbol_count > CACHE_MAX_SYMBOLS)
    return false;

  for(uint32_t i = 0; i < symbol_count; i++) {
    const uint8_t* name;
    size_t         len;
    if(!reader_sized(r, &name, &len))
      return false;
    if(len > INT_MAX)
      return false;
    // Replaying must append at a fresh index. A name that already exists
    // earlier would be reused and shift every following symbol.
    if(wrenSymbolTableFind(&vm->methodNames, (const char*)name, len) != -1)
      return false;
  }

  if(var_section != NULL)
    *var_section = r->pos;

  uint32_t var_base, var_count;
  if(!reader_u32(r, &var_base) || !reader_u32(r, &var_count))
    return false;
  if(var_base != (uint32_t)module->variables.count)
    return false;
  if(var_count > (uint32_t)MAX_MODULE_VARS ||
     (size_t)var_base + (size_t)var_count > (size_t)MAX_MODULE_VARS)
    return false;

  for(uint32_t i = 0; i < var_count; i++) {
    const uint8_t* name;
    size_t         len;
    if(!reader_sized(r, &name, &len))
      return false;
    if(len > INT_MAX)
      return false;
    if(wrenSymbolTableFind(&module->variableNames, (const char*)name, len) != -1)
      return false;

    uint8_t kind;
    if(!reader_u8(r, &kind))
      return false;
    if(kind == CACHE_VAR_LINE) {
      uint32_t line;
      if(!reader_u32(r, &line))
        return false;
    } else if(kind != CACHE_VAR_NULL) {
      return false;
    }
  }

  return true;
}

// Allocates room for exactly [count] elements and returns it, or NULL when
// [count] is zero. Fill() rounds the capacity up to a power of two, zero-fills
// it, and the replay then overwrote every byte: the debug-line array alone is
// ~91 KB for the embedded framework, written twice per VM. wrenReallocate may
// collect, so the owner of the buffer must already be reachable from a root.
static void* cache_alloc_exact(WrenVM* vm, size_t count, size_t size) {
  if(count == 0)
    return NULL;
  if(count > SIZE_MAX / size) {
    // Cannot happen: every count is bounded by the blob length. Kept so the
    // multiplication is provably safe on 32-bit targets.
    return NULL;
  }
  return wrenReallocate(vm, NULL, 0, count * size);
}

/* ------------------------------------------------------------------------- */
/* Blob deserialisation                                                     */
/* ------------------------------------------------------------------------- */

typedef struct {
  WrenVM*    vm;
  ObjModule* module;
} CacheBuilder;

// Every failure inside build_fn() goes through here, so [r->failed] is always
// set and the callers can test either the return value or the reader.
static ObjFn* build_fn_failed(CacheReader* r) {
  r->failed = true;
  return NULL;
}

// Reads one function node and materialises it.
//
// This is also the validator: every structural check the separate read-only pass
// made happens here, in the same walk that builds, so a replay decodes the blob
// once instead of twice. A rejected node leaves the module untouched -- nothing
// built here is linked from the module, and the only root (the top-level
// function) is dropped by the caller, so the partial graph is reclaimed by the
// next collection.
//
// [slot] is where the new ObjFn is published as soon as it exists: either a cell
// in a rooted parent's constants array, or the caller's temporary root holder for
// the top-level function. That is what keeps the new object reachable across the
// allocations that follow -- there is never more than one temporary root, so a
// deeply nested source cannot exhaust WREN_MAX_TEMP_ROOTS.
static ObjFn* build_fn(CacheReader* r, CacheBuilder* builder, Value* slot,
                       int depth) {
  if(depth > CACHE_MAX_FN_DEPTH)
    return build_fn_failed(r);

  uint32_t code_len;
  if(!reader_u32(r, &code_len) || code_len == 0 || code_len > INT_MAX ||
     code_len > r->len - r->pos)
    return build_fn_failed(r);
  const uint8_t* code = reader_bytes(r, (size_t)code_len);
  if(code == NULL)
    return build_fn_failed(r);

  uint32_t max_slots, num_upvalues, arity;
  if(!reader_u32(r, &max_slots) || !reader_u32(r, &num_upvalues) ||
     !reader_u32(r, &arity))
    return build_fn_failed(r);
  if(max_slots == 0 || max_slots > CACHE_MAX_SLOTS)
    return build_fn_failed(r);
  if(num_upvalues > CACHE_MAX_UPVALUES || arity > CACHE_MAX_ARITY)
    return build_fn_failed(r);

  uint8_t        has_name;
  const uint8_t* name = NULL;
  size_t         name_len = 0;
  if(!reader_u8(r, &has_name) || (has_name && !reader_sized(r, &name, &name_len)))
    return build_fn_failed(r);
  if(name_len > CACHE_MAX_DEBUG_NAME)
    return build_fn_failed(r);

  uint32_t line_count;
  if(!reader_u32(r, &line_count) || line_count != code_len)
    return build_fn_failed(r);
  // One u32 per byte of code, so this is the largest section of the blob. Take
  // it as one bounds-checked byte run and copy it in bulk where the layout
  // matches (CACHE_LINES_ARE_LE32).
  if(line_count > (r->len - r->pos) / sizeof(uint32_t))
    return build_fn_failed(r);
  const uint8_t* lines = reader_bytes(r, (size_t)line_count * sizeof(uint32_t));
  if(lines == NULL)
    return build_fn_failed(r);

  uint32_t constants_count;
  if(!reader_u32(r, &constants_count) || constants_count > CACHE_MAX_CONSTANTS)
    return build_fn_failed(r);

  ObjFn* fn = wrenNewFunction(builder->vm, builder->module, (int)max_slots);
  *slot = OBJ_VAL(fn);

  // Only the outermost function needs an explicit root; every nested function is
  // already reachable through its parent's constants array.
  bool rooted = depth == 0;
  if(rooted)
    wrenPushRoot(builder->vm, (Obj*)fn);

  fn->numUpvalues = (int)num_upvalues;
  fn->arity = (int)arity;

  if(has_name)
    wrenFunctionBindName(builder->vm, fn, (const char*)name, (int)name_len);

  // Bytecode: allocated at its final size and copied once. The function is
  // reachable through [slot] (and, at depth 0, the root), so the allocation can
  // collect. Every failure past this point drops the root it just took, which is
  // why these three returns pop explicitly instead of leaving through the tail.
  fn->code.data = (uint8_t*)cache_alloc_exact(builder->vm, code_len, 1);
  if(fn->code.data == NULL) {
    if(rooted)
      wrenPopRoot(builder->vm);
    return build_fn_failed(r);
  }
  memcpy(fn->code.data, code, (size_t)code_len);
  fn->code.count = fn->code.capacity = (int)code_len;

  // The parallel debug-line array, same deal.
  fn->debug->sourceLines.data =
      (int*)cache_alloc_exact(builder->vm, line_count, sizeof(int));
  if(fn->debug->sourceLines.data == NULL) {
    if(rooted)
      wrenPopRoot(builder->vm);
    return build_fn_failed(r);
  }
  fn->debug->sourceLines.count = fn->debug->sourceLines.capacity = (int)line_count;
#if CACHE_LINES_ARE_LE32
  // The writer stored line i as (uint32_t)sourceLines.data[i] in little-endian,
  // which is exactly the byte layout of an int array here.
  memcpy(fn->debug->sourceLines.data, lines, (size_t)line_count * sizeof(int));
#else
  for(uint32_t i = 0; i < line_count; i++)
    fn->debug->sourceLines.data[i] =
        (int)reader_le32(lines + (size_t)i * sizeof(uint32_t));
#endif

  // Reserve every constant slot before filling any of them, so a nested function
  // can be published into its cell as soon as it exists, and fill it with
  // NULL_VAL before the loop can allocate: a collection triggered by a string or
  // a nested function must not scan an unwritten slot.
  Value* constants =
      (Value*)cache_alloc_exact(builder->vm, constants_count, sizeof(Value));
  if(constants == NULL && constants_count > 0) {
    if(rooted)
      wrenPopRoot(builder->vm);
    return build_fn_failed(r);
  }
  fn->constants.data = constants;
  fn->constants.count = fn->constants.capacity = (int)constants_count;
  for(uint32_t i = 0; i < constants_count; i++)
    constants[i] = NULL_VAL;

  for(uint32_t i = 0; i < constants_count && !r->failed; i++) {
    uint8_t tag;
    if(!reader_u8(r, &tag)) {
      r->failed = true;
      break;
    }

    switch(tag) {
      case CACHE_CONST_NUM: {
        double value;
        if(!reader_double(r, &value)) {
          r->failed = true;
          break;
        }
        constants[i] = NUM_VAL(value);
        break;
      }
      case CACHE_CONST_NULL:
        constants[i] = NULL_VAL;
        break;
      case CACHE_CONST_TRUE:
        constants[i] = TRUE_VAL;
        break;
      case CACHE_CONST_FALSE:
        constants[i] = FALSE_VAL;
        break;
      case CACHE_CONST_STRING: {
        const uint8_t* text;
        size_t         len;
        if(!reader_sized(r, &text, &len)) {
          r->failed = true;
          break;
        }
        // wrenNewStringLength() copies the bytes and can GC. The returned value
        // is stored immediately afterwards, with no allocation in between, so it
        // cannot be collected before it becomes reachable.
        constants[i] = wrenNewStringLength(builder->vm, (const char*)text, len);
        break;
      }
      case CACHE_CONST_QUERY: {
        const uint8_t* text;
        size_t         len;
        if(!reader_sized(r, &text, &len)) {
          r->failed = true;
          break;
        }
        constants[i] = wrenNewQueryLength(builder->vm, (const char*)text, len);
        break;
      }
      case CACHE_CONST_FN:
        if(!build_fn(r, builder, &constants[i], depth + 1))
          r->failed = true;
        break;
      default:
        r->failed = true;
        break;
    }
  }

  if(rooted)
    wrenPopRoot(builder->vm);
  return r->failed ? NULL : fn;
}

// Declares the module's new variables in the recorded order. The caller has
// already checked that every name is absent and that the count fits, so this
// cannot fail for a blob the writer produced; it appends exactly the slots the
// bytecode's module-variable operands index into. The one a hand-damaged blob
// could still provoke is a repeated name inside this section, which would leave
// the earlier slots declared and make the fallback compile reject the
// redeclaration -- unreachable from write_module_state(), which lists each
// declared variable once.
static bool build_module_vars(CacheReader* r, WrenVM* vm, ObjModule* module) {
  uint32_t var_base, var_count;
  if(!reader_u32(r, &var_base) || !reader_u32(r, &var_count))
    return false;
  (void)var_base;

  for(uint32_t i = 0; i < var_count; i++) {
    const uint8_t* name;
    size_t         len;
    if(!reader_sized(r, &name, &len))
      return false;

    uint8_t kind;
    if(!reader_u8(r, &kind))
      return false;

    if(kind == CACHE_VAR_NULL) {
      if(wrenDefineVariable(vm, module, (const char*)name, len, NULL_VAL, NULL) < 0)
        return false;
    } else {
      uint32_t line;
      if(!reader_u32(r, &line))
        return false;
      if(wrenDeclareVariable(vm, module, (const char*)name, len, (int)line) < 0)
        return false;
    }
  }

  return true;
}

// Appends the recorded method names. Each must land at exactly the index the
// original compile gave it.
static bool build_symbols(CacheReader* r, WrenVM* vm) {
  uint8_t  core_flag;
  uint32_t symbol_base, symbol_count;
  if(!reader_u8(r, &core_flag) || !reader_u32(r, &symbol_base) ||
     !reader_u32(r, &symbol_count))
    return false;
  (void)core_flag;

  for(uint32_t i = 0; i < symbol_count; i++) {
    const uint8_t* name;
    size_t         len;
    if(!reader_sized(r, &name, &len))
      return false;
    int symbol = wrenSymbolTableEnsure(vm, &vm->methodNames, (const char*)name, len);
    // Extra symbols appended by a rejected earlier attempt keep their positions,
    // so this only fails when the VM state genuinely differs from the one the
    // blob was built for. Nothing is served in that case.
    if(symbol != (int)(symbol_base + i))
      return false;
  }

  return true;
}

static bool read_header(CacheReader* r) {
  uint32_t version, wren_version;
  return reader_u32(r, &version) && reader_u32(r, &wren_version) &&
         version == CACHE_BLOB_VERSION &&
         wren_version == (uint32_t)WREN_VERSION_NUMBER;
}

// Materialises [blob] into [vm] and [module] and, on success, stores the module
// body closure in [out].
//
// One pass: the header and the module-state deltas are checked first, because
// they are cheap, allocation-free and compare against the VM (a blob built for a
// different state is a plain miss), then the function tree is built and
// validated in the same walk. Nothing in the module is written until that tree
// is complete, and the symbols and variables are appended only after it, so a
// rejected blob leaves the module exactly as the caller found it and the compile
// fallback starts from a clean slate.
static CacheReplay build_blob(const uint8_t* blob, size_t blob_len, WrenVM* vm,
                              ObjModule* module, bool is_core, ObjClosure** out) {
  CacheReader r = {blob, blob_len, 0, false};
  if(!read_header(&r))
    return CACHE_REPLAY_CORRUPT;

  // Both module-state sections sit between the header and the function tree.
  // Record where the first one starts: both are replayed only after the tree
  // exists, so a failed build never appends a symbol or a variable either.
  size_t symbol_section = r.pos;
  size_t var_section = symbol_section;
  if(!validate_module_state(&r, vm, module, is_core, &var_section))
    return CACHE_REPLAY_MISMATCH;

  CacheBuilder builder = {vm, module};
  Value        holder = NULL_VAL;
  ObjFn*       fn = build_fn(&r, &builder, &holder, 0);
  // A well-formed blob is consumed exactly: trailing bytes mean the writer and
  // the reader disagree, which is corruption.
  if(fn == NULL || r.failed || r.pos != r.len)
    return CACHE_REPLAY_CORRUPT;

  wrenPushRoot(vm, (Obj*)fn);
  CacheReader symbols = {blob, blob_len, symbol_section, false};
  CacheReader vars = {blob, blob_len, var_section, false};
  bool ok = build_symbols(&symbols, vm) && build_module_vars(&vars, vm, module);
  ObjClosure* closure = ok ? wrenNewClosure(vm, fn) : NULL;
  wrenPopRoot(vm);

  if(!ok)
    return CACHE_REPLAY_CORRUPT;

  *out = closure;
  return CACHE_REPLAY_OK;
}

/* ------------------------------------------------------------------------- */
/* Entry table                                                              */
/* ------------------------------------------------------------------------- */

static bool entry_find_locked(const char* key, size_t* index) {
  for(size_t i = 0; i < cache_entry_count; i++) {
    if(strcmp(cache_entries[i].key, key) == 0) {
      *index = i;
      return true;
    }
  }
  return false;
}

static void entry_clear(CacheEntry* entry) {
  free(entry->key);
  free(entry->blob);
  memset(entry, 0, sizeof(*entry));
}

/* ------------------------------------------------------------------------- */
/* Cache lookup                                                             */
/* ------------------------------------------------------------------------- */

typedef enum {
  CACHE_LOOKUP_HIT,
  CACHE_LOOKUP_MISS,
  CACHE_LOOKUP_CORRUPT,
  CACHE_LOOKUP_EMBEDDED_MISMATCH, // embedded blob that no longer fits: a miss
                                  // that must not pass silently
} CacheLookupResult;

// Looks [key] up and, on a hit, builds the closure. A MISS is a normal cache
// miss (no entry, source changed, or the blob does not fit this VM's state);
// CORRUPT means the blob itself is unusable and is reported once, and an
// EMBEDDED_MISMATCH is the same report for a blob this binary built itself.
static CacheLookupResult cache_lookup(WrenVM* vm, ObjModule* module,
                                      const char* module_name, const char* key,
                                      const char* source, size_t source_len,
                                      const struct BialetFileStat* stat,
                                      ObjClosure**                 out) {
  CacheLookupResult result = CACHE_LOOKUP_MISS;

  cache_lock_acquire();
  size_t index;
  if(entry_find_locked(key, &index)) {
    CacheEntry* entry = &cache_entries[index];
    if(cache_entry_matches(entry, source, source_len)) {
      CacheReplay replay = build_blob(entry->blob, entry->blob_len, vm, module,
                                      module_name == NULL, out);
      if(replay == CACHE_REPLAY_OK) {
        result = CACHE_LOOKUP_HIT;
        // The bytes matched, so this is the same source under a newer
        // signature (a touch or a no-op save): remember it so the next
        // request can stop at stat() and skip the read and hash entirely.
        if(stat != NULL && stat->valid) {
          entry->has_stat = true;
          entry->stat = *stat;
        }
      } else if(replay == CACHE_REPLAY_CORRUPT) {
        result = CACHE_LOOKUP_CORRUPT;
      } else if(entry->embedded) {
        // An embedded blob is built by this binary for exactly this VM state, so
        // a mismatch means the cache is quietly doing nothing for every VM. Say
        // it once instead of just recompiling in silence.
        result = CACHE_LOOKUP_EMBEDDED_MISMATCH;
      }
    }
  }
  cache_lock_release();

  return result;
}

/* ------------------------------------------------------------------------- */
/* Cache store                                                              */
/* ------------------------------------------------------------------------- */

// [is_core] is true when the source compiles into the anonymous core module,
// which is the case for both embedded modules (wren_core.wren and bialet.wren)
// and never for a named user module. It selects the blob's module-state flag,
// which is what ties the blob to that module's bootstrap state.
static void cache_store(WrenVM* vm, ObjModule* module, bool is_core, const char* key,
                        const char* source, size_t source_len,
                        const struct BialetFileStat* stat, int symbol_base,
                        int var_base, ObjFn* fn) {
  CacheWriter writer = {NULL, 0, 0, false};
  writer_u32(&writer, CACHE_BLOB_VERSION);
  writer_u32(&writer, (uint32_t)WREN_VERSION_NUMBER);

  bool ok =
      write_module_state(&writer, vm, module, symbol_base, var_base, is_core) &&
      write_fn(&writer, fn, 0);
  if(!ok || writer.failed) {
    free(writer.data);
    // Not an error: a function shape this version cannot represent is simply
    // left uncached.
    return;
  }

  char* key_copy = string_safe_copy(key);
  if(key_copy == NULL) {
    free(writer.data);
    return;
  }

  uint64_t source_hash = 0;
  if(!is_core) {
    if(source == NULL) {
      free(key_copy);
      free(writer.data);
      return;
    }
    source_hash = bytecode_cache_hash(source, source_len);
  }

  cache_lock_acquire();
  size_t index;
  if(entry_find_locked(key, &index)) {
    // Replace on mismatch (an edited file, or a blob that no longer fits) so
    // there is at most one entry per path.
    entry_clear(&cache_entries[index]);
  } else {
    if(cache_entry_count == cache_entry_capacity) {
      size_t capacity = cache_entry_capacity == 0 ? 16 : cache_entry_capacity * 2;
      CacheEntry* entries =
          (CacheEntry*)realloc(cache_entries, capacity * sizeof(CacheEntry));
      if(entries == NULL) {
        cache_lock_release();
        free(key_copy);
        free(writer.data);
        return;
      }
      cache_entries = entries;
      cache_entry_capacity = capacity;
    }
    index = cache_entry_count++;
  }

  CacheEntry* entry = &cache_entries[index];
  entry->key = key_copy;
  entry->blob = writer.data;
  entry->blob_len = writer.len;
  entry->embedded = is_core;
  entry->source_len = source_len;
  entry->source_hash = source_hash;
  entry->has_stat = !is_core && stat != NULL && stat->valid;
  if(entry->has_stat)
    entry->stat = *stat;
  // Test hook (see the top of this file): damage the one framework blob, so the
  // next VM has to reject it. The following request stores a fresh copy.
  if(cache_fault && !cache_fault_done && is_core && entry->blob_len > 1 &&
     strcmp(key, BYTECODE_CACHE_FRAMEWORK_KEY) == 0) {
    cache_fault_done = true;
    entry->blob_len--;
  }
  cache_lock_release();
}

/* ------------------------------------------------------------------------- */
/* Public entry points                                                      */
/* ------------------------------------------------------------------------- */

// Shared by the embedded and user-file entry points. [embedded] sources come
// from this binary and are always cacheable and never content-checked; user
// entries need user-file caching to be on and a resolved key.
static ObjClosure* cache_compile(WrenVM* vm, const char* module_name,
                                 const char* source, const char* cache_key,
                                 const struct BialetFileStat* stat, bool embedded) {
  if(!embedded && (!cache_user_files || cache_key == NULL))
    return wrenCompileSource(vm, module_name, source, false, true);

  size_t source_len = strlen(source);

  // Find (or create) the module exactly as compileInModule() would, so the
  // module-variable base and the implicit core-variable copy match a compile.
  Value name_value = NULL_VAL;
  if(module_name != NULL) {
    name_value = wrenNewString(vm, module_name);
    wrenPushRoot(vm, AS_OBJ(name_value));
  }

  ObjModule* module = wrenGetOrCreateModule(vm, name_value);

  int symbol_base = vm->methodNames.count;
  int var_base = module->variables.count;

  ObjClosure*       closure = NULL;
  CacheLookupResult lookup = cache_lookup(vm, module, module_name, cache_key, source,
                                          source_len, stat, &closure);
  if(lookup == CACHE_LOOKUP_CORRUPT) {
    // The entry is replaced by the store below (same key) and the source is
    // compiled normally. Never serve a half-built module.
    cache_warn("Discarding a corrupt cached module; recompiling");
  } else if(lookup == CACHE_LOOKUP_EMBEDDED_MISMATCH) {
    cache_warn("Embedded bytecode blob no longer fits this VM; recompiling");
  }

  if(closure == NULL) {
    ObjFn* fn = wrenCompile(vm, module, source, false, true);
    if(fn == NULL) {
      if(module_name != NULL)
        wrenPopRoot(vm);
      return NULL;
    }

    wrenPushRoot(vm, (Obj*)fn);
    closure = wrenNewClosure(vm, fn);
    wrenPopRoot(vm); // fn is now reachable through the closure.

    // Serialise before the module body runs: class bodies back-patch
    // superclass constants and field slots into their ObjFns during execution,
    // so only the pristine compile output is a valid cache entry.
    wrenPushRoot(vm, (Obj*)closure);
    cache_store(vm, module, module_name == NULL, cache_key, source, source_len, stat,
                symbol_base, var_base, fn);
    wrenPopRoot(vm);
  }

  if(module_name != NULL)
    wrenPopRoot(vm); // name_value

  // Test hook (see the top of this file): the only way to tell a replay from a
  // silent recompile from outside the process.
  if(cache_trace && embedded) {
    message(yellow("Bytecode cache"),
            strcmp(cache_key, BYTECODE_CACHE_CORE_KEY) == 0 ? "core:" : "framework:",
            lookup == CACHE_LOOKUP_HIT ? "replayed" : "compiled");
  }

  return closure;
}

WrenInterpretResult bytecode_cache_interpret_framework(WrenVM*     vm,
                                                       const char* source) {
  ObjClosure* closure =
      cache_compile(vm, NULL, source, BYTECODE_CACHE_FRAMEWORK_KEY, NULL, true);
  if(closure == NULL)
    return WREN_RESULT_COMPILE_ERROR;

  return wrenRunClosure(vm, closure);
}

// The core module boots first, before any class exists: wrenInitializeCore has
// defined Object, Class and Object metaclass and bound their primitives, and
// nothing else. That fixed starting state is what the blob's module-state
// header records, so a blob built by the first VM in the process loads into
// every later one; anything else (a different symbol or variable base) is a
// mismatch and falls back to compiling wren_core.wren.
//
// Strings deserialised here still have a NULL class: stringClass does not exist
// yet, exactly as for a fresh compile, and wrenInitializeCore() fixes them up
// once it does.
WrenInterpretResult bytecode_cache_interpret_core(WrenVM* vm, const char* source) {
  ObjClosure* closure =
      cache_compile(vm, NULL, source, BYTECODE_CACHE_CORE_KEY, NULL, true);
  if(closure == NULL)
    return WREN_RESULT_COMPILE_ERROR;

  return wrenRunClosure(vm, closure);
}

ObjClosure* bytecode_cache_compile_user(WrenVM* vm, const char* module_name,
                                        const char* source, const char* cache_key,
                                        const struct BialetFileStat* stat) {
  return cache_compile(vm, module_name, source, cache_key, stat, false);
}

ObjClosure* bytecode_cache_load_by_stat(WrenVM* vm, const char* module_name,
                                        const char*                  cache_key,
                                        const struct BialetFileStat* stat) {
  if(!cache_user_files || cache_key == NULL || stat == NULL || !stat->valid)
    return NULL;

  // The module must exist (with its implicit core variables) exactly as it
  // would for a compile, because the blob's variable indices assume that base.
  Value name_value = NULL_VAL;
  if(module_name != NULL) {
    name_value = wrenNewString(vm, module_name);
    wrenPushRoot(vm, AS_OBJ(name_value));
  }
  ObjModule* module = wrenGetOrCreateModule(vm, name_value);

  ObjClosure* closure = NULL;
  bool        corrupt = false;

  cache_lock_acquire();
  size_t index;
  if(entry_find_locked(cache_key, &index)) {
    CacheEntry* entry = &cache_entries[index];
    if(cache_stat_matches(entry, stat)) {
      CacheReplay replay = build_blob(entry->blob, entry->blob_len, vm, module,
                                      module_name == NULL, &closure);
      if(replay == CACHE_REPLAY_CORRUPT)
        corrupt = true;
    }
  }
  cache_lock_release();

  if(module_name != NULL)
    wrenPopRoot(vm); // name_value

  if(corrupt)
    cache_warn("Discarding a corrupt cached module; recompiling");

  return closure;
}

WrenInterpretResult bytecode_cache_interpret_user(WrenVM*     vm,
                                                  const char* module_name,
                                                  const char* source,
                                                  const char* cache_key) {
  if(!cache_user_files || cache_key == NULL)
    return wrenInterpret(vm, module_name, source);

  ObjClosure* closure =
      cache_compile(vm, module_name, source, cache_key, NULL, false);
  if(closure == NULL)
    return WREN_RESULT_COMPILE_ERROR;

  return wrenRunClosure(vm, closure);
}
