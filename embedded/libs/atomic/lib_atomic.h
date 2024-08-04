/**
 * @file lib_atomic.h
 * @brief Atomic operations library
 */


// NOTE: if other compilers are used, atomic access should be implemented for them here

#pragma once

#include "stdint.h"

#ifdef __GNUC__
#ifdef STM32F1
__attribute__((always_inline)) inline void atomicAddU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU64(volatile uint64_t *ptr, uint64_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU64(volatile uint64_t *ptr, uint64_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU32(volatile uint32_t *ptr, uint32_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU32(volatile uint32_t *ptr, uint32_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU16(volatile uint16_t *ptr, uint16_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU16(volatile uint16_t *ptr, uint16_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU8(volatile uint8_t *ptr, uint8_t val)      { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU8(volatile uint8_t *ptr, uint8_t val)    { (void)(~(*ptr &= val)); }
#else // ifdef STM32F1
__attribute__((always_inline)) inline void atomicAddU64(volatile uint64_t *ptr, uint64_t val)  { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU64(volatile uint64_t *ptr, uint64_t val)  { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU64(volatile uint64_t *ptr, uint64_t val)   { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU64(volatile uint64_t *ptr, uint64_t val)  { (void)__sync_fetch_and_xor(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU64(volatile uint64_t *ptr, uint64_t val)  { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU64(volatile uint64_t *ptr, uint64_t val) { (void)__sync_nand_and_fetch(ptr, val); }

__attribute__((always_inline)) inline void atomicAddU32(volatile uint32_t *ptr, uint32_t val)  { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU32(volatile uint32_t *ptr, uint32_t val)  { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU32(volatile uint32_t *ptr, uint32_t val)   { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU32(volatile uint32_t *ptr, uint32_t val)  { (void)__sync_fetch_and_xor(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU32(volatile uint32_t *ptr, uint32_t val)  { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU32(volatile uint32_t *ptr, uint32_t val) { (void)__sync_nand_and_fetch(ptr, val); }

__attribute__((always_inline)) inline void atomicAddU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU16(volatile uint16_t *ptr, uint16_t val)   { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_fetch_and_xor(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU16(volatile uint16_t *ptr, uint16_t val)  { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU16(volatile uint16_t *ptr, uint16_t val) { (void)__sync_nand_and_fetch(ptr, val); }

__attribute__((always_inline)) inline void atomicAddU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_add_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicSubU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_sub_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicOrU8(volatile uint8_t *ptr, uint8_t val)      { (void)__sync_or_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicXorU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_fetch_and_xor(ptr, val); }
__attribute__((always_inline)) inline void atomicAndU8(volatile uint8_t *ptr, uint8_t val)     { (void)__sync_and_and_fetch(ptr, val); }
__attribute__((always_inline)) inline void atomicNandU8(volatile uint8_t *ptr, uint8_t val)    { (void)__sync_nand_and_fetch(ptr, val); }
#endif
#else // ifdef __GNUC__
__attribute__((always_inline)) inline void atomicAddU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU64(volatile uint64_t *ptr, uint64_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU64(volatile uint64_t *ptr, uint64_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU64(volatile uint64_t *ptr, uint64_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU32(volatile uint32_t *ptr, uint32_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU32(volatile uint32_t *ptr, uint32_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU32(volatile uint32_t *ptr, uint32_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU16(volatile uint16_t *ptr, uint16_t val)   { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU16(volatile uint16_t *ptr, uint16_t val)  { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU16(volatile uint16_t *ptr, uint16_t val) { (void)(~(*ptr &= val)); }

__attribute__((always_inline)) inline void atomicAddU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr += val); }
__attribute__((always_inline)) inline void atomicSubU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr -= val); }
__attribute__((always_inline)) inline void atomicOrU8(volatile uint8_t *ptr, uint8_t val)      { (void)(*ptr |= val); }
__attribute__((always_inline)) inline void atomicXorU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr ^= val); }
__attribute__((always_inline)) inline void atomicAndU8(volatile uint8_t *ptr, uint8_t val)     { (void)(*ptr &= val); }
__attribute__((always_inline)) inline void atomicNandU8(volatile uint8_t *ptr, uint8_t val)    { (void)(~(*ptr &= val)); }
#endif
