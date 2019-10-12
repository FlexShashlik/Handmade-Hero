union v2
{
    struct
    {
        real32 x, y;
    };
    real32 e[2];
};

inline v2
operator*(real32 a, v2 b)
{
    v2 result;

    result.x = a * b.x;
    result.y = a * b.y;

    return result;
}

inline v2
operator*(v2 b, real32 a)
{
    v2 result = a * b;

    return result;
}

inline v2 &
operator*=(v2 &a, real32 b)
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
