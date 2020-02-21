/*

  NOTE:

  1) Everywhere outside the renderer, _Y_ always goes upward,
  _X_ to the right.

  2) All bitmaps including the render target are
  assumed to be bottom-up (the first row pointer points to the
  bottom-most row when viewed on screen).

  3) All inputs to the renderer are in world coordinate("meters"), NOT
  pixels. Anything that in pixel values will be explicitly marked as
  such.

  4) _Z_ is a special coordinate because it is broken up into discrete
  slices, and the renderer actually understands these slices. _Z_
  slices are what control the scaling of things, whereas _Z_ offsets
  inside a slice are what control _Y_ offseting.

  5) All color values specified to the renderer as V4's are in
  NON-premultiplied alpha.
    
 */

struct loaded_bitmap
{
    v2 alignPercentage;
    r32 widthOverHeight;
    
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

enum render_group_entry_type
{
    RenderGroupEntryType_render_entry_clear,
    RenderGroupEntryType_render_entry_coordinate_system,
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle
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
    v2 origin;
    v2 xAxis;
    v2 yAxis;
    v4 color;
    loaded_bitmap *texture;
    loaded_bitmap *normalMap;   
    
    //r32 pixelsToMeters; // TODO: Need to store this for lighting!

    environment_map *top;
    environment_map *middle;
    environment_map *bottom;
};

struct render_entry_bitmap
{
    loaded_bitmap *bitmap;
    
    v4 color;
    v2 p;
    v2 size;
};

struct render_entry_rectangle
{
    v4 color;
    v2 p;
    v2 dim;
};

struct render_transform
{
    // NOTE: Camera parameters
    r32 metersToPixels;
    v2 screenCenter;

    r32 focalLength;
    r32 distanceAboveTarget;

    v3 offsetP;
    r32 scale;
};

struct render_group
{
    r32 globalAlpha;
    
    v2 monitorHalfDimInMeters;
    
    render_transform transform;

    ui32 pushBufferSize;
    ui32 maxPushBufferSize;
    ui8 *pushBufferBase;
};
