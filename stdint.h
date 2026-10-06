/* Целые типы для pdp11-aout-gcc (int — 16 бит) и для хостовой сборки */
#ifndef STDINT_H
#define STDINT_H
#ifdef HOST
#include_next <stdint.h>
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;              /* в xgcc/include/stdint.h здесь int, т.е. 16 бит */
typedef unsigned long uint32_t;
typedef int16_t intptr_t;
typedef uint16_t uintptr_t;
#define INT8_MIN   (-128)
#define INT8_MAX   (127)
#define UINT8_MAX  (255)
#define INT16_MIN  (-32768)
#define INT16_MAX  (32767)
#define UINT16_MAX (65535u)
#define INT32_MIN  (-2147483647L - 1)
#define INT32_MAX  (2147483647L)
#define UINT32_MAX (4294967295UL)
#endif
#endif
