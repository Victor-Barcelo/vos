// DataTypes.h - Stub for StateMachine.h compatibility with VOS
#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef bool BOOL;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif // DATATYPES_H
