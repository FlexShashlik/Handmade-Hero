#include "test_asset_builder.h"

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

struct loaded_bitmap
{
    i32 width;
    i32 height;
    i32 pitch;
    void *memory;

    void *free;
};

struct entire_file
{
    ui32 contentsSize;
    void *contents;
};

entire_file
ReadEntireFile(char *fileName)
{
    entire_file result = {};

    FILE *in = fopen(fileName, "rb");
    if(in)
    {
        fseek(in, 0, SEEK_END);
        result.contentsSize = ftell(in);
        fseek(in, 0, SEEK_SET);
        
        result.contents = malloc(result.contentsSize);
        fread(result.contents, result.contentsSize, 1, in);
        fclose(in);
    }
    else
    {
        printf("ERROR: Cannot open file %s.\n", fileName);
    }
    
    return result;
}

internal loaded_bitmap
LoadBMP(char *fileName)
{
    loaded_bitmap result = {};

    // NOTE: Byte order in memory is determined by the header itself
    
    entire_file readResult = ReadEntireFile(fileName);

    if(readResult.contentsSize != 0)
    {
        result.free = readResult.contents;

        bitmap_header *header = (bitmap_header *)readResult.contents;
        
        result.width = header->width;
        result.height = header->height;

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

struct loaded_sound
{
    ui32 sampleCount; // NOTE: This is the sample count divided by 8
    ui32 channelCount;
    i16 *samples[2];
    void *free;
};

internal loaded_sound
LoadWAV(char *fileName, ui32 sectionFirstSampleIndex, ui32 sectionSampleCount)
{
    loaded_sound result = {};
    
    entire_file readResult = ReadEntireFile(fileName);
    if(readResult.contentsSize != 0)
    {
        result.free = readResult.contents;

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
    asset_source *source = assets->assetSources + result.value;
    hha_asset *hha = assets->assets + result.value;
    
    hha->firstTagIndex = assets->tagCount;
    hha->onePastLastTagIndex = hha->firstTagIndex;
    hha->bitmap.alignPercentage[0] = alignPercentageX;
    hha->bitmap.alignPercentage[1] = alignPercentageY;

    source->type = AssetType_Bitmap;
    source->fileName = fileName;
    
    assets->assetIndex = result.value;

    return result;
}

internal sound_id
AddSoundAsset(game_assets *assets, char *fileName, ui32 firstSampleIndex = 0, ui32 sampleCount = 0)
{
    Assert(assets->debugAssetType);
    Assert(assets->debugAssetType->onePastLastAssetIndex < ArrayCount(assets->assets));

    sound_id result = {assets->debugAssetType->onePastLastAssetIndex++};
    asset_source *source = assets->assetSources + result.value;
    hha_asset *hha = assets->assets + result.value;
    
    hha->firstTagIndex = assets->tagCount;
    hha->onePastLastTagIndex = hha->firstTagIndex;
    hha->sound.sampleCount = sampleCount;
    hha->sound.nextIDToPlay = 0;

    source->type = AssetType_Sound;
    source->fileName = fileName;
    source->firstSampleIndex = firstSampleIndex;

    assets->assetIndex = result.value;

    return result;
}

internal void
AddTag(game_assets *assets, asset_tag_id id, r32 value)
{
    Assert(assets->assetIndex);

    hha_asset *hha = assets->assets + assets->assetIndex;
    hha->onePastLastTagIndex++;
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
    assets->assetIndex = 0;
}

int
main(int argCount, char **args)
{
    game_assets assets_;
    game_assets *assets = &assets_;

    assets->tagCount = 1;
    assets->assetCount = 1;
    assets->debugAssetType = 0;
    assets->assetIndex = 0;
    
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
            assets->assets[lastMusic.value].sound.nextIDToPlay = thisMusic.value;
        }
        
        lastMusic = thisMusic;
    }
    EndAssetType(assets);

    BeginAssetType(assets, Asset_Puhp);
    AddSoundAsset(assets, "test3/puhp_00.wav");
    AddSoundAsset(assets, "test3/puhp_01.wav");
    EndAssetType(assets);
    
    FILE *out = fopen("test.hha", "wb");
    if(out)
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

        fwrite(&header, sizeof(header), 1, out);
        fwrite(assets->tags, tagArraySize, 1, out);
        fwrite(assets->assetTypes, assetTypeArraySize, 1, out);
        fseek(out, assetArraySize, SEEK_CUR);
        for(ui32 assetIndex = 1; assetIndex < header.assetCount; assetIndex++)
        {
            asset_source *source = assets->assetSources + assetIndex;
            hha_asset *dest = assets->assets + assetIndex;
            
            dest->dataOffset = ftell(out);
                
            if(source->type == AssetType_Sound)
            {
                loaded_sound wav = LoadWAV(source->fileName,
                                            source->firstSampleIndex,
                                            dest->sound.sampleCount);
                dest->sound.sampleCount = wav.sampleCount;
                dest->sound.channelCount = wav.channelCount;
                
                for(ui32 channelIndex = 0; channelIndex < wav.channelCount; channelIndex++)
                {
                    fwrite(wav.samples[channelIndex], dest->sound.sampleCount * sizeof(i16), 1, out);
                }
                
                free(wav.free);
            }
            else
            {
                Assert(source->type == AssetType_Bitmap);

                loaded_bitmap bitmap = LoadBMP(source->fileName);

                dest->bitmap.dim[0] = bitmap.width;
                dest->bitmap.dim[1] = bitmap.height;

                Assert(bitmap.width * 4 == bitmap.pitch);
                fwrite(bitmap.memory, bitmap.width * bitmap.height * 4, 1, out);
                
                free(bitmap.free);
            }
        }
        fseek(out, (ui32)header.assets, SEEK_SET);
        fwrite(assets->assets, assetArraySize, 1, out);
        
        fclose(out);
    }
    else
    {
        printf("ERROR: Couldn't open file :C\n");
    }
}
