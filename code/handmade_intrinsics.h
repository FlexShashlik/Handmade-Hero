//
// TODO: Convert to platform-efficient versions
// and get rid of the math.h
//

#include "math.h"

inline int32
RoundReal32ToInt32(real32 value)
{
    return (int32)(value + 0.5f);
}

inline uint32
RoundReal32ToUInt32(real32 value)
{
    return (uint32)(value + 0.5f);
}

inline int32
FloorReal32ToInt32(real32 value)
{
    return (int32)floorf(value);
}

inline int32
TruncateReal32ToInt32(real32 value)
{
    return (int32)value;
}

inline real32
Sin(real32 angle)
{
    return sinf(angle);
}

inline real32
Cos(real32 angle)
{
    return cosf(angle);
}

inline real32
Atan2(real32 y, real32 x)
{
    return atan2f(y, x);
}
