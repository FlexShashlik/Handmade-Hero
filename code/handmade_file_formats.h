#define HHA_CODE(a, b, c, d) (((ui32)(a) << 0) | ((ui32)(b) << 8) | ((ui32)(c) << 16) | ((ui32)(d) << 24))

#pragma pack(push, 1)
struct bitmap_id
{
    ui32 value;
};

struct sound_id
{
    ui32 value;
};

struct hha_header
{
#define HHA_MAGIC_VALUE HHA_CODE('h', 'h', 'a', 'f')
    ui32 magicValue;

#define HHA_VERSION 0
    ui32 version;

    ui32 tagCount;
    ui32 assetTypeCount;
    ui32 assetCount;

    ui64 tags;
    ui64 assetTypes;
    ui64 assets;

    // TODO: Primacy numbers for asset files
};

struct hha_tag
{
    ui32 id;
    r32 value;
};

struct hha_asset_type
{
    ui32 typeID;
    ui32 firstAssetIndex;
    ui32 onePastLastAssetIndex;
};

struct hha_bitmap
{
    ui32 dim[2];
    r32 alignPercentage[2];
};

struct hha_sound
{
    ui32 sampleCount;
    ui32 channelCount;
    sound_id nextIDToPlay;
};

struct hha_asset
{
    ui64 dataOffset;
    ui32 firstTagIndex;
    ui32 onePastLastTagIndex;

    union
    {
        hha_bitmap bitmap;
        hha_sound sound;
    };
};
#pragma pack(pop)
