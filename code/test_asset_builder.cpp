#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"

FILE *Out = 0;

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
    ui32 nextIDToPlay;
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
    ui64 dataOffset;
    ui32 firstTagIndex;
    ui32 onePastLastTagIndex;
};

#define VERY_LARGE_NUMBER 4096

ui32 bitmapCount;
asset_bitmap_info bitmapInfos[VERY_LARGE_NUMBER];

ui32 soundCount;
asset_sound_info soundInfos[VERY_LARGE_NUMBER];

ui32 tagCount;
asset_tag tags[VERY_LARGE_NUMBER];
    
ui32 assetCount;
asset assets[VERY_LARGE_NUMBER];

asset_type assetTypes[Asset_Count];

ui32 debugUsedBitmapCount;
ui32 debugUsedSoundCount;
ui32 debugUsedAssetCount;
ui32 debugUsedTagCount;
asset_type *debugAssetType;
asset *debugAsset;

internal void
BeginAssetType(asset_type_id typeID)
{
    Assert(debugAssetType == 0);
    
    debugAssetType = assetTypes + typeID;
    debugAssetType->firstAssetIndex = debugUsedAssetCount;
    debugAssetType->onePastLastAssetIndex = debugAssetType->firstAssetIndex;
}

internal void
AddBitmapAsset(char *fileName, r32 alignPercentageX, r32 alignPercentageY)
{
    Assert(debugAssetType);
    Assert(debugAssetType->onePastLastAssetIndex < assetCount);
    
    asset *asset = assets + debugAssetType->onePastLastAssetIndex++;
    asset->firstTagIndex = debugUsedTagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;
    asset->slotID = DEBUGAddBitmapInfo(assets, fileName, alignPercentageX, alignPercentageY).value;
/*
    internal bitmap_id
        DEBUGAddBitmapInfo(char *fileName, v2 alignPercentage)
    {
        Assert(assets->debugUsedBitmapCount < assets->bitmapCount);
        bitmap_id id = {assets->debugUsedBitmapCount++};

        asset_bitmap_info *info = assets->bitmapInfos + id.value;
        info->fileName = PushString(&assets->arena, fileName);
        info->alignPercentage = alignPercentage;
    
        return id;
    }
*/

    assets->debugAsset = asset;
}

internal asset *
AddSoundAsset(game_assets *assets, char *fileName, ui32 firstSampleIndex = 0, ui32 sampleCount = 0)
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < assets->assetCount);
    
    asset *asset = assets->assets + assets->debugAssetType->onePastLastAssetIndex++;
    asset->firstTagIndex = assets->debugUsedTagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;
    asset->slotID = DEBUGAddSoundInfo(assets, fileName, firstSampleIndex, sampleCount).value;
/*
    internal sound_id
        DEBUGAddSoundInfo(char *fileName, ui32 firstSampleIndex, ui32 sampleCount)
    {
        Assert(assets->debugUsedSoundCount < assets->soundCount);
        sound_id id = {assets->debugUsedSoundCount++};

        asset_sound_info *info = assets->soundInfos + id.value;
        info->fileName = PushString(&assets->arena, fileName);
        info->firstSampleIndex = firstSampleIndex;
        info->sampleCount = sampleCount;
        info->nextIDToPlay.value = 0;
    
        return id;
    }
*/
    
    assets->debugAsset = asset;

    return asset;
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

int
main(int argCount, char **args)
{
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

    ui32 oneMusicChunk = 10 * 40000;
    ui32 totalMusicSampleCount = 7468095;
    BeginAssetType(assets, Asset_Music);
    asset *lastMusic = 0;
    for(ui32 firstSampleIndex = 0; firstSampleIndex < totalMusicSampleCount; firstSampleIndex += oneMusicChunk)
    {
        ui32 sampleCount = totalMusicSampleCount - firstSampleIndex;
        if(sampleCount > oneMusicChunk)
        {
            sampleCount = oneMusicChunk;
        }
        
        asset *thisMusic= AddSoundAsset(assets, "test3/music_test.wav", firstSampleIndex, sampleCount);
        if(lastMusic)
        {
            assets->soundInfos[lastMusic->slotID].nextIDToPlay.value = thisMusic->slotID;
        }
        
        lastMusic = thisMusic;
    }
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Puhp);
    AddSoundAsset(assets, "test3/puhp_00.wav");
    AddSoundAsset(assets, "test3/puhp_01.wav");
    EndAssetType(assets);
    
    Out = fopen("test.hha", "wb");
    if(Out)
    {
        
        fclose(Out);
    }
}
