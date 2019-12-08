struct render_basis
{
    v3 p;
};

struct render_entity_basis
{
    render_basis *basis;
    v2 offset;
    r32 offsetZ;
    r32 entityZC;
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
    v4 color;
};

struct render_entry_bitmap
{
    render_group_entry_header header;
    render_entity_basis entityBasis;
    loaded_bitmap *bitmap;
    r32 r, g, b, a;
};

struct render_entry_rectangle
{
    render_group_entry_header header;
    render_entity_basis entityBasis;
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
