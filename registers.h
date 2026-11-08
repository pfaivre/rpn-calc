/**
MIT License

Copyright (c) 2016-2026 Pierre Faivre

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef REGISTERS_H
#define REGISTERS_H

#include <stddef.h>

#include <gmp.h>

#include "stack.h"

/** Number of registers, each named by a single byte (as in dc) */
#define REGISTER_COUNT 256

/** One cell of a register's array (dc's : and ; commands) */
typedef struct ArrayEntry {
    mpz_t index;  /**< owned */
    Value val;    /**< owned */
} ArrayEntry;

/**
 * A named register: a stack of values (s/l/S/L) and, independently, an
 * array of cells (:/;), as in dc.
 */
typedef struct Register {
    Stack stack;          /**< value stack, created lazily */
    ArrayEntry *entries;  /**< array cells, grown on demand */
    size_t size;
    size_t cap;
} Register;

typedef struct StructRegisters {
    Register regs[REGISTER_COUNT];
} StructRegisters;

typedef StructRegisters* Registers;

Registers registers_init();

void registers_delete(Registers);

/** The value stack of a register, created on first use */
Stack register_stack(Registers, unsigned char name);

/**
 * Store a value in the register's array at the given index, replacing the
 * cell if the index already exists (dc's :)
 * @param val Value to store; ownership is taken (moved into the array)
 */
void register_array_store(Registers, unsigned char name, const mpz_t index, Value val);

/**
 * Find the array cell at the given index (dc's ;)
 * @return The stored value, or NULL when the index was never stored
 */
const Value *register_array_find(Registers, unsigned char name, const mpz_t index);

#endif
