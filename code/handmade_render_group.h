struct render_basis
{
    v3 p;
};

// NOTE: render_group_entry is a "memory-efficient/compact discriminated union"
enum render_group_entry_type
{
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle
};

struct render_group_entry_header
{
    render_group_entry_type type;
};

struct render_entry_clear
{
    render_group_entry_header header;
    r32 r, g, b, a;
};

struct render_entry_rectangle
{
    render_basis *basis;
    loaded_bitmap *bitmap;
    v2 offset;
    r32 offsetZ;
    r32 entityZC;

    r32 r, g, b, a;
    v2 dim;
};

struct render_group
{
    render_basis *defaultBasis;
    r32 metersToPixels;

    ui32 pushBufferSize;
    ui32 maxPushBufferSize;
    ui8 *pushBufferBase;
};

#define PushRenderElement(group, type) (type *)PushRenderElement_(group, sizeof(type), RenderGroupEntryType_##type)
inline render_group_entry_header *
PushRenderElement_(render_group *group, ui32 size, render_group_entry_type type)
{
    render_group_entry_header *result = 0;
    if(group->pushBufferSize + size < group->maxPushBufferSize)
    {
        result = (render_group_entry_header *)(group->pushBufferBase + group->pushBufferSize);
        result->type = type;
        group->pushBufferSize += size;
    }
    else
    {
        InvalidCodePath;
    }

    return result;
}

inline void
PushPiece
(
    render_group *group,
    loaded_bitmap *bitmap,
    v2 offset, r32 offsetZ,
    v2 align, v2 dim,
    v4 color,
    r32 entityZC = 1.0f
)
{
    render_entry_rectangle *piece = PushRenderElement(group, render_entry_rectangle);
    if(piece)
    {
        piece->basis = group->defaultBasis;
        piece->bitmap = bitmap;
        piece->offset = group->metersToPixels * v2{offset.x, -offset.y} - align;
        piece->offsetZ = offsetZ;
        piece->r = color.r;
        piece->g = color.g;
        piece->b = color.b;
        piece->a = color.a;
        piece->entityZC = entityZC;
        piece->dim = dim;
    }
}

inline void
PushBitmap
(
    render_group *group,
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
    render_group *group,
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
    render_group *group,
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
