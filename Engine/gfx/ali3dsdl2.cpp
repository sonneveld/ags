//
//  ali3dsdl2.cpp
//  AGSKit
//
//  Created by Nick Sonneveld on 22/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include "ali3dsdl2.hpp"

#include "gfx/gfxfilter_ogl.h"

namespace AGS
{
    namespace Engine
    {
        namespace OGL
        {

            SDL2GraphicsDriver::SDL2GraphicsDriver() {
                
            }

            SDL2GraphicsDriver::~SDL2GraphicsDriver() {}
            
            const char *SDL2GraphicsDriver::GetDriverName() { return "SDL2"; }
            const char *SDL2GraphicsDriver::GetDriverID() { return "SDL2"; }
            



            bool SDL2GraphicsDriver::Init(const DisplayMode &mode, const Size src_size, const Rect dst_rect, volatile int *loopTimer) { return false; }  // TODO
            void SDL2GraphicsDriver::UnInit() {}  // TODO

            bool SDL2GraphicsDriver::IsModeSupported(const DisplayMode &mode) { return false; }  // TODO
            IGfxModeList *SDL2GraphicsDriver::GetSupportedModeList(int color_depth) { return nullptr; }   // TODO

            void SDL2GraphicsDriver::SetGraphicsFilter(OGLGfxFilter *filter) {}  // TODO
            IGfxFilter *SDL2GraphicsDriver::GetGraphicsFilter() const { return nullptr; }  // TODO

            void SDL2GraphicsDriver::SetCallbackForPolling(GFXDRV_CLIENTCALLBACK callback) {}  // TODO
            void SDL2GraphicsDriver::SetCallbackToDrawScreen(GFXDRV_CLIENTCALLBACK callback) {}  // TODO
            void SDL2GraphicsDriver::SetCallbackOnInit(GFXDRV_CLIENTCALLBACKINITGFX callback) {}  // TODO
            void SDL2GraphicsDriver::SetCallbackForNullSprite(GFXDRV_CLIENTCALLBACKXY callback) {}  // TODO
            void SDL2GraphicsDriver::SetTintMethod(TintMethod method) {}  // TODO
            void SDL2GraphicsDriver::SetScreenTint(int red, int green, int blue) {}  // TODO
            void SDL2GraphicsDriver::UseSmoothScaling(bool enabled) {}   // TODO

            void SDL2GraphicsDriver::ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) { /* do nothing */ }
            void SDL2GraphicsDriver::DrawSprite(int x, int y, IDriverDependantBitmap* bitmap) {}  // TODO
            void SDL2GraphicsDriver::ClearDrawList() {}  // TODO
            void SDL2GraphicsDriver::RenderToBackBuffer() {}  // TODO
            void SDL2GraphicsDriver::Render() {}  // TODO
            void SDL2GraphicsDriver::Render(GlobalFlipType flip) {}  // TODO

            AGS::Common::Bitmap *SDL2GraphicsDriver::ConvertBitmapToSupportedColourDepth(AGS::Common::Bitmap *bitmap) { return nullptr; }  // TODO
            IDriverDependantBitmap* SDL2GraphicsDriver::CreateDDBFromBitmap(AGS::Common::Bitmap *bitmap, bool hasAlpha, bool opaque) { return nullptr; }  // TODO
            void SDL2GraphicsDriver::UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, AGS::Common::Bitmap *bitmap, bool hasAlpha) {}  // TODO
            void SDL2GraphicsDriver::DestroyDDB(IDriverDependantBitmap* bitmap) {}  // TODO
            void SDL2GraphicsDriver::GetCopyOfScreenIntoBitmap(AGS::Common::Bitmap *destination) {}  // TODO

            void SDL2GraphicsDriver::FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) {}  // TODO
            void SDL2GraphicsDriver::FadeIn(int speed, PALETTE pal, int targetColourRed, int targetColourGreen, int targetColourBlue) {}  // TODO
            void SDL2GraphicsDriver::BoxOutEffect(bool blackingOut, int speed, int delay) {}  // TODO
                
            bool SDL2GraphicsDriver::PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) { /* do nothing */ return true; }
                
            bool SDL2GraphicsDriver::SupportsGammaControl()  { return false; }
            void SDL2GraphicsDriver::SetGamma(int newGamma) { /* do nothing */ }

            void SDL2GraphicsDriver::EnableVsyncBeforeRender(bool enabled) { /* do nothing */  }
            void SDL2GraphicsDriver::Vsync() { /* do nothing */ }
            bool SDL2GraphicsDriver::RequiresFullRedrawEachFrame() { return true; }
            bool SDL2GraphicsDriver::HasAcceleratedStretchAndFlip() { return true; }
            bool SDL2GraphicsDriver::UsesMemoryBackBuffer() { return false; }
            AGS::Common::Bitmap *SDL2GraphicsDriver::GetMemoryBackBuffer() { return nullptr; };
            void SDL2GraphicsDriver::SetMemoryBackBuffer(AGS::Common::Bitmap *backBuffer) { /* do nothing */ }


            

            

            SDLGraphicsFactory *SDLGraphicsFactory::_factory = NULL;
            
            SDLGraphicsFactory::~SDLGraphicsFactory()
            {
                _factory = NULL;
            }
            
            size_t SDLGraphicsFactory::GetFilterCount() const
            {
                return 1;
            }
            
            const GfxFilterInfo *SDLGraphicsFactory::GetFilterInfo(size_t index) const
            {
                return index == 0 ? &OGLGfxFilter::FilterInfo : NULL;
            }
            
            String SDLGraphicsFactory::GetDefaultFilterID() const
            {
                return OGLGfxFilter::FilterInfo.Id;
            }
            
            /* static */ SDLGraphicsFactory *SDLGraphicsFactory::GetFactory()
            {
                if (!_factory)
                    _factory = new SDLGraphicsFactory();
                return _factory;
            }
            
            SDL2GraphicsDriver *SDLGraphicsFactory::EnsureDriverCreated()
            {
                if (!_driver)
                    _driver = new SDL2GraphicsDriver();
                return _driver;
            }
            
            OGLGfxFilter *SDLGraphicsFactory::CreateFilter(const String &id)
            {
                if (OGLGfxFilter::FilterInfo.Id.CompareNoCase(id) == 0)
                    return new OGLGfxFilter();
                return NULL;
            }
            
            
            
        } // namespace OGL
    } // namespace Engine
} // namespace AGS