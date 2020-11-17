struct load_asset_work
{
    task_with_memory *task;
    asset_slot *slot;

    platform_file_handle *handle;
    ui64 offset;
    ui64 size;
    void *destination;

    asset_state finalState;
};

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work *work = (load_asset_work *)data;

    Platform.ReadDataFromFile(work->handle, work->offset, work->size, work->destination);

    CompletePreviousWritesBeforeFutureWrites;

    // TODO: Should we fill in bogus data here and set to final state anyway?
    if(PlatformNoFileErrors(work->handle))
    {
        work->slot->state = work->finalState;
    }
    
    EndTaskWithMemory(work->task);
}

inline platform_file_handle *
GetFileHandleFor(game_assets *assets, ui32 fileIndex)
{
    Assert(fileIndex < assets->fileCount);
    platform_file_handle *result = assets->files[fileIndex].handle;

    return result;
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
            asset *asset = assets->assets + id.value;
            hha_bitmap *info = &asset->hha.bitmap;
            loaded_bitmap *bitmap = PushStruct(&assets->arena, loaded_bitmap);
            
            bitmap->alignPercentage = v2{info->alignPercentage[0], info->alignPercentage[1]};
            bitmap->widthOverHeight = (r32)info->dim[0] / (r32)info->dim[1];
            bitmap->width = info->dim[0];
            bitmap->height = info->dim[1];
            bitmap->pitch = 4 * info->dim[0];
            ui32 memorySize = bitmap->pitch * bitmap->height;
            bitmap->memory = PushSize(&assets->arena, memorySize);

            load_asset_work *work = PushStruct(&task->arena, load_asset_work);
            work->task = task;
            work->slot = assets->slots + id.value;
            work->handle = GetFileHandleFor(assets, asset->fileIndex);
            work->offset = asset->hha.dataOffset;
            work->size = memorySize;
            work->destination = bitmap->memory;
            work->finalState = AssetState_Loaded;
            work->slot->bitmap = bitmap;
    
            Platform.AddEntry(assets->tranState->lowPriorityQueue, LoadAssetWork, work);
        }
        else
        {
            assets->slots[id.value].state = AssetState_Unloaded;
        }
    }
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
            asset *asset = assets->assets + id.value;
            hha_sound *info = &asset->hha.sound;

            loaded_sound *sound = PushStruct(&assets->arena, loaded_sound);
            sound->sampleCount = info->sampleCount;
            sound->channelCount = info->channelCount;
            ui32 channelSize = sound->sampleCount * sizeof(i16);
            ui32 memorySize = sound->channelCount * channelSize;
            void *memory = PushSize(&assets->arena, memorySize);
            
            i16 *soundAt = (i16 *)memory;
            for(ui32 channelIndex = 0;
                channelIndex < sound->channelCount;
                channelIndex++)
            {
                sound->samples[channelIndex] = soundAt;
                soundAt += channelSize;
            }
            
            load_asset_work *work = PushStruct(&task->arena, load_asset_work);
            work->task = task;
            work->slot = assets->slots + id.value;
            work->handle = GetFileHandleFor(assets, asset->fileIndex);
            work->offset = asset->hha.dataOffset;
            work->size = memorySize;
            work->destination = memory;
            work->finalState = AssetState_Loaded;
            work->slot->sound = sound;
            
            Platform.AddEntry(assets->tranState->lowPriorityQueue, LoadAssetWork, work);
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
        for(ui32 tagIndex = asset->hha.firstTagIndex; tagIndex < asset->hha.onePastLastTagIndex; tagIndex++)
        {
            hha_tag *tag = assets->tags + tagIndex;

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

    assets->tagCount = 0;
    assets->assetCount = 0;

    {
        platform_file_group fileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        assets->fileCount = fileGroup.fileCount;
        assets->files = PushArray(arena, assets->fileCount, asset_file);
        for(ui32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++)
        {
            asset_file *file = assets->files + fileIndex;

            file->tagBase = assets->tagCount;
            
            ZeroStruct(file->header);
            file->handle = Platform.OpenFile(fileGroup, fileIndex);
            Platform.ReadDataFromFile(file->handle, 0, sizeof(file->header), &file->header);

            ui64 assetTypeArraySize = file->header.assetTypeCount * sizeof(hha_asset_type);

            file->assetTypeArray = (hha_asset_type *)PushSize(arena, assetTypeArraySize);
            Platform.ReadDataFromFile(file->handle, file->header.assetTypes,
                                     assetTypeArraySize, file->assetTypeArray);

            if(file->header.magicValue != HHA_MAGIC_VALUE)
            {
                Platform.FileError(file->handle, "HHA file has an invalid magic value.");
            }

            if(file->header.version > HHA_VERSION)
            {
                Platform.FileError(file->handle, "HHA file is of a later version.");
            }
        
            if(PlatformNoFileErrors(file->handle))
            {
                assets->tagCount += file->header.tagCount;
                assets->assetCount += file->header.assetCount;
            }
            else
            {
                // TODO: Notifying users about this
                InvalidCodePath;
            }
        }
        
        Platform.GetAllFilesOfTypeEnd(fileGroup);
    }
    
    // NOTE: Allocate all metadata space
    assets->assets = PushArray(arena, assets->assetCount, asset);
    assets->slots = PushArray(arena, assets->assetCount, asset_slot);
    assets->tags = PushArray(arena, assets->tagCount, hha_tag);

    // NOTE: Load tags
    for(ui32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++)
    {
        asset_file *file = assets->files + fileIndex;
        if(PlatformNoFileErrors(file->handle))
        {
            ui32 tagArraySize = sizeof(hha_tag) * file->header.tagCount;
            Platform.ReadDataFromFile(file->handle, file->header.tags,
                                      tagArraySize, assets->tags + file->tagBase);
        }
    }
    
    ui32 assetCount = 0;
    for(ui32 destTypeID = 0; destTypeID < Asset_Count; destTypeID++)
    {
        asset_type *destType = assets->assetTypes + destTypeID;
        destType->firstAssetIndex = assetCount;
        
        for(ui32 fileIndex = 0; fileIndex < assets->fileCount; fileIndex++)
        {
            asset_file *file = assets->files + fileIndex;
            if(PlatformNoFileErrors(file->handle))
            {
                for(ui32 sourceIndex = 0;
                    sourceIndex < file->header.assetTypeCount;
                    sourceIndex++)
                {
                    hha_asset_type *sourceType = file->assetTypeArray + sourceIndex;
                    
                    if(sourceType->typeID == destTypeID)
                    {
                        ui32 assetCountForType = (sourceType->onePastLastAssetIndex -
                                                  sourceType->firstAssetIndex);

                        temporary_memory tempMem = BeginTemporaryMemory(&tranState->tranArena);
                        hha_asset *hhaAssetArray = PushArray(&tranState->tranArena,
                                                             assetCountForType, hha_asset);
                        Platform.ReadDataFromFile(file->handle,
                                                 file->header.assets + sourceType->firstAssetIndex * sizeof(hha_asset),
                                                 assetCountForType * sizeof(hha_asset),
                                                 hhaAssetArray);

                        for(ui32 assetIndex = 0;
                            assetIndex < assetCountForType;
                            assetIndex++)
                        {
                            hha_asset *hhaAsset = hhaAssetArray + assetIndex;
                            
                            Assert(assetCount < assets->assetCount);
                            asset *asset = assets->assets + assetCount++;
                            
                            asset->fileIndex = fileIndex;
                            asset->hha = *hhaAsset;
                            asset->hha.firstTagIndex += file->tagBase;
                            asset->hha.onePastLastTagIndex += file->tagBase;
                        }

                        EndTemporaryMemory(tempMem);
                    }
                }
            }
        }

        destType->onePastLastAssetIndex = assetCount;
    }
    
    Assert(assetCount == assets->assetCount);
    
#if 0
    debug_read_file_result readResult = Platform.DEBUGReadEntireFile("test.hha");
    if(readResult.contentsSize != 0)
    {
        hha_header *header = (hha_header *)readResult.contents;
        
        assets->assetCount = header->assetCount;
        assets->assets = (hha_asset *)((ui8 *)readResult.contents + header->assets);
        assets->slots = PushArray(arena, assets->assetCount, asset_slot);
        
        assets->tagCount = header->tagCount;
        assets->tags = (hha_tag *)((ui8 *)readResult.contents + header->tags);
        
        hha_asset_type *hhaAssetTypes = (hha_asset_type *)((ui8 *)readResult.contents + header->assetTypes);
        for(ui32 index = 0; index < header->assetTypeCount; index++)
        {
            hha_asset_type *source = hhaAssetTypes + index;
            if(source->typeID < Asset_Count)
            {
                asset_type *dest = assets->assetTypes + source->typeID;

                // TODO: Support merging
                Assert(dest->firstAssetIndex == 0);
                Assert(dest->onePastLastAssetIndex == 0);
                    
                dest->firstAssetIndex = source->firstAssetIndex;
                dest->onePastLastAssetIndex = source->onePastLastAssetIndex;
            }
        }
        
        assets->hhaContents = (ui8 *)readResult.contents;
    }
#endif
    
    return assets;
}
