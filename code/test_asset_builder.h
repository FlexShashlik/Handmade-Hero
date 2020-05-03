#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"
#include "handmade_file_formats.h"

struct bitmap_id
{
    ui32 value;
};

struct sound_id
{
    ui32 value;
};

struct asset_bitmap_info
{
    char *fileName;
    r32 alignPercentage[2];
};

struct asset_sound_info
{
    char *fileName;
    ui32 firstSampleIndex;
    ui32 sampleCount;
    sound_id nextIDToPlay;
};

struct asset
{
    ui64 dataOffset;
    ui32 firstTagIndex;
    ui32 onePastLastTagIndex;

    union
    {
        asset_bitmap_info bitmap;
        asset_sound_info sound;
    };
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
    ui32 tagCount;
    hha_tag tags[VERY_LARGE_NUMBER];
    
    ui32 assetTypeCount;
    hha_asset_type assetTypes[Asset_Count];
    
    ui32 assetCount;
    asset assets[VERY_LARGE_NUMBER];

    hha_asset_type *debugAssetType;
    asset *debugAsset;
};
