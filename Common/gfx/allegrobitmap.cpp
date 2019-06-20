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

#include "gfx/allegrobitmap.h"

#include <aastr.h>
#include "debug/assert.h"

extern void __my_setcolor(int *ctset, int newcol, int wantColDep);

namespace AGS
{
namespace Common
{


int64_t Bitmap::next_bitmap_id = 1;

Bitmap::Bitmap()
    : _alBitmap(nullptr)
    , _isDataOwner(false)
    , id_(next_bitmap_id++)
    , version_(1)
{
}

Bitmap::Bitmap(int width, int height, int color_depth)
    : _alBitmap(nullptr)
    , _isDataOwner(false)
    , id_(next_bitmap_id++)
    , version_(1)
{
    Create(width, height, color_depth);
}

Bitmap::Bitmap(BITMAP *al_bmp, bool shared_data)
    : _alBitmap(nullptr)
    , _isDataOwner(false)
    , id_(next_bitmap_id++)
    , version_(1)
{
    WrapAllegroBitmap(al_bmp, shared_data);
}

Bitmap::~Bitmap()
{
    Destroy();
    deleted_ = true;
}

//=============================================================================
// Creation and destruction
//=============================================================================

bool Bitmap::Create(int width, int height, int color_depth)
{
    assert(!deleted_);
    Destroy();
    if (!color_depth) { color_depth = get_color_depth(); }  // system depth
    _alBitmap = create_bitmap_ex(color_depth, width, height);
    _isDataOwner = true;
    return _alBitmap != nullptr;
}

#ifdef AGS_DELETE_FOR_3_6

bool Bitmap::CreateTransparent(int width, int height, int color_depth)
{
    if (Create(width, height, color_depth))
    {
        clear_to_color(_alBitmap, bitmap_mask_color(_alBitmap));
        return true;
    }
    return false;
}

#endif

bool Bitmap::CreateSubBitmap(Bitmap *src, const Rect &rc)
{
    assert(!deleted_);
    Destroy();
    _alBitmap = create_sub_bitmap(src->_alBitmap, rc.Left, rc.Top, rc.GetWidth(), rc.GetHeight());
    _isDataOwner = true;
    return _alBitmap != nullptr;
}

bool Bitmap::CreateCopy(const Bitmap *src, int color_depth)
{
    assert(!deleted_);
    if (!Create(src->_alBitmap->w, src->_alBitmap->h, color_depth ? color_depth : bitmap_color_depth(src->_alBitmap))) { return false;  }
    blit(src->_alBitmap, _alBitmap, 0, 0, 0, 0, _alBitmap->w, _alBitmap->h);
    return true;
}

bool Bitmap::WrapAllegroBitmap(BITMAP *al_bmp, bool shared_data)
{
    assert(!deleted_);
    Destroy();
    _alBitmap = al_bmp;
    _isDataOwner = !shared_data;
    return _alBitmap != nullptr;
}

void Bitmap::Destroy()
{
    assert(!deleted_);
    version_++;
    if (_isDataOwner) {
        destroy_bitmap(_alBitmap);
    }
    _alBitmap = nullptr;
    _isDataOwner = false;
}

bool Bitmap::LoadFromFile(const char *filename)
{
    assert(!deleted_);
    Destroy();

	BITMAP *al_bmp = load_bitmap(filename, nullptr);
	if (al_bmp)
	{
		_alBitmap = al_bmp;
        _isDataOwner = true;
	}
	return _alBitmap != nullptr;
}

bool Bitmap::SaveToFile(const char *filename, const void *palette) const
{
    assert(!deleted_);
    return save_bitmap(filename, _alBitmap, (const RGB*)palette) == 0;
}

// TODO: This is temporary solution for cases when we cannot replace
// use of raw BITMAP struct with Bitmap
BITMAP *Bitmap::GetAllegroBitmap()
{
    assert(!deleted_);
    version_++;
    return _alBitmap;
}

#if AGS_DELETE_FOR_3_6

// Checks if bitmap has zero size: either width or height (or both) is zero
bool Bitmap::IsEmpty() const
{
    return GetWidth() == 0 || GetHeight() == 0;
}
#endif

int  Bitmap::GetWidth() const
{
    assert(!deleted_);
    return _alBitmap->w;
}

int  Bitmap::GetHeight() const
{
    assert(!deleted_);
    return _alBitmap->h;
}

Size Bitmap::GetSize() const
{
    assert(!deleted_);
    return Size(_alBitmap->w, _alBitmap->h);
}

int  Bitmap::GetColorDepth() const
{
    assert(!deleted_);
    return bitmap_color_depth(_alBitmap);
}

// BPP: bytes per pixel
int  Bitmap::GetBPP() const
{
    assert(!deleted_);
    return (GetColorDepth() + 7) / 8;
}

#if AGS_DELETE_FOR_3_6

// CHECKME: probably should not be exposed, see comment to GetData()
int  Bitmap::GetDataSize() const
{
    return GetWidth() * GetHeight() * GetBPP();
}

#endif

// Gets scanline length in bytes (is the same for any scanline)
int  Bitmap::GetLineLength() const
{
    // line may include padding:
    assert(!deleted_);
    if (GetHeight() >= 2) {
        return (char*)_alBitmap->line[1] - (char*)_alBitmap->line[0];
    }
    return GetWidth() * GetBPP();
}

// TODO: replace with byte *
// Gets a pointer to underlying graphic data
const unsigned char *Bitmap::GetData() const
{
    assert(!deleted_);
    return _alBitmap->line[0];
}

// Get scanline for direct reading
unsigned char *Bitmap::GetScanLine(int index) const
{
    assert(!deleted_);
    return (index >= 0 && index < GetHeight()) ? _alBitmap->line[index] : nullptr;
}

color_t Bitmap::GetMaskColor() const
{
    assert(!deleted_);
    return bitmap_mask_color(_alBitmap);
}

color_t Bitmap::GetCompatibleColor(color_t color) const
{
    assert(!deleted_);
    color_t compat_color = 0;
    __my_setcolor(&compat_color, color, bitmap_color_depth(_alBitmap));
    return compat_color;
}

//=============================================================================
// Clipping
//=============================================================================

void Bitmap::SetClip(const Rect &rc)
{
    assert(!deleted_);
    version_++;
    set_clip_rect(_alBitmap, rc.Left, rc.Top, rc.Right, rc.Bottom);
}

#ifdef AGS_DELETE_FOR_3_6

Rect Bitmap::GetClip() const
{
	Rect temp;
	get_clip_rect(_alBitmap, &temp.Left, &temp.Top, &temp.Right, &temp.Bottom);
	return temp;
}

#endif

//=============================================================================
// Blitting operations (drawing one bitmap over another)
//=============================================================================

void Bitmap::Blit(const Bitmap *src, int dst_x, int dst_y, BitmapMaskOption mask)
{	
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	// WARNING: For some evil reason Allegro expects dest and src bitmaps in different order for blit and draw_sprite
	if (mask == kBitmap_Transparency)
	{
		draw_sprite(_alBitmap, al_src_bmp, dst_x, dst_y);
	}
	else
	{
		blit(al_src_bmp, _alBitmap, 0, 0, dst_x, dst_y, al_src_bmp->w, al_src_bmp->h);
	}
}

void Bitmap::Blit(const Bitmap *src, int src_x, int src_y, int dst_x, int dst_y, int width, int height, BitmapMaskOption mask)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	if (mask == kBitmap_Transparency)
	{
		masked_blit(al_src_bmp, _alBitmap, src_x, src_y, dst_x, dst_y, width, height);
	}
	else
	{
		blit(al_src_bmp, _alBitmap, src_x, src_y, dst_x, dst_y, width, height);
	}
}

void Bitmap::StretchBlt(const Bitmap *src, const Rect &dst_rc, BitmapMaskOption mask)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	// WARNING: For some evil reason Allegro expects dest and src bitmaps in different order for blit and draw_sprite
	if (mask == kBitmap_Transparency)
	{
		stretch_sprite(_alBitmap, al_src_bmp,
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
	else
	{
		stretch_blit(al_src_bmp, _alBitmap,
			0, 0, al_src_bmp->w, al_src_bmp->h,
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
}

void Bitmap::StretchBlt(Bitmap const *src, const Rect &src_rc, const Rect &dst_rc, BitmapMaskOption mask)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	if (mask == kBitmap_Transparency)
	{
		masked_stretch_blit(al_src_bmp, _alBitmap,
			src_rc.Left, src_rc.Top, src_rc.GetWidth(), src_rc.GetHeight(),
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
	else
	{
		stretch_blit(al_src_bmp, _alBitmap,
			src_rc.Left, src_rc.Top, src_rc.GetWidth(), src_rc.GetHeight(),
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
}

void Bitmap::AAStretchBlt(const Bitmap *src, const Rect &dst_rc, BitmapMaskOption mask)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	// WARNING: For some evil reason Allegro expects dest and src bitmaps in different order for blit and draw_sprite
	if (mask == kBitmap_Transparency)
	{
		aa_stretch_sprite(_alBitmap, al_src_bmp,
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
	else
	{
		aa_stretch_blit(al_src_bmp, _alBitmap,
			0, 0, al_src_bmp->w, al_src_bmp->h,
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
}

#ifdef AGS_DELETE_FOR_3_6

void Bitmap::AAStretchBlt(Bitmap *src, const Rect &src_rc, const Rect &dst_rc, BitmapMaskOption mask)
{
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	if (mask == kBitmap_Transparency)
	{
		// TODO: aastr lib does not expose method for masked stretch blit; should do that at some point since 
		// the source code is a gift-ware anyway
		// aa_masked_blit(_alBitmap, al_src_bmp, src_rc.Left, src_rc.Top, src_rc.GetWidth(), src_rc.GetHeight(), dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
		throw "aa_masked_blit is not yet supported!";
	}
	else
	{
		aa_stretch_blit(al_src_bmp, _alBitmap,
			src_rc.Left, src_rc.Top, src_rc.GetWidth(), src_rc.GetHeight(),
			dst_rc.Left, dst_rc.Top, dst_rc.GetWidth(), dst_rc.GetHeight());
	}
}

#endif

void Bitmap::TransBlendBlt(const Bitmap *src, int dst_x, int dst_y)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	draw_trans_sprite(_alBitmap, al_src_bmp, dst_x, dst_y);
}

void Bitmap::LitBlendBlt(const Bitmap *src, int dst_x, int dst_y, int light_amount)
{
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	draw_lit_sprite(_alBitmap, al_src_bmp, dst_x, dst_y, light_amount);
}

void Bitmap::FlipBlt(const Bitmap *src, int dst_x, int dst_y, BitmapFlip flip)
{	
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	if (flip == kBitmap_HFlip)
	{
		draw_sprite_h_flip(_alBitmap, al_src_bmp, dst_x, dst_y);
	}
	else if (flip == kBitmap_VFlip)
	{
		draw_sprite_v_flip(_alBitmap, al_src_bmp, dst_x, dst_y);
	}
	else if (flip == kBitmap_HVFlip)
	{
		draw_sprite_vh_flip(_alBitmap, al_src_bmp, dst_x, dst_y);
	}
}

#ifdef AGS_DELETE_FOR_3_6

void Bitmap::RotateBlt(Bitmap *src, int dst_x, int dst_y, fixed_t angle)
{
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	rotate_sprite(_alBitmap, al_src_bmp, dst_x, dst_y, angle);
}

#endif

void Bitmap::RotateBlt(const Bitmap *src, int dst_x, int dst_y, int pivot_x, int pivot_y, fixed_t angle)
{	
    assert(!deleted_);
    version_++;
    BITMAP *al_src_bmp = src->_alBitmap;
	pivot_sprite(_alBitmap, al_src_bmp, dst_x, dst_y, pivot_x, pivot_y, angle);
}

//=============================================================================
// Pixel operations
//=============================================================================

void Bitmap::Clear(color_t color)
{
    assert(!deleted_);
    version_++;
    if (color)
	{
		clear_to_color(_alBitmap, color);
	}
	else
	{
		clear_bitmap(_alBitmap);	
	}
}

void Bitmap::ClearTransparent()
{
    assert(!deleted_);
    version_++;
    clear_to_color(_alBitmap, bitmap_mask_color(_alBitmap));
}

void Bitmap::PutPixel(int x, int y, color_t color)
{
    assert(!deleted_);
    if (x < 0 || x >= _alBitmap->w || y < 0 || y >= _alBitmap->h)
    {
        return;
    }
    version_++;

	switch (bitmap_color_depth(_alBitmap))
	{
	case 8:
		return _putpixel(_alBitmap, x, y, color);
	case 15:
		return _putpixel15(_alBitmap, x, y, color);
	case 16:
		return _putpixel16(_alBitmap, x, y, color);
	case 24:
		return _putpixel24(_alBitmap, x, y, color);
	case 32:
		return _putpixel32(_alBitmap, x, y, color);
	}
    assert(0); // this should not normally happen
	return putpixel(_alBitmap, x, y, color);
}

int Bitmap::GetPixel(int x, int y) const
{
    assert(!deleted_);
    if (x < 0 || x >= _alBitmap->w || y < 0 || y >= _alBitmap->h)
    {
        return -1; // Allegros getpixel() implementation returns -1 in this case
    }

	switch (bitmap_color_depth(_alBitmap))
	{
	case 8:
		return _getpixel(_alBitmap, x, y);
	case 15:
		return _getpixel15(_alBitmap, x, y);
	case 16:
		return _getpixel16(_alBitmap, x, y);
	case 24:
		return _getpixel24(_alBitmap, x, y);
	case 32:
		return _getpixel32(_alBitmap, x, y);
	}
    assert(0); // this should not normally happen
	return getpixel(_alBitmap, x, y);
}

//=============================================================================
// Vector drawing operations
//=============================================================================

void Bitmap::DrawLine(const Line &ln, color_t color)
{
    assert(!deleted_);
    version_++;
    line(_alBitmap, ln.X1, ln.Y1, ln.X2, ln.Y2, color);
}

void Bitmap::DrawTriangle(const Triangle &tr, color_t color)
{
    assert(!deleted_);
    version_++;
    triangle(_alBitmap, tr.X1, tr.Y1, tr.X2, tr.Y2, tr.X3, tr.Y3, color);
}

void Bitmap::DrawRect(const Rect &rc, color_t color)
{
    assert(!deleted_);
    version_++;
    rect(_alBitmap, rc.Left, rc.Top, rc.Right, rc.Bottom, color);
}

void Bitmap::FillRect(const Rect &rc, color_t color)
{
    assert(!deleted_);
    version_++;
    rectfill(_alBitmap, rc.Left, rc.Top, rc.Right, rc.Bottom, color);
}

void Bitmap::FillCircle(const Circle &circle, color_t color)
{
    assert(!deleted_);
    version_++;
    circlefill(_alBitmap, circle.X, circle.Y, circle.Radius, color);
}

void Bitmap::Fill(color_t color)
{
    assert(!deleted_);
    version_++;
    if (color)
	{
		clear_to_color(_alBitmap, color);
	}
	else
	{
		clear_bitmap(_alBitmap);	
	}
}

void Bitmap::FillTransparent()
{
    assert(!deleted_);
    version_++;
    clear_to_color(_alBitmap, bitmap_mask_color(_alBitmap));
}

void Bitmap::FloodFill(int x, int y, color_t color)
{
    assert(!deleted_);
    version_++;
    floodfill(_alBitmap, x, y, color);
}

//=============================================================================
// Direct access operations
//=============================================================================

unsigned char *Bitmap::GetScanLineForWriting(int index)
{
    assert(!deleted_);
    if (index < 0) { return nullptr; }
    if (index >= GetHeight()) { return nullptr; }
    version_++;
    return _alBitmap->line[index];
}
unsigned char *Bitmap::GetDataForWriting()
{
    assert(!deleted_);
    version_++;
    return _alBitmap->line[0];
}

#ifdef AGS_DELETE_FOR_3_6

namespace BitmapHelper
{

Bitmap *CreateRawBitmapOwner(BITMAP *al_bmp)
{
	Bitmap *bitmap = new Bitmap();
	if (!bitmap->WrapAllegroBitmap(al_bmp, false))
	{
		delete bitmap;
		bitmap = nullptr;
	}
	return bitmap;
}

Bitmap *CreateRawBitmapWrapper(BITMAP *al_bmp)
{
	Bitmap *bitmap = new Bitmap();
	if (!bitmap->WrapAllegroBitmap(al_bmp, true))
	{
		delete bitmap;
		bitmap = nullptr;
	}
	return bitmap;
}

} // namespace BitmapHelper

#endif


} // namespace Common
} // namespace AGS
