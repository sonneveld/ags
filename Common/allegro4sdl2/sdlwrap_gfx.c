//
//  sdlwrap_gfx.c
//  AGSKit
//
//  Created by Nick Sonneveld on 19/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include <stdio.h>

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include <SDL2/SDL.h>

RGB_MAP *rgb_map = NULL; 

PALETTE black_palette;
PALETTE _current_palette;

int _current_palette_changed = 0xFFFFFFFF;

int _palette_color8[256];               /* palette -> pixel mapping */
int _palette_color15[256];
int _palette_color16[256];
int _palette_color24[256];
int _palette_color32[256];

int *palette_color = _palette_color8;



BLENDER_FUNC _blender_func15 = NULL;   /* truecolor pixel blender routines */
BLENDER_FUNC _blender_func16 = NULL;
BLENDER_FUNC _blender_func24 = NULL;
BLENDER_FUNC _blender_func32 = NULL;

BLENDER_FUNC _blender_func15x = NULL;
BLENDER_FUNC _blender_func16x = NULL;
BLENDER_FUNC _blender_func24x = NULL;

int _blender_col_15 = 0;               /* for truecolor lit sprites */
int _blender_col_16 = 0;
int _blender_col_24 = 0;
int _blender_col_32 = 0;

int _blender_alpha = 0;                /* for truecolor translucent drawing */



int _drawing_mode = DRAW_MODE_SOLID;

BITMAP *_drawing_pattern = NULL;

int _drawing_x_anchor = 0;
int _drawing_y_anchor = 0;

unsigned int _drawing_x_mask = 0;
unsigned int _drawing_y_mask = 0;

void drawing_mode(int mode, BITMAP *pattern, int x_anchor, int y_anchor)
{
    _drawing_mode = mode;
    _drawing_pattern = pattern;
    _drawing_x_anchor = x_anchor;
    _drawing_y_anchor = y_anchor;
    
    _drawing_x_mask = _drawing_y_mask = 0;
}

/* solid_mode:
 *  Shortcut function for selecting solid drawing mode.
 */
void solid_mode(void)
{
    drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
}



/* _palette_expansion_table:
 *  Creates a lookup table for expanding 256->truecolor.
 */
static int *palette_expansion_table(int bpp)
{
    int *table;
    int c;
    
    switch (bpp) {
        case 15: table = _palette_color15; break;
        case 16: table = _palette_color16; break;
        case 24: table = _palette_color24; break;
        case 32: table = _palette_color32; break;
        default: ASSERT(FALSE); return NULL;
    }
    
    if (_current_palette_changed & (1<<(bpp-1))) {
        for (c=0; c<PAL_SIZE; c++) {
            table[c] = makecol_depth(bpp,
                                     _rgb_scale_6[_current_palette[c].r],
                                     _rgb_scale_6[_current_palette[c].g],
                                     _rgb_scale_6[_current_palette[c].b]);
        }
        
        _current_palette_changed &= ~(1<<(bpp-1));
    }
    
    return table;
}



/* this has to be called through a function pointer, so MSVC asm can use it */
int *(*_palette_expansion_table)(int) = palette_expansion_table;


/* set_blender_mode:
 *  Specifies a custom set of blender functions for interpolating between
 *  truecolor pixels. The 24 bit blender is shared between the 24 and 32 bit
 *  modes. Pass a NULL table for unused color depths (you must not draw
 *  translucent graphics in modes without a handler, though!). Your blender
 *  will be passed two 32 bit colors in the appropriate format (5.5.5, 5.6.5,
 *  or 8.8.8), and an alpha value, should return the result of combining them.
 *  In translucent drawing modes, the two colors are taken from the source
 *  and destination images and the alpha is specified by this function. In
 *  lit modes, the alpha is specified when you call the drawing routine, and
 *  the interpolation is between the source color and the RGB values you pass
 *  to this function.
 */
void set_blender_mode(BLENDER_FUNC b15, BLENDER_FUNC b16, BLENDER_FUNC b24, int r, int g, int b, int a)
{
    _blender_func15 = b15;
    _blender_func16 = b16;
    _blender_func24 = b24;
    _blender_func32 = b24;
    
    _blender_func15x = _blender_black;
    _blender_func16x = _blender_black;
    _blender_func24x = _blender_black;
    
    _blender_col_15 = makecol15(r, g, b);
    _blender_col_16 = makecol16(r, g, b);
    _blender_col_24 = makecol24(r, g, b);
    _blender_col_32 = makecol32(r, g, b);
    
    _blender_alpha = a;
}



/* set_blender_mode_ex
 *  Specifies a custom set of blender functions for interpolating between
 *  truecolor pixels, providing a more complete set of routines, which
 *  differentiate between 24 and 32 bit modes, and have special routines
 *  for blending 32 bit RGBA pixels onto a destination of any format.
 */
void set_blender_mode_ex(BLENDER_FUNC b15, BLENDER_FUNC b16, BLENDER_FUNC b24, BLENDER_FUNC b32, BLENDER_FUNC b15x, BLENDER_FUNC b16x, BLENDER_FUNC b24x, int r, int g, int b, int a)
{
    _blender_func15 = b15;
    _blender_func16 = b16;
    _blender_func24 = b24;
    _blender_func32 = b32;
    
    _blender_func15x = b15x;
    _blender_func16x = b16x;
    _blender_func24x = b24x;
    
    _blender_col_15 = makecol15(r, g, b);
    _blender_col_16 = makecol16(r, g, b);
    _blender_col_24 = makecol24(r, g, b);
    _blender_col_32 = makecol32(r, g, b);
    
    _blender_alpha = a;
}




void get_clip_rect (BITMAP *bitmap, int *x1, int *y_1, int *x2, int *y2)
{
    ASSERT(bitmap);
    
    /* internal clipping is inclusive-exclusive */
    *x1 = bitmap->cl;
    *y_1 = bitmap->ct;
    *x2 = bitmap->cr-1;
    *y2 = bitmap->cb-1;
}

/* set_clip_rect:
 *  Sets the two opposite corners of the clipping rectangle to be used when
 *  drawing to the bitmap. Nothing will be drawn to positions outside of this
 *  rectangle. When a new bitmap is created the clipping rectangle will be
 *  set to the full area of the bitmap.
 */
void set_clip_rect(BITMAP *bitmap, int x1, int y1, int x2, int y2)
{
    ASSERT(bitmap);
    
    /* internal clipping is inclusive-exclusive */
    x2++;
    y2++;
    
    bitmap->cl = CLAMP(0, x1, bitmap->w-1);
    bitmap->ct = CLAMP(0, y1, bitmap->h-1);
    bitmap->cr = CLAMP(0, x2, bitmap->w);
    bitmap->cb = CLAMP(0, y2, bitmap->h);
}


int _color_conv = COLORCONV_TOTAL;     /* which formats to auto convert? */
static int color_conv_set = FALSE;     /* has the user set conversion mode? */

/* set_color_conversion:
 *  Sets a bit mask specifying which types of color format conversions are
 *  valid when loading data from disk.
 */
void set_color_conversion(int mode)
{
    _color_conv = mode;
    
    color_conv_set = TRUE;
}



/* get_color_conversion:
 *  Returns the bitmask specifying which types of color format
 *  conversion are valid when loading data from disk.
 */
int get_color_conversion(void)
{
    return _color_conv;
}


 void clear_to_color (BITMAP *bitmap, int color)
{
    ASSERT(bitmap);
    
    bitmap->vtable->clear_to_color(bitmap, color);
}


 int is_same_bitmap (BITMAP *bmp1, BITMAP *bmp2)
{
    unsigned long m1;
    unsigned long m2;
    
    if ((!bmp1) || (!bmp2))
        return FALSE;
    
    if (bmp1 == bmp2)
        return TRUE;
    
    m1 = bmp1->id & BMP_ID_MASK;
    m2 = bmp2->id & BMP_ID_MASK;
    
    return ((m1) && (m1 == m2));
}

// palette
//int black_palette() { printf("STUB: black_palette\n");      return 0; }




/* set_palette:
 *  Sets the entire color palette.
 */
void set_palette(AL_CONST PALETTE p)
{
    set_palette_range(p, 0, PAL_SIZE-1, TRUE);
}



/* set_palette_range:
 *  Sets a part of the color palette.
 */
void set_palette_range(AL_CONST PALETTE p, int from, int to, int vsync)
{
    int c;
    
    ASSERT(from >= 0 && from < PAL_SIZE);
    ASSERT(to >= 0 && to < PAL_SIZE)
    
    for (c=from; c<=to; c++) {
        _current_palette[c] = p[c];
        
        if (_color_depth != 8)
            palette_color[c] = makecol(_rgb_scale_6[p[c].r], _rgb_scale_6[p[c].g], _rgb_scale_6[p[c].b]);
    }
    
    _current_palette_changed = 0xFFFFFFFF & ~(1<<(_color_depth-1));
    
//    if (gfx_driver) {
//        if ((screen->vtable->color_depth == 8) && (!_dispsw_status))
//            gfx_driver->set_palette(p, from, to, vsync);
//    }
//    else if ((system_driver) && (system_driver->set_palette_range))
//        system_driver->set_palette_range(p, from, to, vsync);
}



/* get_palette:
 *  Retrieves the entire color palette.
 */
void get_palette(PALETTE p)
{
    get_palette_range(p, 0, PAL_SIZE-1);
}



/* get_palette_range:
 *  Retrieves a part of the color palette.
 */
void get_palette_range(PALETTE p, int from, int to)
{
    int c;
    
    ASSERT(from >= 0 && from < PAL_SIZE);
    ASSERT(to >= 0 && to < PAL_SIZE);
    
//    if ((system_driver) && (system_driver->read_hardware_palette))
//        system_driver->read_hardware_palette();
    
    for (c=from; c<=to; c++)
        p[c] = _current_palette[c];
}



/* fade_interpolate:
 *  Calculates a palette part way between source and dest, returning it
 *  in output. The pos indicates how far between the two extremes it should
 *  be: 0 = return source, 64 = return dest, 32 = return exactly half way.
 *  Only affects colors between from and to (inclusive).
 */
void fade_interpolate(AL_CONST PALETTE source, AL_CONST PALETTE dest, PALETTE output, int pos, int from, int to)
{
    int c;
    
    ASSERT(pos >= 0 && pos <= 64);
    ASSERT(from >= 0 && from < PAL_SIZE);
    ASSERT(to >= 0 && to < PAL_SIZE);
    
    for (c=from; c<=to; c++) {
        output[c].r = ((int)source[c].r * (63-pos) + (int)dest[c].r * pos) / 64;
        output[c].g = ((int)source[c].g * (63-pos) + (int)dest[c].g * pos) / 64;
        output[c].b = ((int)source[c].b * (63-pos) + (int)dest[c].b * pos) / 64;
    }
}



/* previous palette, so the image loaders can restore it when they are done */
int _got_prev_current_palette = FALSE;
PALETTE _prev_current_palette;
static int prev_palette_color[PAL_SIZE];



/* select_palette:
 *  Sets the aspects of the palette tables that are used for converting
 *  between different image formats, without altering the display settings.
 *  The previous settings are copied onto a one-deep stack, from where they
 *  can be restored by calling unselect_palette().
 */
void select_palette(AL_CONST PALETTE p)
{
    int c;
    
    for (c=0; c<PAL_SIZE; c++) {
        _prev_current_palette[c] = _current_palette[c];
        _current_palette[c] = p[c];
    }
    
    if (_color_depth != 8) {
        for (c=0; c<PAL_SIZE; c++) {
            prev_palette_color[c] = palette_color[c];
            palette_color[c] = makecol(_rgb_scale_6[p[c].r], _rgb_scale_6[p[c].g], _rgb_scale_6[p[c].b]);
        }
    }
    
    _got_prev_current_palette = TRUE;
    
    _current_palette_changed = 0xFFFFFFFF & ~(1<<(_color_depth-1));
}



/* unselect_palette:
 *  Restores palette settings from before the last call to select_palette().
 */
void unselect_palette(void)
{
    int c;
    
    for (c=0; c<PAL_SIZE; c++)
        _current_palette[c] = _prev_current_palette[c];
    
    if (_color_depth != 8) {
        for (c=0; c<PAL_SIZE; c++)
            palette_color[c] = prev_palette_color[c];
    }
    
    ASSERT(_got_prev_current_palette == TRUE);
    _got_prev_current_palette = FALSE;
    
    _current_palette_changed = 0xFFFFFFFF & ~(1<<(_color_depth-1));
}


/* do_line:
 *  Calculates all the points along a line between x1, y1 and x2, y2,
 *  calling the supplied function for each one. This will be passed a
 *  copy of the bmp parameter, the x and y position, and a copy of the
 *  d parameter (so do_line() can be used with putpixel()).
 */
void do_line(BITMAP *bmp, int x1, int y1, int x2, int y2, int d, void (*proc)(BITMAP *, int, int, int))
{
    int dx = x2-x1;
    int dy = y2-y1;
    int i1, i2;
    int x, y;
    int dd;
    
    /* worker macro */
#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)     \
{                                                                         \
if (d##pri_c == 0) {                                                   \
proc(bmp, x1, y1, d);                                               \
return;                                                             \
}                                                                      \
\
i1 = 2 * d##sec_c;                                                     \
dd = i1 - (sec_sign (pri_sign d##pri_c));                              \
i2 = dd - (sec_sign (pri_sign d##pri_c));                              \
\
x = x1;                                                                \
y = y1;                                                                \
\
while (pri_c pri_cond pri_c##2) {                                      \
proc(bmp, x, y, d);                                                 \
\
if (dd sec_cond 0) {                                                \
sec_c = sec_c sec_sign 1;                                        \
dd += i2;                                                        \
}                                                                   \
else                                                                \
dd += i1;                                                        \
\
pri_c = pri_c pri_sign 1;                                           \
}                                                                      \
}
    
    if (dx >= 0) {
        if (dy >= 0) {
            if (dx >= dy) {
                /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
                DO_LINE(+, x, <=, +, y, >=);
            }
            else {
                /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
                DO_LINE(+, y, <=, +, x, >=);
            }
        }
        else {
            if (dx >= -dy) {
                /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
                DO_LINE(+, x, <=, -, y, <=);
            }
            else {
                /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
                DO_LINE(-, y, >=, +, x, >=);
            }
        }
    }
    else {
        if (dy >= 0) {
            if (-dx >= dy) {
                /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
                DO_LINE(-, x, >=, +, y, >=);
            }
            else {
                /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
                DO_LINE(+, y, <=, -, x, <=);
            }
        }
        else {
            if (-dx >= -dy) {
                /* (x1 > x2) && (y1 > y2) && (dx >= dy) */
                DO_LINE(-, x, >=, -, y, <=);
            }
            else {
                /* (x1 > x2) && (y1 > y2) && (dx < dy) */
                DO_LINE(-, y, >=, -, x, <=);
            }
        }
    }
    
#undef DO_LINE
}


void triangle (BITMAP *bmp, int x1, int y_1, int x2, int y2, int x3, int y3, int color)
{
    ASSERT(bmp);
    _soft_triangle(bmp, x1, y_1, x2, y2, x3, y3, color);
}

void polygon (BITMAP *bmp, int vertices, AL_CONST int *points, int color)
{
    ASSERT(bmp);
    _soft_polygon(bmp, vertices, points, color);
}
