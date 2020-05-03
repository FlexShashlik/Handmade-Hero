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

struct WAVE_header
{
    ui32 riffID;
    ui32 size;
    ui32 waveID;
};

#define RIFF_CODE(a, b, c, d) (((ui32)(a) << 0) | ((ui32)(b) << 8) | ((ui32)(c) << 16) | ((ui32)(d) << 24))
enum
{
    WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};

struct WAVE_chunk
{
    ui32 id;
    ui32 size;
};

struct WAVE_fmt
{
    ui16 wFormatTag;
    ui16 nChannels;
    ui32 nSamplesPerSec;
    ui32 nAvgBytesPerSec;
    ui16 nBlockAlign;
    ui16 wBitsPerSample;
    ui16 cbSize;
    ui16 wValidBitsPerSample;
    ui32 dwChannelMask;
    ui8 subFormat[16];
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

struct riff_iterator
{
    ui8 *at;
    ui8 *stop;
};

inline riff_iterator
ParseChunkAt(void *at, void *stop)
{
    riff_iterator iter;

    iter.at = (ui8 *)at;
    iter.stop = (ui8 *)stop;
    
    return iter;
}

inline riff_iterator
NextChunk(riff_iterator iter)
{
    WAVE_chunk *chunk = (WAVE_chunk *)iter.at;
    ui32 size = (chunk->size + 1) & ~1;
    iter.at += sizeof(WAVE_chunk) + size;
    
    return iter;
}

inline b32
IsValid(riff_iterator iter)
{
    b32 result = iter.at < iter.stop;
    return result;
}

inline void *
GetChunkData(riff_iterator iter)
{
    void *result = iter.at + sizeof(WAVE_chunk);
    return result;
}

inline ui32
GetType(riff_iterator iter)
{
    WAVE_chunk *chunk = (WAVE_chunk *)iter.at;
    ui32 result = chunk->id;
    return result;
}

inline ui32
GetChunkDataSize(riff_iterator iter)
{
    WAVE_chunk *chunk = (WAVE_chunk *)iter.at;
    ui32 result = chunk->size;
    return result;
}

internal loaded_sound
DEBUGLoadWAV(char *fileName, ui32 sectionFirstSampleIndex, ui32 sectionSampleCount)
{
    loaded_sound result = {};
    
    debug_read_file_result readResult = DEBUGPlatformReadEntireFile(fileName);
    if(readResult.contentsSize != 0)
    {
        WAVE_header *header = (WAVE_header *)readResult.contents;
        Assert(header->riffID == WAVE_ChunkID_RIFF);
        Assert(header->waveID == WAVE_ChunkID_WAVE);

        ui32 channelCount = 0;
        ui32 sampleDataSize = 0;
        i16 *sampleData = 0;
        for(riff_iterator iter = ParseChunkAt(header + 1, (ui8 *)(header + 1) + header->size - 4);
            IsValid(iter);
            iter = NextChunk(iter))
        {
            switch(GetType(iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(iter);
                    Assert(fmt->wFormatTag == 1); // NOTE: Only support PCM
                    Assert(fmt->nSamplesPerSec == 48000);
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == sizeof(i16) * fmt->nChannels);
                    channelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    sampleData = (i16 *)GetChunkData(iter);
                    sampleDataSize = GetChunkDataSize(iter);
                } break;
            }
        }

        Assert(channelCount && sampleData);

        result.channelCount = channelCount;
        ui32 sampleCount = sampleDataSize / (channelCount * sizeof(i16));
        if(channelCount == 1)
        {
            result.samples[0] = sampleData;
            result.samples[1] = 0;
        }
        else if(channelCount == 2)
        {
            result.samples[0] = sampleData;
            result.samples[1] = sampleData + sampleCount;
/*
            for(ui32 sampleIndex = 0; sampleIndex < result.sampleCount; sampleIndex++)
            {
                sampleData[2 * sampleIndex + 0] = (i16)sampleIndex;
                sampleData[2 * sampleIndex + 1] = (i16)sampleIndex;
            }
*/
            for(ui32 sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
            {
                i16 source = sampleData[2 * sampleIndex];
                sampleData[2 * sampleIndex] = sampleData[sampleIndex];
                sampleData[sampleIndex] = source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO: Load right channel!
        b32 atEnd = true;
        result.channelCount = 1;
        if(sectionSampleCount)
        {
            Assert(sectionFirstSampleIndex + sectionSampleCount <= sampleCount);
            atEnd = sectionFirstSampleIndex + sectionSampleCount == sampleCount;
            sampleCount = sectionSampleCount;
            for(ui32 channelIndex = 0; channelIndex < result.channelCount; channelIndex++)
            {
                result.samples[channelIndex] += sectionFirstSampleIndex;
            }
        }

        if(atEnd)
        {
            // TODO: All sounds have to be padded with their subsequent
            // sound out to 8 samples past their end
            for(ui32 channelIndex = 0; channelIndex < result.channelCount; channelIndex++)
            {
                for(ui32 sampleIndex = sampleCount; sampleIndex < sampleCount + 8; sampleIndex++)
                {
                    result.samples[channelIndex][sampleIndex] = 0;
                }
            }
        }

        result.sampleCount = sampleCount;
    }

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

    asset_bitmap_info *info = &work->assets->assets[work->id.value].bitmap;
    *work->bitmap = DEBUGLoadBMP(info->fileName, info->alignPercentage);
    
    CompletePreviousWritesBeforeFutureWrites;
    
    work->assets->slots[work->id.value].bitmap = work->bitmap;
    work->assets->slots[work->id.value].state = work->finalState;

    EndTaskWithMemory(work->task);
}

internal void
LoadBitmap(game_assets *assets, bitmap_id id)
{
    if(id.value &&
       AtomicCompareExchangeUI32((ui32 *)&assets->slots[id.value].state, AssetState_Queued, AssetState_Unloaded)
       == AssetState_Unloaded)
    {
        task_with_memory *task = BeginTaskWithMemory(assets->tranState);

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
        else
        {
            assets->slots[id.value].state = AssetState_Unloaded;
        }
    }
}

struct load_sound_work
{
    game_assets *assets;
    sound_id id;
    task_with_memory *task;
    loaded_sound *sound;

    asset_state finalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadSoundWork)
{
    load_sound_work *work = (load_sound_work *)data;

    asset_sound_info *info = &work->assets->assets[work->id.value].sound;
    *work->sound = DEBUGLoadWAV(info->fileName, info->firstSampleIndex, info->sampleCount);
    
    CompletePreviousWritesBeforeFutureWrites;
    
    work->assets->slots[work->id.value].sound = work->sound;
    work->assets->slots[work->id.value].state = work->finalState;

    EndTaskWithMemory(work->task);
}

internal void
LoadSound(game_assets *assets, sound_id id)
{
    if(id.value &&
       AtomicCompareExchangeUI32((ui32 *)&assets->slots[id.value].state, AssetState_Queued, AssetState_Unloaded)
       == AssetState_Unloaded)
    {
        task_with_memory *task = BeginTaskWithMemory(assets->tranState);

        if(task)
        {
            load_sound_work *work = PushStruct(&task->arena, load_sound_work);

            work->assets = assets;
            work->id = id;
            work->task = task;
            work->sound = PushStruct(&assets->arena, loaded_sound);
            work->finalState = AssetState_Loaded;
            
            PlatformAddEntry(assets->tranState->lowPriorityQueue, LoadSoundWork, work);
        }
        else
        {
            assets->slots[id.value].state = AssetState_Unloaded;
        }
    }
}

internal ui32
GetBestMatchAssetFrom
(
    game_assets *assets, asset_type_id typeID,
    asset_vector *matchVector, asset_vector *weightVector
)
{
    ui32 result = 0;
    
    r32 bestDiff = R32MAX;

    asset_type *type = assets->assetTypes + typeID;
    for(ui32 assetIndex = type->firstAssetIndex; assetIndex < type->onePastLastAssetIndex; assetIndex++)
    {
        asset *asset = assets->assets + assetIndex;

        r32 totalWeightedDiff = 0.0f;
        for(ui32 tagIndex = asset->firstTagIndex; tagIndex < asset->onePastLastTagIndex; tagIndex++)
        {
            asset_tag *tag = assets->tags + tagIndex;

            r32 a = matchVector->e[tag->id];
            r32 b = tag->value;
            r32 d0 = AbsoluteValue(a - b);
            r32 d1 = AbsoluteValue(a - assets->tagRange[tag->id] * SignOf(a) - b);
            r32 difference = Minimum(d0, d1);
            
            r32 weighted = weightVector->e[tag->id] * difference;
            totalWeightedDiff += weighted;
        }
        
        if(bestDiff > totalWeightedDiff)
        {
            bestDiff = totalWeightedDiff;
            result = assetIndex;
        }
    }

    return result;
}

internal bitmap_id
GetBestMatchBitmapFrom
(
    game_assets *assets, asset_type_id typeID,
    asset_vector *matchVector, asset_vector *weightVector
)
{
    bitmap_id result = {GetBestMatchAssetFrom(assets, typeID, matchVector, weightVector)};
    return result;
}

internal sound_id
GetBestMatchSoundFrom
(
    game_assets *assets, asset_type_id typeID,
    asset_vector *matchVector, asset_vector *weightVector
)
{
    sound_id result = {GetBestMatchAssetFrom(assets, typeID, matchVector, weightVector)};
    return result;
}

internal ui32
GetRandomSlotFrom(game_assets *assets, asset_type_id typeID, random_series *series)
{
    ui32 result = 0;

    asset_type *type = assets->assetTypes + typeID;
    if(type->firstAssetIndex != type->onePastLastAssetIndex)
    {
        ui32 count = type->onePastLastAssetIndex - type->firstAssetIndex;
        ui32 choice = RandomChoice(series, count);
        
        result = type->firstAssetIndex + choice;
    }
    
    return result;
}

internal bitmap_id
GetRandomBitmapFrom(game_assets *assets, asset_type_id typeID, random_series *series)
{
    bitmap_id result = {GetRandomSlotFrom(assets, typeID, series)};
    return result;
}

internal sound_id
GetRandomSoundFrom(game_assets *assets, asset_type_id typeID, random_series *series)
{
    sound_id result = {GetRandomSlotFrom(assets, typeID, series)};
    return result;
}

internal ui32
GetFirstSlotFrom(game_assets *assets, asset_type_id typeID)
{
    ui32 result = 0;

    asset_type *type = assets->assetTypes + typeID;
    if(type->firstAssetIndex != type->onePastLastAssetIndex)
    {
        result = type->firstAssetIndex;
    }
    
    return result;
}

inline bitmap_id
GetFirstBitmapFrom(game_assets *assets, asset_type_id typeID)
{
    bitmap_id result = {GetFirstSlotFrom(assets, typeID)};
    return result;
}

inline sound_id
GetFirstSoundFrom(game_assets *assets, asset_type_id typeID)
{
    sound_id result = {GetFirstSlotFrom(assets, typeID)};
    return result;
}

internal void
BeginAssetType(game_assets *assets, asset_type_id typeID)
{
    Assert(assets->debugAssetType == 0);
    
    assets->debugAssetType = assets->assetTypes + typeID;
    assets->debugAssetType->firstAssetIndex = assets->debugUsedAssetCount;
    assets->debugAssetType->onePastLastAssetIndex = assets->debugAssetType->firstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets *assets, char *fileName, v2 alignPercentage = {0.5f, 0.5f})
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < assets->assetCount);

    bitmap_id result = {assets->debugAssetType->onePastLastAssetIndex++};
    asset *asset = assets->assets + result.value;
    asset->firstTagIndex = assets->debugUsedTagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;
    
    asset->bitmap.fileName = PushString(&assets->arena, fileName);
    asset->bitmap.alignPercentage = alignPercentage;
        
    assets->debugAsset = asset;

    return result;
}

internal sound_id
AddSoundAsset(game_assets *assets, char *fileName, ui32 firstSampleIndex = 0, ui32 sampleCount = 0)
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < assets->assetCount);

    sound_id result = {assets->debugAssetType->onePastLastAssetIndex++};
    asset *asset = assets->assets + result.value;
    asset->firstTagIndex = assets->debugUsedTagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;

    asset->sound.fileName = PushString(&assets->arena, fileName);
    asset->sound.firstSampleIndex = firstSampleIndex;
    asset->sound.sampleCount = sampleCount;
    asset->sound.nextIDToPlay.value = 0;

    assets->debugAsset = asset;

    return result;
}

internal void
AddTag(game_assets *assets, asset_tag_id id, r32 value)
{
    Assert(assets->debugAsset);

    assets->debugAsset->onePastLastTagIndex++;
    asset_tag *tag = assets->tags + assets->debugUsedTagCount++;

    tag->id = id;
    tag->value = value;
}

internal void
EndAssetType(game_assets *assets)
{
    Assert(assets->debugAssetType);
    assets->debugUsedAssetCount = assets->debugAssetType->onePastLastAssetIndex;
    assets->debugAssetType = 0;
    assets->debugAsset = 0;
}

internal game_assets *
AllocateGameAssets(memory_arena *arena, memory_index size, transient_state *tranState)
{
    game_assets *assets = PushStruct(arena, game_assets);
    SubArena(&assets->arena, arena, size);
    assets->tranState = tranState;

    for(ui32 tagType = 0; tagType < Tag_Count; tagType++)
    {
        assets->tagRange[Tag_FacingDirection] = 1000000.0f;
    }
    
    assets->tagRange[Tag_FacingDirection] = Tau32;
    
    assets->assetCount = 2 * 256 * Asset_Count;
    assets->assets = PushArray(arena, assets->assetCount, asset);
    assets->slots = PushArray(arena, assets->assetCount, asset_slot);

    assets->tagCount = 1024 * Asset_Count;
    assets->tags = PushArray(arena, assets->tagCount, asset_tag);

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

    r32 angleRight = 0.0f * Tau32;
    r32 angleBack = 0.25f * Tau32;
    r32 angleLeft = 0.5f * Tau32;
    r32 angleFront = 0.75f * Tau32;
    
    v2 heroAlign = {0.5f, 0.156682029f};
    
    BeginAssetType(assets, Asset_Head);
    AddBitmapAsset(assets, "test/test_hero_right_head.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_head.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_head.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_head.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleFront);
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Cape);
    AddBitmapAsset(assets, "test/test_hero_right_cape.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_cape.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_cape.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_cape.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleFront);
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Torso);
    AddBitmapAsset(assets, "test/test_hero_right_torso.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_torso.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_torso.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_torso.bmp", heroAlign);
    AddTag(assets, Tag_FacingDirection, angleFront);
    EndAssetType(assets);

    //
    //
    //

    BeginAssetType(assets, Asset_Bloop);
    AddSoundAsset(assets, "test3/bloop_00.wav");
    AddSoundAsset(assets, "test3/bloop_01.wav");
    AddSoundAsset(assets, "test3/bloop_02.wav");
    AddSoundAsset(assets, "test3/bloop_03.wav");
    AddSoundAsset(assets, "test3/bloop_04.wav");
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Crack);
    AddSoundAsset(assets, "test3/crack_00.wav");
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Drop);
    AddSoundAsset(assets, "test3/drop_00.wav");
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Glide);
    AddSoundAsset(assets, "test3/glide_00.wav");
    EndAssetType(assets);

    ui32 oneMusicChunk = 10 * 48000;
    ui32 totalMusicSampleCount = 7468095;
    BeginAssetType(assets, Asset_Music);
    sound_id lastMusic = {0};
    for(ui32 firstSampleIndex = 0; firstSampleIndex < totalMusicSampleCount; firstSampleIndex += oneMusicChunk)
    {
        ui32 sampleCount = totalMusicSampleCount - firstSampleIndex;
        if(sampleCount > oneMusicChunk)
        {
            sampleCount = oneMusicChunk;
        }
        
        sound_id thisMusic = AddSoundAsset(assets, "test3/music_test.wav", firstSampleIndex, sampleCount);
        if(IsValid(lastMusic))
        {
            assets->assets[lastMusic.value].sound.nextIDToPlay = thisMusic;
        }
        
        lastMusic = thisMusic;
    }
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Puhp);
    AddSoundAsset(assets, "test3/puhp_00.wav");
    AddSoundAsset(assets, "test3/puhp_01.wav");
    EndAssetType(assets);
    
    return assets;
}
