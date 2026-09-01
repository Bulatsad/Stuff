#pragma once

#include <stdint.h>
#include <stddef.h>
#include <climits>

static_assert(sizeof(uint8_t)  == 1, "incorrect base datatype size");
static_assert(sizeof(uint16_t) == 2, "incorrect base datatype size");
static_assert(sizeof(uint32_t) == 4, "incorrect base datatype size");
static_assert(sizeof(uint64_t) == 8, "incorrect base datatype size");

static_assert(sizeof(int8_t)   == 1, "incorrect base datatype size");
static_assert(sizeof(int16_t)  == 2, "incorrect base datatype size");
static_assert(sizeof(int32_t)  == 4, "incorrect base datatype size");
static_assert(sizeof(int64_t)  == 8, "incorrect base datatype size");

typedef uint8_t  buint8;
typedef uint16_t buint16;
typedef uint32_t buint32;
typedef uint64_t buint64;

typedef int8_t   bint8;
typedef int16_t  bint16;
typedef int32_t  bint32;
typedef int64_t  bint64;

constexpr bint8  bint8Max  = UINT8_MAX;
constexpr bint16 bint16Max = UINT8_MAX;
constexpr bint32 bint32Max = UINT8_MAX;
constexpr bint64 bint64Max = UINT8_MAX;

constexpr buint8  buint8Max  = UINT8_MAX;
constexpr buint16 buint16Max = UINT16_MAX;
constexpr buint32 buint32Max = UINT32_MAX;
constexpr buint64 buint64Max = UINT64_MAX;
