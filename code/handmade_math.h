union v2
{
    struct
    {
        r32 x, y;
    };
    r32 e[2];
};

union v3
{
    struct
    {
        r32 x, y, z;
    };
    struct
    {
        r32 r, g, b;
    };
    r32 e[3];
};

union v4
{
    struct
    {
        r32 x, y, z, w;
    };
    struct
    {
        r32 r, g, b, a;
    };
    r32 e[4];
};

inline v2
operator*(r32 a, v2 b)
{
    v2 result;

    result.x = a * b.x;
    result.y = a * b.y;

    return result;
}

inline v2
operator*(v2 b, r32 a)
{
    v2 result = a * b;
    return result;
}

inline v2 &
operator*=(v2 &a, r32 b)
{
    a = b * a;
    return a;
}

inline v2
operator-(v2 a)
{
    v2 result;

    result.x = -a.x;
    result.y = -a.y;

    return result;
}

inline v2
operator+(v2 a, v2 b)
{
    v2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

inline v2 &
operator+=(v2 &a, v2 b)
{
    a = a + b;
    return a;
}

inline v2
operator-(v2 a, v2 b)
{
    v2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline r32
Square(r32 value)
{
    r32 result = value * value;
    return result;
}

inline r32
Inner(v2 a, v2 b)
{
    r32 result = a.x * b.x + a.y * b.y;    
    return result;
}

inline r32
LengthSq(v2 a)
{
    r32 result = Inner(a, a);    
    return result;
}

struct rectangle2
{
    v2 min;
    v2 max;
};

inline v2
GetMinCorner(rectangle2 rect)
{
    v2 result = rect.min;
    return result;
}

inline v2
GetMaxCorner(rectangle2 rect)
{
    v2 result = rect.max;
    return result;
}

inline v2
GetCenter(rectangle2 rect)
{
    v2 result = 0.5f * (rect.min + rect.max);
    return result;
}

inline rectangle2
RectMinMax(v2 min, v2 max)
{
    rectangle2 result;

    result.min = min;
    result.max = max;

    return result;
}

inline rectangle2
RectMinDim(v2 min, v2 dim)
{
    rectangle2 result;

    result.min = min;
    result.max = min + dim;

    return result;
}

inline rectangle2
RectCenterHalfDim(v2 center, v2 halfDim)
{
    rectangle2 result;

    result.min = center - halfDim;
    result.max = center + halfDim;

    return result;
}

inline rectangle2
RectCenterDim(v2 center, v2 dim)
{
    rectangle2 result = RectCenterHalfDim(center, 0.5f * dim);
    return result;
}

inline b32
IsInRectangle(rectangle2 rect, v2 test)
{
    b32 result = (test.x >= rect.min.x &&
                  test.y >= rect.min.y &&
                  test.x < rect.max.x &&
                  test.y < rect.max.y);

    return result;
}
