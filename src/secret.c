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
#include "secret.h"

#include "bialet.h"
#include "hash.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* A key is 32 bytes written as hex (64 characters), or a passphrase of any
 * length. 4KB leaves room for a long passphrase without turning the file into
 * an accidental data channel. */
#define SECRET_KEY_MAX 4096

static char   secret_key_buf[SECRET_KEY_MAX + 1];
static size_t secret_key_len = 0;
static int    secret_key_loaded = 0;
static int    secret_key_loose_perms = 0;

int secret_key_available(void) {
  return secret_key_loaded;
}

const char* secret_key_bytes(size_t* len) {
  if(!secret_key_loaded) {
    if(len != NULL)
      *len = 0;
    return NULL;
  }
  if(len != NULL)
    *len = secret_key_len;
  return secret_key_buf;
}

int secret_key_was_group_readable(void) {
  return secret_key_loose_perms;
}

void secret_key_clear(void) {
  secure_zero(secret_key_buf, sizeof(secret_key_buf));
  secret_key_len = 0;
  secret_key_loaded = 0;
  secret_key_loose_perms = 0;
}

int secret_key_load(const char* path, const char* app_root, char* err,
                    size_t err_size) {
  if(path == NULL || *path == '\0') {
    snprintf(err, err_size, "--secret-key-file was given an empty path");
    return -1;
  }

  char resolved[PATH_MAX];
  if(realpath_n(path, resolved, sizeof(resolved)) == NULL) {
    snprintf(err, err_size, "secret key file not found: %s", path);
    return -1;
  }

  struct stat st;
  if(stat(resolved, &st) != 0) {
    snprintf(err, err_size, "cannot stat secret key file: %s", resolved);
    return -1;
  }
  /* A FIFO or device would block the server forever on read, and a directory
   * is never a key. Only a regular file is accepted. */
  if(!S_ISREG(st.st_mode)) {
    snprintf(err, err_size, "secret key file is not a regular file: %s", resolved);
    return -1;
  }

  /* Refuse a key that lives under the app root. The point of the file is to be
   * the one artifact that is *not* in the tree the database is copied with. */
  if(app_root != NULL && strlen(app_root) > 1) {
    size_t root_len = strlen(app_root);
    if(strncmp(resolved, app_root, root_len) == 0 &&
       (resolved[root_len] == '/' || resolved[root_len] == '\0')) {
      snprintf(err, err_size,
               "secret key file must live outside the app root (%s); a key "
               "stored with the database protects nothing",
               app_root);
      return -1;
    }
  }

#ifndef _WIN32
  /* Not fatal -- a 0600 enforcement would break read-only mounts and container
   * secrets -- but worth telling the operator about. main.c prints it. */
  if((st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    secret_key_loose_perms = 1;
#endif

  FILE* f = fopen(resolved, "rb");
  if(f == NULL) {
    snprintf(err, err_size, "cannot open secret key file: %s", resolved);
    return -1;
  }
  /* Read one byte past the limit so "exactly at the limit" and "over it" are
   * distinguishable. */
  size_t n = fread(secret_key_buf, 1, SECRET_KEY_MAX + 1, f);
  int    read_error = ferror(f);
  fclose(f);

  if(read_error) {
    snprintf(err, err_size, "cannot read secret key file: %s", resolved);
    return -1;
  }
  if(n > SECRET_KEY_MAX) {
    snprintf(err, err_size, "secret key file is larger than %d bytes: %s",
             SECRET_KEY_MAX, resolved);
    return -1;
  }

  /* Trailing newline from `echo`, indentation from a YAML-ish file: trim
   * whitespace, never anything inside the value. */
  size_t start = 0;
  while(start < n && isspace((unsigned char)secret_key_buf[start]))
    start++;
  size_t end = n;
  while(end > start && isspace((unsigned char)secret_key_buf[end - 1]))
    end--;

  if(end == start) {
    snprintf(err, err_size, "secret key file is empty: %s", resolved);
    return -1;
  }
  /* A NUL means this is binary key material pasted into a text file by
   * accident. Accepting it would silently truncate the key at the NUL. */
  if(memchr(secret_key_buf + start, '\0', end - start) != NULL) {
    snprintf(err, err_size,
             "secret key file contains a NUL byte; expected a passphrase or 64 "
             "hex characters: %s",
             resolved);
    return -1;
  }

  memmove(secret_key_buf, secret_key_buf + start, end - start);
  secret_key_len = end - start;
  secret_key_buf[secret_key_len] = '\0';
  secret_key_loaded = 1;
  return 0;
}
