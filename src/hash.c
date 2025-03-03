/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2025 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#include "hash.h"

#include <string.h>

#ifndef OPENSSL_OK
#define SALT_LENGTH 16
#define HASH_LENGTH 64

void unsafe_hash(const char* input, char* output) {
  unsigned int hash = 5381;
  int          c;
  while((c = *input++))
    hash = ((hash << 5) + hash) + c; // hash * 33 + c
  snprintf(output, HASH_LENGTH + 1, "%08x%08x%08x%08x", hash, hash * 3, hash * 5,
           hash * 7);
}

void generate_salt(char* salt, size_t length) {
  for(size_t i = 0; i < length; i++) {
    salt[i] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"[rand() %
                                                                         62];
  }
  salt[length] = '\0';
}
#endif

void hashPassword(WrenVM* vm) {
  const char* password = wrenGetSlotString(vm, 1);
#ifdef OPENSSL_OK
  unsigned char salt[16];
  if(!RAND_bytes(salt, sizeof(salt))) {
    perror("Failed to generate salt");
    return;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, hash, &hash_len);

  EVP_MD_CTX_free(ctx);

  static char result[128];
  for(unsigned int i = 0; i < hash_len; i++) {
    sprintf(result + (i * 2), "%02x", hash[i]);
  }

  strcat(result, "/");
  for(int i = 0; i < sizeof(salt); i++) {
    sprintf(result + strlen(result), "%02x", salt[i]);
  }
#else
  char salt[SALT_LENGTH + 1];
  generate_salt(salt, SALT_LENGTH);
  char saltedPassword[strlen(password) + SALT_LENGTH + 1];
  snprintf(saltedPassword, sizeof(saltedPassword), "%s%s", password, salt);
  char hash[HASH_LENGTH + 1];
  unsafe_hash(saltedPassword, hash);
  char result[HASH_LENGTH + SALT_LENGTH + 2];
  snprintf(result, sizeof(result), "%s$%s", hash, salt); // Formato: hash$salt
#endif

  wrenEnsureSlots(vm, 2);
  wrenSetSlotString(vm, 0, result);
}

void verifyPassword(WrenVM* vm) {
  int         result = 0;
  const char* password = wrenGetSlotString(vm, 1);
  const char* hash_and_salt = wrenGetSlotString(vm, 2);
#ifdef OPENSSL_OK

  char stored_hash[65], stored_salt[33];
  strncpy(stored_hash, hash_and_salt, 64);
  stored_hash[64] = 0;
  strcpy(stored_salt, hash_and_salt + 65);

  unsigned char salt[16];
  for(int i = 0; i < 16; i++) {
    sscanf(stored_salt + i * 2, "%2hhx", &salt[i]);
  }

  unsigned char new_hash[EVP_MAX_MD_SIZE];
  unsigned int  new_hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, new_hash, &new_hash_len);

  EVP_MD_CTX_free(ctx);

  char new_hash_str[65];
  for(unsigned int i = 0; i < new_hash_len; i++) {
    sprintf(new_hash_str + (i * 2), "%02x", new_hash[i]);
  }
  new_hash_str[64] = 0;
  result = strcmp(new_hash_str, stored_hash) == 0;
#else
  char storedHash[HASH_LENGTH + 1];
  char storedSalt[SALT_LENGTH + 1];
  if(sscanf(hash_and_salt, "%64[^$]$%16s", storedHash, storedSalt) != 2) {
    wrenEnsureSlots(vm, 2);
    wrenSetSlotBool(vm, 0, 0);
    return;
  }
  char saltedPassword[strlen(password) + SALT_LENGTH + 1];
  snprintf(saltedPassword, sizeof(saltedPassword), "%s%s", password, storedSalt);
  char computedHash[HASH_LENGTH + 1];
  unsafe_hash(saltedPassword, computedHash);
  result = (strncmp(computedHash, storedHash, HASH_LENGTH) == 0);
#endif
  wrenEnsureSlots(vm, 2);
  wrenSetSlotBool(vm, 0, result);
}
