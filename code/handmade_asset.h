struct hero_bitmaps
{
    loaded_bitmap head;
    loaded_bitmap cape;
    loaded_bitmap torso;
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
    loaded_bitmap *bitmap;
};

enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Flatness,

    Tag_Count
};

enum asset_type_id
{
    Asset_None,
    Asset_Backdrop,
    Asset_Shadow,
    Asset_Tree,
    Asset_Sword,
    Asset_Stairwell,
    Asset_Rock,
    
    Asset_Count
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

struct asset
{
    ui32 firstTagIndex;
    ui32 onePastLastTagIndex;
    ui32 slotId;
};

struct asset_bitmap_info
{
    v2 alignPercentage;
    r32 widthOverHeight;
    i32 width;
    i32 height;
};

struct game_assets
{
    // TODO: Not thrilled about this back-pointer
    struct transient_state *tranState;
    memory_arena arena;

    ui32 bitmapCount;
    asset_slot *bitmaps;

    ui32 soundCount;
    asset_slot *sounds;

    ui32 tagCount;
    asset_tag *tags;
    
    ui32 assetCount;
    asset *assets;
    
    asset_type assetTypes[Asset_Count];

    // NOTE: Array'd assets
    loaded_bitmap grass[2];
    loaded_bitmap stone[4];
    loaded_bitmap tuft[3];

    // NOTE: Structured assets
    hero_bitmaps heroBitmaps[4];
};

struct bitmap_id
{
    ui32 value;
};

struct audio_id
{
    ui32 value;
};

inline loaded_bitmap *
GetBitmap(game_assets *assets, bitmap_id id)
{
    loaded_bitmap *result = assets->bitmaps[id.value].bitmap;
    return result;
}

internal void LoadBitmap(game_assets *assets, bitmap_id id);
internal void LoadSound(game_assets *assets, audio_id id);
