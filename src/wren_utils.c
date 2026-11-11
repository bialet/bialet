#include "wren_utils.h"

#include "wren_vm.h"
#include <string.h>

DEFINE_BUFFER(Byte, uint8_t);
DEFINE_BUFFER(Int, int);
DEFINE_BUFFER(String, ObjString*);

void wrenSymbolTableInit(SymbolTable* symbols) {
  symbols->data = NULL;
  symbols->count = 0;
  symbols->capacity = 0;
  symbols->ids = NULL;
  symbols->idsCapacity = 0;
}

void wrenSymbolTableClear(WrenVM* vm, SymbolTable* symbols) {
  wrenReallocate(vm, symbols->data, 0, 0);
  wrenReallocate(vm, symbols->ids, 0, 0);
  wrenSymbolTableInit(symbols);
}

// FNV-1a over the name, used only to pick a bucket in the [ids] index. A fixed
// seed is fine here: the names come from the source being compiled and method
// signatures, never from request data, and an attacker who picks them already
// runs the code.
static uint32_t symbol_hash(const char* name, size_t length) {
  uint32_t hash = 2166136261u;
  for(size_t i = 0; i < length; i++) {
    hash ^= (unsigned char)name[i];
    hash *= 16777619u;
  }
  return hash;
}

// Rebuilds [symbols]->ids at [idsCapacity] (a power of two) from the current
// symbol list.
static void symbol_index_rebuild(WrenVM* vm, SymbolTable* symbols, int idsCapacity) {
  symbols->ids = (int*)wrenReallocate(vm, symbols->ids,
                                      (size_t)symbols->idsCapacity * sizeof(int),
                                      (size_t)idsCapacity * sizeof(int));
  symbols->idsCapacity = idsCapacity;
  memset(symbols->ids, 0, (size_t)idsCapacity * sizeof(int));

  uint32_t mask = (uint32_t)idsCapacity - 1;
  for(int i = 0; i < symbols->count; i++) {
    uint32_t slot =
        symbol_hash(symbols->data[i]->value, (size_t)symbols->data[i]->length) &
        mask;
    while(symbols->ids[slot] != 0)
      slot = (slot + 1) & mask;
    symbols->ids[slot] = i + 1;
  }
}

int wrenSymbolTableAdd(WrenVM* vm, SymbolTable* symbols, const char* name,
                       size_t length) {
  ObjString* symbol = AS_STRING(wrenNewStringLength(vm, name, length));

  wrenPushRoot(vm, &symbol->obj);
  // Grows both arrays: the symbol list in powers of two, and the lookup index
  // so it stays under a 3/4 load factor.
  if(symbols->count + 1 > symbols->capacity) {
    int capacity = wrenPowerOf2Ceil(symbols->count + 1);
    symbols->data = (ObjString**)wrenReallocate(
        vm, symbols->data, (size_t)symbols->capacity * sizeof(ObjString*),
        (size_t)capacity * sizeof(ObjString*));
    symbols->capacity = capacity;
  }
  symbols->data[symbols->count++] = symbol;

  if(symbols->idsCapacity == 0 || symbols->count * 4 >= symbols->idsCapacity * 3) {
    int idsCapacity =
        symbols->idsCapacity == 0 ? 64 : wrenPowerOf2Ceil(symbols->idsCapacity * 2);
    symbol_index_rebuild(vm, symbols, idsCapacity);
  } else {
    uint32_t mask = (uint32_t)symbols->idsCapacity - 1;
    uint32_t slot = symbol_hash(name, length) & mask;
    while(symbols->ids[slot] != 0)
      slot = (slot + 1) & mask;
    symbols->ids[slot] = symbols->count;
  }
  wrenPopRoot(vm);

  return symbols->count - 1;
}

int wrenSymbolTableEnsure(WrenVM* vm, SymbolTable* symbols, const char* name,
                          size_t length) {
  // See if the symbol is already defined.
  int existing = wrenSymbolTableFind(symbols, name, length);
  if(existing != -1)
    return existing;

  // New symbol, so add it.
  return wrenSymbolTableAdd(vm, symbols, name, length);
}

int wrenSymbolTableFind(const SymbolTable* symbols, const char* name,
                        size_t length) {
  if(symbols->count == 0)
    return -1;

  // No index (only possible for a table built before an id was added through
  // wrenSymbolTableAdd): fall back to the linear scan.
  if(symbols->idsCapacity == 0) {
    for(int i = 0; i < symbols->count; i++) {
      if(wrenStringEqualsCString(symbols->data[i], name, length))
        return i;
    }
    return -1;
  }

  uint32_t mask = (uint32_t)symbols->idsCapacity - 1;
  uint32_t slot = symbol_hash(name, length) & mask;
  for(;;) {
    int id = symbols->ids[slot];
    if(id == 0)
      return -1;
    ObjString* symbol = symbols->data[id - 1];
    if((size_t)symbol->length == length &&
       (length == 0 || memcmp(symbol->value, name, length) == 0))
      return id - 1;
    slot = (slot + 1) & mask;
  }
}

void wrenBlackenSymbolTable(WrenVM* vm, SymbolTable* symbolTable) {
  for(int i = 0; i < symbolTable->count; i++) {
    wrenGrayObj(vm, &symbolTable->data[i]->obj);
  }

  // Keep track of how much memory is still in use.
  vm->bytesAllocated += symbolTable->capacity * sizeof(*symbolTable->data);
  vm->bytesAllocated += (size_t)symbolTable->idsCapacity * sizeof(int);
}

int wrenUtf8EncodeNumBytes(int value) {
  ASSERT(value >= 0, "Cannot encode a negative value.");

  if(value <= 0x7f)
    return 1;
  if(value <= 0x7ff)
    return 2;
  if(value <= 0xffff)
    return 3;
  if(value <= 0x10ffff)
    return 4;
  return 0;
}

int wrenUtf8Encode(int value, uint8_t* bytes) {
  if(value <= 0x7f) {
    // Single byte (i.e. fits in ASCII).
    *bytes = value & 0x7f;
    return 1;
  } else if(value <= 0x7ff) {
    // Two byte sequence: 110xxxxx 10xxxxxx.
    *bytes = 0xc0 | ((value & 0x7c0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 2;
  } else if(value <= 0xffff) {
    // Three byte sequence: 1110xxxx 10xxxxxx 10xxxxxx.
    *bytes = 0xe0 | ((value & 0xf000) >> 12);
    bytes++;
    *bytes = 0x80 | ((value & 0xfc0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 3;
  } else if(value <= 0x10ffff) {
    // Four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
    *bytes = 0xf0 | ((value & 0x1c0000) >> 18);
    bytes++;
    *bytes = 0x80 | ((value & 0x3f000) >> 12);
    bytes++;
    *bytes = 0x80 | ((value & 0xfc0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 4;
  }

  // Invalid Unicode value. See: http://tools.ietf.org/html/rfc3629
  UNREACHABLE();
  return 0;
}

int wrenUtf8Decode(const uint8_t* bytes, uint32_t length) {
  // The truncation guard below tests `remainingBytes > length - 1`, which
  // underflows to a huge value when length is 0.
  if(length == 0)
    return -1;

  // Single byte (i.e. fits in ASCII).
  if(*bytes <= 0x7f)
    return *bytes;

  int      value;
  uint32_t remainingBytes;
  if((*bytes & 0xe0) == 0xc0) {
    // Two byte sequence: 110xxxxx 10xxxxxx.
    value = *bytes & 0x1f;
    remainingBytes = 1;
  } else if((*bytes & 0xf0) == 0xe0) {
    // Three byte sequence: 1110xxxx	 10xxxxxx 10xxxxxx.
    value = *bytes & 0x0f;
    remainingBytes = 2;
  } else if((*bytes & 0xf8) == 0xf0) {
    // Four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
    value = *bytes & 0x07;
    remainingBytes = 3;
  } else {
    // Invalid UTF-8 sequence.
    return -1;
  }

  // Don't read past the end of the buffer on truncated UTF-8.
  if(remainingBytes > length - 1)
    return -1;

  while(remainingBytes > 0) {
    bytes++;
    remainingBytes--;

    // Remaining bytes must be of form 10xxxxxx.
    if((*bytes & 0xc0) != 0x80)
      return -1;

    value = value << 6 | (*bytes & 0x3f);
  }

  return value;
}

int wrenUtf8DecodeNumBytes(uint8_t byte) {
  // If the byte starts with 10xxxxx, it's the middle of a UTF-8 sequence, so
  // don't count it at all.
  if((byte & 0xc0) == 0x80)
    return 0;

  // The first byte's high bits tell us how many bytes are in the UTF-8
  // sequence.
  if((byte & 0xf8) == 0xf0)
    return 4;
  if((byte & 0xf0) == 0xe0)
    return 3;
  if((byte & 0xe0) == 0xc0)
    return 2;
  return 1;
}

// From:
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int wrenPowerOf2Ceil(int n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;

  return n;
}

uint32_t wrenValidateIndex(uint32_t count, int64_t value) {
  // Negative indices count from the end.
  if(value < 0)
    value = count + value;

  // Check bounds.
  if(value >= 0 && value < count)
    return (uint32_t)value;

  return UINT32_MAX;
}
