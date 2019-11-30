#define InvalidPos v3{100000.0f, 100000.0f, 100000.0f}

inline b32
IsSet(sim_entity *_entity, ui32 flag)
{
    b32 result = _entity->flags & flag;
    return result;
}

inline void
AddFlags(sim_entity *_entity, ui32 flag)
{
    _entity->flags |= flag;
}

inline void
ClearFlags(sim_entity *_entity, ui32 flag)
{
    _entity->flags &= ~flag;
}

inline void
MakeEntityNonSpatial(sim_entity *_entity)
{
    AddFlags(_entity, EntityFlag_Nonspatial);
    _entity->pos = InvalidPos;
}

inline void
MakeEntitySpatial(sim_entity *_entity, v3 pos, v3 dPos)
{
    ClearFlags(_entity, EntityFlag_Nonspatial);
    _entity->pos = pos;
    _entity->dPos = dPos;
}

inline v3
GetEntityGroundPoint(sim_entity *_entity, v3 forEntityPos)
{
    v3 result = forEntityPos;
    return result;
}

inline v3
GetEntityGroundPoint(sim_entity *_entity)
{
    v3 result = _entity->pos;
    return result;
}

inline r32
GetStairGround(sim_entity *_entity, v3 atGroundPoint)
{
    Assert(_entity->type == EntityType_Stairwell);
    
    rectangle2 regionRect = RectCenterDim
        (
            _entity->pos.xy, _entity->walkableDim
        );
        
    v2 bary = Clamp01(GetBarycentric(regionRect, atGroundPoint.xy));

    r32 result = _entity->pos.z + bary.y * _entity->walkableHeight;

    return result;
}
