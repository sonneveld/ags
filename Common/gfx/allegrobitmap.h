//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================
//
// Allegro lib based bitmap
//
// TODO: probably should be moved to the Engine; check again when (if) it is
// clear that AGS.Native does not need allegro for drawing.
//
//=============================================================================
#ifndef __AGS_CN_GFX__ALLEGROBITMAP_H
#define __AGS_CN_GFX__ALLEGROBITMAP_H

#include <utility>
#include <allegro.h>
#include "core/types.h"
#include "gfx/bitmap.h"

namespace AGS
{
namespace Common
{

class Bitmap
{

public:
    Bitmap();
    Bitmap(int width, int height, int color_depth = 0);
    Bitmap(BITMAP *al_bmp, bool shared_data);

    ~Bitmap();
    Bitmap(const Bitmap& other) = delete;
    Bitmap& operator=(const Bitmap& other) = delete;

    // Allocate new bitmap
    // CHECKME: color_depth = 0 is used to call Allegro's create_bitmap, which uses
    // some global color depth setting; not sure if this is OK to use for generic class,
    // revise this in future
    bool    Create(int width, int height, int color_depth = 0);
    // Allow this object to share existing bitmap data
    bool    CreateSubBitmap(Bitmap *src, const Rect &rc);
    // Create a copy of given bitmap
    bool	CreateCopy(const Bitmap *src, int color_depth = 0);
    // TODO: a temporary solution for plugin support
    bool    WrapAllegroBitmap(BITMAP *al_bmp, bool shared_data);
    // Deallocate bitmap
    void	Destroy();

    bool    LoadFromFile(const char *filename);
    bool    SaveToFile(const char *filename, const void *palette) const;

    // TODO: This is temporary solution for cases when we cannot replace
	// use of raw BITMAP struct with Bitmap
    BITMAP *GetAllegroBitmap();

    // Checks if bitmap cannot be used
    //bool IsNull() const;
    // Checks if bitmap has zero size: either width or height (or both) is zero
    //bool IsEmpty() const;
    int  GetWidth() const;
    int  GetHeight() const;
    Size GetSize() const;
    int  GetColorDepth() const;
    // BPP: bytes per pixel
    int  GetBPP() const;

    // CHECKME: probably should not be exposed, see comment to GetData()
    // int  GetDataSize() const;
    // Gets scanline length in bytes (is the same for any scanline)
    // might take into account padding.
    int  GetLineLength() const;

	// TODO: replace with byte *
	// Gets a pointer to underlying graphic data
    const unsigned char *GetData() const;

    // Get scanline for direct reading
    unsigned char *GetScanLine(int index) const;

    color_t GetMaskColor() const;

    // Converts AGS color-index into RGB color according to the bitmap format.
    color_t GetCompatibleColor(color_t color) const;

    //=========================================================================
    // Clipping
    //=========================================================================
    void    SetClip(const Rect &rc);
#ifdef AGS_DELETE_FOR_3_6
    Rect    GetClip() const;
#endif

    //=========================================================================
    // Blitting operations (drawing one bitmap over another)
    //=========================================================================
    // Draw other bitmap over current one
    void    Blit(const Bitmap *src, int dst_x = 0, int dst_y = 0, BitmapMaskOption mask = kBitmap_Copy);
    void    Blit(const Bitmap *src, int src_x, int src_y, int dst_x, int dst_y, int width, int height, BitmapMaskOption mask = kBitmap_Copy);
    // Copy other bitmap, stretching or shrinking its size to given values
    void    StretchBlt(const Bitmap *src, const Rect &dst_rc, BitmapMaskOption mask = kBitmap_Copy);
    void    StretchBlt(const Bitmap *src, const Rect &src_rc, const Rect &dst_rc, BitmapMaskOption mask = kBitmap_Copy);
    // Antia-aliased stretch-blit
    void    AAStretchBlt(const Bitmap *src, const Rect &dst_rc, BitmapMaskOption mask = kBitmap_Copy);
#ifdef AGS_DELETE_FOR_3_6
    void    AAStretchBlt(const Bitmap *src, const Rect &src_rc, const Rect &dst_rc, BitmapMaskOption mask = kBitmap_Copy);
#endif
    // TODO: find more general way to call these operations, probably require pointer to Blending data struct?
    // Draw bitmap using translucency preset
    void    TransBlendBlt(const Bitmap *src, int dst_x, int dst_y);
    // Draw bitmap using lighting preset
    void    LitBlendBlt(const Bitmap *src, int dst_x, int dst_y, int light_amount);
    // TODO: generic "draw transformed" function? What about mask option?
    void    FlipBlt(const Bitmap *src, int dst_x, int dst_y, BitmapFlip flip);
#ifdef AGS_DELETE_FOR_3_6
    void    RotateBlt(const Bitmap *src, int dst_x, int dst_y, fixed_t angle);
#endif
    void    RotateBlt(const Bitmap *src, int dst_x, int dst_y, int pivot_x, int pivot_y, fixed_t angle);

    //=========================================================================
    // Pixel operations
    //=========================================================================
    // Fills the whole bitmap with given color (black by default)
    void	Clear(color_t color = 0);
    void    ClearTransparent();
    // The PutPixel and GetPixel are supposed to be safe and therefore
    // relatively slow operations. They should not be used for changing large
    // blocks of bitmap memory - reading/writing from/to scan lines should be
    // done in such cases.
    void	PutPixel(int x, int y, color_t color);
    int		GetPixel(int x, int y) const;

    //=========================================================================
    // Vector drawing operations
    //=========================================================================
    void    DrawLine(const Line &ln, color_t color);
    void    DrawTriangle(const Triangle &tr, color_t color);
    void    DrawRect(const Rect &rc, color_t color);
    void    FillRect(const Rect &rc, color_t color);
    void    FillCircle(const Circle &circle, color_t color);
    // Fills the whole bitmap with given color
    void    Fill(color_t color);
    void    FillTransparent();
    // Floodfills an enclosed area, starting at point
    void    FloodFill(int x, int y, color_t color);

	//=========================================================================
	// Direct access operations
	//=========================================================================
    // TODO: think how to increase safety over this (some fixed memory buffer class with iterator?)
	// Gets scanline for directly writing into it
    unsigned char *GetScanLineForWriting(int index);
    unsigned char *GetDataForWriting();



private:

    static int64_t next_bitmap_id;

	BITMAP			*_alBitmap;
	bool			_isDataOwner;

    // sanity check since we have issues with use after free.
    bool deleted_ = false;


public:

    int64_t         id_;
    int64_t         version_;
};


#ifdef AGS_DELETE_FOR_3_6

namespace BitmapHelper
{
    // TODO: revise those functions later (currently needed in a few very specific cases)
	// NOTE: the resulting object __owns__ bitmap data from now on
	Bitmap *CreateRawBitmapOwner(BITMAP *al_bmp);
	// NOTE: the resulting object __does not own__ bitmap data
	Bitmap *CreateRawBitmapWrapper(BITMAP *al_bmp);
} // namespace BitmapHelper

#endif

} // namespace Common
} // namespace AGS

#endif // __AGS_CN_GFX__ALLEGROBITMAP_H
