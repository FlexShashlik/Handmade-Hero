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
    v4 color
)
{
    r32 r = color.r;
    r32 g = color.g;
    r32 b = color.b;
    r32 a = color.a;
    
    i32 minX = RoundR32ToI32(vMin.x);
    i32 minY = RoundR32ToI32(vMin.y);
    i32 maxX = RoundR32ToI32(vMax.x);
    i32 maxY = RoundR32ToI32(vMax.y);

    if(minX < 0)
    {
        minX = 0;
    }

    if(minY < 0)
    {
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
    
    ui8 *endOfBuffer = (ui8 *)buffer->memory + buffer->pitch * buffer->height;
    
    ui32 color32 =
        (
            RoundR32ToUI32(a * 255.0f) << 24 |
            RoundR32ToUI32(r * 255.0f) << 16 |
            RoundR32ToUI32(g * 255.0f) << 8 |
            RoundR32ToUI32(b * 255.0f) << 0
        );

    ui8 *row = (ui8 *)buffer->memory + minX * BITMAP_BYTES_PER_PIXEL + minY * buffer->pitch;
    for(i32 y = minY; y < maxY; y++)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = minX; x < maxX; x++)
        {        
            *pixel++ = color32;            
        }
        
        row += buffer->pitch;
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
                BEGIN_TIMED_BLOCK(FillPixel);
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

                END_TIMED_BLOCK(FillPixel);
            }
#else
            *pixel = color32;
#endif
            pixel++;

            END_TIMED_BLOCK(TestPixel);
        }
        
        row += buffer->pitch;
    }

    END_TIMED_BLOCK(DrawRectangleSlowly);
}

internal void
DrawRectangleHopefullyQuickly
(
    loaded_bitmap *buffer,
    v2 origin, v2 xAxis, v2 yAxis,
    v4 color,
    loaded_bitmap *texture,
    r32 pixelsToMeters
)
{
    BEGIN_TIMED_BLOCK(DrawRectangleHopefullyQuickly);
    
    // NOTE: Premultiply color up front
    color.rgb *= color.a;
    
    r32 xAxisLength = Length(xAxis);
    r32 yAxisLength = Length(yAxis);
    
    //v2 nXAxis = (yAxisLength / xAxisLength) * xAxis;
    //v2 nYAxis = (xAxisLength / yAxisLength) * yAxis;
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

    v2 nXAxis = invXAxisLengthSq * xAxis;
    v2 nYAxis = invYAxisLengthSq * yAxis;

    r32 inv255 = 1.0f / 255.0f;
    
    ui8 *row = ((ui8 *)buffer->memory +
                xMin * BITMAP_BYTES_PER_PIXEL +
                yMin * buffer->pitch);
    for(i32 y = yMin; y <= yMax; y++)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = xMin; x <= xMax; x++)
        {
            BEGIN_TIMED_BLOCK(TestPixel);
            
            v2 pixelP = V2i(x, y);
            v2 d = pixelP - origin;
            
            r32 u = Inner(d, nXAxis);
            r32 v = Inner(d, nYAxis);
                
            if(u >= 0.0f &&
               u <= 1.0f &&
               v >= 0.0f &&
               v <= 1.0f)
            {
                BEGIN_TIMED_BLOCK(FillPixel);
                
                r32 tX = (u * (r32)(texture->width - 2));
                r32 tY = (v * (r32)(texture->height - 2));
                
                i32 x = (i32)tX;
                i32 y = (i32)tY;

                r32 fX = tX - (r32)x;
                r32 fY = tY - (r32)y;

                Assert(x >= 0 && x < texture->width);
                Assert(y >= 0 && y < texture->height);

                
                ui8 *texelPtr = (((ui8 *)texture->memory) +
                                 y * texture->pitch +
                                 x * sizeof(ui32));
                ui32 sampleA = *(ui32 *)(texelPtr);
                ui32 sampleB = *(ui32 *)(texelPtr + sizeof(ui32));
                ui32 sampleC = *(ui32 *)(texelPtr + texture->pitch);
                ui32 sampleD = *(ui32 *)(texelPtr + texture->pitch + sizeof(ui32));
                
                r32 texelAr = (r32)((sampleA >> 16) & 0xFF);
                r32 texelAg = (r32)((sampleA >> 8) & 0xFF);
                r32 texelAb = (r32)((sampleA >> 0) & 0xFF);
                r32 texelAa = (r32)((sampleA >> 24) & 0xFF);

                r32 texelBr = (r32)((sampleB >> 16) & 0xFF);
                r32 texelBg = (r32)((sampleB >> 8) & 0xFF);
                r32 texelBb = (r32)((sampleB >> 0) & 0xFF);
                r32 texelBa = (r32)((sampleB >> 24) & 0xFF);

                r32 texelCr = (r32)((sampleC >> 16) & 0xFF);
                r32 texelCg = (r32)((sampleC >> 8) & 0xFF);
                r32 texelCb = (r32)((sampleC >> 0) & 0xFF);
                r32 texelCa = (r32)((sampleC >> 24) & 0xFF);

                r32 texelDr = (r32)((sampleD >> 16) & 0xFF);
                r32 texelDg = (r32)((sampleD >> 8) & 0xFF);
                r32 texelDb = (r32)((sampleD >> 0) & 0xFF);
                r32 texelDa = (r32)((sampleD >> 24) & 0xFF);
                                
                // NOTE: Convert texture from sRGB to "linear" brightness space
                texelAr = Square(inv255 * texelAr);
                texelAg = Square(inv255 * texelAg);
                texelAb = Square(inv255 * texelAb);
                texelAa = inv255 * texelAa;

                texelBr = Square(inv255 * texelBr);
                texelBg = Square(inv255 * texelBg);
                texelBb = Square(inv255 * texelBb);
                texelBa = inv255 * texelBa;

                texelCr = Square(inv255 * texelCr);
                texelCg = Square(inv255 * texelCg);
                texelCb = Square(inv255 * texelCb);
                texelCa = inv255 * texelCa;

                texelDr = Square(inv255 * texelDr);
                texelDg = Square(inv255 * texelDg);
                texelDb = Square(inv255 * texelDb);
                texelDa = inv255 * texelDa;

                // NOTE: Bilinear texture blend
                r32 ifX = 1.0f - fX;
                r32 ifY = 1.0f - fY;
                
                r32 l0 = ifY * ifX;
                r32 l1 = ifY * fX;
                r32 l2 = fY * ifX;
                r32 l3 = fY * fX;

                r32 texelr = l0 * texelAr + l1 * texelBr + l2 * texelCr + l3 * texelDr;
                r32 texelg = l0 * texelAg + l1 * texelBg + l2 * texelCg + l3 * texelDg;
                r32 texelb = l0 * texelAb + l1 * texelBb + l2 * texelCb + l3 * texelDb;
                r32 texela = l0 * texelAa + l1 * texelBa + l2 * texelCa + l3 * texelDa;

                // NOTE: Modulate by incoming color
                texelr = texelr * color.r;
                texelg = texelg * color.g;
                texelb = texelb * color.b;
                texela = texela * color.a;

                // NOTE: Clamp colors to valid range
                texelr = Clamp01(texelr);
                texelg = Clamp01(texelg);
                texelb = Clamp01(texelb);

                // NOTE: Load destination
                r32 destr = (r32)((*pixel >> 16) & 0xFF);
                r32 destg = (r32)((*pixel >> 8) & 0xFF);
                r32 destb = (r32)((*pixel >> 0) & 0xFF);
                r32 desta = (r32)((*pixel >> 24) & 0xFF);
                
                // NOTE: Go from sRGB to "linear" brightness space
                destr = Square(inv255 * destr);
                destg = Square(inv255 * destg);
                destb = Square(inv255 * destb);
                desta = inv255 * desta;

                // NOTE: Destination blend
                r32 invTexelA = 1.0f - texela;
                r32 blendedr = invTexelA * destr + texelr;
                r32 blendedg = invTexelA * destg + texelg;
                r32 blendedb = invTexelA * destb + texelb;
                r32 blendeda = invTexelA * desta + texela;
                
                // NOTE: Go from "linear" brightness space to sRGB space
                blendedr = 255.0f * SqRt(blendedr);
                blendedg = 255.0f * SqRt(blendedg);
                blendedb = 255.0f * SqRt(blendedb);
                blendeda = 255.0f * blendeda;

                // NOTE: Repack
                *pixel = ((ui32)(blendeda + 0.5f) << 24|
                          (ui32)(blendedr + 0.5f) << 16|
                          (ui32)(blendedg + 0.5f) << 8 |
                          (ui32)(blendedb + 0.5f) << 0);

                END_TIMED_BLOCK(FillPixel);
            }
            
            pixel++;

            END_TIMED_BLOCK(TestPixel);
        }
        
        row += buffer->pitch;
    }

    END_TIMED_BLOCK(DrawRectangleHopefullyQuickly);
}

inline void
DrawRectangleOutline
(
    loaded_bitmap *buffer,
    v2 vMin, v2 vMax,
    v3 color,
    r32 r = 2.0f
)
{
    // NOTE: Top and bottom
    DrawRectangle
        (
            buffer,
            v2{vMin.x - r, vMin.y - r},
            v2{vMax.x + r, vMin.y + r},
            V4(color, 1.0f)
        );
    DrawRectangle
        (
            buffer,
            v2{vMin.x - r, vMax.y - r},
            v2{vMax.x + r, vMax.y + r},
            V4(color, 1.0f)
        );
    
    // NOTE: Left and right
    DrawRectangle
        (
            buffer,
            v2{vMin.x - r, vMin.y - r},
            v2{vMin.x + r, vMax.y + r},
            V4(color, 1.0f)
        );
    DrawRectangle
        (
            buffer,
            v2{vMax.x - r, vMin.y - r},
            v2{vMax.x + r, vMax.y + r},
            V4(color, 1.0f)
        );    
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
    render_group *renderGroup, loaded_bitmap *outputTarget
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
                        entry->color
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
                DrawRectangleHopefullyQuickly
                    (
                        outputTarget, basis.p,
                        basis.scale * v2{entry->size.x, 0},
                        basis.scale * v2{0, entry->size.y},
                        entry->color, entry->bitmap,
                        pixelsToMeters
                    );
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
                        entry->color
                    );
                
                baseAddress += sizeof(*entry);
            } break;
            
            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system *entry = (render_entry_coordinate_system *)data;

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
                
                baseAddress += sizeof(*entry);
            } break;

            InvalidDefaultCase;
        }
    }

    END_TIMED_BLOCK(RenderGroupToOutput);
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
