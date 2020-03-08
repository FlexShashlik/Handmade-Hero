#pragma pack(push, 1)
struct bitmap_header
{
    ui16 fileType;
	ui32 fileSize;
	ui16 reserved1;
	ui16 reserved2;
	ui32 bitmapOffset;
    ui32 size;
    i32 width;
	i32 height;
	ui16 planes;
	ui16 bitsPerPixel;
    ui32 compression;
    ui32 sizeOfBitmap;
    i32 horzResolution;
    i32 vertResolution;
    ui32 colorsUsed;
    ui32 colorsImportant;

    ui32 redMask;
    ui32 greenMask;
    ui32 blueMask;
};
#pragma pack(pop)

inline v2
TopDownAlign(loaded_bitmap *bmp, v2 align)
{
    align.y = (r32)(bmp->height - 1) - align.y;

    align.x = SafeRatio0(align.x, (r32)bmp->width);
    align.y = SafeRatio0(align.y, (r32)bmp->height);
    
    return align;
}

internal void
SetTopDownAlign(hero_bitmaps *bmp, v2 align)
{
    align = TopDownAlign(&bmp->head, align);
    
    bmp->head.alignPercentage = align;
    bmp->cape.alignPercentage = align;
    bmp->torso.alignPercentage = align;
}

internal loaded_bitmap
DEBUGLoadBMP(char *fileName, i32 alignX, i32 topDownAlignY)
{
    loaded_bitmap result = {};

    // NOTE: Byte order in memory is determined by the header itself
    
    debug_read_file_result readResult;
    readResult = DEBUGPlatformReadEntireFile(fileName);

    if(readResult.contentsSize != 0)
    {
        bitmap_header *header = (bitmap_header *)readResult.contents;
        
        result.width = header->width;
        result.height = header->height;
        result.alignPercentage = TopDownAlign(&result, V2i(alignX, topDownAlignY));
        result.widthOverHeight = SafeRatio0((r32)result.width, (r32)result.height);

        Assert(result.height >= 0);
        Assert(header->compression == 3);

        ui32 *pixels = (ui32 *)((ui8 *)readResult.contents + header->bitmapOffset);
        result.memory = pixels;
        
        ui32 redMask = header->redMask;
        ui32 greenMask = header->greenMask;
        ui32 blueMask = header->blueMask;
        ui32 alphaMask = ~(redMask | greenMask | blueMask);
        
        bit_scan_result redScan = FindLeastSignificantSetBit(redMask);
        bit_scan_result greenScan = FindLeastSignificantSetBit(greenMask);
        bit_scan_result blueScan = FindLeastSignificantSetBit(blueMask);
        bit_scan_result alphaScan = FindLeastSignificantSetBit(alphaMask);

        Assert(redScan.isFound);
        Assert(greenScan.isFound);
        Assert(blueScan.isFound);
        Assert(alphaScan.isFound);

        i32 redShiftDown = (i32)redScan.index;
        i32 greenShiftDown = (i32)greenScan.index;
        i32 blueShiftDown = (i32)blueScan.index;
        i32 alphaShiftDown = (i32)alphaScan.index;
        
        ui32 *sourceDest = pixels;
        for(i32 y = 0; y < header->height; y++)
        {
            for(i32 x = 0; x < header->width; x++)
            {
                ui32 c = *sourceDest;

                v4 texel =
                    {
                        (r32)((c & redMask) >> redShiftDown),
                        (r32)((c & greenMask) >> greenShiftDown),
                        (r32)((c & blueMask) >> blueShiftDown),
                        (r32)((c & alphaMask) >> alphaShiftDown)
                    };

                texel = SRGB255ToLinear1(texel);
                
#if 1
                texel.rgb *= texel.a;
#endif

                texel = Linear1ToSRGB255(texel);
                
                *sourceDest++ = ((ui32)(texel.a + 0.5f) << 24|
                                 (ui32)(texel.r + 0.5f) << 16|
                                 (ui32)(texel.g + 0.5f) << 8 |
                                 (ui32)(texel.b + 0.5f) << 0);
            }
        }
    }

    result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
#if 0
    // NOTE: For top-down
    result.memory = (ui8 *)result.memory + result.pitch * (result.height - 1);
#endif
    
    return result;
}

internal loaded_bitmap
DEBUGLoadBMP(char *fileName)
{
    loaded_bitmap result = DEBUGLoadBMP(fileName, 0, 0);
    result.alignPercentage = v2{0.5f, 0.5f};

    return result;
}

struct load_bitmap_work
{
    game_assets *assets;
    char *fileName;
    bitmap_id id;
    task_with_memory *task;
    loaded_bitmap *bitmap;

    b32 hasAlignment;
    i32 alignX;
    i32 topDownAlignY;

    asset_state finalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
    load_bitmap_work *work = (load_bitmap_work *)data;

    if(work->hasAlignment)
    {
        *work->bitmap = DEBUGLoadBMP(work->fileName, work->alignX, work->topDownAlignY);
    }
    else
    {
        *work->bitmap = DEBUGLoadBMP(work->fileName);
    }

    CompletePreviousWritesBeforeFutureWrites;
    
    work->assets->bitmaps[work->id.value].bitmap = work->bitmap;
    work->assets->bitmaps[work->id.value].state = work->finalState;

    EndTaskWithMemory(work->task);
}

internal void
LoadBitmap(game_assets *assets, bitmap_id id)
{
    task_with_memory *task = BeginTaskWithMemory(assets->tranState);

    if(id.value &&
       AtomicComareExchangeUI32((ui32 *)&assets->bitmaps[id.value].state, AssetState_Unloaded, AssetState_Queued)
       == AssetState_Unloaded)
    {
        if(task)
        {
            load_bitmap_work *work = PushStruct(&task->arena, load_bitmap_work);

            work->assets = assets;
            work->id = id;
            work->fileName = "";
            work->task = task;
            work->bitmap = PushStruct(&assets->arena, loaded_bitmap);
            work->hasAlignment = false;
            work->finalState = AssetState_Loaded;
    
            switch(id.value)
            {
                case Asset_Backdrop:
                {
                    work->fileName = "test/test_background.bmp";
                } break;

                case Asset_Shadow:
                {
                    work->fileName = "test/test_hero_shadow.bmp";
                    work->hasAlignment = true;
                    work->alignX = 72;
                    work->topDownAlignY = 182;
                } break;

                case Asset_Tree:
                {
                    work->fileName = "test2/tree00.bmp";
                    work->hasAlignment = true;
                    work->alignX = 40;
                    work->topDownAlignY = 80;
                } break;

                case Asset_Stairwell:
                {
                    work->fileName = "test2/rock02.bmp";
                } break;

                case Asset_Sword:
                {
                    work->fileName = "test2/rock03.bmp";
                    work->hasAlignment = true;
                    work->alignX = 29;
                    work->topDownAlignY = 10;
                } break;
            }
        
            PlatformAddEntry(assets->tranState->lowPriorityQueue, LoadBitmapWork, work);
        }
    }
}

internal void
LoadSound(game_assets *assets, ui32 id)
{
}

internal bitmap_id
GetFirstBitmapID(game_assets *assets, asset_type_id typeId)
{
    bitmap_id result = {};

    asset_type *type = assets->assetTypes + typeId;
    if(type->firstAssetIndex != type->onePastLastAssetIndex)
    {
        asset *asset = assets->assets + type->firstAssetIndex;
        result.value = asset->slotId;
    }
    
    return result;
}

internal game_assets *
AllocateGameAssets(memory_arena *arena, memory_index size, transient_state *tranState)
{
    game_assets *assets = PushStruct(arena, game_assets);
    SubArena(&assets->arena, arena, size);
    assets->tranState = tranState;

    assets->bitmapCount = Asset_Count;
    assets->bitmaps = PushArray(arena, assets->bitmapCount, asset_slot);

    assets->soundCount = 1;
    assets->sounds = PushArray(arena, assets->soundCount, asset_slot);

    assets->tagCount = 0;
    assets->tags = 0;
    
    assets->assetCount = assets->bitmapCount;
    assets->assets = PushArray(arena, assets->assetCount, asset);

    for(ui32 assetId = 0; assetId < Asset_Count; assetId++)
    {
        asset_type *type = assets->assetTypes + assetId;
        type->firstAssetIndex = assetId;
        type->onePastLastAssetIndex = assetId + 1;

        asset *asset = assets->assets + type->firstAssetIndex;
        asset->firstTagIndex = 0;
        asset->onePastLastTagIndex = 0;
        asset->slotId = type->firstAssetIndex;
    }
    
    assets->grass[0] = DEBUGLoadBMP("test2/grass00.bmp");

    assets->grass[1] = DEBUGLoadBMP("test2/grass01.bmp");

    assets->tuft[0] = DEBUGLoadBMP("test2/tuft00.bmp");
    
    assets->tuft[1] = DEBUGLoadBMP("test2/tuft01.bmp");
    
    assets->tuft[2] = DEBUGLoadBMP("test2/tuft02.bmp");

    assets->stone[0] = DEBUGLoadBMP("test2/ground00.bmp");

    assets->stone[1] = DEBUGLoadBMP("test2/ground01.bmp");

    assets->stone[2] = DEBUGLoadBMP("test2/ground02.bmp");

    assets->stone[3] = DEBUGLoadBMP("test2/ground03.bmp");
        
    hero_bitmaps *heroBMP = assets->heroBitmaps;

    heroBMP->head = DEBUGLoadBMP("test/test_hero_right_head.bmp");
        
    heroBMP->cape = DEBUGLoadBMP("test/test_hero_right_cape.bmp");
            
    heroBMP->torso = DEBUGLoadBMP("test/test_hero_right_torso.bmp");

    SetTopDownAlign(heroBMP, v2{72, 182});

    heroBMP++;

    heroBMP->head = DEBUGLoadBMP("test/test_hero_back_head.bmp");
        
    heroBMP->cape = DEBUGLoadBMP("test/test_hero_back_cape.bmp");
            
    heroBMP->torso = DEBUGLoadBMP("test/test_hero_back_torso.bmp");

    SetTopDownAlign(heroBMP, v2{72, 182});

    heroBMP++;

    heroBMP->head = DEBUGLoadBMP("test/test_hero_left_head.bmp");
        
    heroBMP->cape = DEBUGLoadBMP("test/test_hero_left_cape.bmp");
            
    heroBMP->torso = DEBUGLoadBMP("test/test_hero_left_torso.bmp");

    SetTopDownAlign(heroBMP, v2{72, 182});
        
    heroBMP++;

    heroBMP->head = DEBUGLoadBMP("test/test_hero_front_head.bmp");
        
    heroBMP->cape = DEBUGLoadBMP("test/test_hero_front_cape.bmp");
            
    heroBMP->torso = DEBUGLoadBMP("test/test_hero_front_torso.bmp");

    SetTopDownAlign(heroBMP, v2{72, 182});
        
    heroBMP++;

    return assets;
}
