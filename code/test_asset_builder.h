#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"
#include "handmade_file_formats.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

enum asset_type
{
    AssetType_Sound,
    AssetType_Bitmap
};

struct asset_source
{
    asset_type type;
    char *fileName;
    ui32 firstSampleIndex;
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
    ui32 tagCount;
    hha_tag tags[VERY_LARGE_NUMBER];
    
    ui32 assetTypeCount;
    hha_asset_type assetTypes[Asset_Count];
    
    ui32 assetCount;
    asset_source assetSources[VERY_LARGE_NUMBER];
    hha_asset assets[VERY_LARGE_NUMBER];
    
    hha_asset_type *debugAssetType;
    ui32 assetIndex;
};
