#ifndef wren_debug_h
#define wren_debug_h

#include "wren_value.h"
#include "wren_vm.h"

// Prints the runtime error for the current fiber, then its stack trace unless
// [frames] is false.
//
// Used when a fiber throws a runtime error which is not caught. A failed import
// passes false: the compile errors already name the broken file, and the only
// frame left would blame the importer's `import` line.
void wrenDebugPrintStackTrace(WrenVM* vm, bool frames);

// The "dump" functions are used for debugging Wren itself. Normal code paths
// will not call them unless one of the various DEBUG_ flags is enabled.

// Prints a representation of [value] to stdout.
void wrenDumpValue(Value value);

// Prints a representation of the bytecode for [fn] at instruction [i].
int wrenDumpInstruction(WrenVM* vm, ObjFn* fn, int i);

// Prints the disassembled code for [fn] to stdout.
void wrenDumpCode(WrenVM* vm, ObjFn* fn);

// Prints the contents of the current stack for [fiber] to stdout.
void wrenDumpStack(ObjFiber* fiber);

#endif
