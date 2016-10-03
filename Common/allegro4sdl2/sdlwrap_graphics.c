//
//  sdlwrap.c
//  AGSKit
//
//  Created by Nick Sonneveld on 9/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include "sdlwrap.h"
#include <SDL2/SDL.h>
#include "allegro.h"
#include "allegro/internal/aintern.h"
#include <strings.h>
#include <ctype.h>


// --------------------------------------------------------------------------
// Graphics
// --------------------------------------------------------------------------

/* default truecolor pixel format */
//#define DEFAULT_RGB_R_SHIFT_15  0
//#define DEFAULT_RGB_G_SHIFT_15  5
//#define DEFAULT_RGB_B_SHIFT_15  10
//#define DEFAULT_RGB_R_SHIFT_16  0
//#define DEFAULT_RGB_G_SHIFT_16  5
//#define DEFAULT_RGB_B_SHIFT_16  11
//#define DEFAULT_RGB_R_SHIFT_24  0
//#define DEFAULT_RGB_G_SHIFT_24  8
//#define DEFAULT_RGB_B_SHIFT_24  16
//#define DEFAULT_RGB_R_SHIFT_32  0
//#define DEFAULT_RGB_G_SHIFT_32  8
//#define DEFAULT_RGB_B_SHIFT_32  16
//#define DEFAULT_RGB_A_SHIFT_32  24



#define DEFAULT_RGB_R_SHIFT_15  10
#define DEFAULT_RGB_G_SHIFT_15  5
#define DEFAULT_RGB_B_SHIFT_15  0

#define DEFAULT_RGB_R_SHIFT_16  11
#define DEFAULT_RGB_G_SHIFT_16  5
#define DEFAULT_RGB_B_SHIFT_16  0

#define DEFAULT_RGB_R_SHIFT_24  16
#define DEFAULT_RGB_G_SHIFT_24  8
#define DEFAULT_RGB_B_SHIFT_24  0

#define DEFAULT_RGB_R_SHIFT_32  16
#define DEFAULT_RGB_G_SHIFT_32  8
#define DEFAULT_RGB_B_SHIFT_32  0
#define DEFAULT_RGB_A_SHIFT_32  24




int _rgb_r_shift_15 = DEFAULT_RGB_R_SHIFT_15;     /* truecolor pixel format */
int _rgb_g_shift_15 = DEFAULT_RGB_G_SHIFT_15;
int _rgb_b_shift_15 = DEFAULT_RGB_B_SHIFT_15;

int _rgb_r_shift_16 = DEFAULT_RGB_R_SHIFT_16;
int _rgb_g_shift_16 = DEFAULT_RGB_G_SHIFT_16;
int _rgb_b_shift_16 = DEFAULT_RGB_B_SHIFT_16;

int _rgb_r_shift_24 = DEFAULT_RGB_R_SHIFT_24;    // not used by ags?
int _rgb_g_shift_24 = DEFAULT_RGB_G_SHIFT_24;
int _rgb_b_shift_24 = DEFAULT_RGB_B_SHIFT_24;

int _rgb_r_shift_32 = DEFAULT_RGB_R_SHIFT_32;
int _rgb_g_shift_32 = DEFAULT_RGB_G_SHIFT_32;
int _rgb_b_shift_32 = DEFAULT_RGB_B_SHIFT_32;
int _rgb_a_shift_32 = DEFAULT_RGB_A_SHIFT_32;

/* lookup table for scaling 5 bit colors up to 8 bits */
int _rgb_scale_5[32] =
{
    0,   8,   16,  24,  33,  41,  49,  57,
    66,  74,  82,  90,  99,  107, 115, 123,
    132, 140, 148, 156, 165, 173, 181, 189,
    198, 206, 214, 222, 231, 239, 247, 255
};


/* lookup table for scaling 6 bit colors up to 8 bits */
int _rgb_scale_6[64] =
{
    0,   4,   8,   12,  16,  20,  24,  28,
    32,  36,  40,  44,  48,  52,  56,  60,
    65,  69,  73,  77,  81,  85,  89,  93,
    97,  101, 105, 109, 113, 117, 121, 125,
    130, 134, 138, 142, 146, 150, 154, 158,
    162, 166, 170, 174, 178, 182, 186, 190,
    195, 199, 203, 207, 211, 215, 219, 223,
    227, 231, 235, 239, 243, 247, 251, 255
};


//  graphics vtables

// TODO, getpixel is called directly but others not.
//  AND do_stretch_blit


 void _stub_unbank_switch(BITMAP *bmp) {printf("STUB: unbank_switch depth=%d\n", bmp->vtable->color_depth);  }
//static int _stub_getpixel (struct BITMAP *bmp, int x, int y) { printf("STUB:  getpixel depth=%d\n", bmp->vtable->color_depth); return 0; };
//static void _stub_putpixel (struct BITMAP *bmp, int x, int y, int color) { printf("STUB: putpixel depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_vline (struct BITMAP *bmp, int x, int y_1, int y2, int color) { printf("STUB: vline depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_hline (struct BITMAP *bmp, int x1, int y, int x2, int color) { printf("STUB: hline depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_hfill (struct BITMAP *bmp, int x1, int y, int x2, int color) { printf("STUB: hfill depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_line (struct BITMAP *bmp, int x1, int y_1, int x2, int y2, int color) { printf("STUB: line depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_fastline (struct BITMAP *bmp, int x1, int y_1, int x2, int y2, int color) { printf("STUB: fastline depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_rectfill (struct BITMAP *bmp, int x1, int y_1, int x2, int y2, int color) { printf("STUB: rectfill depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_triangle (struct BITMAP *bmp, int x1, int y_1, int x2, int y2, int x3, int y3, int color) { printf("STUB: triangle depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y){};
//static void _stub_draw_256_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y) { printf("STUB: draw_256_sprite depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_sprite_v_flip (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y) { printf("STUB: draw_sprite_v_flip depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_sprite_h_flip (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y) { printf("STUB: draw_sprite_h_flip depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_sprite_vh_flip (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y) { printf("STUB: draw_sprite_vh_flip depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_trans_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y) { printf("STUB: draw_trans_sprite depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_draw_trans_rgba_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y){ printf("STUB: draw_trans_rgba_sprite depth=%d\n", bmp->vtable->color_depth);};
//static void _stub_draw_lit_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y, int color) { printf("STUB: draw_lit_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_rle_sprite (struct BITMAP *bmp, AL_CONST struct RLE_SPRITE *sprite, int x, int y) { printf("STUB: draw_rle_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_trans_rle_sprite (struct BITMAP *bmp, AL_CONST struct RLE_SPRITE *sprite, int x, int y) { printf("STUB: draw_trans_rle_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_trans_rgba_rle_sprite (struct BITMAP *bmp, AL_CONST struct RLE_SPRITE *sprite, int x, int y) { printf("STUB: draw_trans_rgba_rle_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_lit_rle_sprite (struct BITMAP *bmp, AL_CONST struct RLE_SPRITE *sprite, int x, int y, int color) { printf("STUB: draw_lit_rle_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_character (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y, int color, int bg) { printf("STUB: draw_character depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_glyph (struct BITMAP *bmp, AL_CONST struct FONT_GLYPH *glyph, int x, int y, int color, int bg) { printf("STUB: draw_glyph depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_blit_from_memory (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_from_memory depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_blit_to_memory (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_to_memory depth=%d\n", source->vtable->color_depth);  };
//static void _stub_blit_from_system (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_from_system depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_blit_to_system (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_to_system depth=%d\n", source->vtable->color_depth);  };
//static void _stub_blit_to_self (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_to_self depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_blit_to_self_forward (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_to_self_forward depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_blit_to_self_backward (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: blit_to_self_backward depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_masked_blit (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height) { printf("STUB: masked_blit depth=%d\n", dest->vtable->color_depth);  };
//static void _stub_clear_to_color (struct BITMAP *bitmap, int color) { printf("STUB: clear_to_color depth=%d\n", bitmap->vtable->color_depth);  };
static void _stub_draw_gouraud_sprite (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y, int c1, int c2, int c3, int c4) { printf("STUB: draw_gouraud_sprite depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_sprite_end (void) { printf("STUB: draw_sprite_end depth=???\n");  };
static void _stub_blit_end (void) { printf("STUB: blit_end depth=???\n");  };
static void _stub_polygon (struct BITMAP *bmp, int vertices, AL_CONST int *points, int color) { printf("STUB: polygon depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_rect (struct BITMAP *bmp, int x1, int y_1, int x2, int y2, int color) { printf("STUB: rect depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_circle (struct BITMAP *bmp, int x, int y, int radius, int color) { printf("STUB: circle depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_circlefill (struct BITMAP *bmp, int x, int y, int radius, int color) { printf("STUB: circlefill depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_ellipse (struct BITMAP *bmp, int x, int y, int rx, int ry, int color) { printf("STUB: ellipse depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_ellipsefill (struct BITMAP *bmp, int x, int y, int rx, int ry, int color) { printf("STUB: ellipsefill depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_arc (struct BITMAP *bmp, int x, int y, fixed ang1, fixed ang2, int r, int color) { printf("STUB: arc depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_spline (struct BITMAP *bmp, AL_CONST int points[8], int color) { printf("STUB: spline depth=%d\n", bmp->vtable->color_depth);  };
//static void _stub_floodfill (struct BITMAP *bmp, int x, int y, int color) { printf("STUB: floodfill depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_polygon3d (struct BITMAP *bmp, int type, struct BITMAP *texture, int vc, V3D *vtx[]) { printf("STUB: polygon3d depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_polygon3d_f (struct BITMAP *bmp, int type, struct BITMAP *texture, int vc, V3D_f *vtx[]) { printf("STUB: polygon3d_f depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_triangle3d (struct BITMAP *bmp, int type, struct BITMAP *texture, V3D *v1, V3D *v2, V3D *v3) { printf("STUB: triangle3d depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_triangle3d_f (struct BITMAP *bmp, int type, struct BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3) { printf("STUB: triangle3d_f depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_quad3d (struct BITMAP *bmp, int type, struct BITMAP *texture, V3D *v1, V3D *v2, V3D *v3, V3D *v4) { printf("STUB: quad3d depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_quad3d_f (struct BITMAP *bmp, int type, struct BITMAP *texture, V3D_f *v1, V3D_f *v2, V3D_f *v3, V3D_f *v4) { printf("STUB: quad3d_f depth=%d\n", bmp->vtable->color_depth);  };
static void _stub_draw_sprite_ex (struct BITMAP *bmp, struct BITMAP *sprite, int x, int y, int mode, int flip ) { printf("STUB: draw_sprite_ex depth=%d\n", bmp->vtable->color_depth);  };

static GFX_VTABLE _stub_vtable8 =
{
    8,
    MASK_COLOR_8,
    _stub_unbank_switch,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _linear_getpixel8, /* getpixel */
    _linear_putpixel8, /* putpixel */
    _linear_vline8,
    _linear_hline8,
    _linear_hline8,
    _stub_line,
    _stub_fastline,
    _normal_rectfill,
    _stub_triangle,
    _linear_draw_sprite8,
    _linear_draw_sprite8,
    _linear_draw_sprite_v_flip8,
    _linear_draw_sprite_h_flip8,
    _linear_draw_sprite_vh_flip8,
    _linear_draw_trans_sprite8,
    NULL,
    _linear_draw_lit_sprite8,
    _stub_draw_rle_sprite,
    _stub_draw_trans_rle_sprite,
    _stub_draw_trans_rgba_rle_sprite,
    _stub_draw_lit_rle_sprite,
    _stub_draw_character,
    _stub_draw_glyph,
    _linear_blit8,
    _linear_blit8,
    _linear_blit8,
    _linear_blit8,
    _linear_blit8,
    _linear_blit8,
    _linear_blit_backward8,
    _blit_between_formats,
    _linear_masked_blit8,
    _linear_clear_to_color8,
    _pivot_scaled_sprite_flip,
    NULL,
    _stub_draw_gouraud_sprite,
    _stub_draw_sprite_end,
    _stub_blit_end,
    _stub_polygon,
    _soft_rect,
    _stub_circle,
    _stub_circlefill,
    _stub_ellipse,
    _stub_ellipsefill,
    _stub_arc,
    _stub_spline,
    _soft_floodfill,
    _stub_polygon3d,
    _stub_polygon3d_f,
    _stub_triangle3d,
    _stub_triangle3d_f,
    _stub_quad3d,
    _stub_quad3d_f,
    _stub_draw_sprite_ex,
};


static GFX_VTABLE _stub_vtable15 =
{
    15,
    MASK_COLOR_15,
    _stub_unbank_switch,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _linear_getpixel16, /* getpixel */
    _linear_putpixel15, /* putpixel */
    _linear_vline15,
    _linear_hline15,
    _linear_hline15,
    _stub_line,
    _stub_fastline,
    _normal_rectfill,
    _stub_triangle,
    _linear_draw_sprite16,
    _linear_draw_256_sprite16,
    _linear_draw_sprite_v_flip16,
    _linear_draw_sprite_h_flip16,
    _linear_draw_sprite_vh_flip16,
    _linear_draw_trans_sprite15,
    _linear_draw_trans_rgba_sprite15,
    _linear_draw_lit_sprite15,
    _stub_draw_rle_sprite,
    _stub_draw_trans_rle_sprite,
    _stub_draw_trans_rgba_rle_sprite,
    _stub_draw_lit_rle_sprite,
    _stub_draw_character,
    _stub_draw_glyph,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit_backward16,
    _blit_between_formats,
    _linear_masked_blit16,
    _linear_clear_to_color16,
    _pivot_scaled_sprite_flip,
    NULL,
    _stub_draw_gouraud_sprite,
    _stub_draw_sprite_end,
    _stub_blit_end,
    _stub_polygon,
    _soft_rect,
    _stub_circle,
    _stub_circlefill,
    _stub_ellipse,
    _stub_ellipsefill,
    _stub_arc,
    _stub_spline,
    _soft_floodfill,
    _stub_polygon3d,
    _stub_polygon3d_f,
    _stub_triangle3d,
    _stub_triangle3d_f,
    _stub_quad3d,
    _stub_quad3d_f,
    _stub_draw_sprite_ex,
};

static GFX_VTABLE _stub_vtable16 =
{
    16,
    MASK_COLOR_16,
    _stub_unbank_switch,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _linear_getpixel16, /* getpixel */
    _linear_putpixel16, /* putpixel */
    _linear_vline16,
    _linear_hline16,
    _linear_hline16,
    _stub_line,
    _stub_fastline,
    _normal_rectfill,
    _stub_triangle,
    _linear_draw_sprite16,
    _linear_draw_256_sprite16,
    _linear_draw_sprite_v_flip16,
    _linear_draw_sprite_h_flip16,
    _linear_draw_sprite_vh_flip16,
    _linear_draw_trans_sprite16,
    _linear_draw_trans_rgba_sprite16,
    _linear_draw_lit_sprite16,
    _stub_draw_rle_sprite,
    _stub_draw_trans_rle_sprite,
    _stub_draw_trans_rgba_rle_sprite,
    _stub_draw_lit_rle_sprite,
    _stub_draw_character,
    _stub_draw_glyph,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit16,
    _linear_blit_backward16,
    _blit_between_formats,
    _linear_masked_blit16,
    _linear_clear_to_color16,
    _pivot_scaled_sprite_flip,
    NULL,
    _stub_draw_gouraud_sprite,
    _stub_draw_sprite_end,
    _stub_blit_end,
    _stub_polygon,
    _soft_rect,
    _stub_circle,
    _stub_circlefill,
    _stub_ellipse,
    _stub_ellipsefill,
    _stub_arc,
    _stub_spline,
    _soft_floodfill,
    _stub_polygon3d,
    _stub_polygon3d_f,
    _stub_triangle3d,
    _stub_triangle3d_f,
    _stub_quad3d,
    _stub_quad3d_f,
    _stub_draw_sprite_ex,
};

static GFX_VTABLE _stub_vtable32 =
{
    32,
    MASK_COLOR_32,
    _stub_unbank_switch,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _linear_getpixel32, /* getpixel */
    _linear_putpixel32, /* putpixel */
    _linear_vline32,
    _linear_hline32,
    _linear_hline32,
    _stub_line,
    _stub_fastline,
    _normal_rectfill,
    _stub_triangle,
    _linear_draw_sprite32,
    _linear_draw_256_sprite32,
    _linear_draw_sprite_v_flip32,
    _linear_draw_sprite_h_flip32,
    _linear_draw_sprite_vh_flip32,
    _linear_draw_trans_sprite32,
    _linear_draw_trans_sprite32,  // this one broke, because of alpha when rendering to screen
    _linear_draw_lit_sprite32,
    _stub_draw_rle_sprite,
    _stub_draw_trans_rle_sprite,
    _stub_draw_trans_rgba_rle_sprite,
    _stub_draw_lit_rle_sprite,
    _stub_draw_character,
    _stub_draw_glyph,
    _linear_blit32,
    _linear_blit32,
    _linear_blit32,
    _linear_blit32,
    _linear_blit32,
    _linear_blit32,
    _linear_blit_backward32,
    _blit_between_formats,
    _linear_masked_blit32,
    _linear_clear_to_color32,
    _pivot_scaled_sprite_flip,
    NULL,
    _stub_draw_gouraud_sprite,
    _stub_draw_sprite_end,
    _stub_blit_end,
    _stub_polygon,
    _soft_rect,
    _stub_circle,
    _stub_circlefill,
    _stub_ellipse,
    _stub_ellipsefill,
    _stub_arc,
    _stub_spline,
    _soft_floodfill,
    _stub_polygon3d,
    _stub_polygon3d_f,
    _stub_triangle3d,
    _stub_triangle3d_f,
    _stub_quad3d,
    _stub_quad3d_f,
    _stub_draw_sprite_ex,
};


static BITMAP *_original_screen = NULL;
BITMAP *screen = NULL;





int get_desktop_resolution(int *width, int *height)
{
    *width = -1;
    *height = -1;
    
    const int displayIndex = 0;
    SDL_DisplayMode mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
    if (SDL_GetDesktopDisplayMode(displayIndex, &mode) < 0) {
        return -1;
    };
    
    *width = mode.w;
    *height = mode.h;
    return 0;
}

static int is_card_fullscreen(int card) {
    switch (card) {
        case GFX_AUTODETECT_FULLSCREEN:
#ifdef _WIN32
        case GFX_DIRECTX:
#endif
#ifdef LINUX_VERSION
        case GFX_XWINDOWS_FULLSCREEN:
#endif
            return 1;
            break;
            
        case GFX_AUTODETECT_WINDOWED:
#ifdef _WIN32
        case GFX_DIRECTX_WIN:
#endif
#ifdef LINUX_VERSION
        case GFX_XWINDOWS:
#endif
            return 0;
            break;
            
        default:
            return 0;
    }
}


//  TODO.. sdl will convert whatever format we have.. so just report 8, 16, 32 for all resolutions
GFX_MODE_LIST *get_gfx_mode_list(int card)
{
    if (!is_card_fullscreen(card)) { return NULL; }
    
    GFX_MODE_LIST *_mode_list = malloc(sizeof(GFX_MODE_LIST));
    
    const int displayIndex = 0;
    const int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);

    _mode_list->num_modes = numDisplayModes;

    _mode_list->mode = malloc(sizeof(GFX_MODE) * (numDisplayModes + 1));
    _mode_list->mode[numDisplayModes] = (GFX_MODE) {0,0,0}; // one after last
    
    SDL_DisplayMode mode =  { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
    for (int i = 0; i < numDisplayModes; i++) {
        SDL_GetDisplayMode(displayIndex, i, &mode);
        _mode_list->mode[i] = (GFX_MODE) {mode.w, mode.h, SDL_BITSPERPIXEL(mode.format)};
    }
    
    
    return _mode_list;
}

void destroy_gfx_mode_list(GFX_MODE_LIST *mode_list)
{
    if (mode_list) {
        mode_list->num_modes = -1;
        if (mode_list->mode) {
            free (mode_list->mode);
        }
        mode_list->mode = 0;
        free (mode_list);
    }
}



// used once legitimately, then later for routing
int _color_depth = 8;                  /* how many bits per pixel? */
void set_color_depth(int depth)
{
    //  allegro did no checking
    _color_depth = depth;
}

static SDL_Window *_sdl_window = 0;
static SDL_Renderer *_sdl_renderer = 0;
SDL_Texture *_sdl_screen_texture;
int set_gfx_mode(int card, int w, int h, int v_w, int v_h)
{
    printf("STUB: set_gfx_mode\n");
    
    switch (_color_depth) {
        case 8:
        case 15:
        case 16:
        case 24:
        case 32:
            break;
        default:
            return -1;
    }
    
    _original_screen = create_bitmap_ex(_color_depth, w, h);
    
    Uint32 window_flags = 0;
    if (is_card_fullscreen(card)) {
        //window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (SDL_CreateWindowAndRenderer(w, h, window_flags, &_sdl_window, &_sdl_renderer) < 0) {
        return -1;
    };
    
    SDL_Surface *s = _original_screen->extra;
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 100, 0, 0));
    
    _sdl_screen_texture = SDL_CreateTextureFromSurface(_sdl_renderer, _original_screen->extra);
    
    screen = _original_screen;

    return 0;
}

void vsync() { /* printf("STUB: vsync\n"); */ }

//int gfx_driver() { printf("STUB: gfx_driver\n");      return 0; }
//int _current_palette() { printf("STUB: _current_palette\n");      return 0; }

void set_window_title(AL_CONST char *name) { }

GFX_VTABLE *_get_vtable(int color_depth) { printf("STUB: _get_vtable\n");      return 0; }
void request_refresh_rate(int rate) { printf("STUB: request_refresh_rate\n");      }

int set_display_switch_callback(int dir, void (*cb)(void)) { printf("STUB: set_display_switch_callback\n");      return 0; }
int set_display_switch_mode(int mode) { printf("STUB: set_display_switch_mode\n");      return 0; }



//  bitmap
void acquire_bitmap (BITMAP *bmp)
{
    if (bmp->extra) {
        SDL_LockSurface(bmp->extra);
    }
}
void release_bitmap (BITMAP *bmp)
{
    if (bmp->extra) {
        SDL_UnlockSurface(bmp->extra);
    }
}

int bitmap_color_depth (BITMAP *bmp)
{
    ASSERT(bmp);
    return bmp->vtable->color_depth;
}
int bitmap_mask_color (BITMAP *bmp)
{
    ASSERT(bmp);
    return bmp->vtable->mask_color;
}
void clear_bitmap(BITMAP *bitmap)
{
    clear_to_color(bitmap, 0);
}
BITMAP *create_bitmap(int width, int height)
{
    return create_bitmap_ex(_color_depth, width, height);
}

uintptr_t _stub_bank_switch(BITMAP *bmp, int y)
{
    return (uintptr_t)bmp->line[y];
}

BITMAP *create_bitmap_ex(int color_depth, int width, int height)
{
   // printf("STUB: create_bitmap_ex (bpp=%d, w=%d, h=%d)\n", color_depth, width, height);
    
    Uint32 rmask=0, gmask=0, bmask=0, amask=0;
    
    switch (color_depth) {
        case 8: break;
        case 15:
            rmask = 0x1f << _rgb_r_shift_15;
            gmask = 0x1f << _rgb_g_shift_15;
            bmask = 0x1f << _rgb_b_shift_15;
            break;
        case 16:
            rmask = 0x1f << _rgb_r_shift_16;
            gmask = 0x3f << _rgb_g_shift_16;
            bmask = 0x1f << _rgb_b_shift_16;
            break;
//        case 24:
//            rmask = 0xff << _rgb_r_shift_32;
//            gmask = 0xff << _rgb_g_shift_32;
//            bmask = 0xff << _rgb_b_shift_32;
//            break;
        case 32:
            rmask = 0xff << _rgb_r_shift_32;
            gmask = 0xff << _rgb_g_shift_32;
            bmask = 0xff << _rgb_b_shift_32;
            amask = 0xff << _rgb_a_shift_32;
            break;
        default:
            return 0;
    }
    
    Uint32 flags = 0;
    SDL_Surface *sdl_surface = SDL_CreateRGBSurface(flags, width, height, color_depth, rmask, gmask, bmask, amask);
    
    //  need to turn off blend mode because we just want to write surfaces to screen.
    SDL_SetSurfaceBlendMode(sdl_surface, SDL_BLENDMODE_NONE) ; //<<--- oh god super important.
    
    SDL_FillRect(sdl_surface, NULL, SDL_MapRGB(sdl_surface->format, 255, 0, 0));
    
    
    BITMAP *b = malloc(sizeof(BITMAP) + height * sizeof(unsigned char *) );

    b->dat = sdl_surface->pixels;
    b->w = b->cr = width;
    b->h = b->cb = height;
    b->clip = TRUE;
    b->cl = b->ct = 0;
    switch (color_depth) {
        case 8:
            b->vtable = &_stub_vtable8;
            break;
        case 15:
            b->vtable = &_stub_vtable15;
            break;
        case 16:
            b->vtable = &_stub_vtable16;
            break;
        case 32:
            b->vtable = &_stub_vtable32;
            break;
    }
    b->write_bank = b->read_bank = _stub_bank_switch;
    b->id = 0;
    b->extra = sdl_surface;
    b->x_ofs = 0;
    b->y_ofs = 0;
    b->seg = 0;  // default 0 on most systems

	//unsigned char **l = b->line;
	unsigned char *p = (unsigned char*)sdl_surface->pixels;
    for (int i = 0; i < sdl_surface->h; i++) {
        b->line[i] = p;
        p += sdl_surface->pitch;
    }

 


    
    
//    typedef struct BITMAP            /* a bitmap structure */
//    {
//        int w, h;                     /* width and height in pixels */
//        int clip;                     /* flag if clipping is turned on */
//        int cl, cr, ct, cb;           /* clip left, right, top and bottom values */
//        GFX_VTABLE *vtable;           /* drawing functions */
//        void *write_bank;             /* C func on some machines, asm on i386 */
//        void *read_bank;              /* C func on some machines, asm on i386 */
//        void *dat;                    /* the memory we allocated for the bitmap */
//        unsigned long id;             /* for identifying sub-bitmaps */
//        void *extra;                  /* points to a structure with more info */
//        int x_ofs;                    /* horizontal offset (for sub-bitmaps) */
//        int y_ofs;                    /* vertical offset (for sub-bitmaps) */
//        int seg;                      /* bitmap segment */
//        ZERO_SIZE_ARRAY(unsigned char *, line);
//    } BITMAP;
    
    return b;
    
 }
//BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int width, int height) {
//    printf("STUB: create_sub_bitmap\n");
//    return 0;
//}



 int _sub_bitmap_id_count = 0;

//#define BYTES_PER_PIXEL(bpp)     (((int)(bpp) + 7) / 8)


//  copied from allegro
/* create_sub_bitmap:
 *  Creates a sub bitmap, ie. a bitmap sharing drawing memory with a
 *  pre-existing bitmap, but possibly with different clipping settings.
 *  Usually will be smaller, and positioned at some arbitrary point.
 *
 *  Mark Wodrich is the owner of the brain responsible this hugely useful
 *  and beautiful function.
 */
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int width, int height)
{
    BITMAP *bitmap;
    int nr_pointers;
    int i;
    
    ASSERT(parent);
    ASSERT((x >= 0) && (y >= 0) && (x < parent->w) && (y < parent->h));
    ASSERT((width > 0) && (height > 0));
    
    if (x+width > parent->w)
        width = parent->w-x;
    
    if (y+height > parent->h)
        height = parent->h-y;
    
    /* get memory for structure and line pointers */
    /* (see create_bitmap for the reason we need at least two) */
    nr_pointers = MAX(2, height);
    bitmap = (BITMAP*)malloc(sizeof(BITMAP) + (sizeof(char *) * nr_pointers));
    if (!bitmap)
        return NULL;
    
    acquire_bitmap(parent);
    
    bitmap->w = bitmap->cr = width;
    bitmap->h = bitmap->cb = height;
    bitmap->clip = TRUE;
    bitmap->cl = bitmap->ct = 0;
    bitmap->vtable = parent->vtable;
    bitmap->x_ofs = x + parent->x_ofs;
    bitmap->y_ofs = y + parent->y_ofs;
    bitmap->extra = 0;
    
    /* All bitmaps are created with zero ID's. When a sub-bitmap is created,
     * a unique ID is needed to identify the relationship when blitting from
     * one to the other. This is obtained from the global variable
     * _sub_bitmap_id_count, which provides a sequence of integers (yes I
     * know it will wrap eventually, but not for a long time :-) If the
     * parent already has an ID the sub-bitmap adopts it, otherwise a new
     * ID is given to both the parent and the child.
     */
    if (!(parent->id & BMP_ID_MASK)) {
        parent->id |= _sub_bitmap_id_count;
        _sub_bitmap_id_count = (_sub_bitmap_id_count+1) & BMP_ID_MASK;
    }
    
    bitmap->id = parent->id | BMP_ID_SUB;
    bitmap->id &= ~BMP_ID_LOCKED;
    
    x *= BYTES_PER_PIXEL(bitmap_color_depth(bitmap));
    
    /* setup line pointers: each line points to a line in the parent bitmap */
    for (i=0; i<height; i++) {
        unsigned char *ptr = parent->line[y+i] + x;
        bitmap->line[i] = ptr;
    }
    
    release_bitmap(parent);
    
    return bitmap;
}


/* destroy_bitmap:
 *  Destroys a memory bitmap.
 */
void destroy_bitmap(BITMAP *bitmap)
{
    if (bitmap) {
        if (bitmap->extra) {
            SDL_Surface *s = bitmap->extra;
            SDL_FreeSurface(s);
        }
        free(bitmap);
    }
}


int is_planar_bitmap (BITMAP *bmp) {/* printf("STUB: is_planar_bitmap\n");  */     return 0; }
int is_screen_bitmap (BITMAP *bmp)
{
    return is_same_bitmap(bmp, screen);
//    printf("STUB: is_screen_bitmap\n");
    //  TODO.. this is a bit more complicated
    // is this bmp or any sub bmp from the screen. as in that screen variable.
//    return bmp == screen;
}
int is_linear_bitmap(BITMAP *bmp) { /* printf("STUB: is_linear_bitmap\n"); */      return 1; }
int is_memory_bitmap(BITMAP *bmp) { /* printf("STUB: is_memory_bitmap\n");  */    return 1; }

//void set_clip_rect (BITMAP *bitmap, int x1, int y_1, int x2, int y2) { printf("STUB: set_clip_rect\n");     }
//void get_clip_rect(BITMAP *bitmap, int *x1, int *y_1, int *x2, int *y2) { printf("STUB: get_clip_rect\n");  }


// transparency and patterned drawing
COLOR_MAP *color_map = NULL;


// blitting and sprites
void _nick_blit (struct BITMAP *source, struct BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
    
    // sub surfaces are terrible.
    if (!source->extra) { return; }
    if (!dest->extra) { return; }
    

    
    SDL_Rect srcrect = {.x=source_x, .y=source_y, .w=width, .h= height};
    SDL_Rect dstrect = {.x=dest_x, .y=dest_y, .w=width, .h= height};
    if (SDL_BlitSurface(source->extra, &srcrect, dest->extra, &dstrect) < 0) {
//            if (SDL_BlitSurface(source->extra, NULL, dest->extra, NULL) < 0) {
        printf("failed to blit");
    }
   
    
    
    
    if (dest == _original_screen) {
        SDL_Surface *s = _original_screen->extra;
        
        // very hacky/slow
        // look into stremaing textures http://slouken.blogspot.com.au/2011/02/streaming-textures-with-sdl-13.html
        SDL_UpdateTexture(_sdl_screen_texture, NULL, s->pixels, s->pitch);
 //       _sdl_screen_texture = SDL_CreateTextureFromSurface(_sdl_renderer, s);

         SDL_RenderClear(_sdl_renderer);
        SDL_RenderCopy(_sdl_renderer, _sdl_screen_texture, NULL, NULL);
        SDL_RenderPresent(_sdl_renderer);
       // printf("** PRESENTED\n");
    }
    
    /* if screen, flip */

}

void _sdl_render_it() {
    SDL_Surface *s = _original_screen->extra;
    
    // very hacky/slow
    // look into stremaing textures http://slouken.blogspot.com.au/2011/02/streaming-textures-with-sdl-13.html
            SDL_UpdateTexture(_sdl_screen_texture, NULL, s->pixels, s->pitch);
    //_sdl_screen_texture = SDL_CreateTextureFromSurface(_sdl_renderer, s);
    
    
    SDL_RenderClear(_sdl_renderer);
    SDL_RenderCopy(_sdl_renderer, _sdl_screen_texture, NULL, NULL);
    SDL_RenderPresent(_sdl_renderer);
    //printf("** PRESENTED\n");
    
}


void circlefill(BITMAP *bmp, int x, int y, int radius, int color) { printf("STUB: circlefill\n");      }
void line(BITMAP *bmp, int x1, int y_1, int x2, int y2, int color) { printf("STUB: line\n");        }


struct BITMAP * load_pcx(AL_CONST char *filename, struct RGB *pal) { printf("STUB: load_pcx\n");      return 0; }
struct BITMAP * load_bitmap(AL_CONST char *filename, struct RGB *pal) { printf("STUB: load_bitmap\n");      return 0; }
int save_bitmap(AL_CONST char *filename, struct BITMAP *bmp, AL_CONST struct RGB *pal) { printf("STUB: save_bitmap\n");      return -1; }


int play_fli(AL_CONST char *filename, struct BITMAP *bmp, int loop, AL_METHOD(int, callback, (void))) { printf("STUB: play_fli\n");      return 0; }



