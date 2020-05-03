struct bitmap_id
{
    ui32 value;
};

struct sound_id
{
    ui32 value;
};

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

struct asset_tag
{
    ui32 id;
    r32 value;
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

struct asset
{
    ui32 firstTagIndex;
    ui32 onePastLastTagIndex;

    union
    {
        asset_bitmap_info bitmap;
        asset_sound_info sound;
    };
};

struct asset_vector
{
    r32 e[Tag_Count];
};

struct game_assets
{
    // TODO: Not thrilled about this back-pointer
    struct transient_state *tranState;
    memory_arena arena;

    r32 tagRange[Tag_Count];
    
    ui32 tagCount;
    asset_tag *tags;
    
    ui32 assetCount;
    asset *assets;

    asset_slot *slots;
    
    asset_type assetTypes[Asset_Count];
    
    // NOTE: Structured assets
    //hero_bitmaps heroBitmaps[4];

    // TODO: These should go away when we actually load an asset pack-file
    ui32 debugUsedAssetCount;
    ui32 debugUsedTagCount;
    asset_type *debugAssetType;
    asset *debugAsset;
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

inline asset_sound_info *
GetSoundInfo(game_assets *assets, sound_id id)
{
    Assert(id.value <= assets->assetCount);
    asset_sound_info *result = &assets->assets[id.value].sound;

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
