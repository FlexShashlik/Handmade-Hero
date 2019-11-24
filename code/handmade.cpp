#include "handmade.h"
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

internal void
DrawRectangle
(
    loaded_bitmap *buffer,
    v2 vMin, v2 vMax,
    r32 r, r32 g, r32 b
)
{
    i32 minX = RoundR32ToI32(vMin.x);
    i32 minY = RoundR32ToI32(vMin.y);
    i32 maxX = RoundR32ToI32(vMax.x);
    i32 maxY = RoundR32ToI32(vMax.y);

    if(minX < 0)
    {
        minX = 0;
    }

    if(minY < 0)
    {
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui8 *endOfBuffer = (ui8 *)buffer->memory + buffer->pitch * buffer->height;
    
    ui32 color =
        (
            RoundR32ToUI32(r * 255.0f) << 16 |
            RoundR32ToUI32(g * 255.0f) << 8 |
            RoundR32ToUI32(b * 255.0f) << 0
        );

    ui8 *row = (ui8 *)buffer->memory + minX * BITMAP_BYTES_PER_PIXEL + minY * buffer->pitch;
    for(i32 y = minY; y < maxY; y++)
    {
        ui32 *pixel = (ui32 *)row;
        for(i32 x = minX; x < maxX; x++)
        {        
            *pixel++ = color;            
        }
        
        row += buffer->pitch;
    }
}

internal void
DrawBitmap
(
    loaded_bitmap *buffer,
    loaded_bitmap *bmp,
    r32 rX, r32 rY,
    r32 cAlpha = 1.0f
)
{
    i32 minX = RoundR32ToI32(rX);
    i32 minY = RoundR32ToI32(rY);
    i32 maxX = minX + bmp->width;
    i32 maxY = minY + bmp->height;

    i32 sourceOffsetX = 0;    
    if(minX < 0)
    {
        sourceOffsetX = -minX;
        minX = 0;
    }

    i32 sourceOffsetY = 0;
    if(minY < 0)
    {
        sourceOffsetY = -minY;
        minY = 0;
    }

    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }

    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }
    
    ui8 *sourceRow = (ui8 *)bmp->memory +
        sourceOffsetY * bmp->pitch +
        BITMAP_BYTES_PER_PIXEL * sourceOffsetX;
    
    ui8 *destRow = (ui8 *)buffer->memory +
        minX * BITMAP_BYTES_PER_PIXEL + minY * buffer->pitch;
    
    for(i32 y = minY; y < maxY; y++)
    {        
        ui32 *dest = (ui32 *)destRow;
        ui32 *source = (ui32 *)sourceRow;

        for(i32 x = minX; x < maxX; x++)
        {
            r32 sa = (r32)((*source >> 24) & 0xFF);
            r32 rsa = (sa / 255.0f) * cAlpha;
            r32 sr = (r32)((*source >> 16) & 0xFF) * cAlpha;
            r32 sg = (r32)((*source >> 8) & 0xFF) * cAlpha;
            r32 sb = (r32)((*source >> 0) & 0xFF) * cAlpha;
            
            r32 da = (r32)((*dest >> 24) & 0xFF);
            r32 dr = (r32)((*dest >> 16) & 0xFF);
            r32 dg = (r32)((*dest >> 8) & 0xFF);
            r32 db = (r32)((*dest >> 0) & 0xFF);
            r32 rda = (da / 255.0f);
                            
            r32 invRSA = (1.0f - rsa);            
            r32 a = 255.0f * (rsa + rda - rsa * rda);
            r32 r = invRSA * dr + sr;
            r32 g = invRSA * dg + sg;
            r32 b = invRSA * db + sb;

            *dest = ((ui32)(a + 0.5f) << 24|
                     (ui32)(r + 0.5f) << 16|
                     (ui32)(g + 0.5f) << 8 |
                     (ui32)(b + 0.5f) << 0);
            
            dest++;
            source++;
        }

        destRow += buffer->pitch;
        sourceRow += bmp->pitch;
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
    _entity.low->sim.walkableHeight = gameState->_world->tileDepthInMeters;
    
    return _entity;
}

inline void
PushPiece
(
    entity_visible_piece_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    Assert(group->pieceCount < ArrayCount(group->pieces));
    entity_visible_piece *piece = group->pieces + group->pieceCount++;
    piece->bitmap = bitmap;
    piece->offset = group->gameState->metersToPixels * v2{offset.x, -offset.y} - align;
    piece->offsetZ = offsetZ;
    piece->r = color.r;
    piece->g = color.g;
    piece->b = color.b;
    piece->a = color.a;
    piece->entityZC = entityZC;
    piece->dim = dim;
}

inline void
PushBitmap
(
    entity_visible_piece_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, r32 alpha = 1.0f,
    r32 entityZC = 1.0f
)
{
    PushPiece
        (
            group, bitmap,
            offset, offsetZ,
            align, v2{0, 0},
            v4{1.0f, 1.0f, 1.0f, alpha},
            entityZC
        );
}

inline void
PushRect
(
    entity_visible_piece_group *group,
    v2 offset, r32 offsetZ,
    v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    PushPiece
        (
            group, 0,
            offset, offsetZ,
            v2{0, 0}, dim,
            color,
            entityZC
        );
}

inline void
PushRectOutline
(
    entity_visible_piece_group *group,
    v2 offset, r32 offsetZ,
    v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    r32 thickness = 0.1f;
    
    // NOTE: Top and bottom
    PushPiece
        (
            group, 0,
            offset - v2{0, 0.5f * dim.y}, offsetZ,
            v2{0, 0}, v2{dim.x, thickness},
            color,
            entityZC
        );
    PushPiece
        (
            group, 0,
            offset + v2{0, 0.5f * dim.y}, offsetZ,
            v2{0, 0}, v2{dim.x, thickness},
            color,
            entityZC
        );
    
    // NOTE: Left and right
    PushPiece
        (
            group, 0,
            offset - v2{0.5f * dim.x, 0}, offsetZ,
            v2{0, 0}, v2{thickness, dim.y},
            color,
            entityZC
        );
    PushPiece
        (
            group, 0,
            offset + v2{0.5f * dim.x, 0}, offsetZ,
            v2{0, 0}, v2{thickness, dim.y},
            color,
            entityZC
        );
}

internal void
DrawHitPoints
(
    sim_entity *entity,
    entity_visible_piece_group *pieceGroup
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
DrawGroundChunk
(
    game_state *gameState,
    loaded_bitmap *buffer, world_position *chunkPos
)
{
    random_series series = RandomSeed
        (139 * chunkPos->chunkX +
         593 * chunkPos->chunkY +
         329 * chunkPos->chunkZ);

    r32 width = (r32)buffer->width;
    r32 height = (r32)buffer->height;
    v2 center = 0.5f * v2{width, height};
    for(ui32 grassIndex = 0;
        grassIndex < 1000;
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

        v2 pos = offset - bitmapCenter;
        
        DrawBitmap
            (
                buffer, stamp,
                pos.x, pos.y
            );
    }

    for(ui32 grassIndex = 0;
        grassIndex < 1000;
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

        v2 pos = offset - bitmapCenter;
        
        DrawBitmap
            (
                buffer, stamp,
                pos.x, pos.y
            );
    }
}

loaded_bitmap
MakeEmptyBitmap(memory_arena *arena, i32 width, i32 height)
{
    loaded_bitmap result = {};
    result.width = width;
    result.height = height;
    result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
    i32 totalBitmapSize = width * height * BITMAP_BYTES_PER_PIXEL;
    result.memory = PushSize_(arena, totalBitmapSize);
    ZeroSize(totalBitmapSize, result.memory);

    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);
        
    game_state *gameState = (game_state *)memory->permanentStorage;
    if(!memory->isInitialized)
    {
        ui32 tilesPerWidth = 17;
        ui32 tilesPerHeight = 9;
        
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
        
        InitializeWorld(_world, 1.4f, 3.0f);
        
        i32 tileSideInPixels = 60;
        gameState->metersToPixels = (r32)tileSideInPixels / (r32)_world->tileSideInMeters;

        gameState->nullCollision = MakeNullCollision(gameState);
        gameState->swordCollision = MakeSimpleGroundedCollision
            (
                gameState, 1.0f, 0.5f, 0.1f
            );
        gameState->stairCollision = MakeSimpleGroundedCollision
            (
                gameState,
                gameState->_world->tileSideInMeters,
                2.0f * gameState->_world->tileSideInMeters,
                1.1f * gameState->_world->tileDepthInMeters
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
                gameState->_world->tileSideInMeters,
                gameState->_world->tileSideInMeters,
                gameState->_world->tileDepthInMeters
            );
        gameState->standardRoomCollision = MakeSimpleGroundedCollision
            (
                gameState,
                tilesPerWidth * gameState->_world->tileSideInMeters,
                tilesPerHeight * gameState->_world->tileSideInMeters,
                0.9f * gameState->_world->tileDepthInMeters
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

        r32 screenWidth = (r32)buffer->width;
        r32 screenHeight = (r32)buffer->height;
        r32 maximumZScale = 0.5f;
        r32 groundOverscan = 1.5f;
        ui32 groundBufferWidth = RoundR32ToI32(groundOverscan * screenWidth);
        ui32 groundBufferHeight = RoundR32ToI32(groundOverscan * screenHeight);
        
        gameState->groundBuffer = MakeEmptyBitmap
            (
                &gameState->worldArena, groundBufferWidth, groundBufferHeight
            );
        gameState->groundBufferPos = gameState->cameraPos;
        
        DrawGroundChunk
            (
                gameState,
                &gameState->groundBuffer,
                &gameState->groundBufferPos
            );
        
        memory->isInitialized = true;
    }
    
    world *_world = gameState->_world;
    
    r32 metersToPixels = gameState->metersToPixels;
    
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

    ui32 tileSpanX = 17 * 3;
    ui32 tileSpanY = 9 * 3;
    ui32 tileSpanZ = 1;
    rectangle3 cameraBounds = RectCenterDim
        (
            v3{0, 0, 0},
            _world->tileSideInMeters *
            v3{(r32)tileSpanX, (r32)tileSpanY, (r32)tileSpanZ}
        );

    memory_arena simArena;
    InitializeArena
        (
            &simArena,
            memory->transientStorageSize,
            memory->transientStorage
        );
    sim_region *simRegion = BeginSim
        (
            &simArena, gameState, gameState->_world,
            gameState->cameraPos, cameraBounds, input->deltaTime
        );
    
    //
    // NOTE: Render
    //

    loaded_bitmap drawBuffer_ = {};
    loaded_bitmap *drawBuffer = &drawBuffer_;
    drawBuffer->width = buffer->width;
    drawBuffer->height = buffer->height;
    drawBuffer->pitch = buffer->pitch;
    drawBuffer->memory = buffer->memory;
    
    DrawRectangle
        (
            drawBuffer,
            v2{0.0f, 0.0f},
            v2{(r32)drawBuffer->width, (r32)drawBuffer->height},
            0.5f, 0.5f, 0.5f
        );
    
    r32 screenCenterX = 0.5f * (r32)drawBuffer->width;
    r32 screenCenterY = 0.5f * (r32)drawBuffer->height;

    v2 ground =
        {
            screenCenterX - 0.5f * (r32)gameState->groundBuffer.width,
            screenCenterY - 0.5f * (r32)gameState->groundBuffer.height
        };

    v3 delta = Subtract
        (
            gameState->_world,
            &gameState->groundBufferPos, &gameState->cameraPos
        );
    
    delta.y = -delta.y;
    ground += metersToPixels * delta.xy;
    
    DrawBitmap
        (
            drawBuffer, &gameState->groundBuffer,
            ground.x, ground.y
        );
    
    // TODO: Move this out into handmade.cpp
    entity_visible_piece_group pieceGroup = {};
    pieceGroup.gameState = gameState;
    sim_entity *_entity = simRegion->entities;
    for(ui32 entityIndex = 0;
        entityIndex < simRegion->entityCount;
        entityIndex++, _entity++)
    {
        if(_entity->isUpdatable)
        {
            pieceGroup.pieceCount = 0;
            r32 deltaTime = input->deltaTime;
            // TODO: Should be computed after update
            r32 shadowAlpha = 1.0f - 0.5f * _entity->pos.z;
            if(shadowAlpha < 0.0f)
            {
                shadowAlpha = 0.0f;
            }

            move_spec moveSpec = DefaultMoveSpec();
            v3 ddPos = {};
            
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
                            &pieceGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
            
                    PushBitmap
                        (
                            &pieceGroup, &heroBitmaps->torso,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );

                    PushBitmap
                        (
                            &pieceGroup, &heroBitmaps->cape,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );
    
                    PushBitmap
                        (
                            &pieceGroup, &heroBitmaps->head,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );

                    DrawHitPoints(_entity, &pieceGroup);
                } break;
            
                case EntityType_Wall:
                {
                    PushBitmap
                        (
                            &pieceGroup, &gameState->tree,
                            v2{0, 0}, 0,
                            v2{40, 80}
                        );
                } break;

                case EntityType_Stairwell:
                {
                    PushRect
                        (
                            &pieceGroup, v2{0, 0}, 0,
                            _entity->walkableDim,
                            v4{1, 0.5f, 0, 1},
                            0.0f
                        );
                    PushRect
                        (
                            &pieceGroup, v2{0, 0},
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
                            &pieceGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
                    PushBitmap
                        (
                            &pieceGroup, &gameState->sword,
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
                            &pieceGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            0.5f * shadowAlpha + 0.2f * bobSin, 0.0f
                        );
                    PushBitmap
                        (
                            &pieceGroup, &heroBitmaps->head,
                            v2{0, 0},
                            0.25f * bobSin,
                            heroBitmaps->align
                        );
                } break;

                case EntityType_Monster:
                {
                    PushBitmap
                        (
                            &pieceGroup, &gameState->shadow,
                            v2{0, 0}, 0,
                            heroBitmaps->align,
                            shadowAlpha, 0.0f
                        );
                    PushBitmap
                        (
                            &pieceGroup, &heroBitmaps->torso,
                            v2{0, 0}, 0,
                            heroBitmaps->align
                        );
                
                    DrawHitPoints(_entity, &pieceGroup);
                } break;

                case EntityType_Space:
                {
#if 0
                    for(ui32 volumeIndex = 0;
                        volumeIndex < _entity->collision->volumeCount;
                        volumeIndex++)
                    {
                        sim_entity_collision_volume *volume =
                            _entity->collision->volumes + volumeIndex;
                        PushRectOutline
                            (
                                &pieceGroup, volume->offsetPos.xy, 0,
                                volume->dim.xy,
                                v4{0, 0.5f, 1.0f, 1},
                                0.0f
                            );
                    }
#endif
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
        
            for(ui32 pieceIndex = 0;
                pieceIndex < pieceGroup.pieceCount;
                pieceIndex++)
            {
                entity_visible_piece *piece = pieceGroup.pieces + pieceIndex;

                v3 entityBasePos = GetEntityGroundPoint(_entity); 
                r32 zFudge = 1.0f + 0.1f * (entityBasePos.z + piece->offsetZ);
            
                r32 entityGroundX = screenCenterX + metersToPixels *
                    zFudge * entityBasePos.x;
                r32 entityGroundY = screenCenterY - metersToPixels *
                    zFudge * entityBasePos.y;

                r32 entityZ = -metersToPixels * entityBasePos.z;

                v2 center =
                    {
                        entityGroundX + piece->offset.x,
                        entityGroundY + piece->offset.y + piece->entityZC * entityZ
                    };
                if(piece->bitmap)
                {
                    DrawBitmap
                        (
                            drawBuffer, piece->bitmap,
                            center.x, center.y,
                            piece->a
                        );
                }
                else
                {
                    v2 halfDim = 0.5f * metersToPixels * piece->dim;
                    DrawRectangle
                        (
                            drawBuffer,
                            center - halfDim, center + halfDim,
                            piece->r, piece->g, piece->b
                        );
                }
            }
        }
    }
    
    world_position worldOrigin = {};
    v3 diff = Subtract(simRegion->_world, &worldOrigin, &simRegion->origin);
    DrawRectangle(drawBuffer, diff.xy, v2{10.0f, 10.0f}, 1.0f, 1.0f, 0.0f);
    
    EndSim(simRegion, gameState);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);
}
