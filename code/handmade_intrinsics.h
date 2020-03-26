//
// TODO: Convert to platform-efficient versions
// and get rid of the math.h
//

#include "math.h"

#if COMPILER_MSVC
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline ui32 AtomicCompareExchangeUI32(ui32 volatile *value, ui32 newValue, ui32 expected)
{
    ui32 result = _InterlockedCompareExchange((long *)value, newValue, expected);
    return result;
}
#elif COMPILER_LLVM
#define CompletePreviousWritesBeforeFutureWrites asm_volatile("" ::: "memory")
inline ui32 AtomicCompareExchangeUI32(ui32 volatile *value, ui32 newValue, ui32 expected)
{
    ui32 result = __sync_val_compare_and_swap((long *)value, expected, newValue);
    return result;
}
#else
// TODO: Need GCC/LLVM equivalents!
#endif

inline i32
SignOf(i32 value)
{
    i32 result = (value >= 0) ? 1 : -1;
    return result;
}

inline r32
SignOf(r32 value)
{
    r32 result = (value >= 0) ? 1.0f : -1.0f;
    return result;
}

inline r32
SqRt(r32 value)
{
    r32 result = sqrtf(value);
    return result;
}

inline r32
AbsoluteValue(r32 value)
{
    return (r32)fabs(value);
}

inline ui32
RotateLeft(ui32 value, i32 amount)
{
#if COMPILER_MSVC
    ui32 result = _rotl(value, amount);
#else
    amount &= 31;
    ui32 result = ((value << amount) | (value >> (32 - amount)));
#endif
    
    return result;
}

inline ui32
RotateRight(ui32 value, i32 amount)
{
#if COMPILER_MSVC
    ui32 result = _rotr(value, amount);
#else
    amount &= 31;
    ui32 result = ((value >> amount) | (value << (32 - amount)));
#endif
    
    return result;
}

inline i32
RoundR32ToI32(r32 value)
{
    i32 result = (i32)roundf(value);

    return result;
}

inline ui32
RoundR32ToUI32(r32 value)
{
    ui32 result = (ui32)roundf(value);
    
    return result;
}

inline i32
FloorR32ToI32(r32 value)
{
    i32 result = (i32)floorf(value);
    
    return result;
}

inline i32
CeilR32ToI32(r32 value)
{
    i32 result = (i32)ceilf(value);

    return result;
}

inline i32
TruncateR32ToI32(r32 value)
{
    return (i32)value;
}

inline r32
Sin(r32 angle)
{
    return sinf(angle);
}

inline r32
Cos(r32 angle)
{
    return cosf(angle);
}

inline r32
Atan2(r32 y, r32 x)
{
    return atan2f(y, x);
}

struct bit_scan_result
{
    b32 isFound;
    ui32 index;
};

inline bit_scan_result
FindLeastSignificantSetBit(ui32 value)
{
    bit_scan_result result = {};

#if COMPILER_MSVC
    result.isFound = _BitScanForward((unsigned long *)&result.index, value);
#else
    for(ui32 test = 0; test < 32; test++)
    {
        if(value & (1 << test))
        {
            result.isFound = true;
            result.index = test;
            break;
        }
    }
#endif

    return result;
}
