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
DEBUGLoadBMP(char *fileName, v2 alignPercentage = {0.5f, 0.5f})
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
        result.alignPercentage = alignPercentage;
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

struct load_bitmap_work
{
    game_assets *assets;
    bitmap_id id;
    task_with_memory *task;
    loaded_bitmap *bitmap;

    asset_state finalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
    load_bitmap_work *work = (load_bitmap_work *)data;

    asset_bitmap_info *info = work->assets->bitmapInfos + work->id.value;
    *work->bitmap = DEBUGLoadBMP(info->fileName, info->alignPercentage);
    
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
            work->task = task;
            work->bitmap = PushStruct(&assets->arena, loaded_bitmap);
            work->finalState = AssetState_Loaded;
            
            PlatformAddEntry(assets->tranState->lowPriorityQueue, LoadBitmapWork, work);
        }
    }
}

internal void
LoadSound(game_assets *assets, ui32 id)
{
}

internal bitmap_id
RandomAssetFrom(game_assets *assets, asset_type_id typeID, random_series *series)
{
    bitmap_id result = {};

    asset_type *type = assets->assetTypes + typeID;
    if(type->firstAssetIndex != type->onePastLastAssetIndex)
    {
        ui32 count = type->onePastLastAssetIndex - type->firstAssetIndex;
        ui32 choice = RandomChoice(series, count);
        
        asset *asset = assets->assets + type->firstAssetIndex + choice;
        result.value = asset->slotID;
    }
    
    return result;
}

internal bitmap_id
GetFirstBitmapID(game_assets *assets, asset_type_id typeID)
{
    bitmap_id result = {};

    asset_type *type = assets->assetTypes + typeID;
    if(type->firstAssetIndex != type->onePastLastAssetIndex)
    {
        asset *asset = assets->assets + type->firstAssetIndex;
        result.value = asset->slotID;
    }
    
    return result;
}

internal bitmap_id
DEBUGAddBitmapInfo(game_assets *assets, char *fileName, v2 alignPercentage)
{
    Assert(assets->debugUsedBitmapCount < assets->bitmapCount);
    bitmap_id id = {assets->debugUsedBitmapCount++};

    asset_bitmap_info *info = assets->bitmapInfos + id.value;
    info->fileName = fileName;
    info->alignPercentage = alignPercentage;
    
    return id;
}

internal void
BeginAssetType(game_assets *assets, asset_type_id typeID)
{
    Assert(assets->debugAssetType == 0);
    assets->debugAssetType = assets->assetTypes + typeID;
    assets->debugAssetType->firstAssetIndex = assets->debugUsedAssetCount;
    assets->debugAssetType->onePastLastAssetIndex = assets->debugAssetType->firstAssetIndex;
}

internal void
AddBitmapAsset(game_assets *assets, char *fileName, v2 alignPercentage = {0.5f, 0.5f})
{
    Assert(assets->debugAssetType);
    asset *asset = assets->assets + assets->debugAssetType->onePastLastAssetIndex++;
    asset->firstTagIndex = 0;
    asset->onePastLastTagIndex = 0;
    asset->slotID = DEBUGAddBitmapInfo(assets, fileName, alignPercentage).value;
}

internal void
EndAssetType(game_assets *assets)
{
    Assert(assets->debugAssetType);
    assets->debugUsedAssetCount = assets->debugAssetType->onePastLastAssetIndex;
    assets->debugAssetType = 0;
}

internal game_assets *
AllocateGameAssets(memory_arena *arena, memory_index size, transient_state *tranState)
{
    game_assets *assets = PushStruct(arena, game_assets);
    SubArena(&assets->arena, arena, size);
    assets->tranState = tranState;

    assets->bitmapCount = 256 * Asset_Count;
    assets->bitmapInfos = PushArray(arena, assets->bitmapCount, asset_bitmap_info);
    assets->bitmaps = PushArray(arena, assets->bitmapCount, asset_slot);

    assets->soundCount = 1;
    assets->sounds = PushArray(arena, assets->soundCount, asset_slot);

    assets->assetCount = assets->soundCount + assets->bitmapCount;
    assets->assets = PushArray(arena, assets->assetCount, asset);

    assets->debugUsedBitmapCount = 1;
    assets->debugUsedAssetCount = 1;
    
    BeginAssetType(assets, Asset_Shadow);
    AddBitmapAsset(assets, "test/test_hero_shadow.bmp", {0.5f, 0.156682029f});
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Tree);
    AddBitmapAsset(assets, "test2/tree00.bmp", {0.493827164f, 0.295652181f});
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Sword);
    AddBitmapAsset(assets, "test2/rock03.bmp", {0.5f, 0.65625f});
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Grass);
    AddBitmapAsset(assets, "test2/grass00.bmp");
    AddBitmapAsset(assets, "test2/grass01.bmp");
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Tuft);
    AddBitmapAsset(assets, "test2/tuft00.bmp");
    AddBitmapAsset(assets, "test2/tuft01.bmp");
    AddBitmapAsset(assets, "test2/tuft02.bmp");
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Stone);
    AddBitmapAsset(assets, "test2/ground00.bmp");
    AddBitmapAsset(assets, "test2/ground01.bmp");
    AddBitmapAsset(assets, "test2/ground02.bmp");
    AddBitmapAsset(assets, "test2/ground03.bmp");
    EndAssetType(assets);
        
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
