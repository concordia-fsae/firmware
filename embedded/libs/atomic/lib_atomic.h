/**
 * @file lib_atomic.h
 * @brief Atomic operations library
 */


// NOTE: if other compilers are used, atomic access should be implemented for them here

#pragma once

#include "stdint.h"

#ifdef __GNUC__
// *FORMAT-OFF*
__attribute__((always_inline)) inline void atomicAddU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU16(volatile uint16_t *ptr, uint16_t val)   { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_xor_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU16(volatile uint16_t *ptr, uint16_t val) { (void)__sync_nand_and_fetch(ptr, val); }

__attribute__((always_inline)) inline void atomicAddU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU8(volatile uint8_t *ptr, uint8_t val)      { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_xor_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU8(volatile uint8_t *ptr, uint8_t val)    { (void)__sync_nand_and_fetch(ptr, val); }
// *FORMAT-ON*
#else // ifdef __GNUC__
// *FORMAT-OFF*
__attribute__((always_inline)) inline void atomicAddU16(volatile uint16_t *ptr, uint16_t val)  { *ptr += val; }
__attribute__((always_inline)) inline void atomicSubU16(volatile uint16_t *ptr, uint16_t val)  { *ptr -= val; }
__attribute__((always_inline)) inline void atomicOrU16(volatile uint16_t *ptr, uint16_t val)   { *ptr |= val; }
__attribute__((always_inline)) inline void atomicXorU16(volatile uint16_t *ptr, uint16_t val)  { *ptr ^= val; }
__attribute__((always_inline)) inline void atomicAndU16(volatile uint16_t *ptr, uint16_t val)  { *ptr &= val; }
__attribute__((always_inline)) inline void atomicNandU16(volatile uint16_t *ptr, uint16_t val) { ~(*ptr &= val); }

__attribute__((always_inline)) inline void atomicAddU8(volatile uint8_t *ptr, uint8_t val)  { *ptr += val; }
__attribute__((always_inline)) inline void atomicSubU8(volatile uint8_t *ptr, uint8_t val)  { *ptr -= val; }
__attribute__((always_inline)) inline void atomicOrU8(volatile uint8_t *ptr, uint8_t val)   { *ptr |= val; }
__attribute__((always_inline)) inline void atomicXorU8(volatile uint8_t *ptr, uint8_t val)  { *ptr ^= val; }
__attribute__((always_inline)) inline void atomicAndU8(volatile uint8_t *ptr, uint8_t val)  { *ptr &= val; }
__attribute__((always_inline)) inline void atomicNandU8(volatile uint8_t *ptr, uint8_t val) { ~(*ptr &= val); }
// *FORMAT-ON*
#endif // ifdef __GNUC__
