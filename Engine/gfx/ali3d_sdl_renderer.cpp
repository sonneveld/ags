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
// Allegro Interface for 3D; Software mode Allegro driver
//
//=============================================================================


#include "gfx/ali3dexception.h"
#include "gfx/ali3d_sdl_renderer.h"
#include "gfx/gfxfilter_sdl_renderer.h"
#include "gfx/gfx_util.h"
#include "main/main_allegro.h"
#include "platform/base/agsplatformdriver.h"

#include "sdl2alleg.h"

#include "ac/timer.h"

#include <algorithm>  // for std::max and std::min
#undef min
#undef max

extern void process_pending_events();

namespace AGS
{
namespace Engine
{
namespace SDLRenderer
{

using namespace Common;

// ----------------------------------------------------------------------------
// SDLRendererGfxModeList
// ----------------------------------------------------------------------------

bool SDLRendererGfxModeList::GetMode(int index, DisplayMode &mode) const
{
    if (_gfxModeList && index >= 0 && index < _gfxModeList->num_modes)
    {
        mode.Width = _gfxModeList->mode[index].width;
        mode.Height = _gfxModeList->mode[index].height;
        mode.ColorDepth = _gfxModeList->mode[index].bpp;
        return true;
    }
    return false;
}



// ----------------------------------------------------------------------------
// SDLRendererGraphicsDriver
// ----------------------------------------------------------------------------

SDLRendererGraphicsDriver* lastDisplayedDriver = nullptr;

#define GFX_SDL2_WINDOW                     AL_ID('S','D','L','W')
#define GFX_SDL2_FULLSCREEN                 AL_ID('S','D','L','F')

const int DEFAULT_DISPLAY_INDEX = 0;

#if SDL_VERSION_ATLEAST(2, 0, 5)
// used to add alpha back to rendered screen.
static auto fix_alpha_blender = SDL_ComposeCustomBlendMode(
      SDL_BLENDFACTOR_ZERO,
      SDL_BLENDFACTOR_ONE,
      SDL_BLENDOPERATION_ADD,
      SDL_BLENDFACTOR_ONE,
      SDL_BLENDFACTOR_ZERO,
      SDL_BLENDOPERATION_ADD
    );
#endif

SDLRendererGraphicsDriver::SDLRendererGraphicsDriver()
{
  _tint_red = 0;
  _tint_green = 0;
  _tint_blue = 0;
  _gfxModeList = NULL;

  _allegroScreenWrapper = NULL;
  _origVirtualScreen = NULL;
  virtualScreen = NULL;
  _stageVirtualScreen = NULL;

  // Initialize default sprite batch, it will be used when no other batch was activated
  SDLRendererGraphicsDriver::InitSpriteBatch(0, _spriteBatchDesc[0]);
}

void SDLRendererGraphicsDriver::UpdateDeviceScreen(const Size &screenSize) {}

bool SDLRendererGraphicsDriver::IsModeSupported(const DisplayMode &mode)
{
  if (mode.Width <= 0 || mode.Height <= 0)
  {
    set_allegro_error("Invalid resolution parameters: %d x %d x %d", mode.Width, mode.Height);
    return false;
  }

  switch (mode.ColorDepth) {
    case 8:
    case 16:
    case 32:
      break;
    default:
      set_allegro_error("Invalid colour depth: %d", mode.ColorDepth);
      return false;
  }

  // Everything is drawn to a virtual screen, so all resolutions are supported.
  return true;
}

int SDLRendererGraphicsDriver::GetDisplayDepthForNativeDepth(int native_color_depth) const
{
    // TODO: check for device caps to know which depth is supported?
    if (native_color_depth > 8)
        return 32;
    return native_color_depth;
}

IGfxModeList *SDLRendererGraphicsDriver::GetSupportedModeList(int color_depth)
{
  if (_gfxModeList == NULL)
  {
    _gfxModeList = get_gfx_mode_list(GetAllegroGfxDriverID(false));
  }
  if (_gfxModeList == NULL)
  {
    return NULL;
  }
  return new SDLRendererGfxModeList(_gfxModeList);
}

PGfxFilter SDLRendererGraphicsDriver::GetGraphicsFilter() const
{
    return _filter;
}
int SDLRendererGraphicsDriver::GetAllegroGfxDriverID(bool windowed)
{
  return windowed ? GFX_SDL2_WINDOW : GFX_SDL2_FULLSCREEN;
}

void SDLRendererGraphicsDriver::SetGraphicsFilter(PALSWFilter filter)
{
  _filter = filter;
  OnSetFilter();

  // If we already have a gfx mode set, then use the new filter to update virtual screen immediately
  CreateVirtualScreen();
}

void SDLRendererGraphicsDriver::SetScreenFade(int red, int green, int blue)
{
  // TODO?
}

void SDLRendererGraphicsDriver::SetTintMethod(TintMethod method) 
{
  // TODO: support new D3D-style tint method
}


static Uint32 _sdl2_palette[256];

static void sdl_renderer_set_palette_range(AL_CONST PALETTE p, int from, int to, int vsync) 
{
   for (int i=from; i<=to; i++) {
      RGB c = p[i];
      _sdl2_palette[i] = (_rgb_scale_6[c.r] << 16) | (_rgb_scale_6[c.g] << 8) | (_rgb_scale_6[c.b]);
   }

  if (lastDisplayedDriver) {
    lastDisplayedDriver->Present();
  }
}

bool SDLRendererGraphicsDriver::SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer)
{
  if (!IsModeSupported(mode)) { return false; }

  ReleaseDisplayMode();

  if (_initGfxCallback != NULL)
    _initGfxCallback(NULL);

  set_color_depth(mode.ColorDepth);

   /* We probably don't want to do this because it makes
    * Allegro "forget" the color layout of previously set
    * graphics modes. But it should be retained if bitmaps
    * created in those modes are to be used in the new mode.
    */
#if 0
   /* restore default truecolor pixel format */
   _rgb_r_shift_15 = 0; 
   _rgb_g_shift_15 = 5;
   _rgb_b_shift_15 = 10;
   _rgb_r_shift_16 = 0;
   _rgb_g_shift_16 = 5;
   _rgb_b_shift_16 = 11;
   _rgb_r_shift_24 = 0;
   _rgb_g_shift_24 = 8;
   _rgb_b_shift_24 = 16;
   _rgb_r_shift_32 = 0;
   _rgb_g_shift_32 = 8;
   _rgb_b_shift_32 = 16;
   _rgb_a_shift_32 = 24;
#endif

   /*
   SDL_PIXELFORMAT_ARGB8888:
   *Amask = masks[0] = 0xFF000000;   24
   *Rmask = masks[1] = 0x00FF0000;   16  
   *Gmask = masks[2] = 0x0000FF00;   8
   *Bmask = masks[3] = 0x000000FF;   0 
   */

   _rgb_r_shift_15 = 10;
   _rgb_g_shift_15 = 5;
   _rgb_b_shift_15 = 0;

   _rgb_r_shift_16 = 11;
   _rgb_g_shift_16 = 5;
   _rgb_b_shift_16 = 0;

   _rgb_r_shift_24 = 16;
   _rgb_g_shift_24 = 8;
   _rgb_b_shift_24 = 0;

   _rgb_a_shift_32 = 24;
   _rgb_r_shift_32 = 16; 
   _rgb_g_shift_32 = 8; 
   _rgb_b_shift_32 = 0;

   switch(mode.ColorDepth) {
      case 8:
      case 16:
      case 32:
         break;
      default:
         printf("unsupported colour depth\n");
         return false;
   }

   Uint32 flags = SDL_WINDOW_RESIZABLE;
   if (!mode.Windowed) {
     flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
   }

   window = SDL_CreateWindow(
        window_title,                  // window title
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial x position
        SDL_WINDOWPOS_CENTERED_DISPLAY(DEFAULT_DISPLAY_INDEX),           // initial y position
        mode.Width,                               // width, in pixels
        mode.Height,                               // height, in pixels
        flags                 // flags 
    );

  if (SDL_GetWindowGammaRamp(window, default_gamma_red, default_gamma_green, default_gamma_blue) == 0) {
    has_gamma = true;
  }

  Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
  if (mode.Vsync) {
    rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
  }

  renderer = SDL_CreateRenderer(window, -1, rendererFlags);

  SDL_RendererInfo rinfo {};
  if (SDL_GetRendererInfo(renderer, &rinfo) == 0) {
    printf("Created Renderer: %s\n", rinfo.name);
    printf("Available texture formats:\n");
    for (int i = 0; i < rinfo.num_texture_formats; i++) {
        printf(" - %s\n", SDL_GetPixelFormatName(rinfo.texture_formats[i]));
    }
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  // SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
  SDL_RenderSetLogicalSize(renderer, mode.Width, mode.Height);

  screenTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mode.Width, mode.Height);
 
  gfx_backing_bitmap = create_bitmap_ex(mode.ColorDepth, mode.Width, mode.Height);
  screen = gfx_backing_bitmap;

  /* set up the default colors */
  system_driver->set_palette_range = sdl_renderer_set_palette_range;
  set_palette_range(default_palette, 0, 255, 0);

  fake_texture_bitmap = (BITMAP *)calloc(1, sizeof(BITMAP) + (sizeof(char *) * mode.Height));
  fake_texture_bitmap->w = mode.Width;
  fake_texture_bitmap->cr = mode.Width;
  fake_texture_bitmap->h = mode.Height;
  fake_texture_bitmap->cb = mode.Height;
  fake_texture_bitmap->clip = true;
  fake_texture_bitmap->cl = 0;
  fake_texture_bitmap->ct = 0;
  fake_texture_bitmap->id = 0;
  fake_texture_bitmap->extra = nullptr;
  fake_texture_bitmap->x_ofs = 0;
  fake_texture_bitmap->y_ofs = 0;
  fake_texture_bitmap->dat = nullptr;

  auto tmpbitmap = create_bitmap_ex(32, 1, 1);
  fake_texture_bitmap->vtable = tmpbitmap->vtable;
  fake_texture_bitmap->write_bank = tmpbitmap->write_bank;
  fake_texture_bitmap->read_bank = tmpbitmap->read_bank;
  destroy_bitmap(tmpbitmap);

  // set_gfx_mode is an allegro function that creates screen bitmap;
  // following code assumes the screen is already created, therefore we should
  // ensure global bitmap wraps over existing allegro screen bitmap.
  _allegroScreenWrapper = BitmapHelper::CreateRawBitmapWrapper(screen);

  OnModeSet(mode);

  // If we already have a gfx filter, then use it to update virtual screen immediately
  CreateVirtualScreen();
  _allegroScreenWrapper->Clear();

  lastDisplayedDriver = this;

  return true;
}

void SDLRendererGraphicsDriver::CreateVirtualScreen()
{
  if (!IsModeSet() || !IsRenderFrameValid() || !IsNativeSizeValid())
    return;
  DestroyVirtualScreen();
  // Adjust clipping so nothing gets drawn outside the game frame
  _allegroScreenWrapper->SetClip(_dstRect);
  // Initialize scaling filter and receive virtual screen pointer
  // (which may or not be the same as real screen)
  _origVirtualScreen = _allegroScreenWrapper;
  virtualScreen = _allegroScreenWrapper;
  _stageVirtualScreen = _allegroScreenWrapper;
}

void SDLRendererGraphicsDriver::DestroyVirtualScreen()
{
  _origVirtualScreen = NULL;
  virtualScreen = NULL;
  _stageVirtualScreen = NULL;
}

void SDLRendererGraphicsDriver::ReleaseDisplayMode()
{
  OnModeReleased();
  ClearDrawLists();

  DestroyVirtualScreen();

  // Note this does not destroy the underlying allegro screen bitmap, only wrapper.
  delete _allegroScreenWrapper;
  _allegroScreenWrapper = NULL;
}

bool SDLRendererGraphicsDriver::SetNativeSize(const Size &src_size)
{
  OnSetNativeSize(src_size);
  // If we already have a gfx mode and gfx filter set, then use it to update virtual screen immediately
  CreateVirtualScreen();
  return !_srcRect.IsEmpty();
}

bool SDLRendererGraphicsDriver::SetRenderFrame(const Rect &dst_rect)
{
  OnSetRenderFrame(dst_rect);
  // If we already have a gfx mode and gfx filter set, then use it to update virtual screen immediately
  CreateVirtualScreen();
  return !_dstRect.IsEmpty();
}

SDLRendererGraphicsDriver::~SDLRendererGraphicsDriver()
{
  SDLRendererGraphicsDriver::UnInit();
}

void SDLRendererGraphicsDriver::UnInit()
{
  OnUnInit();
  ReleaseDisplayMode();

  if (_gfxModeList != NULL)
  {
    destroy_gfx_mode_list(_gfxModeList);
    _gfxModeList = NULL;
  }
}

bool SDLRendererGraphicsDriver::SupportsGammaControl() 
{
  return has_gamma;
}

void SDLRendererGraphicsDriver::SetGamma(int newGamma)
{
  if (!has_gamma) { return; }

  Uint16 gamma_red[256];
  Uint16 gamma_green[256];
  Uint16 gamma_blue[256];

  for (int i = 0; i < 256; i++) {
    gamma_red[i] = std::min(((int)default_gamma_red[i] * newGamma) / 100, 0xffff);
    gamma_green[i] = std::min(((int)default_gamma_green[i] * newGamma) / 100, 0xffff);
    gamma_blue[i] = std::min(((int)default_gamma_blue[i] * newGamma) / 100, 0xffff);
  }

  SDL_SetWindowGammaRamp(window, gamma_red, gamma_green, gamma_blue);
}

int SDLRendererGraphicsDriver::GetCompatibleBitmapFormat(int color_depth)
{
  return color_depth;
}

IDriverDependantBitmap* SDLRendererGraphicsDriver::CreateDDBFromBitmap(Bitmap *bitmap, bool hasAlpha, bool opaque)
{
  SDLRendererBitmap* newBitmap = new SDLRendererBitmap(bitmap, opaque, hasAlpha);
  return newBitmap;
}

void SDLRendererGraphicsDriver::UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, Bitmap *bitmap, bool hasAlpha)
{
  SDLRendererBitmap* alSwBmp = (SDLRendererBitmap*)bitmapToUpdate;
  alSwBmp->_bmp = bitmap;
  alSwBmp->_hasAlpha = hasAlpha;
}

void SDLRendererGraphicsDriver::DestroyDDB(IDriverDependantBitmap* bitmap)
{
  delete bitmap;
}

void SDLRendererGraphicsDriver::InitSpriteBatch(size_t index, const SpriteBatchDesc &desc)
{
    if (_spriteBatches.size() <= index)
        _spriteBatches.resize(index + 1);
    ALSpriteBatch &batch = _spriteBatches[index];
    batch.List.clear();
    // TODO: correct offsets to have pre-scale (source) and post-scale (dest) offsets!
    int src_w = desc.Viewport.GetWidth() / desc.Transform.ScaleX;
    int src_h = desc.Viewport.GetHeight() / desc.Transform.ScaleY;
    if (desc.Surface != NULL)
    {
        batch.Surface = std::static_pointer_cast<Bitmap>(desc.Surface);
        batch.Opaque = true;
    }
    else if (desc.Viewport.IsEmpty() || !virtualScreen)
    {
        batch.Surface.reset();
        batch.Opaque = false;
    }
    else if (desc.Transform.ScaleX == 1.f && desc.Transform.ScaleY == 1.f)
    {
        Rect rc = RectWH(desc.Viewport.Left - _virtualScrOff.X, desc.Viewport.Top - _virtualScrOff.Y, desc.Viewport.GetWidth(), desc.Viewport.GetHeight());
        batch.Surface.reset(BitmapHelper::CreateSubBitmap(virtualScreen, rc));
        batch.Opaque = true;
    }
    else if (!batch.Surface || batch.Surface->GetWidth() != src_w || batch.Surface->GetHeight() != src_h)
    {
        batch.Surface.reset(new Bitmap(src_w, src_h));
        batch.Opaque = false;
    }
}

void SDLRendererGraphicsDriver::ResetAllBatches()
{
    for (ALSpriteBatches::iterator it = _spriteBatches.begin(); it != _spriteBatches.end(); ++it)
        it->List.clear();
}

void SDLRendererGraphicsDriver::DrawSprite(int x, int y, IDriverDependantBitmap* bitmap)
{
    _spriteBatches[_actSpriteBatch].List.push_back(ALDrawListEntry((SDLRendererBitmap*)bitmap, x, y));
}

void SDLRendererGraphicsDriver::RenderToBackBuffer()
{
    // Render all the sprite batches with necessary transformations
    //
    // NOTE: that's not immediately clear whether it would be faster to first draw upon a camera-sized
    // surface then stretch final result to the viewport on screen, or stretch-blit each individual
    // sprite right onto screen bitmap. We'd need to do proper profiling to know that.
    // An important thing is that Allegro does not provide stretching functions for drawing sprites
    // with blending and translucency; it seems you'd have to first stretch the original sprite onto a
    // temp buffer and then TransBlendBlt / LitBlendBlt it to the final destination. Of course, doing
    // that here would slow things down significantly, so if we ever go that way sprite caching will
    // be required (similarily to how AGS caches flipped/scaled object sprites now for).
    //
    for (size_t i = 0; i <= _actSpriteBatch; ++i)
    {
        const Rect &viewport = _spriteBatchDesc[i].Viewport;
        const SpriteTransform &transform = _spriteBatchDesc[i].Transform;
        const ALSpriteBatch &batch = _spriteBatches[i];

        virtualScreen->SetClip(Rect::MoveBy(viewport, -_virtualScrOff.X, -_virtualScrOff.Y));
        Bitmap *surface = batch.Surface.get();
        // TODO: correct transform offsets to have pre-scale (source) and post-scale (dest) offsets!
        int view_offx = viewport.Left + transform.X - _virtualScrOff.X;
        int view_offy = viewport.Top + transform.Y - _virtualScrOff.Y;
        if (surface)
        {
            if (!batch.Opaque)
                surface->ClearTransparent();
            _stageVirtualScreen = surface;
            RenderSpriteBatch(batch, surface, 0, 0);
            // TODO: extract this to the generic software blit-with-transform function
            virtualScreen->StretchBlt(surface, RectWH(view_offx, view_offy, viewport.GetWidth(), viewport.GetHeight()),
                batch.Opaque ? kBitmap_Copy : kBitmap_Transparency);
        }
        else
        {
            RenderSpriteBatch(batch, virtualScreen, view_offx, view_offy);
        }
        _stageVirtualScreen = virtualScreen;
    }
    ClearDrawLists();
}


// add the alpha values together, used for compositing alpha images
static unsigned long _trans_alpha_blender32(unsigned long x, unsigned long y, unsigned long n)
{
   unsigned long res, g;

   n = (n * geta32(x)) / 256;

   if (n)
      n++;

   res = ((x & 0xFF00FF) - (y & 0xFF00FF)) * n / 256 + y;
   y &= 0xFF00;
   x &= 0xFF00;
   g = (x - y) * n / 256 + y;

   res &= 0xFF00FF;
   g &= 0xFF00;

   return res | g;
}

void SDLRendererGraphicsDriver::RenderSpriteBatch(const ALSpriteBatch &batch, Common::Bitmap *surface, int surf_offx, int surf_offy)
{
  const std::vector<ALDrawListEntry> &drawlist = batch.List;
  for (size_t i = 0; i < drawlist.size(); i++)
  {
    if (drawlist[i].bitmap == NULL)
    {
      if (_nullSpriteCallback)
        _nullSpriteCallback(drawlist[i].x, drawlist[i].y);
      else
        throw Ali3DException("Unhandled attempt to draw null sprite");

      continue;
    }

    SDLRendererBitmap* bitmap = drawlist[i].bitmap;
    int drawAtX = drawlist[i].x + surf_offx;
    int drawAtY = drawlist[i].y + surf_offy;

    if ((bitmap->_opaque) && (bitmap->_bmp == surface))
    { }
    else if (bitmap->_opaque)
    {
        surface->Blit(bitmap->_bmp, 0, 0, drawAtX, drawAtY, bitmap->_bmp->GetWidth(), bitmap->_bmp->GetHeight());
    }
    else if (bitmap->_transparency >= 255)
    {
      // fully transparent... invisible, do nothing
    }
    else if (bitmap->_hasAlpha)
    {
      if (bitmap->_transparency == 0) // this means opaque
        set_alpha_blender();
      else
        // here _transparency is used as alpha (between 1 and 254)
        set_blender_mode(NULL, NULL, _trans_alpha_blender32, 0, 0, 0, bitmap->_transparency);

      surface->TransBlendBlt(bitmap->_bmp, drawAtX, drawAtY);
    }
    else
    {
      // here _transparency is used as alpha (between 1 and 254), but 0 means opaque!
      GfxUtil::DrawSpriteWithTransparency(surface, bitmap->_bmp, drawAtX, drawAtY,
          bitmap->_transparency ? bitmap->_transparency : 255);
    }
  }

  if (((_tint_red > 0) || (_tint_green > 0) || (_tint_blue > 0))
      && (_mode.ColorDepth > 8)) {
    // Common::gl_ScreenBmp tint
    // This slows down the game no end, only experimental ATM
    set_trans_blender(_tint_red, _tint_green, _tint_blue, 0);
    surface->LitBlendBlt(surface, 0, 0, 128);
/*  This alternate method gives the correct (D3D-style) result, but is just too slow!
    if ((_spareTintingScreen != NULL) &&
        ((_spareTintingScreen->GetWidth() != surface->GetWidth()) || (_spareTintingScreen->GetHeight() != surface->GetHeight())))
    {
      destroy_bitmap(_spareTintingScreen);
      _spareTintingScreen = NULL;
    }
    if (_spareTintingScreen == NULL)
    {
      _spareTintingScreen = BitmapHelper::CreateBitmap_(GetColorDepth(surface), surface->GetWidth(), surface->GetHeight());
    }
    tint_image(surface, _spareTintingScreen, _tint_red, _tint_green, _tint_blue, 100, 255);
    Blit(_spareTintingScreen, surface, 0, 0, 0, 0, _spareTintingScreen->GetWidth(), _spareTintingScreen->GetHeight());*/
  }
}

void SDLRendererGraphicsDriver::BlitToTexture()
{
  void *pixels = nullptr;
  int pitch = 0;
  auto res = SDL_LockTexture(screenTex, NULL, &pixels, &pitch);
  if (res != 0) { return; }

  if ((last_pixels != pixels) || (last_pitch != pitch)) {
    fake_texture_bitmap->dat = pixels;
    auto p = (unsigned char *)pixels;
    for (int i=0; i<_mode.Height; i++) {
      fake_texture_bitmap->line[i] = p;
      p += pitch;
    }
    last_pixels = (unsigned char *)pixels;
    last_pitch = pitch;
  }

  blit(virtualScreen->GetAllegroBitmap(), fake_texture_bitmap, 0, 0, 0, 0, gfx_backing_bitmap->w, gfx_backing_bitmap->h);

  SDL_UnlockTexture(screenTex);
}

void SDLRendererGraphicsDriver::Render(int xoff, int yoff, GlobalFlipType flip)
{
  _virtualScrOff = Point(xoff, yoff);

  switch (flip) {
    case kFlip_Both: renderFlip = (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL); break;
    case kFlip_Horizontal: renderFlip = SDL_FLIP_HORIZONTAL; break;
    case kFlip_Vertical: renderFlip = SDL_FLIP_VERTICAL; break;
    default: renderFlip = SDL_FLIP_NONE; break;
  }

  RenderToBackBuffer();

  this->Present();
}

void SDLRendererGraphicsDriver::Present()
{
  if (!renderer) { return; }

  BlitToTexture();

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderFillRect(renderer, nullptr);

  SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);

  SDL_RenderPresent(renderer);
}

void SDLRendererGraphicsDriver::Render()
{
  Render(0, 0, kFlip_None);
}

void SDLRendererGraphicsDriver::Vsync() { }


Bitmap *SDLRendererGraphicsDriver::GetMemoryBackBuffer()
{
  return virtualScreen;
}

Bitmap *SDLRendererGraphicsDriver::GetStageBackBuffer()
{
    return _stageVirtualScreen;
}

void SDLRendererGraphicsDriver::SetMemoryBackBuffer(Bitmap *backBuffer)
{
  if (!backBuffer) {
    virtualScreen = _origVirtualScreen;
    _stageVirtualScreen = _origVirtualScreen;
    _virtualScrOff = Point();
    return;
  }

  virtualScreen = backBuffer;
  _stageVirtualScreen = backBuffer;
}

bool SDLRendererGraphicsDriver::GetCopyOfScreenIntoBitmap(Common::Bitmap *destination, bool at_native_res, GraphicResolution *want_fmt)
{
  if (destination == nullptr || destination->GetSize() != _allegroScreenWrapper->GetSize()) {
    if (want_fmt) {
      const int read_in_colordepth = 32;
      auto need_size = _allegroScreenWrapper->GetSize();
      *want_fmt = GraphicResolution(need_size.Width, need_size.Height, read_in_colordepth);
    }
    return false;
  }

  blit(_allegroScreenWrapper->GetAllegroBitmap(), destination->GetAllegroBitmap(), 
    0, 0,  // src
    0, 0,  // dest
    destination->GetWidth(), destination->GetHeight());

  return true;
}

bool SDLRendererGraphicsDriver::PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen)
{
  return 0;
}

void SDLRendererGraphicsDriver::ToggleFullscreen() {

   int isFullscreen = SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP);

   int result = 0;
   if (isFullscreen) {
      result = SDL_SetWindowFullscreen(window, 0);
   } else {
      result = SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
   }
   if (result != 0) {
      SDL_Log("Unable to toggle fullscreen: %s", SDL_GetError());
   }

}

void SDLRendererGraphicsDriver::FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) 
{
    // Assume palette is correct for rendering here.

    BlitToTexture();

    for (int a = 0; a < 255; a+=speed)
    {
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
      SDL_RenderFillRect(renderer, nullptr);
      SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);

#if SDL_VERSION_ATLEAST(2, 0, 5)
      // add alpha back to screen.
      SDL_SetRenderDrawBlendMode(renderer, fix_alpha_blender);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
      SDL_RenderFillRect(renderer, nullptr);
#endif

      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, targetColourRed, targetColourGreen, targetColourBlue, a);
      SDL_RenderFillRect(renderer, nullptr);

      SDL_RenderPresent(renderer);

        do {
            process_pending_events();
            if (_pollingCallback)
                _pollingCallback();
        } while (waitingForNextTick());
    }

    virtualScreen->Clear();
    BlitToTexture();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, targetColourRed, targetColourGreen, targetColourBlue, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, nullptr);

    SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);
    
    SDL_RenderPresent(renderer);

    RGB faded_out_palette[256];
    for (int i = 0; i < 256; i++) {
      faded_out_palette[i].r = targetColourRed / 4;
      faded_out_palette[i].g = targetColourGreen / 4;
      faded_out_palette[i].b = targetColourBlue / 4;
    }
    system_driver->set_palette_range = nullptr;  // prevent auto present
    set_palette_range(faded_out_palette, 0, 255, 0);
    system_driver->set_palette_range = sdl_renderer_set_palette_range;
}

void SDLRendererGraphicsDriver::FadeIn(int speed, PALETTE p, int targetColourRed, int targetColourGreen, int targetColourBlue) 
{
  system_driver->set_palette_range = nullptr;  // prevent auto present
  set_palette_range(p, 0, 255, 0);
  system_driver->set_palette_range = sdl_renderer_set_palette_range;

  if (_drawScreenCallback != NULL)
    _drawScreenCallback();
  RenderToBackBuffer();
    
  BlitToTexture();

  for (int a = 255; a > 0; a-=speed)
  {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, nullptr);

    SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);

#if SDL_VERSION_ATLEAST(2, 0, 5)
    // add alpha back to screen.
    SDL_SetRenderDrawBlendMode(renderer, fix_alpha_blender);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, nullptr);
#endif

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, targetColourRed, targetColourGreen, targetColourBlue, a);
    SDL_RenderFillRect(renderer, nullptr);

    SDL_RenderPresent(renderer);

    do {
      process_pending_events();
      if (_pollingCallback)
        _pollingCallback();
    } while (waitingForNextTick());
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderFillRect(renderer, nullptr);

  SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);

  SDL_RenderPresent(renderer);
}

void SDLRendererGraphicsDriver::BoxOutEffect(bool blackingOut, int speed, int delay) {

  if (!blackingOut) {
    if (_drawScreenCallback != NULL)
      _drawScreenCallback();
  }

  RenderToBackBuffer();

  BlitToTexture();

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

  float yspeed = (float)(speed) * _srcRect.GetHeight() / _srcRect.GetWidth();
  float box_w = speed;
  float box_h = yspeed;

  while (box_w < _srcRect.GetWidth()) {
    box_w += speed;
    box_h += yspeed;
    float box_x = (_srcRect.GetWidth() - box_w) / 2;
    float box_y = (_srcRect.GetHeight() - box_h) / 2;

    SDL_Rect box {
        static_cast<int>(box_x + 0.5f),
        static_cast<int>(box_y + 0.5f),
        static_cast<int>(box_w + 0.5f),
        static_cast<int>(box_h + 0.5f)
    };

    SDL_RenderFillRect(renderer, nullptr);
    if (blackingOut) {
      // grow a black box from the centre
      SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);
      SDL_RenderFillRect(renderer, &box);
    } else {
      // grow a box of rendered screen from the centre
      SDL_RenderCopyEx(renderer, screenTex, &box, &box, 0.0, nullptr, renderFlip);
    }

    SDL_RenderPresent(renderer);

    do {
      process_pending_events();
      if (_pollingCallback)
        _pollingCallback();
    } while (waitingForNextTick());
  }

  SDL_RenderFillRect(renderer, nullptr);
  if (blackingOut) {
    virtualScreen->Clear();
    BlitToTexture();
  }
  SDL_RenderCopyEx(renderer, screenTex, nullptr, nullptr, 0.0, nullptr, renderFlip);
  SDL_RenderPresent(renderer);
}



// ----------------------------------------------------------------------------
// SDLRendererGraphicsFactory
// ----------------------------------------------------------------------------

SDLRendererGraphicsFactory *SDLRendererGraphicsFactory::_factory = NULL;

SDLRendererGraphicsFactory::~SDLRendererGraphicsFactory()
{
    _factory = NULL;
}

size_t SDLRendererGraphicsFactory::GetFilterCount() const
{
    return 1;
}

const GfxFilterInfo *SDLRendererGraphicsFactory::GetFilterInfo(size_t index) const
{
    switch (index)
    {
    case 0:
        return &SDLRendererGfxFilter::FilterInfo;
    default:
        return NULL;
    }
}

String SDLRendererGraphicsFactory::GetDefaultFilterID() const
{
    return SDLRendererGfxFilter::FilterInfo.Id;
}

/* static */ SDLRendererGraphicsFactory *SDLRendererGraphicsFactory::GetFactory()
{
    if (!_factory)
        _factory = new SDLRendererGraphicsFactory();
    return _factory;
}

SDLRendererGraphicsDriver *SDLRendererGraphicsFactory::EnsureDriverCreated()
{
    if (!_driver)
        _driver = new SDLRendererGraphicsDriver();
    return _driver;
}

SDLRendererGfxFilter *SDLRendererGraphicsFactory::CreateFilter(const String &id)
{
    if (SDLRendererGfxFilter::FilterInfo.Id.CompareNoCase(id) == 0)
        return new SDLRendererGfxFilter();
    return NULL;
}

} // namespace SDLRenderer
} // namespace Engine
} // namespace AGS
