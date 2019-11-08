#define InvalidPos v3{100000.0f, 100000.0f, 100000.0f}

inline b32
IsSet(sim_entity *_entity, ui32 flag)
{
    b32 result = _entity->flags & flag;
    return result;
}

inline void
AddFlag(sim_entity *_entity, ui32 flag)
{
    _entity->flags |= flag;
}

inline void
ClearFlag(sim_entity *_entity, ui32 flag)
{
    _entity->flags &= ~flag;
}

inline void
MakeEntityNonSpatial(sim_entity *_entity)
{
    AddFlag(_entity, EntityFlag_Nonspatial);
    _entity->pos = InvalidPos;
}

inline void
MakeEntitySpatial(sim_entity *_entity, v3 pos, v3 dPos)
{
    ClearFlag(_entity, EntityFlag_Nonspatial);
    _entity->pos = pos;
    _entity->dPos = dPos;
}
