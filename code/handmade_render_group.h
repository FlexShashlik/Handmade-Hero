/*

  NOTE:

  1) Everywhere outside the renderer, _Y_ always goes upward,
  _X_ to the right.

  2) All bitmaps including the render target are
  assumed to be bottom-up (the first row pointer points to the
  bottom-most row when viewed on screen).

  3) Unless otherwise specified, all inputs to the renderer are in
  world coordinate("meters"), NOT pixels. Anything that in pixel
  values will be explicitly marked as such.

  4) _Z_ is a special coordinate because it is broken up into discrete
  slices, and the renderer actually understands these slices
  (potentially).

   //TODO: ZHANDLING
    
 */

struct loaded_bitmap
{
    i32 width;
    i32 height;
    i32 pitch;
    void *memory;
};

struct environment_map
{
    loaded_bitmap lod[4];
    r32 posZ;
};

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

enum render_group_entry_type
{
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_coordinate_system,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_saturation
};

struct render_group_entry_header
{
    render_group_entry_type type;
};

struct render_entry_clear
{
    v4 color;
};

struct render_entry_saturation
{
    r32 level;
};

struct render_entry_coordinate_system
{
    render_group_entry_header header;
    v2 origin;
    v2 xAxis;
    v2 yAxis;
    v4 color;
    loaded_bitmap *texture;
    loaded_bitmap *normalMap;

    environment_map *top;
    environment_map *middle;
    environment_map *bottom;
};

struct render_entry_bitmap
{
    render_entity_basis entityBasis;
    loaded_bitmap *bitmap;
    v4 color;
};

struct render_entry_rectangle
{
    render_entity_basis entityBasis;
    v4 color;
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
