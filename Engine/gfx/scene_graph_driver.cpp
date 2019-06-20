
#include "gfx/scene_graph_driver.h"

namespace AGS { namespace Engine {

// --------------------------------------------------------------------------------------------------------------------
// DRIVER
// --------------------------------------------------------------------------------------------------------------------

const char *SceneGraphDriver::GetDriverID() { return "SGG"; }
const char *SceneGraphDriver::GetDriverName() { return "Scene Graph Generator"; }
bool SceneGraphDriver::IsModeSupported(const DisplayMode &mode) { return true; }


IGfxModeList *SceneGraphDriver::GetSupportedModeList(int color_depth) { return nullptr; }
int SceneGraphDriver::GetDisplayDepthForNativeDepth(int native_color_depth) const { return native_color_depth; }
int  SceneGraphDriver::GetCompatibleBitmapFormat(int color_depth) { return color_depth; }

bool SceneGraphDriver::SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer) {
    displayWidth_ = mode.Width;
    displayHeight_ = mode.Height;
    displayColourDepthBits_ = mode.ColorDepth;
    displayVsync_ = mode.Vsync;
    displayWindowed_ = mode.Windowed;

    displayIsSet_ = true;

    ClearDrawLists();

    if (onInitCallback_ != nullptr) {
        void *displayParameters = nullptr; /* D3DPRESENT_PARAMETERS */
        onInitCallback_(displayParameters);
    }
    
    return true;
}
bool SceneGraphDriver::IsModeSet() const { return displayIsSet_; }
void SceneGraphDriver::UpdateDeviceScreen(const Size &screenSize) {
    displayWidth_ = screenSize.Width;
    displayHeight_ = screenSize.Height;
}
DisplayMode SceneGraphDriver::GetDisplayMode() const {
    auto res = AGS::Engine::GraphicResolution(displayWidth_, displayHeight_, displayColourDepthBits_);
    return DisplayMode(res, displayWindowed_, 0, displayVsync_);
}

void SceneGraphDriver::SetCallbackOnInit(GFXDRV_CLIENTCALLBACKINITGFX callback) { onInitCallback_ = callback; }
void SceneGraphDriver::SetCallbackToDrawScreen(GFXDRV_CLIENTCALLBACK callback) { drawScreenCallback_ = callback; }
void SceneGraphDriver::SetCallbackForNullSprite(GFXDRV_CLIENTCALLBACKXY callback) { nullSpriteCallback_ = callback; }
void SceneGraphDriver::SetCallbackForPolling(GFXDRV_CLIENTCALLBACK callback) {}
void SceneGraphDriver::SetCallbackOnSurfaceUpdate(GFXDRV_CLIENTCALLBACKSURFACEUPDATE) { }


void SceneGraphDriver::EnableVsyncBeforeRender(bool enabled) { displayVsync_ = enabled; }
void SceneGraphDriver::Vsync() { /* unused */ }

bool SceneGraphDriver::RequiresFullRedrawEachFrame() { return true; }
bool SceneGraphDriver::HasAcceleratedTransform() { return true; }

bool SceneGraphDriver::SetNativeSize(const Size &src_size) {
    nativeSizeWidth_ = src_size.Width;
    nativeSizeHeight_ = src_size.Height;

    nativeSizeIsSet_ = true;
    return IsNativeSizeValid();
}
bool SceneGraphDriver::IsNativeSizeValid() const { return nativeSizeIsSet_; }
Size SceneGraphDriver::GetNativeSize() const { return Size(nativeSizeWidth_, nativeSizeHeight_); }
void SceneGraphDriver::SetNativeRenderOffset(int x, int y) {
    nativeRenderOffsetX_ = x;
    nativeRenderOffsetY_ = y;
}


bool SceneGraphDriver::SetRenderFrame(const Rect &dst_rect) {
    renderFrameX_ = dst_rect.Left;
    renderFrameY_ = dst_rect.Top;
    renderFrameWidth_ = dst_rect.GetWidth();
    renderFrameHeight_ = dst_rect.GetHeight();

    renderFrameIsSet_ = true;
    return IsRenderFrameValid();
}
bool SceneGraphDriver::IsRenderFrameValid() const { return renderFrameIsSet_; }
Rect SceneGraphDriver::GetRenderDestination() const { return Rect( renderFrameX_, renderFrameY_, renderFrameX_ + renderFrameWidth_ - 1, renderFrameY_ + renderFrameHeight_ - 1); }



void SceneGraphDriver::SetTintMethod(TintMethod method) { tintMethod_ = method; }
void SceneGraphDriver::SetScreenTint(int red, int green, int blue) {
    screenTintRed_ = red;
    screenTintGreen_ = green;
    screenTintBlue_ = blue;
}

bool SceneGraphDriver::SupportsGammaControl() { return true; /* TODO check with actual driver */ }
void SceneGraphDriver::SetGamma(int newGamma) { gamma_ = newGamma; }


void SceneGraphDriver::RenderSpritesAtScreenResolution(bool enabled, int supersampling) {
    renderSpritesAtScreenResolution_ = enabled;
    renderSpritesAtScreenResolutionSuperSampling_ = supersampling;
}
void SceneGraphDriver::UseSmoothScaling(bool enabled) { useSmoothScaling_ = enabled;}


void SceneGraphDriver::ToggleFullscreen() { displayWindowed_ = !displayWindowed_; }


bool SceneGraphDriver::UsesMemoryBackBuffer() { return false; }
Common::Bitmap* SceneGraphDriver::GetMemoryBackBuffer() { return nullptr; }
void SceneGraphDriver::SetMemoryBackBuffer(Common::Bitmap *backBuffer, int offx, int offy) {}
void SceneGraphDriver::RenderToBackBuffer() {}



bool SceneGraphDriver::GetCopyOfScreenIntoBitmap(Common::Bitmap *destination, bool at_native_res, GraphicResolution *want_fmt) { 
    // TODO
    // TODO
    // maybe allow lambda callbacks to the main thread for this?
    // TODO
    return true; 
}


// manipulating scene graph

void SceneGraphDriver::ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) { /* unused */ } 
void SceneGraphDriver::BeginSpriteBatch(const Rect &viewport, const SpriteTransform &transform, PBitmap surface) {
    spriteTransformViewPortX_ = viewport.Left;
    spriteTransformViewPortY_ = viewport.Top;
    spriteTransformViewPortWidth_ = viewport.GetWidth();
    spriteTransformViewPortHeight_ = viewport.GetHeight();

    spriteTransformTranslateX_ = transform.X;
    spriteTransformTranslateY_ = transform.Y;
    spriteTransformScaleX_ = transform.ScaleX;
    spriteTransformScaleY_ = transform.ScaleY;
    spriteTransformRotateRadians_ = transform.Rotate;

    spriteTransformIsSet_ = true;
}
void SceneGraphDriver::DrawSprite(int x, int y, IDriverDependantBitmap *bitmap, const char* purpose) {



    GFXDRV_CLIENTCALLBACKXY callback = nullptr;

    // we still allow passing in null sprites (for plugin events)
    SceneGraphBitmap *sgBitmap = nullptr;
    if (bitmap != nullptr) {
        sgBitmap = dynamic_cast<SceneGraphBitmap*>(bitmap);
        if (sgBitmap == nullptr) { return; }
    } else {
        callback = nullSpriteCallback_;
    }

    auto operation = SceneGraphOperation {
        /*.callback_ = */callback,

        /*.x_ = */x,
        /*.y_ = */y,
        /*.bitmap_ = */sgBitmap,
        
        /*.spriteTransformViewPortX_ = */spriteTransformViewPortX_,
        /*.spriteTransformViewPortY_ = */spriteTransformViewPortY_,
        /*.spriteTransformViewPortWidth_ = */spriteTransformViewPortWidth_,
        /*.spriteTransformViewPortHeight_ = */spriteTransformViewPortHeight_,
        
        /*.spriteTransformTranslateX_ = */spriteTransformTranslateX_,
        /*.spriteTransformTranslateY_ = */spriteTransformTranslateY_,
        /*.spriteTransformScaleX_ = */spriteTransformScaleX_,
        /*.spriteTransformScaleY_ = */spriteTransformScaleY_,
        /*.spriteTransformRotateRadians_ = */spriteTransformRotateRadians_,
        
        /*.spriteTransformIsSet_ = */spriteTransformIsSet_,


        purpose
    };
    operations_.push_back(operation);
}
void SceneGraphDriver::ClearDrawLists() {
    operations_.clear();
    spriteTransformIsSet_ = false;
}



Common::Bitmap *SceneGraphDriver::GetStageBackBuffer() {
    // TODO
    // TODO
    // TODO
    // TODO
    // TODO
    /*
    add stageBackBuffer to current scene graph
    sceneGraph.add(stageBackBuffer);
    return stageBackBuffer
    */
   // but when to render?  on nul sprite?
   return nullptr;
}

// store flip type and do nothing
void SceneGraphDriver::Render() { globalFlip_ = kFlip_None; }
void SceneGraphDriver::Render(GlobalFlipType flip) { globalFlip_ = flip; }

std::vector<SceneGraphOperation> SceneGraphDriver::GetOperations() { return operations_; }




// weird blocking stuff we should do in other ways.
void SceneGraphDriver::FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) {}
void SceneGraphDriver::FadeIn(int speed, PALETTE p, int targetColourRed, int targetColourGreen, int targetColourBlue) {}
void SceneGraphDriver::BoxOutEffect(bool blackingOut, int speed, int delay) {}
bool SceneGraphDriver::PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) { return false; }



// --------------------------------------------------------------------------------------------------------------------
// BITMAP
// --------------------------------------------------------------------------------------------------------------------

// Some modules, such as walk behind, assumes that DDB copies the bitmap, so we can't just hold onto original bmp

IDriverDependantBitmap *SceneGraphDriver::CreateDDBFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque) {
    return new SceneGraphBitmap(bitmap, hasAlpha, opaque);
}
void SceneGraphDriver::UpdateDDBFromBitmap(IDriverDependantBitmap *ddb, Common::Bitmap *bitmap, bool hasAlpha) {
    auto sgDDB = dynamic_cast<SceneGraphBitmap *>(ddb);
    if (sgDDB == nullptr) { return; }

    sgDDB->UpdateFromBitmap(bitmap, hasAlpha, sgDDB->opaque_);
}
void SceneGraphDriver::DestroyDDB(IDriverDependantBitmap *ddb) {
    delete ddb;
}

SceneGraphBitmap::SceneGraphBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque) {
    hasAlpha_ = hasAlpha;
    opaque_ = opaque;
    bitmap_ = bitmap;
    
    // Copy bitmap info. Some code relies on this existing even if bitmap is invalid.
    bitmapWidth_ = bitmap ? bitmap->GetWidth() : -1;
    bitmapHeight_ = bitmap ? bitmap->GetHeight() : -1;
    bitmapColorDepth_ = bitmap ? bitmap->GetColorDepth() : -1;
}

void SceneGraphBitmap::UpdateFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque) {
    hasAlpha_ = hasAlpha;
    opaque_ = opaque;
    bitmap_ = bitmap;

    // Copy bitmap info. Some code relies on this existing even if bitmap is invalid.
    bitmapWidth_ = bitmap ? bitmap->GetWidth() : -1;
    bitmapHeight_ = bitmap ? bitmap->GetHeight() : -1;
    bitmapColorDepth_ = bitmap ? bitmap->GetColorDepth() : -1;
}

void SceneGraphBitmap::SetTransparency(int transparency) { transparency_ = transparency; }
void SceneGraphBitmap::SetFlippedLeftRight(bool isFlipped) { isFlipped_ = isFlipped; }
void SceneGraphBitmap::SetStretch(int width, int height, bool useResampler) {
    stretchWidth_ = width;
    stretchHeight_ = height;
    stretchUseResampler_ = useResampler;
}
void SceneGraphBitmap::SetLightLevel(int light_level) { lightLevel_ = light_level; }
void SceneGraphBitmap::SetTint(int red, int green, int blue, int tintSaturation) {
    tintRed_ = red;
    tintGreen_ = green;
    tintBlue_ = blue;
    tintSaturation_ = tintSaturation;
}
int SceneGraphBitmap::GetWidth() { return bitmapWidth_; }
int SceneGraphBitmap::GetHeight() { return bitmapHeight_; }
int SceneGraphBitmap::GetColorDepth() { return bitmapColorDepth_; }


// --------------------------------------------------------------------------------------------------------------------
// FILTER
// --------------------------------------------------------------------------------------------------------------------

static auto stdFilter = GfxFilterInfo("StdScale", "Nearest-neighbour");
static auto linearFilter = GfxFilterInfo("Linear", "Linear interpolation");
    

void SceneGraphDriver::SetGraphicsFilter(std::shared_ptr<SceneGraphFilter> filter) { 
    graphicsFilter_ = filter; 
    graphicsFilterMethod_ = filter->method_;
}
PGfxFilter SceneGraphDriver::GetGraphicsFilter() const { return graphicsFilter_; }


const GfxFilterInfo &SceneGraphFilter::GetInfo() const { 
    switch (method_) {
        case FilterNearest: return stdFilter;
        case FilterLinear:  return linearFilter;
        default:            return stdFilter;
    }
}
bool SceneGraphFilter::Initialize(const int color_depth, String &err_str) { return true; }
void SceneGraphFilter::UnInitialize() {}
Rect SceneGraphFilter::SetTranslation(const Size src_size, const Rect dst_rect) { return Rect(); /* unused */ }
Rect SceneGraphFilter::GetDestination() const { return Rect(); /* unused */ }


// --------------------------------------------------------------------------------------------------------------------
// FACTORY
// --------------------------------------------------------------------------------------------------------------------


static SceneGraphGraphicsFactory *factory = nullptr;

SceneGraphGraphicsFactory *getSceneGraphGraphicsFactory() {
    if (factory == nullptr) {
        factory = new SceneGraphGraphicsFactory(); 
    }
    return factory;
}


SceneGraphGraphicsFactory::~SceneGraphGraphicsFactory() { DestroyDriver(); }
void SceneGraphGraphicsFactory::Shutdown() { 
    delete this; 
    factory = nullptr;
}
IGraphicsDriver *SceneGraphGraphicsFactory::GetDriver() {
    if (driver_ == nullptr) {
        driver_ = new SceneGraphDriver();
    }
    return driver_;
}
void SceneGraphGraphicsFactory::DestroyDriver() {
    delete driver_;
    driver_ = nullptr;
}
size_t SceneGraphGraphicsFactory::GetFilterCount() const { return 2; }
const GfxFilterInfo *SceneGraphGraphicsFactory::GetFilterInfo(size_t index) const {
    switch(index) {
        case 0: return &stdFilter;
        case 1: return &linearFilter;
        default: return &stdFilter;
    }
}
String SceneGraphGraphicsFactory::GetDefaultFilterID() const { return stdFilter.Id; }
PGfxFilter SceneGraphGraphicsFactory::SetFilter(const String &id, String &filter_error) {
    GetDriver(); // ensure created

    SceneGraphFilterMethod method = FilterNearest;
    if (id.CompareNoCase("StdScale") == 0) {
        method = FilterNearest;
    } else if (id.CompareNoCase("Linear") == 0) {
        method = FilterLinear;
    }

    auto filter = std::make_shared<SceneGraphFilter>(method);
    driver_->SetGraphicsFilter(filter);
    
    return filter;
}


} } // AGS::Engine
