internal render_group *
AllocateRenderGroup
(
    memory_arena *arena,
    ui32 maxPushBufferSize, r32 metersToPixels
)
{
    render_group *result = PushStruct(arena, render_group);
    result->pushBufferBase = (ui8 *)PushSize(arena, maxPushBufferSize);
    
    result->metersToPixels = metersToPixels;
    result->defaultBasis = PushStruct(arena, render_basis);
    result->defaultBasis->p = v3{0, 0, 0};
    result->pieceCount = 0;
    result->maxPushBufferSize = maxPushBufferSize;
    result->pushBufferSize = 0;
    
    return result;
}
