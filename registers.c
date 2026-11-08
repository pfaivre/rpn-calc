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

#include <stdlib.h>

#include <gmp.h>

#include "io.h"
#include "stack.h"
#include "registers.h"

/**
 * Create the register file: all registers start empty (zeroed by calloc,
 * stacks and arrays are created lazily)
 */
Registers registers_init() {
    Registers regs = calloc(1, sizeof(StructRegisters));
    if (!regs)
        rpn_fatal("memory allocation error");
    return regs;
}

/**
 * Free all registers: their value stacks, array cells and the file itself
 * @param regs The register file to free
 */
void registers_delete(Registers regs) {
    if (!regs)
        return;
    for (int i = 0; i < REGISTER_COUNT; i++) {
        Register *reg = &regs->regs[i];
        if (reg->stack)
            stack_delete(reg->stack);
        for (size_t j = 0; j < reg->size; j++) {
            mpz_clear(reg->entries[j].index);
            value_clear(&reg->entries[j].val);
        }
        free(reg->entries);
    }
    free(regs);
}

/**
 * The value stack of a register, created on first use
 * @param regs Register file
 * @param name Register name (one byte)
 * @return The register's stack
 */
Stack register_stack(Registers regs, unsigned char name) {
    Register *reg = &regs->regs[name];
    if (!reg->stack)
        reg->stack = stack_init();
    return reg->stack;
}

/**
 * Store a value in a register's array at the given index, replacing the
 * cell if the index already exists (:). The array grows by doubling,
 * with the byte size checked against overflow.
 * @param regs Register file
 * @param name Register name
 * @param index Array index (copied)
 * @param val Value to store; ownership is taken (moved into the array)
 */
void register_array_store(Registers regs, unsigned char name, const mpz_t index, Value val) {
    Register *reg = &regs->regs[name];

    for (size_t i = 0; i < reg->size; i++) {
        if (mpz_cmp(reg->entries[i].index, index) == 0) {
            value_clear(&reg->entries[i].val);
            reg->entries[i].val = val;  // move
            return;
        }
    }

    if (reg->size == reg->cap)
        reg->entries = checked_grow(reg->entries, &reg->cap, sizeof(ArrayEntry));

    mpz_init_set(reg->entries[reg->size].index, index);
    reg->entries[reg->size].val = val;  // move
    reg->size++;
}

/**
 * Find the array cell at the given index (;)
 * @param regs Register file
 * @param name Register name
 * @param index Array index to look up
 * @return Pointer to the stored value, or NULL when the index was never stored
 */
const Value *register_array_find(Registers regs, unsigned char name, const mpz_t index) {
    Register *reg = &regs->regs[name];
    for (size_t i = 0; i < reg->size; i++)
        if (mpz_cmp(reg->entries[i].index, index) == 0)
            return &reg->entries[i].val;
    return NULL;
}
