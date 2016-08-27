//
//  ali3dsdl2.hpp
//  AGSKit
//
//  Created by Nick Sonneveld on 22/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#ifndef ali3dsdl2_hpp
#define ali3dsdl2_hpp

#include "allegro.h"
#include "gfx/gfxdriverbase.h"
#include "gfx/gfxdriverfactorybase.h"

#define MAX_DRAW_LIST_SIZE 200

namespace AGS
{
    namespace Common
    {
        class Bitmap;
    }
}

namespace AGS
{
    namespace Engine
    {
        namespace OGL
        {
            
            class OGLGfxFilter;
            
            class SDL2GraphicsDriver : public GraphicsDriverBase
            {
            public:
                SDL2GraphicsDriver();
                
                virtual const char *GetDriverName() override;
                virtual const char *GetDriverID() override;
                virtual void SetTintMethod(TintMethod method) override;
                virtual bool Init(const DisplayMode &mode, const Size src_size, const Rect dst_rect, volatile int *loopTimer) override;
                virtual bool IsModeSupported(const DisplayMode &mode) override;
                virtual IGfxModeList *GetSupportedModeList(int color_depth) override;
                virtual IGfxFilter *GetGraphicsFilter() const override;
                virtual void SetCallbackForPolling(GFXDRV_CLIENTCALLBACK callback) override;
                virtual void SetCallbackToDrawScreen(GFXDRV_CLIENTCALLBACK callback) override;
                virtual void SetCallbackOnInit(GFXDRV_CLIENTCALLBACKINITGFX callback) override;
                virtual void SetCallbackForNullSprite(GFXDRV_CLIENTCALLBACKXY callback) override;
                virtual void UnInit() override;
                virtual void ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) override;
                virtual AGS::Common::Bitmap *ConvertBitmapToSupportedColourDepth(AGS::Common::Bitmap *bitmap) override;
                virtual IDriverDependantBitmap* CreateDDBFromBitmap(AGS::Common::Bitmap *bitmap, bool hasAlpha, bool opaque) override;
                virtual void UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, AGS::Common::Bitmap *bitmap, bool hasAlpha) override;
                virtual void DestroyDDB(IDriverDependantBitmap* bitmap) override;
                virtual void DrawSprite(int x, int y, IDriverDependantBitmap* bitmap) override;
                virtual void ClearDrawList() override;
                virtual void RenderToBackBuffer() override;
                virtual void Render() override;
                virtual void Render(GlobalFlipType flip) override;
                virtual void GetCopyOfScreenIntoBitmap(AGS::Common::Bitmap *destination) override;
                virtual void FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
                virtual void FadeIn(int speed, PALETTE pal, int targetColourRed, int targetColourGreen, int targetColourBlue) override;
                virtual void BoxOutEffect(bool blackingOut, int speed, int delay) override;
                virtual bool PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) override;
                virtual bool SupportsGammaControl()  override;
                virtual void SetGamma(int newGamma) override;
                virtual void UseSmoothScaling(bool enabled) override;
                virtual void EnableVsyncBeforeRender(bool enabled) override;
                virtual void Vsync() override;
                virtual bool RequiresFullRedrawEachFrame() override;
                virtual bool HasAcceleratedStretchAndFlip() override;
                virtual bool UsesMemoryBackBuffer() override;
                virtual AGS::Common::Bitmap *GetMemoryBackBuffer() override;
                virtual void SetMemoryBackBuffer(AGS::Common::Bitmap *backBuffer) override;
                virtual void SetScreenTint(int red, int green, int blue) override;
                virtual ~SDL2GraphicsDriver() override;
                
                void SetGraphicsFilter(OGLGfxFilter *filter);
                OGLGfxFilter *_filter;
            };
           
            
            class SDLGraphicsFactory : public GfxDriverFactoryBase<SDL2GraphicsDriver, OGLGfxFilter>
            {
            public:
                virtual ~SDLGraphicsFactory() override;
                
                virtual size_t               GetFilterCount() const override;
                virtual const GfxFilterInfo *GetFilterInfo(size_t index) const override;
                virtual String               GetDefaultFilterID() const override;
                
                static SDLGraphicsFactory   *GetFactory();
            private:
                virtual SDL2GraphicsDriver   *EnsureDriverCreated() override;
                virtual OGLGfxFilter        *CreateFilter(const String &id) override;
                
                static SDLGraphicsFactory *_factory;
            };
            
        } // namespace OGL
    } // namespace Engine
} // namespace AGS

#endif /* ali3dsdl2_hpp */
