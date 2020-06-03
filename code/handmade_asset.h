struct loaded_sound
{
    ui32 sampleCount; // NOTE: This is the sample count divided by 8
    ui32 channelCount;
    i16 *samples[2];
};

struct asset_bitmap_info
{
    char *fileName;
    v2 alignPercentage;
};

struct asset_sound_info
{
    char *fileName;
    ui32 firstSampleIndex;
    ui32 sampleCount;
    sound_id nextIDToPlay;
};

struct asset_type
{
    ui32 firstAssetIndex;
    ui32 onePastLastAssetIndex;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked
};

struct asset_slot
{
    asset_state state;
    union
    {
        loaded_bitmap *bitmap;
        loaded_sound *sound;
    };
};

struct asset_vector
{
    r32 e[Tag_Count];
};

struct asset_file
{
    //platform_file_handle handle;

    // TODO: If we ever do thread stacks, AssetTypeArray doesn't
    // actually need to be kept here probably.
    hha_header header;
    hha_asset_type *assetTypeArray;

    ui32 tagBase;
};

struct game_assets
{
    // TODO: Not thrilled about this back-pointer
    struct transient_state *tranState;
    memory_arena arena;

    r32 tagRange[Tag_Count];

    ui32 fileCount;
    asset_file *files;
    
    ui32 tagCount;
    hha_tag *tags;
    
    ui32 assetCount;
    hha_asset *assets;

    asset_slot *slots;
    
    asset_type assetTypes[Asset_Count];

    ui8 *hhaContents;

#if 0
    // NOTE: Structured assets
    //hero_bitmaps heroBitmaps[4];

    // TODO: These should go away when we actually load an asset pack-file
    ui32 debugUsedAssetCount;
    ui32 debugUsedTagCount;
    asset_type *debugAssetType;
    asset *debugAsset;
#endif
};

inline loaded_bitmap *
GetBitmap(game_assets *assets, bitmap_id id)
{
    Assert(id.value <= assets->assetCount);
    loaded_bitmap *result = assets->slots[id.value].bitmap;
    
    return result;
}

inline b32
IsValid(bitmap_id id)
{
    b32 result = (id.value != 0);
    return result;
}

inline loaded_sound *
GetSound(game_assets *assets, sound_id id)
{
    Assert(id.value <= assets->assetCount);
    loaded_sound *result = assets->slots[id.value].sound;
    
    return result;
}

inline hha_sound *
GetSoundInfo(game_assets *assets, sound_id id)
{
    Assert(id.value <= assets->assetCount);
    hha_sound *result = &assets->assets[id.value].sound;

    return result;
}

inline b32
IsValid(sound_id id)
{
    b32 result = (id.value != 0);
    return result;
}

internal void LoadBitmap(game_assets *assets, bitmap_id id);
inline void PrefetchBitmap(game_assets *assets, bitmap_id id) {LoadBitmap(assets, id);};
internal void LoadSound(game_assets *assets, sound_id id);
inline void PrefetchSound(game_assets *assets, sound_id id) {LoadSound(assets, id);}
