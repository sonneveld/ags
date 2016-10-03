//
//  sdlwrap_draw.c
//  AGSKit
//
//  Created by Nick Sonneveld on 19/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include <stdio.h>
#include "allegro.h"
#include <SDL2/SDL.h>


// direct drawing
void bmp_unwrite_line(BITMAP *bmp) { }
uintptr_t bmp_write_line(BITMAP *bmp, int lyne) { return (uintptr_t)bmp->line[lyne]; }
uintptr_t bmp_read_line(BITMAP *bmp, int lyne) { return (uintptr_t)bmp->line[lyne]; }

 int getpixel (BITMAP *bmp, int x, int y)
{
    ASSERT(bmp);
    
    return bmp->vtable->getpixel(bmp, x, y);
    

}


void putpixel (BITMAP *bmp, int x, int y, int color)
{
    ASSERT(bmp);
    
     bmp->vtable->putpixel(bmp, x, y, color);
    

}




void draw_sprite (BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    ASSERT(bmp);
    ASSERT(sprite);
    
    if (sprite->vtable->color_depth == 8) {
        bmp->vtable->draw_256_sprite(bmp, sprite, x, y);
    }
    else {
        ASSERT(bmp->vtable->color_depth == sprite->vtable->color_depth);
        bmp->vtable->draw_sprite(bmp, sprite, x, y);
    }
}


void draw_sprite_v_flip (BITMAP *bmp, BITMAP *sprite, int x, int y){
    ASSERT(bmp);
    ASSERT(sprite);
    ASSERT(bmp->vtable->color_depth == sprite->vtable->color_depth);
    
    bmp->vtable->draw_sprite_v_flip(bmp, sprite, x, y);
}

void draw_sprite_h_flip (BITMAP *bmp, BITMAP *sprite, int x, int y){
    ASSERT(bmp);
    ASSERT(sprite);
    ASSERT(bmp->vtable->color_depth == sprite->vtable->color_depth);
    
    bmp->vtable->draw_sprite_h_flip(bmp, sprite, x, y);
}

void draw_sprite_vh_flip (BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    ASSERT(bmp);
    ASSERT(sprite);
    ASSERT(bmp->vtable->color_depth == sprite->vtable->color_depth);
    
    bmp->vtable->draw_sprite_vh_flip(bmp, sprite, x, y);
}

void draw_trans_sprite (BITMAP *bmp, BITMAP *sprite, int x, int y)
{
    ASSERT(bmp);
    ASSERT(sprite);
    
    if (sprite->vtable->color_depth == 32) {
        ASSERT(bmp->vtable->draw_trans_rgba_sprite);
        bmp->vtable->draw_trans_rgba_sprite(bmp, sprite, x, y);
    }
    else {
        ASSERT((bmp->vtable->color_depth == sprite->vtable->color_depth) ||
               ((bmp->vtable->color_depth == 32) &&
                (sprite->vtable->color_depth == 8)));
        bmp->vtable->draw_trans_sprite(bmp, sprite, x, y);
    }
}


void draw_lit_sprite (BITMAP *bmp, BITMAP *sprite, int x, int y, int color)
{
    ASSERT(bmp);
    ASSERT(sprite);
    ASSERT(bmp->vtable->color_depth == sprite->vtable->color_depth);
    
    bmp->vtable->draw_lit_sprite(bmp, sprite, x, y, color);
}



void rotate_sprite (BITMAP *bmp, BITMAP *sprite, int x, int y, fixed angle)
{
    ASSERT(bmp);
    ASSERT(sprite);
    
    bmp->vtable->pivot_scaled_sprite_flip(bmp, sprite, (x<<16) + (sprite->w * 0x10000) / 2,
                                          (y<<16) + (sprite->h * 0x10000) / 2,
                                          sprite->w << 15, sprite->h << 15,
                                          angle, 0x10000, FALSE);
}


void pivot_sprite (BITMAP *bmp, BITMAP *sprite, int x, int y, int cx, int cy, fixed angle)
{
    ASSERT(bmp);
    ASSERT(sprite);
    
    bmp->vtable->pivot_scaled_sprite_flip(bmp, sprite, x<<16, y<<16, cx<<16, cy<<16, angle, 0x10000, FALSE);
}






//  other bmp_read/writes defined in alconfig.h

int bmp_read24 (uintptr_t addr)
{
  unsigned char *p = (unsigned char *)addr;
  int c;
  
  c = READ3BYTES(p);
  
  return c;
}

void bmp_write24 (uintptr_t addr, int c)
{
  unsigned char *p = (unsigned char *)addr;
  
  WRITE3BYTES(p, c);
}


//  super quick put/get pixel without clipping or drawing modes

void _putpixel (BITMAP *bmp, int x, int y, int color)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    bmp_write8(addr+x, color);
}


int _getpixel (BITMAP *bmp, int x, int y)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    return bmp_read8(addr+x);
}


void _putpixel15 (BITMAP *bmp, int x, int y, int color)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    bmp_write15(addr+x*sizeof(short), color);
}


int _getpixel15 (BITMAP *bmp, int x, int y)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    return bmp_read15(addr+x*sizeof(short));
}


void _putpixel16 (BITMAP *bmp, int x, int y, int color)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    bmp_write16(addr+x*sizeof(short), color);
}


int _getpixel16 (BITMAP *bmp, int x, int y)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    return bmp_read16(addr+x*sizeof(short));
}


void _putpixel24 (BITMAP *bmp, int x, int y, int color)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    bmp_write24(addr+x*3, color);
}


int _getpixel24 (BITMAP *bmp, int x, int y)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    return bmp_read24(addr+x*3);
}


void _putpixel32 (BITMAP *bmp, int x, int y, int color)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    bmp_write32(addr+x*sizeof(int32_t), color);
}


int _getpixel32 (BITMAP *bmp, int x, int y)
{
    uintptr_t addr = (uintptr_t)bmp->line[y];
    return bmp_read32(addr+x*sizeof(int32_t));
}





void rectfill (BITMAP *bmp, int x1, int y_1, int x2, int y2, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->rectfill(bmp, x1, y_1, x2, y2, color);
}




void rect (BITMAP *bmp, int x1, int y_1, int x2, int y2, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->rect(bmp, x1, y_1, x2, y2, color);
}


 void _allegro_vline (BITMAP *bmp, int x, int y_1, int y2, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->vline(bmp, x, y_1, y2, color);
}


 void _allegro_hline (BITMAP *bmp, int x1, int y, int x2, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->hline(bmp, x1, y, x2, color);
}


/* rect:
 *  Draws an outline rectangle.
 */
void _soft_rect(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
    int t;
    
    if (x2 < x1) {
        t = x1;
        x1 = x2;
        x2 = t;
    }
    
    if (y2 < y1) {
        t = y1;
        y1 = y2;
        y2 = t;
    }
    
    acquire_bitmap(bmp);
    
    hline(bmp, x1, y1, x2, color);
    
    if (y2 > y1)
        hline(bmp, x1, y2, x2, color);
    
    if (y2-1 >= y1+1) {
        vline(bmp, x1, y1+1, y2-1, color);
        
        if (x2 > x1)
            vline(bmp, x2, y1+1, y2-1, color);
    }
    
    release_bitmap(bmp);
}



/* _normal_rectfill:
 *  Draws a solid filled rectangle, using hfill() to do the work.
 */
void _normal_rectfill(BITMAP *bmp, int x1, int y1, int x2, int y2, int color)
{
    int t;
    
    if (y1 > y2) {
        t = y1;
        y1 = y2;
        y2 = t;
    }
    
    if (bmp->clip) {
        if (x1 > x2) {
            t = x1;
            x1 = x2;
            x2 = t;
        }
        
        if (x1 < bmp->cl)
            x1 = bmp->cl;
        
        if (x2 >= bmp->cr)
            x2 = bmp->cr-1;
        
        if (x2 < x1)
            return;
        
        if (y1 < bmp->ct)
            y1 = bmp->ct;
        
        if (y2 >= bmp->cb)
            y2 = bmp->cb-1;
        
        if (y2 < y1)
            return;
        
        bmp->clip = FALSE;
        t = TRUE;
    }
    else
        t = FALSE;
    
    acquire_bitmap(bmp);
    
    while (y1 <= y2) {
        bmp->vtable->hfill(bmp, x1, y1, x2, color);
        y1++;
    };
    
    release_bitmap(bmp);
    
    bmp->clip = t;
}



 void floodfill (BITMAP *bmp, int x, int y, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->floodfill(bmp, x, y, color);
}

void line (BITMAP *bmp, int x1, int y_1, int x2, int y2, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->line(bmp, x1, y_1, x2, y2, color);
}

void circlefill (BITMAP *bmp, int x, int y, int radius, int color)
{
    ASSERT(bmp);
    
    bmp->vtable->circlefill(bmp, x, y, radius, color);
}

