union v2
{
    struct
    {
        r32 x, y;
    };
    r32 e[2];
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
