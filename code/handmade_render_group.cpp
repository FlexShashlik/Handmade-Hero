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

internal void
DrawRectangle
(
    loaded_bitmap *buffer,
    v2 vMin, v2 vMax,
    r32 r, r32 g, r32 b, r32 a = 1.0f
)
{
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
    
    ui32 color =
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
            *pixel++ = color;            
        }
        
        row += buffer->pitch;
    }
} 

internal void
DrawRectangleSlowly
(
    loaded_bitmap *buffer,
    v2 origin, v2 xAxis, v2 yAxis,
    v4 color, loaded_bitmap *texture
)
{
    r32 InvXAxisLengthSq = 1.0f / LengthSq(xAxis);
    r32 InvYAxisLengthSq = 1.0f / LengthSq(yAxis);
    
    ui32 color32 =
        (
            RoundR32ToUI32(color.a * 255.0f) << 24 |
            RoundR32ToUI32(color.r * 255.0f) << 16 |
            RoundR32ToUI32(color.g * 255.0f) << 8 |
            RoundR32ToUI32(color.b * 255.0f) << 0
        );

    i32 widthMax = buffer->width - 1;
    i32 heightMax = buffer->height - 1;
    
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
                r32 u = InvXAxisLengthSq * Inner(d, xAxis);
                r32 v = InvYAxisLengthSq * Inner(d, yAxis);

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

                ui8 *texelPtr = (((ui8 *)texture->memory) +
                                 y * texture->pitch +
                                 x * sizeof(ui32));
                ui32 texelPtrA = *(ui32 *)(texelPtr);
                ui32 texelPtrB = *(ui32 *)(texelPtr + sizeof(ui32));
                ui32 texelPtrC = *(ui32 *)(texelPtr + texture->pitch);
                ui32 texelPtrD = *(ui32 *)(texelPtr + texture->pitch + sizeof(ui32));

                v4 texelA = 
                {
                    (r32)((texelPtrA >> 16) & 0xFF),
                    (r32)((texelPtrA >> 8) & 0xFF),
                    (r32)((texelPtrA >> 0) & 0xFF),
                    (r32)((texelPtrA >> 24) & 0xFF)
                };

                v4 texelB = 
                {
                    (r32)((texelPtrB >> 16) & 0xFF),
                    (r32)((texelPtrB >> 8) & 0xFF),
                    (r32)((texelPtrB >> 0) & 0xFF),
                    (r32)((texelPtrB >> 24) & 0xFF)
                };
                
                v4 texelC = 
                {
                    (r32)((texelPtrC >> 16) & 0xFF),
                    (r32)((texelPtrC >> 8) & 0xFF),
                    (r32)((texelPtrC >> 0) & 0xFF),
                    (r32)((texelPtrC >> 24) & 0xFF)
                };
                
                v4 texelD = 
                    {
                        (r32)((texelPtrD >> 16) & 0xFF),
                        (r32)((texelPtrD >> 8) & 0xFF),
                        (r32)((texelPtrD >> 0) & 0xFF),
                        (r32)((texelPtrD >> 24) & 0xFF)
                    };

                // NOTE: Go from sRGB to "linear" brightness space
                texelA = SRGB255ToLinear1(texelA);
                texelB = SRGB255ToLinear1(texelB);
                texelC = SRGB255ToLinear1(texelC);
                texelD = SRGB255ToLinear1(texelD);
                
#if 0
                v4 texel = texelA;
#else
                v4 texel = Lerp(Lerp(texelA, fX, texelB),
                                fY,
                                Lerp(texelC, fX, texelD));
#endif
                                
                r32 rsa = texel.a * color.a;

                // NOTE: Go from sRGB to "linear" brightness space
                v4 dest =
                    {
                        (r32)((*pixel >> 16) & 0xFF),
                        (r32)((*pixel >> 8) & 0xFF),
                        (r32)((*pixel >> 0) & 0xFF),
                        (r32)((*pixel >> 24) & 0xFF)
                    };
                
                // NOTE: Go from sRGB to "linear" brightness space
                dest = SRGB255ToLinear1(dest);
                
                r32 rda = dest.a;
                            
                r32 invRSA = (1.0f - rsa);            
                v4 blended =
                    {
                        invRSA * dest.r + color.a * color.r * texel.r,
                        invRSA * dest.g + color.a * color.g * texel.g,
                        invRSA * dest.b + color.a * color.b * texel.b,
                        (rsa + rda - rsa * rda)
                    };

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
            color.r, color.g, color.b
        );
    DrawRectangle
        (
            buffer,
            v2{vMin.x - r, vMax.y - r},
            v2{vMax.x + r, vMax.y + r},
            color.r, color.g, color.b
        );
    
    // NOTE: Left and right
    DrawRectangle
        (
            buffer,
            v2{vMin.x - r, vMin.y - r},
            v2{vMin.x + r, vMax.y + r},
            color.r, color.g, color.b
        );
    DrawRectangle
        (
            buffer,
            v2{vMax.x - r, vMin.y - r},
            v2{vMax.x + r, vMax.y + r},
            color.r, color.g, color.b
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
            r32 a = 255.0f * (rsa + rda - rsa * rda);
            r32 r = invRSA * dr + sr;
            r32 g = invRSA * dg + sg;
            r32 b = invRSA * db + sb;

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

inline v2
GetRenderEntityBasisPos
(
    render_group *renderGroup,
    render_entity_basis *entityBasis, v2 screenCenter
)
{
    v3 entityBaseP = entityBasis->basis->p; 
    r32 zFudge = 1.0f + 0.1f * (entityBaseP.z + entityBasis->offsetZ);
            
    r32 entityGroundX = screenCenter.x + renderGroup->metersToPixels *
        zFudge * entityBaseP.x;
    r32 entityGroundY = screenCenter.y - renderGroup->metersToPixels *
        zFudge * entityBaseP.y;

    r32 entityZ = -renderGroup->metersToPixels * entityBaseP.z;

    v2 center =
        {
            entityGroundX + entityBasis->offset.x,
            entityGroundY + entityBasis->offset.y + entityBasis->entityZC * entityZ
        };

    return center;
}

internal void
RenderGroupToOutput
(
    render_group *renderGroup, loaded_bitmap *outputTarget
)
{
    v2 screenCenter = {0.5f * (r32)outputTarget->width,
                       0.5f * (r32)outputTarget->height};
   
    for(ui32 baseAddress = 0;
        baseAddress < renderGroup->pushBufferSize;
        )
    {
        render_group_entry_header *header = (render_group_entry_header *)
            (renderGroup->pushBufferBase + baseAddress);
        switch(header->type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *entry = (render_entry_clear *)header;

                DrawRectangle
                    (
                        outputTarget,
                        v2{0.0f, 0.0f},
                        v2{(r32)outputTarget->width, (r32)outputTarget->height},
                        entry->color.r, entry->color.g, entry->color.b,
                        entry->color.a
                    );
                
                baseAddress += sizeof(*entry);
            } break;

            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *entry = (render_entry_bitmap *)header;
                v2 pos = GetRenderEntityBasisPos
                    (
                        renderGroup,
                        &entry->entityBasis,
                        screenCenter
                    );
                                
                Assert(entry->bitmap);
                DrawBitmap
                    (
                        outputTarget, entry->bitmap,
                        pos.x, pos.y,
                        entry->a
                    );
                
                baseAddress += sizeof(*entry);
            } break;
            
            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *entry = (render_entry_rectangle *)header;
                v2 pos = GetRenderEntityBasisPos
                    (
                        renderGroup,
                        &entry->entityBasis,
                        screenCenter
                    );
                
                DrawRectangle
                    (
                        outputTarget,
                        pos, pos + entry->dim,
                        entry->r, entry->g, entry->b
                    );
                
                baseAddress += sizeof(*entry);
            } break;
            
            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system *entry = (render_entry_coordinate_system *)header;

                v2 vMax = entry->origin + entry->xAxis + entry->yAxis;
                DrawRectangleSlowly
                    (
                        outputTarget,
                        entry->origin,
                        entry->xAxis,
                        entry->yAxis,
                        entry->color,
                        entry->texture
                    );

                v4 color = {1, 1, 0, 1};
                v2 dim = {2, 2};
                v2 pos = entry->origin;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color.r, color.g, color.b
                    );

                pos = entry->origin + entry->xAxis;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color.r, color.g, color.b
                    );

                pos = entry->origin + entry->yAxis;
                DrawRectangle
                    (
                        outputTarget,
                        pos - dim, pos + dim,
                        color.r, color.g, color.b
                    );

                DrawRectangle
                    (
                        outputTarget,
                        vMax - dim, vMax + dim,
                        color.r, color.g, color.b
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
}

internal render_group *
AllocateRenderGroup
(
    memory_arena *arena,
    ui32 maxPushBufferSize, r32 metersToPixels
)
{
    render_group *result = PushStruct(arena, render_group);
    result->pushBufferBase = (ui8 *)PushSize(arena, maxPushBufferSize);
    
    result->metersToPixels = metersToPixels;
    result->defaultBasis = PushStruct(arena, render_basis);
    result->defaultBasis->p = v3{0, 0, 0};

    result->maxPushBufferSize = maxPushBufferSize;
    result->pushBufferSize = 0;
    
    return result;
}

#define PushRenderElement(group, type) (type *)PushRenderElement_(group, sizeof(type), RenderGroupEntryType_##type)
inline render_group_entry_header *
PushRenderElement_(render_group *group, ui32 size, render_group_entry_type type)
{
    render_group_entry_header *result = 0;
    if(group->pushBufferSize + size < group->maxPushBufferSize)
    {
        result = (render_group_entry_header *)(group->pushBufferBase + group->pushBufferSize);
        result->type = type;
        group->pushBufferSize += size;
    }
    else
    {
        InvalidCodePath;
    }

    return result;
}

inline void
PushPiece
(
    render_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    render_entry_bitmap *piece = PushRenderElement(group, render_entry_bitmap);
    if(piece)
    {
        piece->entityBasis.basis = group->defaultBasis;
        piece->bitmap = bitmap;
        piece->entityBasis.offset = group->metersToPixels * v2{offset.x, -offset.y} - align;
        piece->entityBasis.offsetZ = offsetZ;
        piece->r = color.r;
        piece->g = color.g;
        piece->b = color.b;
        piece->a = color.a;
        piece->entityBasis.entityZC = entityZC;
    }
}

inline void
PushBitmap
(
    render_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, r32 alpha = 1.0f,
    r32 entityZC = 1.0f
)
{
    PushPiece
        (
            group, bitmap,
            offset, offsetZ,
            align, v2{0, 0},
            v4{1.0f, 1.0f, 1.0f, alpha},
            entityZC
        );
}

inline void
PushRect
(
    render_group *group,
    v2 offset, r32 offsetZ,
    v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    render_entry_rectangle *piece = PushRenderElement(group, render_entry_rectangle);
    if(piece)
    {
        v2 halfDim = 0.5f * group->metersToPixels * dim;
        
        piece->entityBasis.basis = group->defaultBasis;
        piece->entityBasis.offset = group->metersToPixels * v2{offset.x, -offset.y} - halfDim;
        piece->entityBasis.offsetZ = offsetZ;
        piece->r = color.r;
        piece->g = color.g;
        piece->b = color.b;
        piece->a = color.a;
        piece->entityBasis.entityZC = entityZC;
        piece->dim = group->metersToPixels * dim;
    }
}

inline void
PushRectOutline
(
    render_group *group,
    v2 offset, r32 offsetZ,
    v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    r32 thickness = 0.1f;
    
    // NOTE: Top and bottom
    PushRect
        (
            group,
            offset - v2{0, 0.5f * dim.y}, offsetZ,
            v2{dim.x, thickness},
            color,
            entityZC
        );
    PushRect
        (
            group,
            offset + v2{0, 0.5f * dim.y}, offsetZ,
            v2{dim.x, thickness},
            color,
            entityZC
        );
    
    // NOTE: Left and right
    PushRect
        (
            group,
            offset - v2{0.5f * dim.x, 0}, offsetZ,
            v2{thickness, dim.y},
            color,
            entityZC
        );
    PushRect
        (
            group,
            offset + v2{0.5f * dim.x, 0}, offsetZ,
            v2{thickness, dim.y},
            color,
            entityZC
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
    loaded_bitmap *texture
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
    }

    return entry;
}
