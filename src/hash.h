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
#ifndef HASH_H
#define HASH_H

#include "wren.h"
#ifdef HAVE_SSL
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define OPENSSL_OK 1
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif
#endif

void verifyPassword(WrenVM* vm);
void hashPassword(WrenVM* vm);

#endif
