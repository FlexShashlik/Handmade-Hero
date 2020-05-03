#include "test_asset_builder.h"

FILE *Out = 0;

internal void
BeginAssetType(game_assets *assets, asset_type_id typeID)
{
    Assert(assets->debugAssetType == 0);
    
    assets->debugAssetType = assets->assetTypes + typeID;
    assets->debugAssetType->typeID = typeID;
    assets->debugAssetType->firstAssetIndex = assets->assetCount;
    assets->debugAssetType->onePastLastAssetIndex = assets->debugAssetType->firstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets *assets, char *fileName, r32 alignPercentageX = 0.5f, r32 alignPercentageY = 0.5f)
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < ArrayCount(assets->assets));

    bitmap_id result = {assets->debugAssetType->onePastLastAssetIndex++};
    asset *asset = assets->assets + result.value;
    asset->firstTagIndex = assets->tagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;
    
    asset->bitmap.fileName = fileName;
    asset->bitmap.alignPercentage[0] = alignPercentageX;
    asset->bitmap.alignPercentage[1] = alignPercentageY;
        
    assets->debugAsset = asset;

    return result;
}

internal sound_id
AddSoundAsset(game_assets *assets, char *fileName, ui32 firstSampleIndex = 0, ui32 sampleCount = 0)
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < ArrayCount(assets->assets));

    sound_id result = {assets->debugAssetType->onePastLastAssetIndex++};
    asset *asset = assets->assets + result.value;
    asset->firstTagIndex = assets->tagCount;
    asset->onePastLastTagIndex = asset->firstTagIndex;

    asset->sound.fileName = fileName;
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
    hha_tag *tag = assets->tags + assets->tagCount++;

    tag->id = id;
    tag->value = value;
}

internal void
EndAssetType(game_assets *assets)
{
    Assert(assets->debugAssetType);
    assets->assetCount = assets->debugAssetType->onePastLastAssetIndex;
    assets->debugAssetType = 0;
    assets->debugAsset = 0;
}

int
main(int argCount, char **args)
{
    game_assets assets_;
    game_assets *assets = &assets_;

    assets->tagCount = 1;
    assets->assetCount = 1;
    assets->debugAssetType = 0;
    assets->debugAsset = 0;
    
    BeginAssetType(assets, Asset_Shadow);
    AddBitmapAsset(assets, "test/test_hero_shadow.bmp", 0.5f, 0.156682029f);
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Tree);
    AddBitmapAsset(assets, "test2/tree00.bmp", 0.493827164f, 0.295652181f);
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Sword);
    AddBitmapAsset(assets, "test2/rock03.bmp", 0.5f, 0.65625f);
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
    
    r32 heroAlign[] = {0.5f, 0.156682029f};
    
    BeginAssetType(assets, Asset_Head);
    AddBitmapAsset(assets, "test/test_hero_right_head.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_head.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_head.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_head.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleFront);
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Cape);
    AddBitmapAsset(assets, "test/test_hero_right_cape.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_cape.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_cape.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_cape.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleFront);
    EndAssetType(assets);
    
    BeginAssetType(assets, Asset_Torso);
    AddBitmapAsset(assets, "test/test_hero_right_torso.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleRight);
    AddBitmapAsset(assets, "test/test_hero_back_torso.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleBack);
    AddBitmapAsset(assets, "test/test_hero_left_torso.bmp", heroAlign[0], heroAlign[1]);
    AddTag(assets, Tag_FacingDirection, angleLeft);
    AddBitmapAsset(assets, "test/test_hero_front_torso.bmp", heroAlign[0], heroAlign[1]);
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
        if(lastMusic.value)
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
    
    Out = fopen("test.hha", "wb");
    if(Out)
    {
        hha_header header = {};
        header.magicValue = HHA_MAGIC_VALUE;
        header.version = HHA_VERSION;
        header.tagCount = assets->tagCount;
        header.assetTypeCount = Asset_Count; // TODO: Do we really want to do this?
        header.assetCount = assets->assetCount;

        ui32 tagArraySize = header.tagCount * sizeof(hha_tag);
        ui32 assetTypeArraySize = header.assetTypeCount * sizeof(hha_asset_type);
        ui32 assetArraySize = header.assetCount * sizeof(hha_asset);

        header.tags = sizeof(header);
        header.assetTypes = header.tags + tagArraySize;
        header.assets = header.assetTypes + assetTypeArraySize;

        fwrite(&header, sizeof(header), 1, Out);
        fwrite(assets->tags, tagArraySize, 1, Out);
        fwrite(assets->assetTypes, assetTypeArraySize, 1, Out);
        //fwrite(assetArray, assetArraySize, 1, Out);
        
        fclose(Out);
    }
    else
    {
        printf("ERROR: Couldn't open file :C\n");
    }
}
