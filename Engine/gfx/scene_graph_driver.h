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

#ifndef __AGS_EE_GFX__SCENE_GRAPH_DRIVER_H
#define __AGS_EE_GFX__SCENE_GRAPH_DRIVER_H

#include <memory>
#include <vector>
#include "allegro/palette.h"

#include "gfx/ddb.h"
#include "gfx/graphicsdriver.h"
#include "gfx/gfxfilter.h"
#include "gfx/allegrobitmap.h"
#include "gfx/gfxdriverfactory.h"

namespace AGS { namespace Engine
{

class SceneGraphBitmap final : public IDriverDependantBitmap
{
public:
    SceneGraphBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque);

    void UpdateFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque);

    // NOTE by CJ:
    // Transparency is a bit counter-intuitive
    // 0=not transparent, 255=invisible, 1..254 barely visible .. mostly visible
    void SetTransparency(int transparency) override;  // 0-255
    void SetFlippedLeftRight(bool isFlipped) override;
    void SetStretch(int width, int height, bool useResampler = true) override;
    void SetLightLevel(int light_level) override;   // 0-255
    void SetTint(int red, int green, int blue, int tintSaturation) override;  // 0-255

    int GetWidth() override;
    int GetHeight() override;
    int GetColorDepth() override;

    AGS::Common::Bitmap *bitmap_ = nullptr;

    int bitmapWidth_ = -1;
    int bitmapHeight_ = -1;
    int bitmapColorDepth_ = -1;

    bool hasAlpha_ = false;
    bool opaque_ = true;

    int transparency_ = 0;

    bool isFlipped_ = false;

    int stretchWidth_ = 0;
    int stretchHeight_ = 0;
    bool stretchUseResampler_ = false;

    int lightLevel_ = 0;

    int tintRed_ = 0;
    int tintGreen_ = 0;
    int tintBlue_ = 0;
    int tintSaturation_ = 0;
};


struct SceneGraphOperation {
    GFXDRV_CLIENTCALLBACKXY callback_;
    
    int x_;
    int y_;
    SceneGraphBitmap *bitmap_;

    int spriteTransformViewPortX_;
    int spriteTransformViewPortY_;
    int spriteTransformViewPortWidth_;
    int spriteTransformViewPortHeight_;

    int spriteTransformTranslateX_;
    int spriteTransformTranslateY_;
    float spriteTransformScaleX_;
    float spriteTransformScaleY_;
    float spriteTransformRotateRadians_;

    bool spriteTransformIsSet_;

    const char *purpose;
};


enum SceneGraphFilterMethod { FilterNearest, FilterLinear };

class SceneGraphFilter final : public IGfxFilter
{
public:
    SceneGraphFilter(SceneGraphFilterMethod method) : method_(method) {}
    ~SceneGraphFilter() override = default;
    const GfxFilterInfo &GetInfo() const override;
    bool Initialize(const int color_depth, String &err_str) override;
    void UnInitialize() override;
    Rect SetTranslation(const Size src_size, const Rect dst_rect) override;
    Rect GetDestination() const override;

    SceneGraphFilterMethod method_ = FilterNearest;
};


class SceneGraphDriver final : public IGraphicsDriver
{
public:
    SceneGraphDriver() = default;
    ~SceneGraphDriver() override = default;

    const char*GetDriverName() override;
    const char*GetDriverID() override;
    void SetTintMethod(TintMethod method) override;
    bool SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer) override;
    bool IsModeSet() const override;
    bool SetNativeSize(const Size &src_size) override;
    bool IsNativeSizeValid() const override;
    bool SetRenderFrame(const Rect &dst_rect) override;
    bool IsRenderFrameValid() const override;
    int  GetDisplayDepthForNativeDepth(int native_color_depth) const override;
    IGfxModeList *GetSupportedModeList(int color_depth) override;
    bool IsModeSupported(const DisplayMode &mode) override;
    DisplayMode GetDisplayMode() const override;
    PGfxFilter GetGraphicsFilter() const override;
    Size GetNativeSize() const override;
    Rect GetRenderDestination() const override;
    void SetCallbackForPolling(GFXDRV_CLIENTCALLBACK callback) override;
    void SetCallbackToDrawScreen(GFXDRV_CLIENTCALLBACK callback) override;
    void SetCallbackOnInit(GFXDRV_CLIENTCALLBACKINITGFX callback) override;
    void SetCallbackOnSurfaceUpdate(GFXDRV_CLIENTCALLBACKSURFACEUPDATE) override;
    void SetCallbackForNullSprite(GFXDRV_CLIENTCALLBACKXY callback) override;
    void ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) override;
    int  GetCompatibleBitmapFormat(int color_depth) override;
    IDriverDependantBitmap* CreateDDBFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque = false) override;
    void UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, Common::Bitmap *bitmap, bool hasAlpha) override;
    void DestroyDDB(IDriverDependantBitmap* bitmap) override;
    void BeginSpriteBatch(const Rect &viewport, const SpriteTransform &transform, PBitmap surface = nullptr) override;
    void DrawSprite(int x, int y, IDriverDependantBitmap* bitmap, const char *purpose) override;
    void ClearDrawLists() override;
    void SetScreenTint(int red, int green, int blue) override;
    void SetNativeRenderOffset(int x, int y) override;
    void RenderToBackBuffer() override;
    void Render() override;
    void Render(GlobalFlipType flip) override;
    bool GetCopyOfScreenIntoBitmap(Common::Bitmap *destination, bool at_native_res, GraphicResolution *want_fmt = nullptr) override;
    void EnableVsyncBeforeRender(bool enabled) override;
    void Vsync() override;
    void RenderSpritesAtScreenResolution(bool enabled, int supersampling = 1) override;
    void FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
    void FadeIn(int speed, PALETTE p, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
    void BoxOutEffect(bool blackingOut, int speed, int delay) override;
    bool PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) override;
    void UseSmoothScaling(bool enabled) override;
    bool SupportsGammaControl() override;
    void SetGamma(int newGamma) override;
    Common::Bitmap* GetMemoryBackBuffer() override;
    void SetMemoryBackBuffer(Common::Bitmap *backBuffer, int offx = 0, int offy = 0) override;
    Common::Bitmap* GetStageBackBuffer() override;
    bool RequiresFullRedrawEachFrame() override;
    bool HasAcceleratedTransform() override;
    bool UsesMemoryBackBuffer() override;
    void UpdateDeviceScreen(const Size &screenSize) override;
    void ToggleFullscreen() override;

    // TODO: set palette


    void SetGraphicsFilter(std::shared_ptr<SceneGraphFilter> filter);
    std::vector<SceneGraphOperation> GetOperations();


private:

    GFXDRV_CLIENTCALLBACKINITGFX onInitCallback_ = nullptr;
    GFXDRV_CLIENTCALLBACK drawScreenCallback_ = nullptr;
    GFXDRV_CLIENTCALLBACKXY nullSpriteCallback_ = nullptr;

    int displayWidth_;
    int displayHeight_;
    int displayColourDepthBits_;
    bool displayVsync_ = true;
    bool displayWindowed_ = true;
    bool displayIsSet_ = false;

    int nativeSizeWidth_;
    int nativeSizeHeight_;
    bool nativeSizeIsSet_ = false;
    int nativeRenderOffsetX_ = 0;
    int nativeRenderOffsetY_ = 0;

    int renderFrameX_ = 0;
    int renderFrameY_ = 0;
    int renderFrameWidth_;
    int renderFrameHeight_;
    int renderFrameIsSet_ = false;

    TintMethod tintMethod_ = TintReColourise;
    int screenTintRed_ = 0;
    int screenTintGreen_ = 0;
    int screenTintBlue_ = 0;

    int gamma_ = 100;

    bool renderSpritesAtScreenResolution_ = false;
    bool renderSpritesAtScreenResolutionSuperSampling_ = 1;

    bool useSmoothScaling_ = false;

    std::shared_ptr<SceneGraphFilter> graphicsFilter_ = nullptr;
    SceneGraphFilterMethod graphicsFilterMethod_ = FilterNearest;


    // transform state that gets reset per game loop

    GlobalFlipType globalFlip_ = kFlip_None;

    int spriteTransformViewPortX_ = 0;
    int spriteTransformViewPortY_ = 0;
    int spriteTransformViewPortWidth_ = -1;
    int spriteTransformViewPortHeight_ = -1;

    int spriteTransformTranslateX_ = 0;
    int spriteTransformTranslateY_ = 0;
    float spriteTransformScaleX_ = 1.0;
    float spriteTransformScaleY_ = 1.0;
    float spriteTransformRotateRadians_ = 0.0;

    bool spriteTransformIsSet_ = false;

    // result

    std::vector<SceneGraphOperation> operations_ {};

};


class SceneGraphGraphicsFactory final : public IGfxDriverFactory
{
public:
    SceneGraphGraphicsFactory() = default;
    ~SceneGraphGraphicsFactory() override;
    SceneGraphGraphicsFactory(const SceneGraphGraphicsFactory&) = delete; // no copies
    SceneGraphGraphicsFactory& operator=(const SceneGraphGraphicsFactory&) = delete; // no self-assignments
    
    void                 Shutdown() override;
    IGraphicsDriver *    GetDriver() override;
    void                 DestroyDriver() override;
    size_t               GetFilterCount() const override;
    const GfxFilterInfo *GetFilterInfo(size_t index) const override;
    String               GetDefaultFilterID() const override;
    PGfxFilter           SetFilter(const String &id, String &filter_error) override;

private:
    SceneGraphDriver *driver_ = nullptr;

};

SceneGraphGraphicsFactory *getSceneGraphGraphicsFactory();


} // namespace Engine
} // namespace AGS

#endif // __AGS_EE_GFX__GRAPHICSDRIVER_H
