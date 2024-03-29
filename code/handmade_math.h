union v2
{
    struct
    {
        r32 x, y;
    };
    struct
    {
        r32 u, v;
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
        r32 u, v, w;
    };
    struct
    {
        r32 r, g, b;
    };
    struct
    {
        v2 xy;
        r32 ignored0_;
    };
    struct
    {
        r32 ignored1_;
        v2 yz;
    };
    struct
    {
        v2 uv;
        r32 ignored2_;
    };
    struct
    {        
        r32 ignored3_;
        v2 vw;
    };
    r32 e[3];
};

union v4
{
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                r32 x, y, z;
            };
        };

        r32 w;
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                r32 r, g, b;
            };
        };

        r32 a;
    };
    struct
    {
        v2 xy;
        r32 ignored0_;
        r32 ignored1_;
    };
    struct
    {
        r32 ignored2_;
        v2 yz;
        r32 ignored3_;
    };
    struct
    {
        r32 ignored4_;
        r32 ignored5_;
        v2 zw;
    };
    r32 e[4];
};

inline v2
V2i(i32 x, i32 y)
{
    v2 result = {(r32)x, (r32)y};
    return result;
}

inline v2
V2i(ui32 x, ui32 y)
{
    v2 result = {(r32)x, (r32)y};
    return result;
}

inline v3
V3(v2 xy, r32 z)
{
    v3 result;

    result.x = xy.x;
    result.y = xy.y;
    result.z = z;

    return result;
}

struct rectangle2
{
    v2 min;
    v2 max;
};

struct rectangle3
{
    v3 min;
    v3 max;
};

inline v4
V4(v3 xyz, r32 w)
{
    v4 result;

    result.xyz = xyz;
    result.w = w;

    return result;
}

//
// NOTE: Scalar operators
//

inline r32
Square(r32 value)
{
    r32 result = value * value;
    return result;
}

inline r32
Lerp(r32 a, r32 t, r32 b)
{
    r32 result = (1.0f - t) * a + t * b;
    return result;
}

inline r32
SafeRatioN(r32 numerator, r32 divisor, r32 n)
{
    r32 result = n;

    if(divisor != 0.0f)
    {
        result = numerator / divisor;
    }

    return result;
}

inline r32
SafeRatio0(r32 numerator, r32 divisor)
{
    r32 result = SafeRatioN(numerator, divisor, 0.0f);
    return result;
}

inline r32
SafeRatio1(r32 numerator, r32 divisor)
{
    r32 result = SafeRatioN(numerator, divisor, 1.0f);
    return result;
}

inline r32
Clamp(r32 min, r32 value, r32 max)
{
    r32 result = value;

    if(result < min)
    {
        result = min;
    }
    else if(result > max)
    {
        result = max;
    }

    return result;
}

inline r32
Clamp01(r32 value)
{
    r32 result = Clamp(0.0f, value, 1.0f);
    return result;
}

inline r32
Clamp01MapToRange(r32 min, r32 t, r32 max)
{
    r32 result = 0.0f;

    r32 range = max - min;
    if(range != 0)
    {
        result = Clamp01((t - min) / range);
    }

    return result;
}

//
// NOTE: v2 operations
//

inline v2
Perp(v2 a)
{
    v2 result = {-a.y, a.x};
    return result;
}

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

inline v2
Hadamard(v2 a, v2 b)
{
    v2 result = {a.x * b.x, a.y * b.y};
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

inline r32
Length(v2 a)
{
    r32 result = SqRt(LengthSq(a));
    return result;
}

inline v2
Clamp01(v2 value)
{
    v2 result;

    result.x = Clamp01(value.x);
    result.y = Clamp01(value.y);

    return result;
}

//
// NOTE: v3 operations
//

inline v3
operator*(r32 a, v3 b)
{
    v3 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;

    return result;
}

inline v3
operator*(v3 b, r32 a)
{
    v3 result = a * b;
    return result;
}

inline v3 &
operator*=(v3 &a, r32 b)
{
    a = b * a;
    return a;
}

inline v3
operator-(v3 a)
{
    v3 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;

    return result;
}

inline v3
operator+(v3 a, v3 b)
{
    v3 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;

    return result;
}

inline v3 &
operator+=(v3 &a, v3 b)
{
    a = a + b;
    return a;
}

inline v3
operator-(v3 a, v3 b)
{
    v3 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return result;
}

inline v3
Hadamard(v3 a, v3 b)
{
    v3 result = {a.x * b.x, a.y * b.y, a.z * b.z};
    return result;
}

inline r32
Inner(v3 a, v3 b)
{
    r32 result = a.x * b.x + a.y * b.y + a.z * b.z;    
    return result;
}

inline r32
LengthSq(v3 a)
{
    r32 result = Inner(a, a);    
    return result;
}

inline r32
Length(v3 a)
{
    r32 result = SqRt(LengthSq(a));
    return result;
}

inline v3
Normalize(v3 a)
{
    v3 result = a * (1.0f / Length(a));
    return result;
}

inline v3
Clamp01(v3 value)
{
    v3 result;

    result.x = Clamp01(value.x);
    result.y = Clamp01(value.y);
    result.z = Clamp01(value.z);

    return result;
}

inline v3
Lerp(v3 a, r32 t, v3 b)
{
    v3 result = (1.0f - t) * a + t * b;
    return result;
}

//
// NOTE: v4 operations
//

inline v4
operator*(r32 a, v4 b)
{
    v4 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;
    result.w = a * b.w;

    return result;
}

inline v4
operator*(v4 b, r32 a)
{
    v4 result = a * b;
    return result;
}

inline v4 &
operator*=(v4 &a, r32 b)
{
    a = b * a;
    return a;
}

inline v4
operator-(v4 a)
{
    v4 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;
    result.w = -a.w;

    return result;
}

inline v4
operator+(v4 a, v4 b)
{
    v4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;

    return result;
}

inline v4 &
operator+=(v4 &a, v4 b)
{
    a = a + b;
    return a;
}

inline v4
operator-(v4 a, v4 b)
{
    v4 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;

    return result;
}

inline v4
Hadamard(v4 a, v4 b)
{
    v4 result = {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
    return result;
}

inline r32
Inner(v4 a, v4 b)
{
    r32 result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;    
    return result;
}

inline r32
LengthSq(v4 a)
{
    r32 result = Inner(a, a);    
    return result;
}

inline r32
Length(v4 a)
{
    r32 result = SqRt(LengthSq(a));
    return result;
}

inline v4
Clamp01(v4 value)
{
    v4 result;

    result.x = Clamp01(value.x);
    result.y = Clamp01(value.y);
    result.z = Clamp01(value.z);
    result.w = Clamp01(value.w);

    return result;
}

inline v4
Lerp(v4 a, r32 t, v4 b)
{
    v4 result = (1.0f - t) * a + t * b;
    return result;
}

//
// NOTE: Rectangle2
//

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
GetDim(rectangle2 rect)
{
    v2 result = rect.max - rect.min;
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

inline rectangle2
AddRadiusTo(rectangle2 a, v2 radius)
{
    rectangle2 result;

    result.min = a.min - radius;
    result.max = a.max + radius;

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

inline v2
GetBarycentric(rectangle2 a, v2 pos)
{
    v2 result;

    result.x = SafeRatio0(pos.x - a.min.x, a.max.x - a.min.x);
    result.y = SafeRatio0(pos.y - a.min.y, a.max.y - a.min.y);
    
    return result;
}

//
// NOTE: Rectangle3
//

inline v3
GetMinCorner(rectangle3 rect)
{
    v3 result = rect.min;
    return result;
}

inline v3
GetMaxCorner(rectangle3 rect)
{
    v3 result = rect.max;
    return result;
}

inline v3
GetDim(rectangle3 rect)
{
    v3 result = rect.max - rect.min;
    return result;
}

inline v3
GetCenter(rectangle3 rect)
{
    v3 result = 0.5f * (rect.min + rect.max);
    return result;
}

inline rectangle3
RectMinMax(v3 min, v3 max)
{
    rectangle3 result;

    result.min = min;
    result.max = max;

    return result;
}

inline rectangle3
RectMinDim(v3 min, v3 dim)
{
    rectangle3 result;

    result.min = min;
    result.max = min + dim;

    return result;
}

inline rectangle3
RectCenterHalfDim(v3 center, v3 halfDim)
{
    rectangle3 result;

    result.min = center - halfDim;
    result.max = center + halfDim;

    return result;
}

inline rectangle3
RectCenterDim(v3 center, v3 dim)
{
    rectangle3 result = RectCenterHalfDim(center, 0.5f * dim);
    return result;
}

inline rectangle3
AddRadiusTo(rectangle3 a, v3 radius)
{
    rectangle3 result;

    result.min = a.min - radius;
    result.max = a.max + radius;

    return result;
}

inline rectangle3
Offset(rectangle3 a, v3 offset)
{
    rectangle3 result;

    result.min = a.min + offset;
    result.max = a.max + offset;
    
    return result;
}

inline b32
IsInRectangle(rectangle3 rect, v3 test)
{
    b32 result = (test.x >= rect.min.x &&
                  test.y >= rect.min.y &&
                  test.z >= rect.min.z &&
                  test.x < rect.max.x &&
                  test.y < rect.max.y &&
                  test.z < rect.max.z);

    return result;
}

inline b32
RectanglesIntersect(rectangle3 a, rectangle3 b)
{
    b32 result = !(b.max.x <= a.min.x ||
                   b.min.x >= a.max.x ||
                   b.max.y <= a.min.y ||
                   b.min.y >= a.max.y ||
                   b.max.z <= a.min.z ||
                   b.min.z >= a.max.z);
    
    return result;
}

inline v3
GetBarycentric(rectangle3 a, v3 pos)
{
    v3 result;

    result.x = SafeRatio0(pos.x - a.min.x, a.max.x - a.min.x);
    result.y = SafeRatio0(pos.y - a.min.y, a.max.y - a.min.y);
    result.z = SafeRatio0(pos.z - a.min.z, a.max.z - a.min.z);

    return result;
}

inline rectangle2
ToRectangleXY(rectangle3 a)
{
    rectangle2 result;

    result.min = a.min.xy;
    result.max = a.max.xy;
    
    return result;
}

//
//
//

struct rectangle2i
{
    i32 minX, minY;
    i32 maxX, maxY;
};

inline rectangle2i
Intersect(rectangle2i a, rectangle2i b)
{
    rectangle2i result;
    
    result.minX = (a.minX < b.minX) ? b.minX : a.minX;
    result.minY = (a.minY < b.minY) ? b.minY : a.minY;
    result.maxX = (a.maxX > b.maxX) ? b.maxX : a.maxX;
    result.maxY = (a.maxY > b.maxY) ? b.maxY : a.maxY;

    return result;
}

inline rectangle2i
Union(rectangle2i a, rectangle2i b)
{
    rectangle2i result;
    
    result.minX = (a.minX < b.minX) ? a.minX : b.minX;
    result.minY = (a.minY < b.minY) ? a.minY : b.minY;
    result.maxX = (a.maxX > b.maxX) ? a.maxX : b.maxX;
    result.maxY = (a.maxY > b.maxY) ? a.maxY : b.maxY;

    return result;
}

inline ui32
GetClampedRectArea(rectangle2i a)
{
    i32 width = (a.maxX - a.minX);
    i32 height = (a.maxY - a.minY);
    i32 result = 0;
    if(width > 0 && height > 0)
    {
        result = width * height;
    }

    return result;
}

inline b32
HasArea(rectangle2i a)
{
    b32 result = (a.minX < a.maxX) && (a.minY < a.maxY);
    return result;
}

inline rectangle2i
InvertedInfinityRectangle(void)
{
    rectangle2i result;

    result.minX = result.minY = INT_MAX;
    result.maxX = result.maxY = -INT_MAX;

    return result;
}

inline v4
SRGB255ToLinear1(v4 c)
{
    v4 result;

    r32 inv255 = 1.0f / 255.0f;
    
    result.r = Square(inv255 * c.r);
    result.g = Square(inv255 * c.g);
    result.b = Square(inv255 * c.b);
    result.a = inv255 * c.a;

    return result;
}

inline v4
Linear1ToSRGB255(v4 c)
{
    v4 result;
    
    result.r = 255.0f * SqRt(c.r);
    result.g = 255.0f * SqRt(c.g);
    result.b = 255.0f * SqRt(c.b);
    result.a = 255.0f * c.a;

    return result;
}
