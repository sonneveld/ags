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
// Software graphics factory, based on Allegro
//
//=============================================================================

#ifndef __AGS_EE_GFX__ALI3D_SDL_RENDERER_H
#define __AGS_EE_GFX__ALI3D_SDL_RENDERER_H

#include <memory>

#include "SDL.h"
#include <allegro.h>

#include "gfx/bitmap.h"
#include "gfx/ddb.h"
#include "gfx/gfxdriverfactorybase.h"
#include "gfx/gfxdriverbase.h"
#include "gfx/gfxfilter_sdl_renderer.h"

namespace AGS
{
namespace Engine
{
namespace SDLRenderer
{

class SDLRendererGfxFilter;
using AGS::Common::Bitmap;

class SDLRendererBitmap final : public IDriverDependantBitmap
{
public:
    // NOTE by CJ:
    // Transparency is a bit counter-intuitive
    // 0=not transparent, 255=invisible, 1..254 barely visible .. mostly visible
    virtual void SetTransparency(int transparency) override { _transparency = transparency; }
    virtual void SetFlippedLeftRight(bool isFlipped) override { _flipped = isFlipped; }
    virtual void SetStretch(int width, int height, bool useResampler = true) override
    {
        _stretchToWidth = width;
        _stretchToHeight = height;
    }
    virtual int GetWidth() override { return _width; }
    virtual int GetHeight() override { return _height; }
    virtual int GetColorDepth() override { return _colDepth; }
    virtual void SetLightLevel(int lightLevel) override { }
    virtual void SetTint(int red, int green, int blue, int tintSaturation) override { }

    SDLRendererBitmap(Bitmap *bmp, bool opaque, bool hasAlpha)
    {
        _bmp = bmp;
        _width = bmp->GetWidth();
        _height = bmp->GetHeight();
        _colDepth = bmp->GetColorDepth();
        _flipped = false;
        _stretchToWidth = 0;
        _stretchToHeight = 0;
        _transparency = 0;
        _opaque = opaque;
        _hasAlpha = hasAlpha;
    }

    int GetWidthToRender() { return (_stretchToWidth > 0) ? _stretchToWidth : _width; }
    int GetHeightToRender() { return (_stretchToHeight > 0) ? _stretchToHeight : _height; }

    void Dispose()
    {
        // do we want to free the bitmap?
    }

    ~SDLRendererBitmap()
    {
        Dispose();
    }

public:     
    Bitmap *_bmp;
    int _width, _height;
    int _colDepth;
    bool _flipped;
    int _stretchToWidth, _stretchToHeight;
    bool _opaque;
    bool _hasAlpha;
    int _transparency;
};


class SDLRendererGfxModeList final : public IGfxModeList
{
public:
    SDLRendererGfxModeList(GFX_MODE_LIST *alsw_gfx_mode_list)
        : _gfxModeList(alsw_gfx_mode_list)
    {
    }

    virtual int GetModeCount() const override
    {
        return _gfxModeList ? _gfxModeList->num_modes : 0;
    }

    virtual bool GetMode(int index, DisplayMode &mode) const override;

private:
    GFX_MODE_LIST *_gfxModeList;
};


typedef SpriteDrawListEntry<SDLRendererBitmap> ALDrawListEntry;
// Software renderer's sprite batch
struct ALSpriteBatch final
{
    // List of sprites to render
    std::vector<ALDrawListEntry> List;
    // Intermediate surface which will be drawn upon and transformed if necessary
    std::shared_ptr<Bitmap>      Surface;
    // Tells whether the surface is treated as opaque or transparent
    bool                         Opaque;
};
typedef std::vector<ALSpriteBatch> ALSpriteBatches;


class SDLRendererGraphicsDriver final : public GraphicsDriverBase
{
public:
    SDLRendererGraphicsDriver();

    virtual const char*GetDriverName() override { return "SDL 2D Accelerated Rendering"; }
    virtual const char*GetDriverID() override { return "Software"; }
    virtual void UpdateDeviceScreen(const Size &screenSize) override;
    virtual void SetTintMethod(TintMethod method) override;
    virtual bool SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer) override;
    virtual bool SetNativeSize(const Size &src_size) override;
    virtual bool SetRenderFrame(const Rect &dst_rect) override;
    virtual bool IsModeSupported(const DisplayMode &mode) override;
    virtual int  GetDisplayDepthForNativeDepth(int native_color_depth) const override;
    virtual IGfxModeList *GetSupportedModeList(int color_depth) override;
    virtual PGfxFilter GetGraphicsFilter() const override;
    virtual void ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) override {}
    virtual int  GetCompatibleBitmapFormat(int color_depth) override;
    virtual IDriverDependantBitmap* CreateDDBFromBitmap(Bitmap *bitmap, bool hasAlpha, bool opaque) override;
    virtual void UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, Bitmap *bitmap, bool hasAlpha) override;
    virtual void DestroyDDB(IDriverDependantBitmap* bitmap) override;

    virtual void DrawSprite(int x, int y, IDriverDependantBitmap* bitmap) override;
    virtual void SetScreenFade(int red, int green, int blue) override;

    virtual void RenderToBackBuffer() override;
    virtual void Render() override;
    virtual void Render(int xoff, int yoff, GlobalFlipType flip) override;
    virtual bool GetCopyOfScreenIntoBitmap(Common::Bitmap *destination, bool at_native_res, GraphicResolution *want_fmt = nullptr) override;
    virtual void FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
    virtual void FadeIn(int speed, PALETTE pal, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
    virtual void BoxOutEffect(bool blackingOut, int speed, int delay) override;
    virtual bool PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) override;
    virtual bool SupportsGammaControl()  override;
    virtual void SetGamma(int newGamma) override;
    virtual void UseSmoothScaling(bool enabled) override { }
    virtual void EnableVsyncBeforeRender(bool enabled) override { }
    virtual void Vsync() override;
    virtual void RenderSpritesAtScreenResolution(bool enabled, int supersampling) override { }
    virtual bool RequiresFullRedrawEachFrame() override { return false; }
    virtual bool HasAcceleratedTransform() override { return false; }
    virtual bool UsesMemoryBackBuffer() override { return true; }
    virtual Bitmap *GetMemoryBackBuffer() override;
    virtual void SetMemoryBackBuffer(Common::Bitmap *backBuffer) override;
    virtual Bitmap *GetStageBackBuffer() override;
    virtual void SetScreenTint(int red, int green, int blue) override { 
        _tint_red = red; _tint_green = green; _tint_blue = blue; }
    virtual void ToggleFullscreen() override;
    virtual ~SDLRendererGraphicsDriver() override;

    typedef std::shared_ptr<SDLRendererGfxFilter> PALSWFilter;

    void SetGraphicsFilter(PALSWFilter filter);
    void Present();


private:

    bool has_gamma = false;
    Uint16 default_gamma_red[256] {};
    Uint16 default_gamma_green[256] {};
    Uint16 default_gamma_blue[256] {};

    BITMAP *fake_texture_bitmap = nullptr;
    unsigned char *last_pixels = nullptr;
    int last_pitch = -1;

    SDL_RendererFlip renderFlip = SDL_FLIP_NONE;

    BITMAP * gfx_backing_bitmap = nullptr;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *screenTex = nullptr;
    const char * window_title = "AGS";

    PALSWFilter _filter;

    Bitmap *_allegroScreenWrapper;
    // Virtual screen bitmap is either a wrapper over Allegro's real screen
    // bitmap, or bitmap provided by the graphics filter. It should not be
    // disposed by the renderer: it is up to filter object to manage it.
    Bitmap *_origVirtualScreen;
    // Current virtual screen bitmap; may be provided either by graphics
    // filter or by external user. It should not be disposed by the renderer.
    Bitmap *virtualScreen;
    // Extra offset for the custom virtual screen.
    // NOTE: the big issue with software renderer is that it handles main viewport changes
    // differently from the hardware-accelerated ones. While D3D and OGL renderers
    // work with the full game frame and offset sprites by the viewport's top-left,
    // software renderer has to work with "virtual screen" bitmap of the main viewport's size,
    // which means that it has to treat 0,0 as a viewport's top-left and not game frame's top-left.
    Point   _virtualScrOff;
    // Stage screen meant for particular rendering stages, may be referencing
    // actual virtual screen or separate bitmap of different size that is
    // blitted to virtual screen at the stage finalization.
    Bitmap *_stageVirtualScreen;
    int _tint_red, _tint_green, _tint_blue;

    ALSpriteBatches _spriteBatches;
    GFX_MODE_LIST *_gfxModeList;

    void UnInit();

    virtual void InitSpriteBatch(size_t index, const SpriteBatchDesc &desc) override;
    virtual void ResetAllBatches() override;

    void BlitToTexture();

    // Use gfx filter to create a new virtual screen
    void CreateVirtualScreen();
    void DestroyVirtualScreen();
    // Unset parameters and release resources related to the display mode
    void ReleaseDisplayMode();
    // Renders single sprite batch on the precreated surface
    void RenderSpriteBatch(const ALSpriteBatch &batch, Common::Bitmap *surface, int surf_offx, int surf_offy);

    int  GetAllegroGfxDriverID(bool windowed);
};


class SDLRendererGraphicsFactory final : public GfxDriverFactoryBase<SDLRendererGraphicsDriver, SDLRendererGfxFilter>
{
public:
    virtual ~SDLRendererGraphicsFactory();

    virtual size_t               GetFilterCount() const;
    virtual const GfxFilterInfo *GetFilterInfo(size_t index) const;
    virtual String               GetDefaultFilterID() const;

    static  SDLRendererGraphicsFactory *GetFactory();

private:
    virtual SDLRendererGraphicsDriver *EnsureDriverCreated();
    virtual SDLRendererGfxFilter         *CreateFilter(const String &id);

    static SDLRendererGraphicsFactory *_factory;
};

} // namespace SDLRenderer
} // namespace Engine
} // namespace AGS

#endif // __AGS_EE_GFX__ALI3D_SDL_RENDERER_H
