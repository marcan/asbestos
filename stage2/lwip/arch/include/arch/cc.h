#ifndef CC_H
#define CC_H

#include "types.h"
#include "string.h"
#include "malloc.h"
#include "debug.h"

typedef u8 u8_t;
typedef u16 u16_t;
typedef u32 u32_t;
typedef s8 s8_t;
typedef s16 s16_t;
typedef s32 s32_t;

typedef u64 mem_ptr_t;

#define LWIP_PROVIDE_ERRNO 1

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END

#define BYTE_ORDER BIG_ENDIAN

#define LWIP_PLATFORM_DIAG(x) printf(x)
#define LWIP_PLATFORM_ASSERT(x) fatal(x)

#define mem_realloc realloc

#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

#endif
