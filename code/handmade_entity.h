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
GetEntityGroundPoint(sim_entity *_entity)
{
    v3 result = _entity->pos + v3{0, 0, -0.5f * _entity->dim.z};
    return result;
}

inline r32
GetStairGround(sim_entity *_entity, v3 atGroundPoint)
{
    rectangle3 regionRect = RectCenterDim
        (
            _entity->pos, _entity->dim
        );
        
    v3 bary = Clamp01(GetBarycentric(regionRect, atGroundPoint));

    r32 result = regionRect.min.z + bary.y * _entity->walkableHeight;

    return result;
}
