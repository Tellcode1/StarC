#ifndef STARC_STD_H
#define STARC_STD_H

#include <stdint.h>
#include <stdlib.h>

#ifndef STAR_C_INTEGER_ALIASES_DEFINED
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

typedef unsigned char uchar;
typedef float f32;
typedef double f64;
#endif

#define TERM_YELLOW  "\x1b[33m"
#define TERM_GREEN   "\x1b[32m"
#define TERM_RED     "\x1b[31m"
#define TERM_RESET   "\033[0m"

#define STARC_MAJOR 0
#define STARC_MINOR 1
#define STARC_PATCH 1

#endif//STARC_STD_H