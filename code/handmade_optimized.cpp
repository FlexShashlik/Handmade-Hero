#define internal
#include "handmade.h"

#if 0
#include <iacaMarks.h>
#else
#define IACA_VC64_START
#define IACA_VC64_END
#endif

void
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
        __m128i startClipMask = _mm_set1_epi8(-1);
        __m128i endClipMask = _mm_set1_epi8(-1); 

        __m128i startClipMasks[] =
        {
            _mm_slli_si128(startClipMask, 0 * 4),
            _mm_slli_si128(startClipMask, 1 * 4),
            _mm_slli_si128(startClipMask, 2 * 4),
            _mm_slli_si128(startClipMask, 3 * 4)
        };
        
        __m128i endClipMasks[] =
        {
            _mm_srli_si128(endClipMask, 0 * 4),
            _mm_srli_si128(endClipMask, 3 * 4),
            _mm_srli_si128(endClipMask, 2 * 4),
            _mm_srli_si128(endClipMask, 1 * 4)
        };

        if(fillRect.minX & 3)
        {
            startClipMask = startClipMasks[fillRect.minX & 3];
            fillRect.minX = fillRect.minX & ~3;
        }

        if(fillRect.maxX & 3)
        {
            endClipMask = endClipMasks[fillRect.maxX & 3];
            fillRect.maxX = (fillRect.maxX & ~3) + 4;
        }
    
        v2 nXAxis = invXAxisLengthSq * xAxis;
        v2 nYAxis = invYAxisLengthSq * yAxis;

        r32 inv255 = 1.0f / 255.0f;
        __m128 inv255_4x = _mm_set1_ps(inv255);

        r32 normalizeC = 1.0f / 255.0f;
        r32 normalizeSqC = 1.0f / Square(255.0f);

        __m128 zero = _mm_set1_ps(0.0f);
        __m128 half = _mm_set1_ps(0.5f);
        __m128 one = _mm_set1_ps(1.0f);
        __m128 four_4x = _mm_set1_ps(4.0f);
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

            __m128i clipMask = startClipMask;
        
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
                    __m128i originalDest = _mm_load_si128((__m128i *)pixel);
            
                    u = _mm_min_ps(_mm_max_ps(u, zero), one);
                    v = _mm_min_ps(_mm_max_ps(v, zero), one);

                    // NOTE: Bias texture coordinates to start on the
                    // boundary between the (0, 0) and (1, 1) pixels.
                    __m128 tX = _mm_add_ps(_mm_mul_ps(u, widthM2), half);
                    __m128 tY = _mm_add_ps(_mm_mul_ps(v, heightM2), half);

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

                    _mm_store_si128((__m128i *)pixel, maskedOut);
                }

                pixelPx = _mm_add_ps(pixelPx, four_4x);
                pixel += 4;

                if(xI + 8 < maxX)
                {
                    clipMask = _mm_set1_epi8(-1);
                }
                else
                {
                    clipMask = endClipMask;
                }
                
                IACA_VC64_END;
            }
        
            row += rowAdvance;
        }

        END_TIMED_BLOCK_COUNTED(ProcessPixel, GetClampedRectArea(fillRect) / 2);
    }
    
    END_TIMED_BLOCK(DrawRectangleQuickly);
}
