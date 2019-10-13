//
// TODO: Convert to platform-efficient versions
// and get rid of the math.h
//

#include "math.h"

inline r32
AbsoluteValue(r32 value)
{
    return (r32)fabs(value);
}

inline i32
RoundR32ToI32(r32 value)
{
    return (i32)roundf(value);
}

inline u32
RoundR32ToU32(r32 value)
{
    return (u32)roundf(value);
}

inline i32
FloorR32ToI32(r32 value)
{
    return (i32)floorf(value);
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
    u32 index;
};

inline bit_scan_result
FindLeastSignificantSetBit(u32 value)
{
    bit_scan_result result = {};

#if COMPILER_MSVC
    result.isFound = _BitScanForward((unsigned long *)&result.index, value);
#else
    for(u32 test = 0; test < 32; test++)
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
