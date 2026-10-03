/**
 * \file ls_bswap.h
 * \author Omar Merroun
 * \brief simple bswap wrapper
 * \version 0.1
 * \date 2026-10-03
 * 
 * \copyright Copyright (c) 2026
 */

#ifndef LS_BSWAP_H
#define LS_BSWAP_H

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
static inline uint16_t ls_bswap16(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t ls_bswap32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t ls_bswap64(uint64_t x) { return __builtin_bswap64(x); }
#else
static inline uint16_t ls_bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint32_t ls_bswap32(uint32_t x) {
    return  (x >> 24) | ((x >> 8) & 0x0000FF00u)
         | ((x << 8) & 0x00FF0000u) | (x << 24);
}
static inline uint64_t ls_bswap64(uint64_t x) {
    return ((uint64_t)ls_bswap32((uint32_t)x) << 32) | ls_bswap32((uint32_t)(x >> 32));
}
#endif

#define ls_bswap(x) _Generic((x), \
    uint16_t: ls_bswap16,         \
    uint32_t: ls_bswap32,         \
    uint64_t: ls_bswap64)(x)

#endif // LS_BSWAP_H
