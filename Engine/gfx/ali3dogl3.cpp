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

#ifdef AGS_ENABLE_OPENGL_DRIVER

#include <algorithm>
#include <string>
#include <iostream>

#include "gfx/ali3dexception.h"
#include "gfx/ali3dogl3.h"
#include "gfx/gfxfilter_ogl.h"
#include "gfx/gfxfilter_aaogl.h"
#include "main/main_allegro.h"
#include "platform/base/agsplatformdriver.h"

namespace AGS
{
namespace Engine
{

using namespace AGS::Common;

enum class Filters { NEAREST, LINEAR };

class NotImplemented2 : public std::logic_error {
public:
    NotImplemented2() : std::logic_error("Not implemented.") { };
};


void ogl_dummy_vsync() { }

#define algetr32(xx) ((xx >> _rgb_r_shift_32) & 0xFF)
#define algetg32(xx) ((xx >> _rgb_g_shift_32) & 0xFF)
#define algetb32(xx) ((xx >> _rgb_b_shift_32) & 0xFF)
#define algeta32(xx) ((xx >> _rgb_a_shift_32) & 0xFF)
#define algetr15(xx) ((xx >> _rgb_r_shift_15) & 0x1F)
#define algetg15(xx) ((xx >> _rgb_g_shift_15) & 0x1F)
#define algetb15(xx) ((xx >> _rgb_b_shift_15) & 0x1F)

#define GFX_OPENGL  AL_ID('O','G','L',' ')

static int ogl_show_mouse(struct BITMAP *bmp, int x, int y) {
    SDL_ShowCursor(SDL_ENABLE);
    return -1; // show cursor, but "fail" because we're not supporting hardware cursors
}

static void ogl_hide_mouse() {
    SDL_ShowCursor(SDL_DISABLE);
}

GFX_DRIVER gfx_opengl =
{
   GFX_OPENGL,
   empty_string,
   empty_string,
   "OpenGL",
   NULL,    // init
   NULL,   // exit
   NULL,                        // AL_METHOD(int, scroll, (int x, int y));
   ogl_dummy_vsync,   // vsync
   NULL,  // setpalette
   NULL,                        // AL_METHOD(int, request_scroll, (int x, int y));
   NULL,                        // AL_METHOD(int, poll_scroll, (void));
   NULL,                        // AL_METHOD(void, enable_triple_buffer, (void));
   NULL,  //create_video_bitmap
   NULL,  //destroy_video_bitmap
   NULL,   //show_video_bitmap
   NULL,
   NULL,  //gfx_directx_create_system_bitmap,
   NULL, //gfx_directx_destroy_system_bitmap,
   NULL, //gfx_directx_set_mouse_sprite,
   ogl_show_mouse, //gfx_directx_show_mouse,
   ogl_hide_mouse, //gfx_directx_hide_mouse,
   NULL, //gfx_directx_move_mouse,
   NULL,                        // AL_METHOD(void, drawing_mode, (void));
   NULL,                        // AL_METHOD(void, save_video_state, (void*));
   NULL,                        // AL_METHOD(void, restore_video_state, (void*));
   NULL,                        // AL_METHOD(void, set_blender_mode, (int mode, int r, int g, int b, int a));
   NULL,                        // AL_METHOD(int, fetch_mode_list, (void));
   0, 0,                        // int w, h;
   FALSE,                        // int linear;
   0,                           // long bank_size;
   0,                           // long bank_gran;
   0,                           // long vid_mem;
   0,                           // long vid_phys_base;
   TRUE                         // int windowed;
};
// ------------------------------------------------------------------------
// UTIL
// ------------------------------------------------------------------------

Bitmap *convertBitmapToColourDepth(Bitmap *bitmap, int wantedColourDepth)
{
  if (bitmap->GetColorDepth() == wantedColourDepth) { return bitmap; }

  switch(wantedColourDepth) {
    case 32: {
           //Uint32 * b = reinterpret_cast< Uint32 *>(bitmap->GetDataForWriting());
      int old_conv = get_color_conversion();
       set_color_conversion(COLORCONV_KEEP_TRANS | COLORCONV_8_TO_32 | COLORCONV_15_TO_32 | COLORCONV_16_TO_32 | COLORCONV_24_TO_32);
      // set_color_conversion(COLORCONV_KEEP_TRANS | COLORCONV_TOTAL);
      Bitmap *tempBmp = BitmapHelper::CreateBitmapCopy(bitmap, 32);
      set_color_conversion(old_conv);
      return tempBmp;
    }
    default:
      return NULL;
  }
}


void checkCompileErrors(GLuint object, std::string type)
{
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM")
    {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(object, 1024, NULL, infoLog);
            std::cout << "| ERROR::SHADER: Compile-time error: Type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- "
                << std::endl;
        }
    }
    else
    {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(object, 1024, NULL, infoLog);
            std::cout << "| ERROR::Shader: Link-time error: Type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- "
                << std::endl;
        }
    }
}

GLuint shaderCompiler(const GLchar* vertexSource, const GLchar* fragmentSource, const GLchar* geometrySource)
{
    GLuint sVertex, sFragment, gShader;
    // Vertex Shader
    sVertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(sVertex, 1, &vertexSource, NULL);
    glCompileShader(sVertex);
    checkCompileErrors(sVertex, "VERTEX");

    // Fragment Shader
    sFragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sFragment, 1, &fragmentSource, NULL);
    glCompileShader(sFragment);
    checkCompileErrors(sFragment, "FRAGMENT");

    // If geometry shader source code is given, also compile geometry shader
    if (geometrySource != nullptr)
    {
        gShader = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gShader, 1, &geometrySource, NULL);
        glCompileShader(gShader);
        checkCompileErrors(gShader, "GEOMETRY");
    }

    // Shader Program
    GLuint ID = glCreateProgram();
    glAttachShader(ID, sVertex);
    glAttachShader(ID, sFragment);
    if (geometrySource != nullptr)
        glAttachShader(ID, gShader);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // Delete the shaders as they're linked into our program now and no longer necessery
    glDeleteShader(sVertex);
    glDeleteShader(sFragment);
    if (geometrySource != nullptr)
        glDeleteShader(gShader);

    return ID;
}


GLuint initRenderData()
{
  GLuint quadVAO = 42;

    // Configure VAO/VBO
    GLuint VBO;
    GLfloat vertices[] = {
        // Pos      // Tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    int location=0;
    glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(location);

    // unbind
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    // glBindVertexArray(0);

  return quadVAO;
}

// ------------------------------------------------------------------------
// FILTER
// ------------------------------------------------------------------------

const GfxFilterInfo StdScaleFilterInfo = GfxFilterInfo("StdScale", "Nearest-neighbour");
const GfxFilterInfo LinearFilterInfo = GfxFilterInfo("Linear", "Linear interpolation");

class OGL3GfxFilter final : public IGfxFilter {
public:
    OGL3GfxFilter(Filters f, GfxFilterInfo const *info) {_f = f; _info=info;}
    const GfxFilterInfo &GetInfo() const override { return *_info; }
    bool Initialize(const int color_depth, String &err_str) override { return true; }
    void UnInitialize() override {}
    Rect SetTranslation(const Size src_size, const Rect dst_rect) override { throw NotImplemented(); }
    Rect GetDestination() const override { throw NotImplemented(); }

private:
    Filters _f;
    GfxFilterInfo const *_info;
};

// OGL3GfxFilter StdScaleFilter = OGL3GfxFilter(Filters::NEAREST, &StdScaleFilterInfo);
// OGL3GfxFilter LinearFilter = OGL3GfxFilter(Filters::LINEAR, &LinearFilterInfo);

// ------------------------------------------------------------------------
// BITMAP
// ------------------------------------------------------------------------


class OGL3Bitmap final : public IDriverDependantBitmap
{
  public:
    int _width;
    int _height;
    int _colourDepth;
    int _texture;
    bool _opaque;

    int _transparency;
    bool _isFlipped;
    int _stretchWidth;
    int _stretchHeight;
    bool _stretchUseResampler;
    int _lightLevel;
    int _tintRed;
    int _tintGreen;
    int _tintBlue;
    int _tintSaturation;


public:
    OGL3Bitmap(int width, int height, int colourDepth, int texture, bool opaque) {
      _width = width;
      _height = height;
      _colourDepth = colourDepth;
      _texture = texture;
      _opaque = opaque;
    }
    ~OGL3Bitmap() {
      GLuint toDelete[1];
      toDelete[0] = _texture;
      glDeleteTextures(1, toDelete);
    }

    int GetWidth() override { return _width; }
    int GetHeight() override { return _height; }
    int GetColorDepth() override { return _colourDepth; }
    int GetTexture() { return _texture; }
    int GetOpaque() { return _opaque; }

    void SetTransparency(int transparency) override {_transparency=transparency;}
    void SetFlippedLeftRight(bool isFlipped) override {_isFlipped = isFlipped;}
    void SetStretch(int width, int height, bool useResampler = true) override {_stretchWidth=width; _stretchHeight=height; _stretchUseResampler=useResampler;}
    void SetLightLevel(int light_level) override {_lightLevel = light_level;}
    void SetTint(int red, int green, int blue, int tintSaturation) override {
      _tintRed = red;
      _tintGreen = green;
      _tintBlue = blue;
      _tintSaturation = tintSaturation; }
};

// ------------------------------------------------------------------------
// DRAW LIST
// ------------------------------------------------------------------------

struct OGL3SpriteDrawListEntry
{
    OGL3Bitmap *bitmap; // TODO: use shared pointer?
    int x, y;
    bool skip;

    OGL3SpriteDrawListEntry(OGL3Bitmap *ddb, int x_ = 0, int y_ = 0)
        : bitmap(ddb)
        , x(x_)
        , y(y_)
        , skip(false)
    {
    }
};


// ------------------------------------------------------------------------
// DRIVER
// ------------------------------------------------------------------------

/*
mouse: allow at any pixel position, vs within pixel boundaries

map mouse coordinates to native coordinates

scale in fixed integers, or allow linear scaling

allow different scaled sprites to background.

if memory back buffer requested, then maybe go into a special software rendering mode.



*/
class OGL3GraphicsDriver final : public IGraphicsDriver
{
public:
    const char*GetDriverName() override;
    const char*GetDriverID() override;

    void SetTintMethod(TintMethod method) override { }
    void SetScreenTint(int red, int green, int blue) override {  }

    IGfxModeList *GetSupportedModeList(int color_depth) override { return NULL; }
    bool IsModeSupported(const DisplayMode &mode) override { return true; }
    int  GetDisplayDepthForNativeDepth(int native_color_depth) const override {
      switch (native_color_depth) {
        case 8: return 8;
        case 32: return 32;
        default: return 32;
      }
    }

    bool SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer) override;
    DisplayMode GetDisplayMode() const override;
    bool IsModeSet() const override { return _window != NULL; }

    bool SetNativeSize(const Size &src_size) override { _nativeSize = src_size; return true; }
    bool IsNativeSizeValid() const override { return !_nativeSize.IsNull(); }
    Size GetNativeSize() const override { return _nativeSize; }

    bool SetRenderFrame(const Rect &dst_rect) override { _renderFrame = dst_rect; return true; }
    bool IsRenderFrameValid() const override {  return !_renderFrame.IsEmpty(); }
    Rect GetRenderDestination() const override { return _renderFrame; }
    void SetRenderOffset(int x, int y) override {  }

    void SetGraphicsFilter(Filters filter);
    PGfxFilter GetGraphicsFilter() const override {
      switch(this->_filter) {
        case Filters::NEAREST:
          return PGfxFilter(new OGL3GfxFilter(Filters::NEAREST, &StdScaleFilterInfo));
        case Filters::LINEAR:
          return PGfxFilter(new OGL3GfxFilter(Filters::LINEAR, &LinearFilterInfo));
        default:
          return PGfxFilter(new OGL3GfxFilter(Filters::NEAREST, &StdScaleFilterInfo));
      }
    }

    // used by some plugins!
    bool UsesMemoryBackBuffer() override { return false; }
    Common::Bitmap *GetMemoryBackBuffer() override { return _backBuffer; }
    void SetMemoryBackBuffer(Common::Bitmap *backBuffer) override { _backBuffer = backBuffer; }

    Common::Bitmap *ConvertBitmapToSupportedColourDepth(Common::Bitmap *bitmap) override;

    IDriverDependantBitmap* CreateDDBFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque = false) override;
    void UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, Common::Bitmap *bitmap, bool hasAlpha) override;
    void DestroyDDB(IDriverDependantBitmap* bitmap) override;

    void ClearRectangle(int x1, int y1, int x2, int y2, RGB *colorToUse) override {};
    void ClearDrawList() override;
    void DrawSprite(int x, int y, IDriverDependantBitmap* bitmap) override;
    void Render() override {   Render(kFlip_None); }
    void Render(GlobalFlipType flip) override;

    void RenderToBackBuffer() override { throw NotImplemented(); }
    void GetCopyOfScreenIntoBitmap(Common::Bitmap *destination, bool at_native_res = false) override { }
    void EnableVsyncBeforeRender(bool enabled) override {  }
    void Vsync() override {  }
    void RenderSpritesAtScreenResolution(bool enabled, int supersampling = 1) override {  }
    void FadeOut(int speed, int targetColourRed, int targetColourGreen, int targetColourBlue) override {  }
    void FadeIn(int speed, PALETTE p, int targetColourRed, int targetColourGreen, int targetColourBlue) override {  }
    void BoxOutEffect(bool blackingOut, int speed, int delay) override {  }
    bool PlayVideo(const char *filename, bool useAVISound, VideoSkipType skipType, bool stretchToFullScreen) override { return true; }
    void UseSmoothScaling(bool enabled) override { _smoothScalingEnabled = enabled; }
    bool SupportsGammaControl() override { return false; }
    void SetGamma(int newGamma) override { throw NotImplemented(); }
    bool RequiresFullRedrawEachFrame() override { return true; }
    bool HasAcceleratedStretchAndFlip() override { return true; }
    void UpdateDeviceScreen(const Size &screenSize) override {


        int w,h;
//
        SDL_GetWindowSize(this->_window, &w, &h);

        glViewport(0, 0, static_cast<GLsizei>(screenSize.Width), static_cast<GLsizei>(screenSize.Height));


    }


    void SetCallbackForPolling(GFXDRV_CLIENTCALLBACK callback) override { _forPollingCallback = callback; }
    void SetCallbackToDrawScreen(GFXDRV_CLIENTCALLBACK callback) override { _toDrawScreenCallback = callback; }
    void SetCallbackOnInit(GFXDRV_CLIENTCALLBACKINITGFX callback) override { _onInitCallback = callback; }
    void SetCallbackOnSurfaceUpdate(GFXDRV_CLIENTCALLBACKSURFACEUPDATE callback) override { _onSurfaceUpdateCallback = callback; }
    void SetCallbackForNullSprite(GFXDRV_CLIENTCALLBACKXY callback) override { _nullSpriteCallback = callback; }

    ~OGL3GraphicsDriver() override { gfx_driver = NULL; }


private:

  GFXDRV_CLIENTCALLBACK _forPollingCallback;
  GFXDRV_CLIENTCALLBACK _toDrawScreenCallback;
  GFXDRV_CLIENTCALLBACKINITGFX _onInitCallback;
  GFXDRV_CLIENTCALLBACKSURFACEUPDATE _onSurfaceUpdateCallback;
  GFXDRV_CLIENTCALLBACKXY _nullSpriteCallback;
  Filters _filter = Filters::NEAREST;
  SDL_Window *_window;
  SDL_GLContext _glContext;

  Size _nativeSize;
  Rect _renderFrame;
  Common::Bitmap *_backBuffer;
  bool _smoothScalingEnabled;

  std::vector<OGL3SpriteDrawListEntry> _drawList;


  GLuint _quadVAO;
  GLuint _shader;


};

const char *OGL3GraphicsDriver::GetDriverName() { return "OpenGL"; }
const char *OGL3GraphicsDriver::GetDriverID() { return "OGL"; }


bool OGL3GraphicsDriver::SetDisplayMode(const DisplayMode &mode, volatile int *loopTimer) {


  if (mode.ColorDepth != 32) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Colour depth %d is not support", mode.ColorDepth);
    return false;
  }

  if (SDL_GL_LoadLibrary(NULL) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load opengl library: %s", SDL_GetError());
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute (SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  int windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  if (!mode.Windowed) {
    windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  }
  this->_window = SDL_CreateWindow("AGS", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.Width, mode.Height, windowFlags);
  if (this->_window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create window: %s", SDL_GetError());
    return false;
  }

  this->_glContext = SDL_GL_CreateContext(this->_window);
  if (this->_glContext == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create gl context: %s", SDL_GetError());
    return false;
  }

  SDL_GL_MakeCurrent(this->_window, this->_glContext);

  SDL_GL_SetSwapInterval(1);

  gladLoadGLLoader(SDL_GL_GetProcAddress);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glEnable(GL_BLEND);
  // The source color is the value written by the fragment shader. The destination color is the color from the image in the framebuffer.
  glBlendFunc(/* sfactor = */ GL_SRC_ALPHA, /* dfactor = */ GL_ONE_MINUS_SRC_ALPHA);

/*

if we need to blend the alpha too.  i.e for rendering to a texture.
glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

*/



  glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    int w,h;
    SDL_GetWindowSize(this->_window, &w, &h);
    UpdateDeviceScreen(Size(w, h));

  _quadVAO = initRenderData();

  const char *vertexShader = R"EOS(
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 projection;

void main()
{
    TexCoords = vertex.zw;
    gl_Position = projection * model * vec4(vertex.xy, 0.0, 1.0);
}
)EOS";
  const char *fragmentShader =  R"EOS(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D image;

void main()
{
    color = texture(image, TexCoords);
    //color = vec4(1.0,0.0,0.0,1.0);
}
)EOS";
  _shader = shaderCompiler(vertexShader, fragmentShader, NULL);

    glUseProgram(_shader);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(_nativeSize.Width), static_cast<GLfloat>(_nativeSize.Height), 0.0f, 1.0f, -1.0f);
    glUniformMatrix4fv(glGetUniformLocation(_shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(_shader, "image"), 0);


  printf("OpenGL loaded\n");
  printf("Vendor:   %s\n", glGetString(GL_VENDOR));
  printf("Renderer: %s\n", glGetString(GL_RENDERER));
  printf("Version:  %s\n", glGetString(GL_VERSION));

//     _rgb_r_shift_15 = 0;
//     _rgb_g_shift_15 = 5;
//     _rgb_b_shift_15 = 10;
//
//     _rgb_r_shift_16 = 0;
//     _rgb_g_shift_16 = 5;
//     _rgb_b_shift_16 = 11;
//
//     _rgb_r_shift_24 = 0;
//     _rgb_g_shift_24 = 8;
//     _rgb_b_shift_24 = 16;
//
//     _rgb_r_shift_32 = 0;
//     _rgb_g_shift_32 = 8;
//     _rgb_b_shift_32 = 16;
//     _rgb_a_shift_32 = 24;

  gfx_driver = &gfx_opengl;
  return true;
}

DisplayMode OGL3GraphicsDriver::GetDisplayMode() const {
  // TODO, get color depth, check vsync.

  int w,h;
  SDL_GetWindowSize(this->_window, &w, &h);

  Uint32 flags = SDL_GetWindowFlags(this->_window);
  bool windowed = !(flags & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP));

  return DisplayMode(GraphicResolution(w, h, 32), windowed, 0, true);
}

void OGL3GraphicsDriver::SetGraphicsFilter(Filters filter) {
  _filter = filter;
}

Bitmap *OGL3GraphicsDriver::ConvertBitmapToSupportedColourDepth(Bitmap *bitmap)
{
  return convertBitmapToColourDepth(bitmap, 32);
}




void massage(Uint32 *buf, int size, bool alpha, bool opaque) {

    Uint32*b = buf;
    int count = size;

    // make it obvious so better solution is found.
    int avg_r=0xff, avg_g=0x0, avg_b=0xff;
//    int divisor = 0;

//    while (count) {
//
//      if (*b != MASK_COLOR_32) {
//        //ca = (*b >> 24) & 0xff;
//        avg_r += (*b >> 16) & 0xff;
//        avg_g += (*b >> 8) & 0xff;
//        avg_b += (*b >> 0) & 0xff;
//          divisor += 1;
//      }
//      b++;
//      count--;
//    }
//
//    if (divisor > 0) {
//      avg_r /= divisor;
//      avg_g /= divisor;
//      avg_b /= divisor;
//    }



    b = buf;
    count = size;



    int ca, cr,cg, cb;

    while (count) {

     if (*b == MASK_COLOR_32) {

         cr = avg_r;
         cg = avg_g;
         cb = avg_b;

         ca = opaque ? 0xff : 0x00;

     } else {

           ca = (*b >> 24) & 0xff;
           cr = (*b >> 16) & 0xff;
           cg = (*b >> 8) & 0xff;
           cb = (*b >> 0) & 0xff;


         if (!alpha) {
             ca = 0xff;
         }


     }

      //cr=0xff;
      //cb = cg = 0;
     // *b = (ca<<24)|(cb<<16)|(cg<<8)|(cr<<0);
        *b = (ca<<24)|(cr<<16)|(cg<<8)|(cb<<0);
        b++;
        count--;
   }
}
//
//void massage_olde(Uint32 *b, int size, bool alpha, bool opaque) {
//    int ca, cr,cg, cb;
//
//    while (size) {
//
//     if (*b == MASK_COLOR_32) {
//
//         cr = 0;
//         cg = 0;
//         cb = 0;
//
//         ca = opaque ? 0xff : 0x00;
//
//     } else {
//
//           ca = (*b >> 24) & 0xff;
//           cr = (*b >> 16) & 0xff;
//           cg = (*b >> 8) & 0xff;
//           cb = (*b >> 0) & 0xff;
//
//
//         if (!alpha) {
//             ca = 0xff;
//         }
//
//
//     }
//
//      //cr=0xff;
//      //cb = cg = 0;
//     // *b = (ca<<24)|(cb<<16)|(cg<<8)|(cr<<0);
//        *b = (ca<<24)|(cr<<16)|(cg<<8)|(cb<<0);
//        //*b = 0xffffffff;
//        b++;
//        size--;
//   }
//}


IDriverDependantBitmap* OGL3GraphicsDriver::CreateDDBFromBitmap(Common::Bitmap *bitmap, bool hasAlpha, bool opaque) {

  unsigned int texture = 42;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  // set the texture wrapping/filtering options (on the currently bound texture object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  Common::Bitmap *textureBitmap = convertBitmapToColourDepth(bitmap, 32);
   Uint32 * b = reinterpret_cast< Uint32 *>(textureBitmap->GetDataForWriting());
   massage(b, textureBitmap->GetWidth()*textureBitmap->GetHeight(), hasAlpha, opaque);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureBitmap->GetWidth(), textureBitmap->GetHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, b);


  glBindTexture(GL_TEXTURE_2D, 0); // unbind


  IDriverDependantBitmap* result = new OGL3Bitmap(textureBitmap->GetWidth(), textureBitmap->GetHeight(), 32, texture, opaque);
    if (textureBitmap != bitmap) { delete textureBitmap; }
    return result;
}

void OGL3GraphicsDriver::UpdateDDBFromBitmap(IDriverDependantBitmap* bitmapToUpdate, Common::Bitmap *bitmap, bool hasAlpha)  {

  OGL3Bitmap *oglBitmap = dynamic_cast<OGL3Bitmap *>(bitmapToUpdate);
  if (oglBitmap == NULL) { return; }

  glBindTexture(GL_TEXTURE_2D, oglBitmap->GetTexture());

  Common::Bitmap *textureBitmap = convertBitmapToColourDepth(bitmap, 32);

    Uint32 * b = reinterpret_cast< Uint32 *>(textureBitmap->GetDataForWriting());
   massage(b, textureBitmap->GetWidth()*textureBitmap->GetHeight(), hasAlpha, oglBitmap->GetOpaque());



  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureBitmap->GetWidth(), textureBitmap->GetHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, b);
  if (textureBitmap != bitmap) { delete textureBitmap; }

  glBindTexture(GL_TEXTURE_2D, 0); // unbind
}

void OGL3GraphicsDriver::DestroyDDB(IDriverDependantBitmap *bitmap) {
  delete bitmap;
}



void OGL3GraphicsDriver::ClearDrawList()
{
  _drawList.clear();
}

void OGL3GraphicsDriver::DrawSprite(int x, int y, IDriverDependantBitmap* bitmap)
{
  OGL3Bitmap *oglBitmap = NULL;

  // if bitmap is null, then x and y are event,data
  if (bitmap) {
    oglBitmap = dynamic_cast<OGL3Bitmap *>(bitmap);
    if (!oglBitmap) { return; }
  }

  _drawList.push_back(OGL3SpriteDrawListEntry(oglBitmap, x, y));
}

void renderSprite(int x, int y, OGL3Bitmap *bitmap, GLuint quadVAO, GLuint shader) {

  // flip https://stackoverflow.com/questions/49110149/flipping-opengl-drawing-without-scale-by-negative-numbers

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, bitmap->GetTexture());

  glm::vec2 position(static_cast<GLfloat>(x), static_cast<GLfloat>(y));
  glm::vec2 size(static_cast<GLfloat>(bitmap->GetWidth()), static_cast<GLfloat>(bitmap->GetHeight()));

   // remember, matrices are transformed in reverse order
  glm::mat4 model(1.0f);
  model = glm::translate(model, glm::vec3(position, 0.0f));
  model = glm::scale(model, glm::vec3(size, 1.0f));

    if (bitmap->_isFlipped)
    {
        model = glm::scale(model, glm::vec3(-1.0f, 1.0f, 1.0f));
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 0.0f));
    }

  glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

}

void OGL3GraphicsDriver::Render(GlobalFlipType flip) {

  glClear( GL_COLOR_BUFFER_BIT  );

    glUseProgram(_shader);

    glm::mat4 projection;
    switch (flip) {
      case kFlip_Horizontal:
        projection = glm::ortho(static_cast<GLfloat>(_nativeSize.Width), 0.0f, static_cast<GLfloat>(_nativeSize.Height), 0.0f, 1.0f, -1.0f);
        break;

      case kFlip_Vertical:
        projection = glm::ortho(0.0f, static_cast<GLfloat>(_nativeSize.Width), 0.0f, static_cast<GLfloat>(_nativeSize.Height), 1.0f, -1.0f);
        break;

      case kFlip_Both:
        projection = glm::ortho(static_cast<GLfloat>(_nativeSize.Width), 0.0f, 0.0f, static_cast<GLfloat>(_nativeSize.Height), 1.0f, -1.0f);
        break;

      case kFlip_None:
      default:
        projection = glm::ortho(0.0f, static_cast<GLfloat>(_nativeSize.Width), static_cast<GLfloat>(_nativeSize.Height), 0.0f, 1.0f, -1.0f);
    }
    glUniformMatrix4fv(glGetUniformLocation(_shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(_shader, "image"), 0);

  for (size_t i = 0; i < _drawList.size(); i++)
  {
    if (_drawList[i].skip) { continue; }

    // fun fact, if sw, then y was passed in as data
    // if d3d, then direct3ddevice was passed in.
    if (_drawList[i].bitmap == NULL) {

      // AGSE_PRERENDER,
      // AGSE_PRESCREENDRAW, AGSE_PREGUIDRAW, AGSE_POSTSCREENDRAW, AGSE_FINALSCREENDRAW

      if (_nullSpriteCallback) {
        _nullSpriteCallback(_drawList[i].x, _drawList[i].y);
      }

      continue;
    }

    renderSprite(_drawList[i].x, _drawList[i].y, _drawList[i].bitmap, _quadVAO, _shader);
  }
  _drawList.clear();

  SDL_GL_SwapWindow(this->_window);
}


// ------------------------------------------------------------------------
// FACTORY
// ------------------------------------------------------------------------

class OGL3GraphicsFactory final : public IGfxDriverFactory
{
public:
    ~OGL3GraphicsFactory() override;
    void                 Shutdown() override;
    IGraphicsDriver *    GetDriver() override;
    void                 DestroyDriver() override;
    size_t               GetFilterCount() const override;
    const GfxFilterInfo *GetFilterInfo(size_t index) const override;
    String               GetDefaultFilterID() const override;
    PGfxFilter           SetFilter(const String &id, String &filter_error) override;

private:

    OGL3GraphicsDriver *_driver = NULL;
};

OGL3GraphicsFactory::~OGL3GraphicsFactory() {}
void OGL3GraphicsFactory::Shutdown() {}

IGraphicsDriver * OGL3GraphicsFactory::GetDriver() {
    if (!_driver) {
      _driver = new OGL3GraphicsDriver();
    }
    return _driver;
}
void OGL3GraphicsFactory::DestroyDriver() {
  delete _driver;
  _driver = NULL;
}

size_t OGL3GraphicsFactory::GetFilterCount() const {
  return 2;
}

const GfxFilterInfo *OGL3GraphicsFactory::GetFilterInfo(size_t index) const {
  switch (index) {
    case 0: return &StdScaleFilterInfo;
    case 1: return &LinearFilterInfo;
    default: return &StdScaleFilterInfo;
  }
}

String OGL3GraphicsFactory::GetDefaultFilterID() const {
  return StdScaleFilterInfo.Id;
}

PGfxFilter OGL3GraphicsFactory::SetFilter(const String &id, String &filter_error) {

    if (!_driver) {
      filter_error = "Graphics driver was not created";
      return PGfxFilter();
    }

    if (id.CompareNoCase("StdScale")) {
      _driver->SetGraphicsFilter(Filters::NEAREST);
      return PGfxFilter(new OGL3GfxFilter(Filters::NEAREST, &StdScaleFilterInfo));
    }

    if (id.CompareNoCase("Linear")) {
      _driver->SetGraphicsFilter(Filters::LINEAR);
      return PGfxFilter(new OGL3GfxFilter(Filters::LINEAR, &LinearFilterInfo));
    }

    return PGfxFilter();
}



static OGL3GraphicsFactory *_factory = NULL;
IGfxDriverFactory *GetOGL3Factory()
{
    if (!_factory) {
        _factory = new OGL3GraphicsFactory();
    }
    return _factory;
}


} // namespace Engine
} // namespace AGS

#endif // only on Windows, Android and iOS
