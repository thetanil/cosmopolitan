#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_MAP_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_MAP_H_
#include "libc/runtime/symbolic.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern const long MAP_32BIT;
extern const long MAP_ANON;
extern const long MAP_ANONYMOUS;
extern const long MAP_CONCEAL;
extern const long MAP_DENYWRITE;
extern const long MAP_EXECUTABLE;
extern const long MAP_FILE;
extern const long MAP_FIXED;
extern const long MAP_FIXED_NOREPLACE;
extern const long MAP_GROWSDOWN;
extern const long MAP_HASSEMAPHORE;
extern const long MAP_HUGETLB;
extern const long MAP_HUGE_MASK;
extern const long MAP_HUGE_SHIFT;
extern const long MAP_INHERIT;
extern const long MAP_LOCKED;
extern const long MAP_NONBLOCK;
extern const long MAP_NORESERVE;
extern const long MAP_NOSYNC;
extern const long MAP_POPULATE;
extern const long MAP_PRIVATE;
extern const long MAP_SHARED;
extern const long MAP_SHARED_VALIDATE;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define MAP_FILE    0
#define MAP_SHARED  1
#define MAP_PRIVATE 2
#define MAP_STACK   6
#define MAP_TYPE    15
#define MAP_FIXED   16

#define MAP_32BIT           SYMBOLIC(MAP_32BIT)
#define MAP_ANONYMOUS       SYMBOLIC(MAP_ANONYMOUS)
#define MAP_CONCEAL         SYMBOLIC(MAP_CONCEAL)
#define MAP_DENYWRITE       SYMBOLIC(MAP_DENYWRITE)
#define MAP_EXECUTABLE      SYMBOLIC(MAP_EXECUTABLE)
#define MAP_FIXED_NOREPLACE SYMBOLIC(MAP_FIXED_NOREPLACE)
#define MAP_GROWSDOWN       SYMBOLIC(MAP_GROWSDOWN)
#define MAP_HASSEMAPHORE    SYMBOLIC(MAP_HASSEMAPHORE)
#define MAP_HUGETLB         SYMBOLIC(MAP_HUGETLB)
#define MAP_HUGE_MASK       SYMBOLIC(MAP_HUGE_MASK)
#define MAP_HUGE_SHIFT      SYMBOLIC(MAP_HUGE_SHIFT)
#define MAP_INHERIT         SYMBOLIC(MAP_INHERIT)
#define MAP_LOCKED          SYMBOLIC(MAP_LOCKED)
#define MAP_NONBLOCK        SYMBOLIC(MAP_NONBLOCK)
#define MAP_NORESERVE       SYMBOLIC(MAP_NORESERVE)
#define MAP_NOSYNC          SYMBOLIC(MAP_NOSYNC)
#define MAP_POPULATE        SYMBOLIC(MAP_POPULATE)
#define MAP_SHARED_VALIDATE SYMBOLIC(MAP_SHARED_VALIDATE)

#define MAP_ANON   MAP_ANONYMOUS
#define MAP_NOCORE MAP_CONCEAL

#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_MAP_H_ */
