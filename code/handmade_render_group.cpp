inline v4
Unpack4x8(ui32 packed)
{
    v4 result =
        {
            (r32)((packed >> 16) & 0xFF),
            (r32)((packed >> 8) & 0xFF),
            (r32)((packed >> 0) & 0xFF),
            (r32)((packed >> 24) & 0xFF)
        };
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

inline v4
UnscaleAndBiasNormal(v4 normal)
{
    v4 result;

    r32 inv255 = 1.0f / 255.0f;

    result.x = -1.0f + 2.0f * (inv255 * normal.x);
    result.y = -1.0f + 2.0f * (inv255 * normal.y);
    result.z = -1.0f + 2.0f * (inv255 * normal.z);

    result.w = inv255 * normal.w;
    
    return result;
}

internal void
DrawRectangle
(
    loaded_bitmap *buffer,
    v2 vMin, v2 vMax,
    v4 color, rectangle2i clipRect, b32 even
)
{
    r32 r = color.r;
    r32 g = color.g;
    r32 b = color.b;
    r32 a = color.a;

    rectangle2i fillRect;
    fillRect.minX = RoundR32ToI32(vMin.x);
    fillRect.minY = RoundR32ToI32(vMin.y);
    fillRect.maxX = RoundR32ToI32(vMax.x);
    fillRect.maxY = RoundR32ToI32(vMax.y);

    fillRect = Intersect(fillRect, clipRect);
    
    if(!even == (fillRect.minY & 1))
    {
        fillRect.minY += 1;
    }
    
    ui32 color32 =
        (
            RoundR32ToUI32(a * 255.0f) << 24 |
            RoundR32ToUI32(r * 255.0f) << 16 |
            RoundR32ToUI32(g * 255.0f) << 8 |
            RoundR32ToUI32(b * 255.0f) << 0
        );

    ui8 *row = ((ui8 *)buffer->memory +
                fillRect.minX * BITMAP_BYTES_PER_PIXEL +
                fillRect.minY * buffer->pitch);
    
    for(i32 y = fillRect.minY; y < fillRect.maxY; y += 2)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = fillRect.minX; x < fillRect.maxX; x++)
        {        
            *pixel++ = color32;            
        }
        
        row += 2 * buffer->pitch;
    }
}

struct bilinear_sample
{
    ui32 a, b, c, d;
};

inline bilinear_sample
BilinearSample(loaded_bitmap *texture, i32 x, i32 y)
{
    bilinear_sample result;
    
    ui8 *texelPtr = (((ui8 *)texture->memory) +
                     y * texture->pitch +
                     x * sizeof(ui32));
    result.a = *(ui32 *)(texelPtr);
    result.b = *(ui32 *)(texelPtr + sizeof(ui32));
    result.c = *(ui32 *)(texelPtr + texture->pitch);
    result.d = *(ui32 *)(texelPtr + texture->pitch + sizeof(ui32));

    return result;
}

inline v4
SRGBBilinearBlend(bilinear_sample texelSample, r32 fX, r32 fY)
{
    v4 texelA = Unpack4x8(texelSample.a);
    v4 texelB = Unpack4x8(texelSample.b);
    v4 texelC = Unpack4x8(texelSample.c);
    v4 texelD = Unpack4x8(texelSample.d);
                
    // NOTE: Go from sRGB to "linear" brightness space
    texelA = SRGB255ToLinear1(texelA);
    texelB = SRGB255ToLinear1(texelB);
    texelC = SRGB255ToLinear1(texelC);
    texelD = SRGB255ToLinear1(texelD);
                
    v4 texel = Lerp(Lerp(texelA, fX, texelB),
                    fY,
                    Lerp(texelC, fX, texelD));

    return texel;
}

inline v3
SampleEnvironmentMap
(
    v2 screenSpaceUV,
    v3 sampleDirection, r32 roughness, environment_map *map,
    r32 distanceFromMapInZ
)
{
    /*
       NOTE:

       screenSpaceUV tells us where the ray is being casted _from_ in
       normalized screen coordinates.

       sampleDirection tells us what direction the cast is going - it
       does not have to be normalized.

       roughness says which LODs of map we sample from.

       distanceFromMapInZ says how far the map is from the sample
       point in _z_, given in meters.
     */

    // NOTE: Pick which LOD to sample from
    ui32 lodIndex = (ui32)(roughness * (r32)(ArrayCount(map->lod - 1) + 0.5f));
    Assert(lodIndex < ArrayCount(map->lod));

    loaded_bitmap *lod = &map->lod[lodIndex];

    // NOTE: Compute the distance to the map and the scaling factor
    // for meters-to-UVs.
    r32 uvPerMeter = 0.1f;
    r32 c = (uvPerMeter * distanceFromMapInZ) / sampleDirection.y;
    v2 offset = c * v2{sampleDirection.x, sampleDirection.z};

    // NOTE: Find the intersection point.
    v2 uv = screenSpaceUV + offset;

    // NOTE: Clamp to the valid range.
    uv.x = Clamp01(uv.x);
    uv.y = Clamp01(uv.y);

    // NOTE: Bilinear sample.
    r32 tX = (uv.x * (r32)(lod->width - 2));
    r32 tY = (uv.y * (r32)(lod->height - 2));
    
    i32 x = (i32)tX;
    i32 y = (i32)tY;

    r32 fX = tX - (r32)x;
    r32 fY = tY - (r32)y;

    Assert(x >= 0 && x < lod->width);
    Assert(y >= 0 && y < lod->height);

#if 0
    // NOTE: Turn on to see where in map you're sampling from
    ui8 *texelPtr = ((ui8 *)lod->memory) + y * lod->pitch + x * sizeof(ui32);
    *(ui32 *)texelPtr = 0xFFFFFFFF;
#endif
    
    bilinear_sample sample = BilinearSample(lod, x, y);
    v3 result = SRGBBilinearBlend(sample, fX, fY).xyz;
    
    return result;
}

internal void
DrawRectangleSlowly
(
    loaded_bitmap *buffer,
    v2 origin, v2 xAxis, v2 yAxis,
    v4 color,
    loaded_bitmap *texture, loaded_bitmap *normalMap,
    environment_map *top,
    environment_map *middle,
    environment_map *bottom,
    r32 pixelsToMeters
)
{
    BEGIN_TIMED_BLOCK(DrawRectangleSlowly);
    
    // NOTE: Premultiply color up front
    color.rgb *= color.a;
    
    r32 xAxisLength = Length(xAxis);
    r32 yAxisLength = Length(yAxis);
    
    v2 nXAxis = (yAxisLength / xAxisLength) * xAxis;
    v2 nYAxis = (xAxisLength / yAxisLength) * yAxis;
    r32 nZScale = 0.5f * (xAxisLength + yAxisLength);
                    
    r32 invXAxisLengthSq = 1.0f / LengthSq(xAxis);
    r32 invYAxisLengthSq = 1.0f / LengthSq(yAxis);
    
    ui32 color32 =
        (
            RoundR32ToUI32(color.a * 255.0f) << 24 |
            RoundR32ToUI32(color.r * 255.0f) << 16 |
            RoundR32ToUI32(color.g * 255.0f) << 8 |
            RoundR32ToUI32(color.b * 255.0f) << 0
        );

    i32 widthMax = buffer->width - 1;
    i32 heightMax = buffer->height - 1;

    r32 invWidthMax = 1.0f / (r32)widthMax;
    r32 invHeightMax = 1.0f / (r32)heightMax;
    
    r32 originZ = 0.0f;
    r32 originY = (origin + 0.5f * xAxis + 0.5f * yAxis).y;
    r32 fixedCastY = invHeightMax * originY;
    
    i32 yMin = heightMax;
    i32 yMax = 0;
    i32 xMin = widthMax;
    i32 xMax = 0;
    
    v2 p[4] =
        {
            origin,
            origin + xAxis,
            origin + xAxis + yAxis,
            origin + yAxis
        };
    for(i32 pIndex = 0;
        pIndex < ArrayCount(p);
        pIndex++)
    {
        v2 testP = p[pIndex];
        i32 floorX = FloorR32ToI32(testP.x);
        i32 ceilX = CeilR32ToI32(testP.x);
        i32 floorY = FloorR32ToI32(testP.y);
        i32 ceilY = CeilR32ToI32(testP.y);

        if(xMin > floorX) {xMin = floorX;}
        if(yMin > floorY) {yMin = floorY;}
        if(xMax < ceilX) {xMax = ceilX;}
        if(yMax < ceilY) {yMax = ceilY;}
    }

    if(xMin < 0) {xMin = 0;}
    if(yMin < 0) {yMin = 0;}
    if(xMax > widthMax) {xMax = widthMax;}
    if(yMax > heightMax) {yMax = heightMax;}
    
    ui8 *row = ((ui8 *)buffer->memory +
                xMin * BITMAP_BYTES_PER_PIXEL +
                yMin * buffer->pitch);

    BEGIN_TIMED_BLOCK(ProcessPixel);
    for(i32 y = yMin; y <= yMax; y++)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = xMin; x <= xMax; x++)
        {
            BEGIN_TIMED_BLOCK(TestPixel);
            
#if 1
            v2 pixelP = V2i(x, y);
            v2 d = pixelP - origin;
            
            r32 edge0 = Inner(d, -Perp(xAxis));
            r32 edge1 = Inner(d - xAxis, -Perp(yAxis));
            r32 edge2 = Inner(d - xAxis - yAxis, Perp(xAxis));
            r32 edge3 = Inner(d - yAxis, Perp(yAxis));
            
            if(edge0 < 0 &&
               edge1 < 0 &&
               edge2 < 0 &&
               edge3 < 0)
            {
#if 1
                v2 screenSpaceUV = {invWidthMax * (r32)x, fixedCastY};
                r32 zDiff = pixelsToMeters * ((r32)y - originY);
#else
                v2 screenSpaceUV = {invWidthMax * (r32)x, invHeightMax * (r32)y};
                r32 zDiff = 0.0f;
#endif
                
                r32 u = invXAxisLengthSq * Inner(d, xAxis);
                r32 v = invYAxisLengthSq * Inner(d, yAxis);

#if 0
                // TODO: SSE clamping
                Assert(u >= 0.0f && u <= 1.0f);
                Assert(v >= 0.0f && v <= 1.0f);
#endif
                
                r32 tX = (u * (r32)(texture->width - 2));
                r32 tY = (v * (r32)(texture->height - 2));
                
                i32 x = (i32)tX;
                i32 y = (i32)tY;

                r32 fX = tX - (r32)x;
                r32 fY = tY - (r32)y;

                Assert(x >= 0 && x < texture->width);
                Assert(y >= 0 && y < texture->height);

                bilinear_sample texelSample = BilinearSample(texture, x, y);
                v4 texel = SRGBBilinearBlend(texelSample, fX, fY);                

#if 0
                if(normalMap)
                {
                    bilinear_sample normalSample = BilinearSample(normalMap, x, y);

                    v4 normalA = Unpack4x8(normalSample.a);
                    v4 normalB = Unpack4x8(normalSample.b);
                    v4 normalC = Unpack4x8(normalSample.c);
                    v4 normalD = Unpack4x8(normalSample.d);
                
                    v4 normal = Lerp(Lerp(normalA, fX, normalB),
                                     fY,
                                     Lerp(normalC, fX, normalD));

                    normal = UnscaleAndBiasNormal(normal);

                    normal.xy = normal.x * nXAxis + normal.y * nYAxis;
                    normal.z *= nZScale;
                    normal.xyz = Normalize(normal.xyz);

                    // NOTE: The eye vector is always assumed to be {0, 0, 1}
                    // Simplified version of the reflection -e + 2 e^T N N
                    v3 bounceDirection = 2.0f * normal.z * normal.xyz;
                    bounceDirection.z -= 1.0f;

                    // TODO: We need to support two mappings, one for
                    // top-down view (which we don't do now) and one
                    // for sideways, which is what's happening here.
                    bounceDirection.z = -bounceDirection.z;

                    environment_map *farMap = 0;
                    r32 posZ = originZ + zDiff;
                    r32 mapZ = 2.0f;
                    r32 tEnvMap = bounceDirection.y;
                    r32 tFarMap = 0;
                    if(tEnvMap < -0.5f)
                    {
                        farMap = bottom;
                        tFarMap = -1.0f - 2.0f * tEnvMap;
                    }
                    else if(tEnvMap > 0.5f)
                    {
                        farMap = top;
                        tFarMap = 2.0f * (tEnvMap - 0.5f);
                    }

                    tFarMap *= tFarMap;
                    tFarMap *= tFarMap;

                    v3 lightColor = {0, 0, 0}; //SampleEnvironmentMap(screenSpaceUV, normal.xyz, normal.w, middle);
                    if(farMap)
                    {
                        r32 distanceFromMapInZ = farMap->posZ - posZ;
                        
                        v3 farMapColor = SampleEnvironmentMap
                            (
                                screenSpaceUV, bounceDirection,
                                normal.w, farMap, distanceFromMapInZ
                            );
                        lightColor = Lerp(lightColor, tFarMap, farMapColor);
                    }

                    texel.rgb = texel.rgb + texel.a * lightColor;

#if 0
                    // NOTE: Draws the bounce direction
                    texel.rgb = v3{0.5f, 0.5f, 0.5f} + 0.5f * bounceDirection;
                    texel.rgb *= texel.a;
#endif
                }
#endif
                
                texel = Hadamard(texel, color);
                texel.rgb = Clamp01(texel.rgb);

                v4 dest =
                    {
                        (r32)((*pixel >> 16) & 0xFF),
                        (r32)((*pixel >> 8) & 0xFF),
                        (r32)((*pixel >> 0) & 0xFF),
                        (r32)((*pixel >> 24) & 0xFF)
                    };
                
                // NOTE: Go from sRGB to "linear" brightness space
                dest = SRGB255ToLinear1(dest);
                
                v4 blended = (1.0f - texel.a) * dest + texel;

                // NOTE: Go from "linear" brightness space to sRGB space
                v4 blended255 = Linear1ToSRGB255(blended);
                
                *pixel = ((ui32)(blended255.a + 0.5f) << 24|
                          (ui32)(blended255.r + 0.5f) << 16|
                          (ui32)(blended255.g + 0.5f) << 8 |
                          (ui32)(blended255.b + 0.5f) << 0);
            }
#else
            *pixel = color32;
#endif
            pixel++;
        }
        
        row += buffer->pitch;
    }
    END_TIMED_BLOCK_COUNTED(ProcessPixel, (xMax - xMin + 1) * (yMax - yMin + 1));

    END_TIMED_BLOCK(DrawRectangleSlowly);
}

struct counts
{
    int mm_add_ps;
    int mm_sub_ps;
    int mm_mul_ps;
    int mm_castps_si128;
    int mm_and_ps;
    int mm_or_si128;
    int mm_cmpge_ps;
    int mm_cmple_ps;
    int mm_min_ps;
    int mm_max_ps;
    int mm_cvttps_epi32;
    int mm_cvtps_epi32;
    int mm_cvtepi32_ps;
    int mm_and_si128;
    int mm_andnot_si128;
    int mm_srli_epi32;
    int mm_slli_epi32;
    int mm_sqrt_ps;
};

#if 0
#include <iacaMarks.h>
#else
#define IACA_VC64_START
#define IACA_VC64_END
#endif

internal void
DrawRectangleQuickly
(
    loaded_bitmap *buffer,
    v2 origin, v2 xAxis, v2 yAxis,
    v4 color, loaded_bitmap *texture,
    r32 pixelsToMeters, rectangle2i clipRect, b32 even
)
{
    BEGIN_TIMED_BLOCK(DrawRectangleQuickly);
    
    // NOTE: Premultiply color up front
    color.rgb *= color.a;
    
    r32 xAxisLength = Length(xAxis);
    r32 yAxisLength = Length(yAxis);
    
    //v2 nXAxis = (yAxisLength / xAxisLength) * xAxis;
    //v2 nYAxis = (xAxisLength / yAxisLength) * yAxis;
    r32 nZScale = 0.5f * (xAxisLength + yAxisLength);
                    
    r32 invXAxisLengthSq = 1.0f / LengthSq(xAxis);
    r32 invYAxisLengthSq = 1.0f / LengthSq(yAxis);
    
    rectangle2i fillRect = InvertedInfinityRectangle();
    
    v2 p[4] =
        {
            origin,
            origin + xAxis,
            origin + xAxis + yAxis,
            origin + yAxis
        };
    for(i32 pIndex = 0;
        pIndex < ArrayCount(p);
        pIndex++)
    {
        v2 testP = p[pIndex];
        i32 floorX = FloorR32ToI32(testP.x);
        i32 ceilX = CeilR32ToI32(testP.x) + 1;
        i32 floorY = FloorR32ToI32(testP.y);
        i32 ceilY = CeilR32ToI32(testP.y) + 1;

        if(fillRect.minX > floorX) {fillRect.minX = floorX;}
        if(fillRect.minY > floorY) {fillRect.minY = floorY;}
        if(fillRect.maxX < ceilX) {fillRect.maxX = ceilX;}
        if(fillRect.maxY < ceilY) {fillRect.maxY = ceilY;}
    }

    fillRect = Intersect(clipRect, fillRect);

    if(!even == (fillRect.minY & 1))
    {
        fillRect.minY += 1;
    }

    if(HasArea(fillRect))
    {
        __m128i startupClipMask = _mm_set1_epi8(-1); 
    
        i32 fillWidth = fillRect.maxX - fillRect.minX;
        i32 fillWidthAlign = fillWidth & 3;
        if(fillWidthAlign > 0)
        {
            i32 adjustment = (4 - fillWidthAlign);
            switch(adjustment)
            {
                case 1: {startupClipMask = _mm_slli_si128(startupClipMask, 1 * 4);} break;
                case 2: {startupClipMask = _mm_slli_si128(startupClipMask, 2 * 4);} break;
                case 3: {startupClipMask = _mm_slli_si128(startupClipMask, 3 * 4);} break;
            }
        
            fillWidth += adjustment;
            fillRect.minX = fillRect.maxX - fillWidth;
        }
    
        v2 nXAxis = invXAxisLengthSq * xAxis;
        v2 nYAxis = invYAxisLengthSq * yAxis;

        r32 inv255 = 1.0f / 255.0f;
        __m128 inv255_4x = _mm_set1_ps(inv255);

        r32 normalizeC = 1.0f / 255.0f;
        r32 normalizeSqC = 1.0f / Square(255.0f);

        __m128 zero = _mm_set1_ps(0.0f);
        __m128 four_4x = _mm_set1_ps(4.0f);
        __m128 one = _mm_set1_ps(1.0f);
        __m128 one255_4x = _mm_set1_ps(255.0f);
        __m128 colorr_4x = _mm_set1_ps(color.r);
        __m128 colorg_4x = _mm_set1_ps(color.g);
        __m128 colorb_4x = _mm_set1_ps(color.b);
        __m128 colora_4x = _mm_set1_ps(color.a);
        __m128 nXAxisx_4x = _mm_set1_ps(nXAxis.x);
        __m128 nXAxisy_4x = _mm_set1_ps(nXAxis.y);
        __m128 nYAxisx_4x = _mm_set1_ps(nYAxis.x);
        __m128 nYAxisy_4x = _mm_set1_ps(nYAxis.y);
        __m128 originx_4x = _mm_set1_ps(origin.x);
        __m128 originy_4x = _mm_set1_ps(origin.y);
        __m128 maxColorValue = _mm_set1_ps(255.0f * 255.0f);
        __m128i texturePitch_4x = _mm_set1_epi32(texture->pitch);
    
        __m128 widthM2 = _mm_set1_ps((r32)(texture->width - 2));
        __m128 heightM2 = _mm_set1_ps((r32)(texture->height - 2));

        __m128i maskFF = _mm_set1_epi32(0xFF);
        __m128i maskFFFF = _mm_set1_epi32(0xFFFF);
        __m128i maskFF00FF = _mm_set1_epi32(0x00FF00FF);
    
        ui8 *row = ((ui8 *)buffer->memory +
                    fillRect.minX * BITMAP_BYTES_PER_PIXEL +
                    fillRect.minY * buffer->pitch);
        i32 rowAdvance = 2 * buffer->pitch;

        void *textureMemory = texture->memory;
        i32 texturePitch = texture->pitch;

        i32 minY = fillRect.minY;
        i32 minX = fillRect.minX;
        i32 maxY = fillRect.maxY;
        i32 maxX = fillRect.maxX;
    
        BEGIN_TIMED_BLOCK(ProcessPixel);
        for(i32 y = minY; y < maxY; y += 2)
        {
            __m128 pixelPy = _mm_set1_ps((r32)y);
            pixelPy = _mm_sub_ps(pixelPy, originy_4x);
        
            __m128 pixelPx = _mm_set_ps((r32)(fillRect.minX + 3),
                                        (r32)(fillRect.minX + 2),
                                        (r32)(fillRect.minX + 1),
                                        (r32)(fillRect.minX + 0));
            pixelPx = _mm_sub_ps(pixelPx, originx_4x);

            __m128i clipMask = startupClipMask;
        
            ui32 *pixel = (ui32 *)row;
            for(i32 xI = minX; xI < maxX; xI += 4)
            {
#define mmSquare(a) _mm_mul_ps(a, a)
#define M(a, i) ((r32 *)&a)[i]
#define Mi(a, i) ((ui32 *)&a)[i]

                IACA_VC64_START;
            
                __m128 u = _mm_add_ps(_mm_mul_ps(pixelPx, nXAxisx_4x), _mm_mul_ps(pixelPy, nXAxisy_4x));
                __m128 v = _mm_add_ps(_mm_mul_ps(pixelPx, nYAxisx_4x), _mm_mul_ps(pixelPy, nYAxisy_4x));
                __m128i writeMask = _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(u, zero),
                                                                           _mm_cmple_ps(u, one)),
                                                                _mm_and_ps(_mm_cmpge_ps(v, zero),
                                                                           _mm_cmple_ps(v, one))));

                writeMask = _mm_and_si128(writeMask, clipMask);
            
                // TODO: Re-check this later
                //if(_mm_movemask_epi8(writeMask))
                {
                    __m128i originalDest = _mm_loadu_si128((__m128i *)pixel);
            
                    u = _mm_min_ps(_mm_max_ps(u, zero), one);
                    v = _mm_min_ps(_mm_max_ps(v, zero), one);

                    __m128 tX = _mm_mul_ps(u, widthM2);
                    __m128 tY = _mm_mul_ps(v, heightM2);

                    __m128i fetchX_4x = _mm_cvttps_epi32(tX);
                    __m128i fetchY_4x = _mm_cvttps_epi32(tY);
            
                    __m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(fetchX_4x));
                    __m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(fetchY_4x));

                    fetchX_4x = _mm_slli_epi32(fetchX_4x, 2);
                    fetchY_4x = _mm_or_si128(_mm_mullo_epi16(fetchY_4x, texturePitch_4x),
                                             _mm_slli_epi32(_mm_mulhi_epi16(fetchY_4x, texturePitch_4x), 16));
                    __m128i fetch_4x = _mm_add_epi32(fetchX_4x, fetchY_4x);
                
                    i32 fetch0 = Mi(fetch_4x, 0);
                    i32 fetch1 = Mi(fetch_4x, 1);
                    i32 fetch2 = Mi(fetch_4x, 2);
                    i32 fetch3 = Mi(fetch_4x, 3);
                
                    ui8 *texelPtr0 = ((ui8 *)textureMemory) + fetch0;
                    ui8 *texelPtr1 = ((ui8 *)textureMemory) + fetch1;
                    ui8 *texelPtr2 = ((ui8 *)textureMemory) + fetch2;
                    ui8 *texelPtr3 = ((ui8 *)textureMemory) + fetch3;

                    __m128i sampleA = _mm_setr_epi32(*(ui32 *)(texelPtr0),
                                                     *(ui32 *)(texelPtr1),
                                                     *(ui32 *)(texelPtr2),
                                                     *(ui32 *)(texelPtr3));
                
                    __m128i sampleB = _mm_setr_epi32(*(ui32 *)(texelPtr0 + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr1 + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr2 + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr3 + sizeof(ui32)));

                    __m128i sampleC = _mm_setr_epi32(*(ui32 *)(texelPtr0 + texturePitch),
                                                     *(ui32 *)(texelPtr1 + texturePitch),
                                                     *(ui32 *)(texelPtr2 + texturePitch),
                                                     *(ui32 *)(texelPtr3 + texturePitch));
                
                    __m128i sampleD = _mm_setr_epi32(*(ui32 *)(texelPtr0 + texturePitch + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr1 + texturePitch + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr2 + texturePitch + sizeof(ui32)),
                                                     *(ui32 *)(texelPtr3 + texturePitch + sizeof(ui32)));
                
                    // NOTE: Unpack bilinear texel samples
                    __m128i texelArb = _mm_and_si128(sampleA, maskFF00FF);
                    __m128i texelAag = _mm_and_si128(_mm_srli_epi32(sampleA, 8), maskFF00FF);
                    texelArb = _mm_mullo_epi16(texelArb, texelArb);
                    __m128 texelAa = _mm_cvtepi32_ps(_mm_srli_epi32(texelAag, 16));
                    texelAag = _mm_mullo_epi16(texelAag, texelAag);

                    __m128i texelBrb = _mm_and_si128(sampleB, maskFF00FF);
                    __m128i texelBag = _mm_and_si128(_mm_srli_epi32(sampleB, 8), maskFF00FF);
                    texelBrb = _mm_mullo_epi16(texelBrb, texelBrb);
                    __m128 texelBa = _mm_cvtepi32_ps(_mm_srli_epi32(texelBag, 16));
                    texelBag = _mm_mullo_epi16(texelBag, texelBag);

                    __m128i texelCrb = _mm_and_si128(sampleC, maskFF00FF);
                    __m128i texelCag = _mm_and_si128(_mm_srli_epi32(sampleC, 8), maskFF00FF);
                    texelCrb = _mm_mullo_epi16(texelCrb, texelCrb);
                    __m128 texelCa = _mm_cvtepi32_ps(_mm_srli_epi32(texelCag, 16));
                    texelCag = _mm_mullo_epi16(texelCag, texelCag);

                    __m128i texelDrb = _mm_and_si128(sampleD, maskFF00FF);
                    __m128i texelDag = _mm_and_si128(_mm_srli_epi32(sampleD, 8), maskFF00FF);
                    texelDrb = _mm_mullo_epi16(texelDrb, texelDrb);
                    __m128 texelDa = _mm_cvtepi32_ps(_mm_srli_epi32(texelDag, 16));
                    texelDag = _mm_mullo_epi16(texelDag, texelDag);
                
                    // NOTE: Load destination
                    __m128 destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(originalDest, 16), maskFF));
                    __m128 destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(originalDest, 8), maskFF));
                    __m128 destb = _mm_cvtepi32_ps(_mm_and_si128(originalDest, maskFF));
                    __m128 desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(originalDest, 24), maskFF));
            
                    // NOTE: Convert texture from sRGB [0;255] to "linear" [0;1] brightness space
                    __m128 texelAr = _mm_cvtepi32_ps(_mm_srli_epi32(texelArb, 16));
                    __m128 texelAg = _mm_cvtepi32_ps(_mm_and_si128(texelAag, maskFFFF));
                    __m128 texelAb = _mm_cvtepi32_ps(_mm_and_si128(texelArb, maskFFFF));

                    __m128 texelBr = _mm_cvtepi32_ps(_mm_srli_epi32(texelBrb, 16));
                    __m128 texelBg = _mm_cvtepi32_ps(_mm_and_si128(texelBag, maskFFFF));
                    __m128 texelBb = _mm_cvtepi32_ps(_mm_and_si128(texelBrb, maskFFFF));

                    __m128 texelCr = _mm_cvtepi32_ps(_mm_srli_epi32(texelCrb, 16));
                    __m128 texelCg = _mm_cvtepi32_ps(_mm_and_si128(texelCag, maskFFFF));
                    __m128 texelCb = _mm_cvtepi32_ps(_mm_and_si128(texelCrb, maskFFFF));

                    __m128 texelDr = _mm_cvtepi32_ps(_mm_srli_epi32(texelDrb, 16));
                    __m128 texelDg = _mm_cvtepi32_ps(_mm_and_si128(texelDag, maskFFFF));
                    __m128 texelDb = _mm_cvtepi32_ps(_mm_and_si128(texelDrb, maskFFFF));
                
                    // NOTE: Bilinear texture blend
                    __m128 ifX = _mm_sub_ps(one, fX);
                    __m128 ifY = _mm_sub_ps(one, fY);
                
                    __m128 l0 = _mm_mul_ps(ifY, ifX);
                    __m128 l1 = _mm_mul_ps(ifY, fX);
                    __m128 l2 = _mm_mul_ps(fY, ifX);
                    __m128 l3 = _mm_mul_ps(fY, fX);

                    __m128 texelr = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, texelAr), _mm_mul_ps(l1, texelBr)),
                                               _mm_add_ps(_mm_mul_ps(l2, texelCr), _mm_mul_ps(l3, texelDr)));
                    __m128 texelg = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, texelAg), _mm_mul_ps(l1, texelBg)),
                                               _mm_add_ps(_mm_mul_ps(l2, texelCg), _mm_mul_ps(l3, texelDg)));
                    __m128 texelb = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, texelAb), _mm_mul_ps(l1, texelBb)),
                                               _mm_add_ps(_mm_mul_ps(l2, texelCb), _mm_mul_ps(l3, texelDb)));
                    __m128 texela = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, texelAa), _mm_mul_ps(l1, texelBa)),
                                               _mm_add_ps(_mm_mul_ps(l2, texelCa), _mm_mul_ps(l3, texelDa)));
            
                    // NOTE: Modulate by incoming color
                    texelr = _mm_mul_ps(texelr, colorr_4x);
                    texelg = _mm_mul_ps(texelg, colorg_4x);
                    texelb = _mm_mul_ps(texelb, colorb_4x);
                    texela = _mm_mul_ps(texela, colora_4x);

                    // NOTE: Clamp colors to valid range
                    texelr = _mm_min_ps(_mm_max_ps(texelr, zero), maxColorValue);
                    texelg = _mm_min_ps(_mm_max_ps(texelg, zero), maxColorValue);
                    texelb = _mm_min_ps(_mm_max_ps(texelb, zero), maxColorValue);
            
                    // NOTE: Go from sRGB to "linear" brightness space
                    destr = mmSquare(destr);
                    destg = mmSquare(destg);
                    destb = mmSquare(destb);

                    // NOTE: Destination blend
                    __m128 invTexelA = _mm_sub_ps(one, _mm_mul_ps(inv255_4x, texela));
      
                    __m128 blendedr = _mm_add_ps(_mm_mul_ps(invTexelA, destr), texelr);
                    __m128 blendedg = _mm_add_ps(_mm_mul_ps(invTexelA, destg), texelg);
                    __m128 blendedb = _mm_add_ps(_mm_mul_ps(invTexelA, destb), texelb);
                    __m128 blendeda = _mm_add_ps(_mm_mul_ps(invTexelA, desta), texela);
                
                    // NOTE: Go from "linear" [0;1] brightness space to sRGB [0;255] space
#if 1
                    blendedr = _mm_mul_ps(blendedr, _mm_rsqrt_ps(blendedr));
                    blendedg = _mm_mul_ps(blendedg, _mm_rsqrt_ps(blendedg));
                    blendedb = _mm_mul_ps(blendedb, _mm_rsqrt_ps(blendedb));
#else
                    blendedr = _mm_sqrt_ps(blendedr);
                    blendedg = _mm_sqrt_ps(blendedg);
                    blendedb = _mm_sqrt_ps(blendedb);
#endif

                    // TODO: Should we set the rounding mode to nearest and save the adds?
                    __m128i intr = _mm_cvtps_epi32(blendedr);
                    __m128i intg = _mm_cvtps_epi32(blendedg);
                    __m128i intb = _mm_cvtps_epi32(blendedb);
                    __m128i inta = _mm_cvtps_epi32(blendeda);
            
                    __m128i sr = _mm_slli_epi32(intr, 16);
                    __m128i sg = _mm_slli_epi32(intg, 8);
                    __m128i sb = intb;
                    __m128i sa = _mm_slli_epi32(inta, 24);

                    __m128i out = _mm_or_si128(_mm_or_si128(sr, sg), _mm_or_si128(sb, sa));

                    __m128i maskedOut = _mm_or_si128(_mm_and_si128(writeMask, out),
                                                     _mm_andnot_si128(writeMask, originalDest));

                    // NOTE: Unaligned store
                    _mm_storeu_si128((__m128i *)pixel, maskedOut);
                }

                pixelPx = _mm_add_ps(pixelPx, four_4x);
                pixel += 4;

                clipMask = _mm_set1_epi8(-1);
            
                IACA_VC64_END;
            }
        
            row += rowAdvance;
        }

        END_TIMED_BLOCK_COUNTED(ProcessPixel, GetClampedRectArea(fillRect) / 2);
    }
    
    END_TIMED_BLOCK(DrawRectangleQuickly);
}

internal void
DrawBitmap
(
    loaded_bitmap *buffer,
    loaded_bitmap *bmp,
    r32 rX, r32 rY,
    r32 cAlpha = 1.0f
)
{
    i32 minX = RoundR32ToI32(rX);
    i32 minY = RoundR32ToI32(rY);
    i32 maxX = minX + bmp->width;
    i32 maxY = minY + bmp->height;

    i32 sourceOffsetX = 0;    
    if(minX < 0)
    {
        sourceOffsetX = -minX;
        minX = 0;
    }

    i32 sourceOffsetY = 0;
    if(minY < 0)
    {
        sourceOffsetY = -minY;
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui8 *sourceRow = (ui8 *)bmp->memory +
        sourceOffsetY * bmp->pitch +
        BITMAP_BYTES_PER_PIXEL * sourceOffsetX;
    
    ui8 *destRow = (ui8 *)buffer->memory +
        minX * BITMAP_BYTES_PER_PIXEL + minY * buffer->pitch;
    
    for(i32 y = minY; y < maxY; y++)
    {        
        ui32 *dest = (ui32 *)destRow;
        ui32 *source = (ui32 *)sourceRow;

        for(i32 x = minX; x < maxX; x++)
        {
            v4 texel =
                {
                    (r32)((*source >> 16) & 0xFF),
                    (r32)((*source >> 8) & 0xFF),
                    (r32)((*source >> 0) & 0xFF),
                    (r32)((*source >> 24) & 0xFF)
                };

            texel = SRGB255ToLinear1(texel);
            texel *= cAlpha;
                        
            v4 d =
                {
                    (r32)((*dest >> 16) & 0xFF),
                    (r32)((*dest >> 8) & 0xFF),
                    (r32)((*dest >> 0) & 0xFF),
                    (r32)((*dest >> 24) & 0xFF)
                };

            d = SRGB255ToLinear1(d);
            
            v4 result = (1.0f - texel.a) * d + texel;

            result = Linear1ToSRGB255(result);
            
            *dest = ((ui32)(result.a + 0.5f) << 24|
                     (ui32)(result.r + 0.5f) << 16|
                     (ui32)(result.g + 0.5f) << 8 |
                     (ui32)(result.b + 0.5f) << 0);
            
            dest++;
            source++;
        }

        destRow += buffer->pitch;
        sourceRow += bmp->pitch;
    }
}


internal void
ChangeSaturation(loaded_bitmap *buffer, r32 level)
{
    ui8 *destRow = (ui8 *)buffer->memory;
    
    for(i32 y = 0; y < buffer->height; y++)
    {        
        ui32 *dest = (ui32 *)destRow;

        for(i32 x = 0; x < buffer->width; x++)
        {  
            v4 d =
                {
                    (r32)((*dest >> 16) & 0xFF),
                    (r32)((*dest >> 8) & 0xFF),
                    (r32)((*dest >> 0) & 0xFF),
                    (r32)((*dest >> 24) & 0xFF)
                };

            d = SRGB255ToLinear1(d);

            r32 avg = (d.r + d.g + d.b) * (1.0f / 3.0f);
            v3 delta = {d.r - avg, d.g - avg, d.b - avg};
            
            v4 result = V4(v3{avg, avg, avg} + level * delta, d.a);

            result = Linear1ToSRGB255(result);
            
            *dest = ((ui32)(result.a + 0.5f) << 24|
                     (ui32)(result.r + 0.5f) << 16|
                     (ui32)(result.g + 0.5f) << 8 |
                     (ui32)(result.b + 0.5f) << 0);
            
            dest++;
        }

        destRow += buffer->pitch;
    }
}

internal void
DrawMatte
(
    loaded_bitmap *buffer,
    loaded_bitmap *bmp,
    r32 rX, r32 rY,
    r32 cAlpha = 1.0f
)
{
    i32 minX = RoundR32ToI32(rX);
    i32 minY = RoundR32ToI32(rY);
    i32 maxX = minX + bmp->width;
    i32 maxY = minY + bmp->height;

    i32 sourceOffsetX = 0;    
    if(minX < 0)
    {
        sourceOffsetX = -minX;
        minX = 0;
    }

    i32 sourceOffsetY = 0;
    if(minY < 0)
    {
        sourceOffsetY = -minY;
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui8 *sourceRow = (ui8 *)bmp->memory +
        sourceOffsetY * bmp->pitch +
        BITMAP_BYTES_PER_PIXEL * sourceOffsetX;
    
    ui8 *destRow = (ui8 *)buffer->memory +
        minX * BITMAP_BYTES_PER_PIXEL + minY * buffer->pitch;
    
    for(i32 y = minY; y < maxY; y++)
    {        
        ui32 *dest = (ui32 *)destRow;
        ui32 *source = (ui32 *)sourceRow;

        for(i32 x = minX; x < maxX; x++)
        {
            r32 sa = (r32)((*source >> 24) & 0xFF);
            r32 rsa = (sa / 255.0f) * cAlpha;
            r32 sr = (r32)((*source >> 16) & 0xFF) * cAlpha;
            r32 sg = (r32)((*source >> 8) & 0xFF) * cAlpha;
            r32 sb = (r32)((*source >> 0) & 0xFF) * cAlpha;
            
            r32 da = (r32)((*dest >> 24) & 0xFF);
            r32 dr = (r32)((*dest >> 16) & 0xFF);
            r32 dg = (r32)((*dest >> 8) & 0xFF);
            r32 db = (r32)((*dest >> 0) & 0xFF);
            r32 rda = (da / 255.0f);
                            
            r32 invRSA = (1.0f - rsa);            
            r32 a = invRSA * da;
            r32 r = invRSA * dr;
            r32 g = invRSA * dg;
            r32 b = invRSA * db;

            *dest = ((ui32)(a + 0.5f) << 24|
                     (ui32)(r + 0.5f) << 16|
                     (ui32)(g + 0.5f) << 8 |
                     (ui32)(b + 0.5f) << 0);
            
            dest++;
            source++;
        }

        destRow += buffer->pitch;
        sourceRow += bmp->pitch;
    }
}

struct entity_basis_p_result
{
    v2 p;
    r32 scale;
    b32 isValid;
};
inline entity_basis_p_result
GetRenderEntityBasisPos
(
    render_group *renderGroup,
    render_entity_basis *entityBasis, v2 screenDim
)
{
    v2 screenCenter = 0.5f * screenDim;
    
    entity_basis_p_result result = {};
    
    v3 entityBaseP = entityBasis->basis->p;

    r32 distanceToPZ = (renderGroup->renderCamera.distanceAboveTarget - entityBaseP.z);
    r32 nearClipPlane = 0.2f;

    v3 rawXY = V3(entityBaseP.xy + entityBasis->offset.xy, 1.0f);

    if(distanceToPZ > nearClipPlane)
    {
        v3 projectedXY = (1.0f / distanceToPZ) * (renderGroup->renderCamera.focalLength * rawXY);
        result.p = screenCenter + renderGroup->metersToPixels * projectedXY.xy;
        result.scale = renderGroup->metersToPixels * projectedXY.z;
        result.isValid = true;
    }
    
    return result;
}

internal void
RenderGroupToOutput
(
    render_group *renderGroup, loaded_bitmap *outputTarget,
    rectangle2i clipRect, b32 even
)
{
    BEGIN_TIMED_BLOCK(RenderGroupToOutput);
    
    v2 screenDim = {(r32)outputTarget->width,
                    (r32)outputTarget->height};
    
    r32 pixelsToMeters = 1.0f / renderGroup->metersToPixels;

    for(ui32 baseAddress = 0;
        baseAddress < renderGroup->pushBufferSize;
        )
    {
        render_group_entry_header *header = (render_group_entry_header *)
            (renderGroup->pushBufferBase + baseAddress);
        void *data = header + 1;
        baseAddress += sizeof(*header);
        
        switch(header->type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *entry = (render_entry_clear *)data;

                DrawRectangle
                    (
                        outputTarget,
                        v2{0.0f, 0.0f},
                        v2{(r32)outputTarget->width, (r32)outputTarget->height},
                        entry->color, clipRect, even
                    );
                
                baseAddress += sizeof(*entry);
            } break;

            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *entry = (render_entry_bitmap *)data;

                entity_basis_p_result basis = GetRenderEntityBasisPos
                    (
                        renderGroup,
                        &entry->entityBasis,
                        screenDim
                    );
                                
                Assert(entry->bitmap);
                
#if 0
                DrawBitmap
                    (
                        outputTarget, entry->bitmap,
                        pos.x, pos.y,
                        entry->color.a
                    );
#else

#if 0
                DrawRectangleSlowly
                    (
                        outputTarget, basis.p,
                        basis.scale * v2{entry->size.x, 0},
                        basis.scale * v2{0, entry->size.y},
                        entry->color, entry->bitmap, 0, 0, 0, 0,
                        pixelsToMeters
                    );
#else
                DrawRectangleQuickly
                    (
                        outputTarget, basis.p,
                        basis.scale * v2{entry->size.x, 0},
                        basis.scale * v2{0, entry->size.y},
                        entry->color, entry->bitmap,
                        pixelsToMeters, clipRect, even
                    );
#endif

#endif
                
                baseAddress += sizeof(*entry);
            } break;
            
            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *entry = (render_entry_rectangle *)data;
                entity_basis_p_result basis = GetRenderEntityBasisPos
                    (
                        renderGroup,
                        &entry->entityBasis,
                        screenDim
                    );

                DrawRectangle
                    (
                        outputTarget,
                        basis.p, basis.p + basis.scale * entry->dim,
                        entry->color, clipRect, even
                    );
                
                baseAddress += sizeof(*entry);
            } break;
            
            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system *entry = (render_entry_coordinate_system *)data;

#if 0
                v2 vMax = entry->origin + entry->xAxis + entry->yAxis;
                DrawRectangleSlowly
                    (
                        outputTarget,
                        entry->origin,
                        entry->xAxis,
                        entry->yAxis,
                        entry->color,
                        entry->texture,
                        entry->normalMap,
                        entry->top, entry->middle, entry->bottom,
                        pixelsToMeters
                    );

                v4 color = {1, 1, 0, 1};
                v2 dim = {2, 2};
                v2 pos = entry->origin;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color
                    );

                pos = entry->origin + entry->xAxis;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color
                    );

                pos = entry->origin + entry->yAxis;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color
                    );

                DrawRectangle
                    (
                        outputTarget,
                        vMax - dim, vMax + dim,
                        color
                    );

#if 0
                for(ui32 pIndex = 0;
                    pIndex < ArrayCount(entry->points);
                    pIndex++)
                {
                    pos = entry->points[pIndex];
                    pos = entry->origin + pos.x * entry->xAxis + pos.y * entry->yAxis;
                    DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        entry->color.r, entry->color.g, entry->color.b
                    );
                }
#endif
#endif
                baseAddress += sizeof(*entry);
            } break;

            InvalidDefaultCase;
        }
    }

    END_TIMED_BLOCK(RenderGroupToOutput);
}

struct tile_render_work
{
    render_group *renderGroup;
    loaded_bitmap *outputTarget;
    rectangle2i clipRect;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork)
{
    tile_render_work *work = (tile_render_work *)data;

    RenderGroupToOutput(work->renderGroup, work->outputTarget, work->clipRect, true);
    RenderGroupToOutput(work->renderGroup, work->outputTarget, work->clipRect, false);
}

internal void
TiledRenderGroupToOutput
(
    platform_work_queue *renderQueue,
    render_group *renderGroup, loaded_bitmap *outputTarget
)
{
    i32 const tileCountX = 4;
    i32 const tileCountY = 4;
    tile_render_work workArray[tileCountX * tileCountY];

    i32 tileWidth = outputTarget->width / tileCountX;
    i32 tileHeight = outputTarget->height / tileCountY;

    int workCount = 0;
    for(i32 tileY = 0; tileY < tileCountY; tileY++)
    {
        for(i32 tileX = 0; tileX < tileCountX; tileX++)
        {
            tile_render_work *work = workArray + workCount++;
            
            rectangle2i clipRect;
            clipRect.minX = tileX * tileWidth + 4;
            clipRect.maxX = clipRect.minX + tileWidth - 4;
            clipRect.minY = tileY * tileHeight + 4;
            clipRect.maxY = clipRect.minY + tileHeight - 4;

            work->renderGroup = renderGroup;
            work->outputTarget = outputTarget;
            work->clipRect = clipRect;

#if 1
            // NOTE: This is the multi-threaded path
            PlatformAddEntry(renderQueue, DoTiledRenderWork, work);
#else
            // NOTE: This is the single-threaded path
            DoTiledRenderWork(renderQueue, work);
#endif
        }
    }

    PlatformCompleteAllWork(renderQueue);
}

internal render_group *
AllocateRenderGroup
(
    memory_arena *arena, ui32 maxPushBufferSize,
    ui32 resolutionPixelsX, ui32 resolutionPixelsY
)
{
    render_group *result = PushStruct(arena, render_group);
    result->pushBufferBase = (ui8 *)PushSize(arena, maxPushBufferSize);

    result->defaultBasis = PushStruct(arena, render_basis);
    result->defaultBasis->p = v3{0, 0, 0};

    result->maxPushBufferSize = maxPushBufferSize;
    result->pushBufferSize = 0;

    result->gameCamera.focalLength = 0.6f;
    result->gameCamera.distanceAboveTarget = 9.0f;

    result->renderCamera = result->gameCamera;
    //result->renderCamera.distanceAboveTarget = 50.0f;
    
    result->globalAlpha = 1.0f;
    
    // NOTE: This is a approximate monitor width
    r32 widthOfMonitor = 0.635f;
    result->metersToPixels = (r32)resolutionPixelsX * widthOfMonitor;

    r32 pixelsToMeters = 1.0f / result->metersToPixels;
    result->monitorHalfDimInMeters =
        {
            0.5f * resolutionPixelsX * pixelsToMeters,
            0.5f * resolutionPixelsY * pixelsToMeters
        };  
    
    return result;
}

#define PushRenderElement(group, type) (type *)PushRenderElement_(group, sizeof(type), RenderGroupEntryType_##type)
inline void *
PushRenderElement_(render_group *group, ui32 size, render_group_entry_type type)
{
    void *result = 0;

    size += sizeof(render_group_entry_header);
    
    if(group->pushBufferSize + size < group->maxPushBufferSize)
    {
        render_group_entry_header *header = (render_group_entry_header *)(group->pushBufferBase + group->pushBufferSize);
        header->type = type;
        result = header + 1;
        group->pushBufferSize += size;
    }
    else
    {
        InvalidCodePath;
    }

    return result;
}

inline void
PushBitmap
(
    render_group *group,
    loaded_bitmap *bitmap,
    r32 height, v3 offset, v4 color = {1, 1, 1, 1}
)
{
    render_entry_bitmap *entry = PushRenderElement(group, render_entry_bitmap);
    if(entry)
    {
        entry->entityBasis.basis = group->defaultBasis;
        entry->bitmap = bitmap;
        v2 size = {height * bitmap->widthOverHeight, height};
        v2 align = Hadamard(bitmap->alignPercentage, size);
        entry->entityBasis.offset = offset - V3(align, 0);
        entry->color = color * group->globalAlpha;
        entry->size = size;
    }
}

inline void
PushRect
(
    render_group *group,
    v3 offset, v2 dim, v4 color = {1, 1, 1, 1}
)
{
    render_entry_rectangle *piece = PushRenderElement(group, render_entry_rectangle);
    if(piece)
    {
        piece->entityBasis.basis = group->defaultBasis;
        piece->entityBasis.offset = (offset - V3(0.5f * dim, 0));
        piece->color = color;
        piece->dim = dim;
    }
}

inline void
PushRectOutline
(
    render_group *group,
    v3 offset, v2 dim,
    v4 color = {1, 1, 1, 1},
    r32 entityZC = 1.0f
)
{
    r32 thickness = 0.1f;
    
    // NOTE: Top and bottom
    PushRect
        (
            group,
            offset - v3{0, 0.5f * dim.y, 0},
            v2{dim.x, thickness}, color
        );
    PushRect
        (
            group,
            offset + v3{0, 0.5f * dim.y, 0},
            v2{dim.x, thickness}, color
        );
    
    // NOTE: Left and right
    PushRect
        (
            group,
            offset - v3{0.5f * dim.x, 0, 0},
            v2{thickness, dim.y}, color
        );
    PushRect
        (
            group,
            offset + v3{0.5f * dim.x, 0, 0},
            v2{thickness, dim.y}, color
        );
}

inline void
Clear(render_group *group, v4 color)
{
    render_entry_clear *entry = PushRenderElement(group, render_entry_clear);
    if(entry)
    {
        entry->color = color;
    }
}

inline render_entry_coordinate_system *
CoordinateSystem
(
    render_group *group,
    v2 origin, v2 xAxis, v2 yAxis,
    v4 color,
    loaded_bitmap *texture,
    loaded_bitmap *normalMap,
    environment_map *top,
    environment_map *middle,
    environment_map *bottom
)
{
    render_entry_coordinate_system *entry = PushRenderElement(group, render_entry_coordinate_system);
    if(entry)
    {
        entry->origin = origin;
        entry->xAxis = xAxis;
        entry->yAxis = yAxis;
        entry->color = color;
        entry->texture = texture;
        entry->normalMap = normalMap;
        entry->top = top;
        entry->middle = middle;
        entry->bottom = bottom;
    }

    return entry;
}

inline v2
Unproject(render_group *group, v2 projectedXY, r32 atDistanceFromCamera)
{
    v2 worldXY = (atDistanceFromCamera / group->gameCamera.focalLength) * projectedXY;
    return worldXY;
}

inline rectangle2
GetCameraRectangleAtDistance(render_group *group, r32 distanceFromCamera)
{
    v2 rawXY = Unproject(group, group->monitorHalfDimInMeters, distanceFromCamera);

    rectangle2 result = RectCenterHalfDim(v2{}, rawXY);
    
    return result;
}

inline rectangle2
GetCameraRectangleAtTarget(render_group *group)
{
    rectangle2 result = GetCameraRectangleAtDistance(group, group->gameCamera.distanceAboveTarget);
    return result;
}
