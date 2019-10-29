inline move_spec
DefaultMoveSpec(void)
{
    move_spec result = {};
    result.isUnitMaxAccelVector = false;
    result.speed = 1.0f;
    result.drag = 0.0f;

    return result;
}

inline void
UpdateFamiliar
(
    sim_region *simRegion, sim_entity *_entity, r32 deltaTime
)
{
    sim_entity *closestHero = 0;
    r32 closestHeroDSq = Square(10.0f);

    sim_entity *testEntity = simRegion->entities;
    for(ui32 testEntityIndex = 0;
        testEntityIndex < simRegion->entityCount;
        testEntityIndex++)
    {
        if(testEntity->type == EntityType_Hero)
        {
            r32 testDSq = LengthSq(testEntity->pos - _entity->pos) * 0.75f;
            if(closestHeroDSq > testDSq)
            {
                closestHero = testEntity;
                closestHeroDSq = testDSq;
            }
        }
    }

    v2 ddPos = {};
    if(closestHero && closestHeroDSq > Square(3.0f))
    {
        v2 deltaPos = closestHero->pos - _entity->pos;
        ddPos = (0.5f / SqRt(closestHeroDSq)) * deltaPos;
    }
    
    move_spec moveSpec = DefaultMoveSpec();
    moveSpec.isUnitMaxAccelVector = true;
    moveSpec.speed = 50.0f;
    moveSpec.drag = 8.0f;
    MoveEntity(simRegion, _entity, deltaTime, &moveSpec, ddPos);
}

inline void
UpdateMonster
(
    sim_region *simRegion, sim_entity *_entity, r32 deltaTime
)
{
}

inline void
UpdateSword
(
    sim_region *simRegion, sim_entity *_entity, r32 deltaTime
)
{
    move_spec moveSpec = DefaultMoveSpec();
    moveSpec.isUnitMaxAccelVector = false;
    moveSpec.speed = 0.0f;
    moveSpec.drag = 0.0f;

    v2 oldPos = _entity->pos;
    MoveEntity(simRegion, _entity, deltaTime, &moveSpec, v2{0, 0});
    r32 distanceTraveled = Length(_entity->pos - oldPos);
    _entity->distanceRemaining -= distanceTraveled;
    if(_entity->distanceRemaining < 0.0f)
    {
        Assert(!"MAKE ENTITIES BE ABLE TO NOT BE THERE");
    }
}
