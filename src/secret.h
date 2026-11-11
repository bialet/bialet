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
#ifndef SECRET_H
#define SECRET_H

#include <stddef.h>

/* The deployment secret: the one piece of key material that must not live in
 * the database.
 *
 * Everything Bialet configures lives in BIALET_CONFIG, and `bialet -r` exists to
 * seed it. That is exactly the wrong place for an encryption key: a copy of
 * _db.sqlite3 would then contain both the ciphertext and the key that opens it.
 * The key comes from a file outside the app root instead, named with
 * --secret-key-file, and is never exposed to Wren (a value reachable from a
 * template, a log line, or %(...) interpolation is a value that eventually
 * leaks into one of them). */

/* Loads [path] into memory. Returns 0 on success, -1 with a human-readable
 * message in [err] on failure. [app_root] is the resolved root directory; a key
 * file inside it is refused, because that tree is the thing that gets copied,
 * backed up and, in a bad day, served. */
int secret_key_load(const char* path, const char* app_root, char* err,
                    size_t err_size);

/* 1 when a secret was loaded, 0 otherwise. */
int secret_key_available(void);

/* Raw key bytes (never NUL-terminated, may contain any byte) and their length.
 * Returns NULL when no secret is loaded. The pointer is owned by this module;
 * do not free it. */
const char* secret_key_bytes(size_t* len);

/* 1 when the key file was readable by group or other at load time. */
int secret_key_was_group_readable(void);

/* Wipes and forgets the key. Safe to call more than once. */
void secret_key_clear(void);

#endif
