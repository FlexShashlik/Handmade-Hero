#include "handmade.h"
#include "handmade_render_group.h"
#include "handmade_render_group.cpp"
#include "handmade_world.cpp"
#include "handmade_random.h"
#include "handmade_sim_region.cpp"
#include "handmade_entity.cpp"

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
    i16 toneVolume = 3000;
    i32 wavePeriod = soundBuffer->samplesPerSecond / 400;
    i16 *sampleOut = soundBuffer->samples;
    
    for(i32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
#if 0
        real32 sineValue = sinf(gameState->tSine);
        i16 sampleValue = (i16)(sineValue * toneVolume);
#else
        i16 sampleValue = 0;
#endif
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
#if 0
        gameState->tSine += 2.0f * Pi32 / (real32)wavePeriod;
        if(gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

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
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP
(
    thread_context *thread,
    debug_platform_read_entire_file *readEntireFile,
    char *fileName
)
{
    loaded_bitmap result = {};

    // NOTE: Byte order in memory is determined by the header itself
    
    debug_read_file_result readResult;
    readResult = readEntireFile(thread, fileName);

    if(readResult.contentsSize != 0)
    {
        bitmap_header *header = (bitmap_header *)readResult.contents;

        Assert(header->compression == 3);
        
        result.width = header->width;
        result.height = header->height;

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

                r32 r = (r32)((c & redMask) >> redShiftDown);
                r32 g = (r32)((c & greenMask) >> greenShiftDown);
                r32 b = (r32)((c & blueMask) >> blueShiftDown);
                r32 a = (r32)((c & alphaMask) >> alphaShiftDown);
                r32 an = (a / 255.0f);

                r *= an;
                g *= an;
                b *= an;

                *sourceDest++ = ((ui32)(a + 0.5f) << 24|
                                 (ui32)(r + 0.5f) << 16|
                                 (ui32)(g + 0.5f) << 8 |
                                 (ui32)(b + 0.5f) << 0);
            }
        }
    }

    result.pitch = -result.width * BITMAP_BYTES_PER_PIXEL;
    result.memory = (ui8 *)result.memory - result.pitch * (result.height - 1);

    return result;
}

internal void
InitHitPoints(low_entity *lowEntity, ui32 hitPointCount)
{
    Assert(hitPointCount < ArrayCount(lowEntity->sim.hitPoint));
    lowEntity->sim.hitPointMax = hitPointCount;

    for(ui32 hitPointIndex = 0;
        hitPointIndex < lowEntity->sim.hitPointMax;
        hitPointIndex++)
    {
        hit_point *hitPoint = lowEntity->sim.hitPoint + hitPointIndex;
        hitPoint->flags = 0;
        hitPoint->filledAmount = HIT_POINT_SUB_COUNT;
    }
}

struct add_low_entity_result
{
    low_entity *low;
    ui32 lowIndex;
};

internal add_low_entity_result
AddLowEntity
(
    game_state *gameState,
    entity_type type, world_position pos
)
{
    Assert(gameState->lowEntityCount < ArrayCount(gameState->lowEntities));
    ui32 entityIndex = gameState->lowEntityCount++;

    low_entity *lowEntity = gameState->lowEntities + entityIndex;
    *lowEntity = {};
    lowEntity->sim.type = type;
    lowEntity->sim.collision = gameState->nullCollision;
    lowEntity->pos = NullPosition();

    ChangeEntityLocation
        (
            &gameState->worldArena,
            gameState->_world,
            entityIndex, lowEntity,
            pos
        );

    add_low_entity_result result = {};
    result.low = lowEntity;
    result.lowIndex = entityIndex;
    
    return result;
}

internal add_low_entity_result
AddGroundedEntity
(
    game_state *gameState,
    entity_type type, world_position pos,
    sim_entity_collision_volume_group *collision
)
{
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, type, pos
        );
    
    _entity.low->sim.collision = collision;
    
    return _entity;
}

inline world_position
ChunkPosFromTilePos
(
    world *_world,
    i32 absTileX, i32 absTileY, i32 absTileZ,
    v3 additionalOffset = v3{0, 0, 0}
)
{
    world_position basePos = {};

    r32 tileSideInMeters = 1.4f;
    r32 tileDepthInMeters = 3.0f;
    
    v3 tileDim =
        {
            tileSideInMeters,
            tileSideInMeters,
            tileDepthInMeters
        };
        
    v3 offset = Hadamard
        (
            tileDim,
            v3{(r32)absTileX, (r32)absTileY, (r32)absTileZ}
        );
        
    world_position result = MapIntoChunkSpace
        (
            _world,
            basePos, additionalOffset + offset
        );
    
    Assert(IsCanonical(_world, result._offset));
    
    return result;
}

internal add_low_entity_result
AddStandardRoom
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Space, pos,
            gameState->standardRoomCollision
        );

    AddFlags(&_entity.low->sim, EntityFlag_Traversable);

    return _entity;
}

internal add_low_entity_result
AddSword(game_state *gameState)
{
    add_low_entity_result _entity = AddLowEntity
        (
            gameState, EntityType_Sword, NullPosition()
        );

    _entity.low->sim.collision = gameState->swordCollision;
    
    AddFlags
        (
            &_entity.low->sim, EntityFlag_Moveable
        );
    
    return _entity;
}

internal add_low_entity_result
AddPlayer(game_state *gameState)
{
    world_position pos = gameState->cameraPos;
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Hero, pos,
            gameState->playerCollision
        );

    AddFlags
        (
            &_entity.low->sim, EntityFlag_Collides|EntityFlag_Moveable
        );
    
    InitHitPoints(_entity.low, 3);

    add_low_entity_result sword = AddSword(gameState);
    _entity.low->sim.sword.index = sword.lowIndex;
    
    if(gameState->cameraFollowingEntityIndex == 0)
    {
        gameState->cameraFollowingEntityIndex = _entity.lowIndex;
    }

    return _entity;
}

internal add_low_entity_result
AddMonster
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Monster, pos,
            gameState->monsterCollision
        );
    
    AddFlags
        (
            &_entity.low->sim,
            EntityFlag_Collides|EntityFlag_Moveable
        );
    
    InitHitPoints(_entity.low, 3);

    return _entity;
}

internal add_low_entity_result
AddFamiliar
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Familiar, pos,
            gameState->familiarCollision
        );
    
    AddFlags
        (
            &_entity.low->sim,
            EntityFlag_Collides|EntityFlag_Moveable
        );
    
    return _entity;
}

internal add_low_entity_result
AddWall
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Wall, pos,
            gameState->wallCollision
        );
    
    AddFlags(&_entity.low->sim, EntityFlag_Collides);
    
    return _entity;
}

internal add_low_entity_result
AddStair
(
    game_state *gameState,
    ui32 absTileX, ui32 absTileY, ui32 absTileZ
)
{
    world_position pos = ChunkPosFromTilePos
        (
            gameState->_world,
            absTileX, absTileY, absTileZ
        );
    add_low_entity_result _entity = AddGroundedEntity
        (
            gameState, EntityType_Stairwell, pos,
            gameState->stairCollision
        );

    AddFlags(&_entity.low->sim, EntityFlag_Collides);
    _entity.low->sim.walkableDim = _entity.low->sim.collision->totalVolume.dim.xy;
    _entity.low->sim.walkableHeight = gameState->typicalFloorHeight;
    
    return _entity;
}

internal void
DrawHitPoints
(
    sim_entity *entity,
    render_group *pieceGroup
)
{
    if(entity->hitPointMax >= 1)
    {
        v2 healthDim = {0.2f, 0.2f};
        r32 spacingX = 1.5f * healthDim.x;
        v2 hitPos =
            {
                (entity->hitPointMax - 1) * -0.5f * spacingX,
                -0.25f
            };
        v2 deltaHitPos = {spacingX, 0.0f};
                
        for(ui32 healthIndex = 0;
            healthIndex < entity->hitPointMax;
            healthIndex++)
        {
            hit_point *hitPoint = entity->hitPoint + healthIndex;
            v4 color = {1.0f, 0.0f, 0.0f, 1.0f};
            if(hitPoint->filledAmount == 0)
            {
                color = v4{0.2f, 0.2f, 0.2f, 1.0f};
            }
                        
            PushRect
                (
                    pieceGroup,
                    hitPos, 0, healthDim,
                    color, 0.0f
                );
            hitPos += deltaHitPos;
        }
    }
}


internal void
ClearCollisionRulesFor(game_state *gameState, ui32 storageIndex)
{
    for(ui32 hashBucket = 0;
        hashBucket < ArrayCount(gameState->collisionRuleHash);
        hashBucket++)
    {
        for(pairwise_collision_rule **rule = &gameState->collisionRuleHash[hashBucket];
            *rule;
            )
        {
            if((*rule)->storageIndexA == storageIndex ||
               (*rule)->storageIndexB == storageIndex)
            {
                pairwise_collision_rule *removedRule = *rule;
                *rule = (*rule)->nextInHash;

                removedRule->nextInHash = gameState->firstFreeCollisionRule;
                gameState->firstFreeCollisionRule = removedRule;
            }
            else
            {
                rule = &(*rule)->nextInHash;
            }
        }
    }
}

internal void
AddCollisionRule
(
    game_state *gameState,
    ui32 storageIndexA, ui32 storageIndexB, b32 canCollide
)
{
    if(storageIndexA > storageIndexB)
    {
        ui32 temp = storageIndexA;
        storageIndexA = storageIndexB;
        storageIndexB = temp;
    }
    
    // TODO: BETTER HASH FUNCTION
    pairwise_collision_rule *found = 0;
    ui32 hashBucket = storageIndexA & (ArrayCount(gameState->collisionRuleHash) - 1);
    for(pairwise_collision_rule *rule = gameState->collisionRuleHash[hashBucket];
        rule;
        rule = rule->nextInHash)
    {
        if(rule->storageIndexA == storageIndexA &&
           rule->storageIndexB == storageIndexB)
        {
            found = rule;
            break;
        }
    }

    if(!found)
    {
        found = gameState->firstFreeCollisionRule;
        if(found)
        {
            gameState->firstFreeCollisionRule = found->nextInHash;
        }
        else
        {
            found = PushStruct(&gameState->worldArena, pairwise_collision_rule);
        }

        found->nextInHash = gameState->collisionRuleHash[hashBucket];
        gameState->collisionRuleHash[hashBucket] = found;
    }

    if(found)
    {
        found->storageIndexA = storageIndexA;
        found->storageIndexB = storageIndexB;
        found->canCollide = canCollide;
    }
}

sim_entity_collision_volume_group *
MakeSimpleGroundedCollision
(
    game_state *gameState,
    r32 dimX, r32 dimY, r32 dimZ
)
{
    // TODO: NOT WORLD ARENA!
    sim_entity_collision_volume_group *group = PushStruct
        (
            &gameState->worldArena,
            sim_entity_collision_volume_group
        );
    group->volumeCount = 1;
    group->volumes = PushArray
        (
            &gameState->worldArena,
            group->volumeCount, sim_entity_collision_volume
        );
    group->totalVolume.offsetPos = v3{0, 0, 0.5f * dimZ};
    group->totalVolume.dim = v3{dimX, dimY, dimZ};
    group->volumes[0] = group->totalVolume;

    return group;
}

sim_entity_collision_volume_group *
MakeNullCollision(game_state *gameState)
{
    // TODO: NOT WORLD ARENA!
    sim_entity_collision_volume_group *group = PushStruct
        (
            &gameState->worldArena,
            sim_entity_collision_volume_group
        );
    group->volumeCount = 0;
    group->volumes = 0;
    group->totalVolume.offsetPos = v3{0, 0, 0};
    group->totalVolume.dim = v3{0, 0, 0};

    return group;
}

internal void
FillGroundChunk
(
    transient_state *tranState, game_state *gameState,
    ground_buffer *groundBuffer, world_position *chunkPos
)
{    
    temporary_memory groundMemory = BeginTemporaryMemory(&tranState->tranArena);
    render_group *renderGroup = AllocateRenderGroup
        (
            &tranState->tranArena,
            Megabytes(4),
            1.0f
        );

    Clear(renderGroup, v4{1.0f, 1.0f, 0.0f, 1.0f});
    
    loaded_bitmap *buffer = &groundBuffer->bitmap;

    groundBuffer->pos = *chunkPos;

    r32 width = (r32)buffer->width;
    r32 height = (r32)buffer->height;

    for(i32 chunkOffsetY = -1;
        chunkOffsetY <= 1;
        chunkOffsetY++)
    {
        for(i32 chunkOffsetX = -1;
            chunkOffsetX <= 1;
            chunkOffsetX++)
        {
            i32 chunkX = chunkPos->chunkX + chunkOffsetX;
            i32 chunkY = chunkPos->chunkY + chunkOffsetY;
            i32 chunkZ = chunkPos->chunkZ;
            
            random_series series = RandomSeed
                (139 * chunkX +
                 593 * chunkY +
                 329 * chunkZ);
    
            v2 center = v2{chunkOffsetX * width, -chunkOffsetY * height};
            
            for(ui32 grassIndex = 0;
                grassIndex < 100;
                grassIndex++)
            {
                loaded_bitmap *stamp;
        
                if(RandomChoice(&series, 2))
                {
                    stamp = gameState->grass + RandomChoice(&series, ArrayCount(gameState->grass));
                }
                else
                {
                    stamp = gameState->stone + RandomChoice(&series, ArrayCount(gameState->stone));
                }
        
                v2 bitmapCenter = 0.5f * V2i(stamp->width,
                                             stamp->height);
                // NOTE: Generate from [-1; -1] to [1; 1]
                v2 offset =
                    {
                        width * RandomUnilateral(&series),
                        height * RandomUnilateral(&series) 
                    };

                v2 pos = center + offset - bitmapCenter;
                
                PushBitmap
                    (
                        renderGroup, stamp,
                        pos, 0.0f, v2{0, 0}
                    );
            }
        }
    }
    
    for(i32 chunkOffsetY = -1;
        chunkOffsetY <= 1;
        chunkOffsetY++)
    {
        for(i32 chunkOffsetX = -1;
            chunkOffsetX <= 1;
            chunkOffsetX++)
        {
            i32 chunkX = chunkPos->chunkX + chunkOffsetX;
            i32 chunkY = chunkPos->chunkY + chunkOffsetY;
            i32 chunkZ = chunkPos->chunkZ;
            
            random_series series = RandomSeed
                (139 * chunkX +
                 593 * chunkY +
                 329 * chunkZ);
    
            v2 center = v2{chunkOffsetX * width, -chunkOffsetY * height};
            
            for(ui32 grassIndex = 0;
                grassIndex < 50;
                grassIndex++)
            {
                loaded_bitmap *stamp = gameState->tuft + RandomChoice(&series, ArrayCount(gameState->tuft));;
        
                v2 bitmapCenter = 0.5f * V2i(stamp->width,
                                             stamp->height);
                // NOTE: Generate from [-1; -1] to [1; 1]
                v2 offset =
                    {
                        width * RandomUnilateral(&series),
                        height * RandomUnilateral(&series) 
                    };

                v2 pos = center + offset - bitmapCenter;
  
                PushBitmap
                    (
                        renderGroup, stamp,
                        pos, 0.0f, v2{0, 0}
                    );
            }
        }
    }

    RenderGroupToOutput(renderGroup, buffer);    
    EndTemporaryMemory(groundMemory);
}

internal void
ClearBitmap(loaded_bitmap *bitmap)
{
    if(bitmap->memory)
    {
        i32 totalBitmapSize = bitmap->width * bitmap->height * BITMAP_BYTES_PER_PIXEL;
        ZeroSize(totalBitmapSize, bitmap->memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap
(
    memory_arena *arena,
    i32 width, i32 height, b32 clearToZero = true
)
{
    loaded_bitmap result = {};
    result.width = width;
    result.height = height;
    result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
    i32 totalBitmapSize = width * height * BITMAP_BYTES_PER_PIXEL;
    result.memory = PushSize_(arena, totalBitmapSize);
    if(clearToZero)
    {
        ClearBitmap(&result);
    }
    
    return result;
}

#if 0
internal void
RequestGroundBuffers
(
    world_position centerPos, rectangle3 bounds
)
{
    bounds = Offset(bounds, centerPos._offset);
    centerPos._offset = v3{0, 0, 0};
    
    for()
    {
    }
    
    // TODO: Real fill, this is just a test
    FillGroundChunk
        (
            tranState, gameState,
            tranState->groundBuffers,
            &gameState->cameraPos
        );
}
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    
    ui32 groundBufferWidth = 256;
    ui32 groundBufferHeight = 256;
    
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        ui32 tilesPerWidth = 17;
        ui32 tilesPerHeight = 9;

        gameState->typicalFloorHeight = 3.0f;
        gameState->metersToPixels = 42.0f;
        gameState->pixelsToMeters = 1.0f / gameState->metersToPixels;

        v3 worldChunkDimInMeters =
            {
                gameState->pixelsToMeters * (r32)groundBufferWidth,
                gameState->pixelsToMeters * (r32)groundBufferHeight,
                gameState->typicalFloorHeight
            };
        
        InitializeArena
            (
                &gameState->worldArena,
                memory->permanentStorageSize - sizeof(game_state),
                (ui8 *)memory->permanentStorage + sizeof(game_state)
            );
        
        // NOTE: Reserve the null entity slot
        AddLowEntity(gameState, EntityType_Null, NullPosition());

        gameState->_world = PushStruct
            (
                &gameState->worldArena,
                world
            );
        world *_world = gameState->_world;
        
        InitializeWorld(_world, worldChunkDimInMeters);

        r32 tileSideInMeters = 1.4f;
        r32 tileDepthInMeters = gameState->typicalFloorHeight;
        
        gameState->nullCollision = MakeNullCollision(gameState);
        gameState->swordCollision = MakeSimpleGroundedCollision
            (
                gameState, 1.0f, 0.5f, 0.1f
            );
        gameState->stairCollision = MakeSimpleGroundedCollision
            (
                gameState,
                tileSideInMeters,
                2.0f * tileSideInMeters,
                1.1f * tileDepthInMeters
            );
        gameState->playerCollision = MakeSimpleGroundedCollision
            (
                gameState, 1.0f, 0.5f, 1.2f
            );
        gameState->monsterCollision = MakeSimpleGroundedCollision
            (
                gameState, 1.0f, 0.5f, 0.5f
            );
        gameState->familiarCollision = MakeSimpleGroundedCollision
            (
                gameState, 1.0f, 0.5f, 0.5f
            );
        gameState->wallCollision = MakeSimpleGroundedCollision
            (
                gameState,
                tileSideInMeters,
                tileSideInMeters,
                tileDepthInMeters
            );
        gameState->standardRoomCollision = MakeSimpleGroundedCollision
            (
                gameState,
                tilesPerWidth * tileSideInMeters,
                tilesPerHeight * tileSideInMeters,
                0.9f * tileDepthInMeters
            );

        gameState->grass[0] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/grass00.bmp"
            );

        gameState->grass[1] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/grass01.bmp"
            );

        gameState->tuft[0] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/tuft00.bmp"
            );
        
        gameState->tuft[1] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/tuft01.bmp"
            );
        
        gameState->tuft[2] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/tuft02.bmp"
            );

        gameState->stone[0] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/ground00.bmp"
            );

        gameState->stone[1] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/ground01.bmp"
            );

        gameState->stone[2] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/ground02.bmp"
            );

        gameState->stone[3] = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/ground03.bmp"
            );
                
        gameState->bmp = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_background.bmp"
            );

        gameState->shadow = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_shadow.bmp"
            );

        gameState->tree = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/tree00.bmp"
            );

        gameState->stairwell = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/rock02.bmp"
            );

        gameState->sword = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test2/rock03.bmp"
            );
        
        hero_bitmaps *heroBMP = gameState->heroBitmaps;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_right_torso.bmp"
            );

        heroBMP->align = v2{72, 182};

        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_back_torso.bmp"
            );

        heroBMP->align = v2{72, 182};

        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_left_torso.bmp"
            );

        heroBMP->align = v2{72, 182};
        
        heroBMP++;

        heroBMP->head = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_head.bmp"
            );
        
        heroBMP->cape = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_cape.bmp"
            );
            
        heroBMP->torso = DEBUGLoadBMP
            (
                thread,
                memory->DEBUGPlatformReadEntireFile,
                "test/test_hero_front_torso.bmp"
            );

        heroBMP->align = v2{72, 182};
        
        heroBMP++;

        random_series series = RandomSeed(1234);
        
        ui32 screenBaseX = 0;
        ui32 screenBaseY = 0;
        ui32 screenBaseZ = 0;
        ui32 screenX = screenBaseX;
        ui32 screenY = screenBaseY;
        ui32 absTileZ = screenBaseZ;

        b32 isDoorTop = false;
        b32 isDoorLeft = false;
        b32 isDoorBottom = false;
        b32 isDoorRight = false;
        b32 isDoorUp = false;
        b32 isDoorDown = false;
        
        for(ui32 screenIndex = 0;
            screenIndex < 2000;
            screenIndex++)
        {
            ui32 doorDirection =
                RandomChoice(&series, 2);
            //RandomChoice(&series, (isDoorUp || isDoorDown) ? 2 : 3);
            
            b32 isCreatedZDoor = false;
            if(doorDirection == 2)
            {
                isCreatedZDoor = true;
                
                if(absTileZ == screenBaseZ)
                {
                    isDoorUp = true;
                }
                else
                {
                    isDoorDown = true;
                }
            }
            else if(doorDirection == 1)
            {
                isDoorRight = true;
            }
            else
            {
                isDoorTop = true;
            }

            AddStandardRoom
                (
                    gameState,
                    screenX * tilesPerWidth + tilesPerWidth / 2,
                    screenY * tilesPerHeight + tilesPerHeight / 2,
                    absTileZ
                );
            
            for(ui32 tileY = 0;
                tileY < tilesPerHeight;
                tileY++)
            {
                for(ui32 tileX = 0;
                    tileX < tilesPerWidth;
                    tileX++)
                {
                    ui32 absTileX = screenX * tilesPerWidth
                        + tileX;
                        
                    ui32 absTileY = screenY * tilesPerHeight
                        + tileY;

                    b32 isWall = false;
                    if(tileX == 0 &&
                       (!isDoorLeft || tileY != tilesPerHeight / 2))
                    {
                        isWall = true;
                    }

                    if(tileX == tilesPerWidth - 1 &&
                       (!isDoorRight || tileY != tilesPerHeight / 2))
                    {
                        isWall = true;
                    }

                    if(tileY == 0 &&
                       (!isDoorBottom || tileX != tilesPerWidth / 2))
                    {
                        isWall = true;
                    }

                    if(tileY == tilesPerHeight - 1 &&
                       (!isDoorTop || tileX != tilesPerWidth / 2))
                    {
                        isWall = true;
                    }
                    
                    if(isWall)
                    {
                        //if(screenIndex == 0)
                        {
                            AddWall(gameState, absTileX, absTileY, absTileZ);
                        }
                    }
                    else if(isCreatedZDoor)
                    {
                        if(tileX == 10 && tileY == 5)
                        {
                            AddStair
                                (
                                    gameState,
                                    absTileX, absTileY,
                                    isDoorDown ? absTileZ - 1 : absTileZ
                                );
                        }
                    }
                }
            }

            isDoorLeft = isDoorRight;
            isDoorBottom = isDoorTop;

            if(isCreatedZDoor)
            {
                isDoorUp = !isDoorUp;
                isDoorDown = !isDoorDown;
            }
            else
            {
                isDoorUp = false;
                isDoorDown = false;
            }
            
            isDoorTop = false;
            isDoorRight = false;

            if(doorDirection == 2)
            {
                if(absTileZ == screenBaseZ)
                {
                    absTileZ = screenBaseZ + 1;
                }
                else
                {
                    absTileZ = screenBaseZ;
                }
            }
            else if(doorDirection == 1)
            {
                screenX++;
            }
            else
            {
                screenY++;
            }
        }
        
        world_position newCameraPos = {};
        ui32 cameraTileX = screenBaseX * tilesPerWidth + 17/2;
        ui32 cameraTileY = screenBaseY * tilesPerHeight + 9/2;
        ui32 cameraTileZ = screenBaseZ;
        newCameraPos = ChunkPosFromTilePos
            (
                gameState->_world,
                cameraTileX, cameraTileY, cameraTileZ
            );

        gameState->cameraPos = newCameraPos;

        AddMonster
            (
                gameState,
                cameraTileX - 3, cameraTileY + 2, cameraTileZ
            );

        for(i32 familiarIndex = 0;
            familiarIndex < 1;
            familiarIndex++)
        {
            i32 familiarOffsetX = RandomBetween(&series, -7, 7);
            i32 familiarOffsetY = RandomBetween(&series, -3, -1);
            if((familiarOffsetX != 0) ||
               (familiarOffsetY != 0))
            {
                AddFamiliar
                    (
                        gameState,
                        cameraTileX + familiarOffsetX,
                        cameraTileY + familiarOffsetY,
                        cameraTileZ
                    );
            }
        }
        
        memory->isInitialized = true;
    }

    // NOTE: Transient initialization
    Assert(sizeof(transient_state) <= memory->transientStorageSize);
    transient_state *tranState = (transient_state *)memory->transientStorage;
    if(!tranState->isInitialized)
    {
        InitializeArena
            (
                &tranState->tranArena,
                memory->transientStorageSize - sizeof(transient_state),
                (ui8 *)memory->transientStorage + sizeof(transient_state)
            );

        tranState->groundBufferCount = 64;
        tranState->groundBuffers = PushArray
            (
                &tranState->tranArena,
                tranState->groundBufferCount,
                ground_buffer
            );

        for(ui32 groundBufferIndex = 0;
            groundBufferIndex < tranState->groundBufferCount;
            groundBufferIndex++)
        {
            ground_buffer *groundBuffer = tranState->groundBuffers + groundBufferIndex;
            groundBuffer->bitmap = MakeEmptyBitmap
                (
                    &tranState->tranArena,
                    groundBufferWidth,
                    groundBufferHeight,
                    false
                );
            groundBuffer->pos = NullPosition();
        }
                
        tranState->isInitialized = true;
    }

#if 0
    if(input->executableReloaded)
    {
        for(ui32 groundBufferIndex = 0;
            groundBufferIndex < tranState->groundBufferCount;
            groundBufferIndex++)
        {
            ground_buffer *groundBuffer = tranState->groundBuffers + groundBufferIndex;
            groundBuffer->pos = NullPosition();
        }
    }
#endif
    
    world *_world = gameState->_world;
    
    r32 metersToPixels = gameState->metersToPixels;
    r32 pixelsToMeters = 1.0f / metersToPixels;
    
    for(i32 controllerIndex = 0;
        controllerIndex < ArrayCount(input->controllers);
        controllerIndex++)
    {
        game_controller_input *controller = GetController(input, controllerIndex);
        controlled_hero *conHero = gameState->controlledHeroes + controllerIndex;
        if(conHero->entityIndex == 0)
        {
            if(controller->start.endedDown)
            {
                *conHero = {};
                conHero->entityIndex = AddPlayer(gameState).lowIndex;
            }
        }
        else
        {
            conHero->dZ = 0.0f;
            conHero->ddPos = {};
            conHero->dPosSword = {};
            
            if(controller->isAnalog)
            {
                // NOTE: Analog movement
                conHero->ddPos = v2
                    {
                        controller->stickAverageX,
                        controller->stickAverageY
                    };
            }
            else
            {
                // NOTE: Digital movement
                if(controller->moveUp.endedDown)
                {        
                    conHero->ddPos.y = 1.0f;
                }
            
                if(controller->moveDown.endedDown)
                {
                    conHero->ddPos.y = -1.0f;
                }
            
                if(controller->moveLeft.endedDown)
                {
                    conHero->ddPos.x = -1.0f;
                }
            
                if(controller->moveRight.endedDown)
                {
                    conHero->ddPos.x = 1.0f;
                }
            }

            if(controller->start.endedDown)
            {
                conHero->dZ = 3.0f;
            }

            conHero->dPosSword = {};
            if(controller->actionUp.endedDown)
            {
                conHero->dPosSword = v2{0.0f, 1.0f};
            }

            if(controller->actionDown.endedDown)
            {
                conHero->dPosSword = v2{0.0f, -1.0f};
            }

            if(controller->actionLeft.endedDown)
            {
                conHero->dPosSword = v2{-1.0f, 0.0f};
            }

            if(controller->actionRight.endedDown)
            {
                conHero->dPosSword = v2{1.0f, 0.0f};
            }
        }
    }
    
    //
    // NOTE: Render
    //
    
    temporary_memory renderMemory = BeginTemporaryMemory(&tranState->tranArena);
    // TODO: Decide what pushbuffer size is
    render_group *renderGroup = AllocateRenderGroup
        (
            &tranState->tranArena,
            Megabytes(4),
            gameState->metersToPixels
        );
    
    loaded_bitmap drawBuffer_ = {};
    loaded_bitmap *drawBuffer = &drawBuffer_;
    drawBuffer->width = buffer->width;
    drawBuffer->height = buffer->height;
    drawBuffer->pitch = buffer->pitch;
    drawBuffer->memory = buffer->memory;

    Clear(renderGroup, v4{1.0f, 0.0f, 1.0f, 0.0f});
    
    v2 screenCenter = {0.5f * (r32)drawBuffer->width,
                       0.5f * (r32)drawBuffer->height};
    
    r32 screenWidthInMeters = drawBuffer->width * pixelsToMeters;
    r32 screenHeightInMeters = drawBuffer->height * pixelsToMeters;
    rectangle3 cameraBoundsInMeters = RectCenterDim
        (
            v3{0, 0, 0},
            v3{screenWidthInMeters, screenHeightInMeters, 0}
        );

    for(ui32 groundBufferIndex = 0;
        groundBufferIndex < tranState->groundBufferCount;
        groundBufferIndex++)
    {
        ground_buffer *groundBuffer = tranState->groundBuffers + groundBufferIndex;
        if(IsValid(groundBuffer->pos))
        {
            loaded_bitmap *bitmap = &groundBuffer->bitmap;
            v3 delta = Subtract
                (
                    gameState->_world,
                    &groundBuffer->pos, &gameState->cameraPos
                );
    
            PushBitmap
                (
                    renderGroup,
                    bitmap, delta.xy, delta.z,
                    0.5f * V2i(bitmap->width, bitmap->height)
                );
        }
    }
    
    {        
        world_position minChunkPos = MapIntoChunkSpace
            (
                _world,
                gameState->cameraPos,
                GetMinCorner(cameraBoundsInMeters)
            );
        world_position maxChunkPos = MapIntoChunkSpace
            (
                _world,
                gameState->cameraPos,
                GetMaxCorner(cameraBoundsInMeters)
            );
    
        for(i32 chunkZ = minChunkPos.chunkZ;
            chunkZ <= maxChunkPos.chunkZ;
            chunkZ++)
        {
            for(i32 chunkY = minChunkPos.chunkY;
                chunkY <= maxChunkPos.chunkY;
                chunkY++)
            {
                for(i32 chunkX = minChunkPos.chunkX;
                    chunkX <= maxChunkPos.chunkX;
                    chunkX++)
                {
                    world_position chunkCenterPos = CenteredChunkPoint(chunkX, chunkY, chunkZ);
                    v3 relPos = Subtract
						(
							_world,
							&chunkCenterPos,
							&gameState->cameraPos
                        );

                    r32 furthestBufferLengthSq = 0.0f;
                    ground_buffer *furthestBuffer = 0;
                    for (ui32 groundBufferIndex = 0;
                         groundBufferIndex < tranState->groundBufferCount;
                         groundBufferIndex++)
                    {
                        ground_buffer *groundBuffer = tranState->groundBuffers + groundBufferIndex;
                        if (AreInSameChunk(_world,
                                           &groundBuffer->pos,
                                           &chunkCenterPos))
                        {
                            furthestBuffer = 0;
                            break;
                        }
                        else if (IsValid(groundBuffer->pos))
                        {
                            v3 relPos = Subtract
								(
									_world,
									&groundBuffer->pos,
									&gameState->cameraPos
                                );

                            r32 bufferLengthSq = LengthSq(relPos.xy);

                            if (furthestBufferLengthSq < bufferLengthSq)
                            {
                                furthestBufferLengthSq = bufferLengthSq;
                                furthestBuffer = groundBuffer;
                            }
                        }
                        else
                        {
                            furthestBufferLengthSq = R32MAX;
                            furthestBuffer = groundBuffer;
                        }
                    }

                    if (furthestBuffer)
                    {
                        FillGroundChunk
							(
								tranState, gameState,
								furthestBuffer, &chunkCenterPos
                            );
                    }

                    PushRectOutline
						(
							renderGroup,
							relPos.xy,
							0.0f,
							_world->chunkDimInMeters.xy,
							v4{ 1.0f, 1.0f, 0.0f, 1.0f }
						);
                }
            }
        }
    }

    // TODO: How big is it actually?
    v3 simBoundsExpansion = {15.0f, 15.0f, 15.0f};
    rectangle3 simBounds = AddRadiusTo
        (
            cameraBoundsInMeters,
            simBoundsExpansion
        );

    temporary_memory simMemory = BeginTemporaryMemory(&tranState->tranArena);
    sim_region *simRegion = BeginSim
        (
            &tranState->tranArena, gameState, gameState->_world,
            gameState->cameraPos, simBounds, input->deltaTime
        );
    
    // TODO: Move this out into handmade_entity.cpp
    for(ui32 entityIndex = 0;
        entityIndex < simRegion->entityCount;
        entityIndex++)
    {
        sim_entity *_entity = simRegion->entities + entityIndex;
        if(_entity->isUpdatable)
        {
            r32 deltaTime = input->deltaTime;
            
            // TODO: Should be computed after update
            r32 shadowAlpha = 1.0f - 0.5f * _entity->pos.z;
            if(shadowAlpha < 0.0f)
            {
                shadowAlpha = 0.0f;
            }

            move_spec moveSpec = DefaultMoveSpec();
            v3 ddPos = {};

            render_basis *basis = PushStruct(&tranState->tranArena, render_basis);
            renderGroup->defaultBasis = basis;
            
            hero_bitmaps *heroBitmaps = &gameState->
                heroBitmaps[_entity->facingDirection];
            switch(_entity->type)
            {
                case EntityType_Hero:
                {
                    for(ui32 controlIndex = 0;
                        controlIndex < ArrayCount(gameState->controlledHeroes);
                        controlIndex++)
                    {
                        controlled_hero *conHero = gameState->controlledHeroes + controlIndex;
                        if(_entity->storageIndex == conHero->entityIndex)
                        {
                            if(conHero->dZ != 0.0f)
                            {
                                _entity->dPos.z = conHero->dZ;
                            }
                            
                            moveSpec.isUnitMaxAccelVector = false;
                            moveSpec.speed = 50.0f;
                            moveSpec.drag = 8.0f;
                            ddPos = V3(conHero->ddPos, 0);
                        }

                        if(conHero->dPosSword.x != 0 || conHero->dPosSword.y != 0)
                        {
                            sim_entity *sword = _entity->sword.ptr;
                            if(sword && IsSet(sword, EntityFlag_Nonspatial))
                            {
                                sword->distanceLimit = 5.0f;
                                MakeEntitySpatial
                                    (
                                        sword,
                                        _entity->pos, 5.0f * V3(conHero->dPosSword, 0)
                                    );
                                
                                AddCollisionRule
                                    (
                                        gameState,
                                        sword->storageIndex, _entity->storageIndex, false
                                    );
                            }
                        }
                    }
            
                    PushBitmap
                        (
                            renderGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
            
                    PushBitmap
                        (
                            renderGroup, &heroBitmaps->torso,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );

                    PushBitmap
                        (
                            renderGroup, &heroBitmaps->cape,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );
    
                    PushBitmap
                        (
                            renderGroup, &heroBitmaps->head,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );

                    DrawHitPoints(_entity, renderGroup);
                } break;
            
                case EntityType_Wall:
                {
                    PushBitmap
                        (
                            renderGroup, &gameState->tree,
                            v2{0, 0}, 0,
                            v2{40, 80}
                        );
                } break;

                case EntityType_Stairwell:
                {
                    PushRect
                        (
                            renderGroup, v2{0, 0}, 0,
                            _entity->walkableDim,
                            v4{1, 0.5f, 0, 1},
                            0.0f
                        );
                    PushRect
                        (
                            renderGroup, v2{0, 0},
                            _entity->walkableHeight,
                            _entity->walkableDim,
                            v4{1, 1, 0, 1},
                            0.0f
                        );
                } break;

                case EntityType_Sword:
                {
                    moveSpec.isUnitMaxAccelVector = false;
                    moveSpec.speed = 0.0f;
                    moveSpec.drag = 0.0f;

                    if(_entity->distanceLimit == 0.0f)
                    {
                        ClearCollisionRulesFor
                            (
                                gameState,
                                _entity->storageIndex
                            );
                        MakeEntityNonSpatial(_entity);
                    }

                    PushBitmap
                        (
                            renderGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
                    PushBitmap
                        (
                            renderGroup, &gameState->sword,
                            v2{0, 0}, 0,
                            v2{29, 10}
                        );
                } break;
            
                case EntityType_Familiar:
                {
                    sim_entity *closestHero = 0;
                    r32 closestHeroDSq = Square(10.0f);

#if 0
                    sim_entity *testEntity = simRegion->entities;
                    for(ui32 testEntityIndex = 0;
                        testEntityIndex < simRegion->entityCount;
                        testEntityIndex++, testEntity++)
                    {
                        if(testEntity->type == EntityType_Hero)
                        {
                            r32 testDSq = LengthSq(testEntity->pos - _entity->pos);
                            if(closestHeroDSq > testDSq)
                            {
                                closestHero = testEntity;
                                closestHeroDSq = testDSq;
                            }
                        }
                    }
#endif
                    if(closestHero && closestHeroDSq > Square(3.0f))
                    {
                        v3 deltaPos = closestHero->pos - _entity->pos;
                        ddPos = (0.5f / SqRt(closestHeroDSq)) * deltaPos;
                    }
    
                    moveSpec.isUnitMaxAccelVector = true;
                    moveSpec.speed = 50.0f;
                    moveSpec.drag = 8.0f;
                    
                    _entity->tBob += deltaTime;
                    if(_entity->tBob > 2.0f * Pi32)
                    {
                        _entity->tBob -= 2.0f * Pi32;
                    }
                    r32 bobSin = Sin(2.0f * _entity->tBob);
                
                    PushBitmap
                        (
                            renderGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            0.5f * shadowAlpha + 0.2f * bobSin, 0.0f
                        );
                    PushBitmap
                        (
                            renderGroup, &heroBitmaps->head,
                            v2{0, 0},
                            0.25f * bobSin,
                            heroBitmaps->align
                        );
                } break;

                case EntityType_Monster:
                {
                    PushBitmap
                        (
                            renderGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
                    PushBitmap
                        (
                            renderGroup, &heroBitmaps->torso,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );
                
                    DrawHitPoints(_entity, renderGroup);
                } break;

                case EntityType_Space:
                {
                    for(ui32 volumeIndex = 0;
                        volumeIndex < _entity->collision->volumeCount;
                        volumeIndex++)
                    {
                        sim_entity_collision_volume *volume =
                            _entity->collision->volumes + volumeIndex;
                        PushRectOutline
                            (
                                renderGroup, volume->offsetPos.xy, 0,
                                volume->dim.xy,
                                v4{0, 0.5f, 1.0f, 1},
                                0.0f
                            );
                    }
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }

            if(!IsSet(_entity, EntityFlag_Nonspatial) &&
               IsSet(_entity, EntityFlag_Moveable))
            {
                MoveEntity
                    (
                        gameState, simRegion,
                        _entity, input->deltaTime,
                        &moveSpec, ddPos
                    );
            }

            basis->p = GetEntityGroundPoint(_entity);
        }
    }
    
    gameState->time += input->deltaTime;
    r32 angle = gameState->time;

    // TODO: Perp operator
    v2 origin = screenCenter;
    v2 xAxis = (50.0f + 50.0f * Cos(angle)) * v2{Cos(angle), Sin(angle)};
    v2 yAxis = (50.0f + 50.0f * Cos(angle)) * v2{Cos(angle + 1.0f), Sin(angle + 1.0f)};

    ui32 pIndex = 0;
    render_entry_coordinate_system *c = CoordinateSystem
        (
            renderGroup, origin, xAxis, yAxis,
            v4{0.5f + 0.5f * Sin(angle),
               0.5f + 0.5f * Sin(3.7f * angle),
               0.5f + 0.5f * Sin(7.9f * angle),
               1}
        );
    for(r32 y = 0;
        y < 1.0f;
        y += 0.25f)
    {
        for(r32 x = 0;
            x < 1.0f;
            x += 0.25f)
        {
            c->points[pIndex++] = {x, y};
        }
    }
    
    RenderGroupToOutput(renderGroup, drawBuffer);
    
    EndSim(simRegion, gameState);
    EndTemporaryMemory(simMemory);
    EndTemporaryMemory(renderMemory);

    CheckArena(&gameState->worldArena);
    CheckArena(&tranState->tranArena);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
