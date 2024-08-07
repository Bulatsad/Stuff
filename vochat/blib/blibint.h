#pragma once

#include <stdint.h>

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
