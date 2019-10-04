/*         ______   ___    ___ 
 *        /\  _  \ /\_ \  /\_ \ 
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___ 
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      MacOS X quartz windowed gfx driver
 *
 *      By Angelo Mottola.
 *
 *      See readme.txt for copyright information.
 */


#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintosx.h"

#ifndef ALLEGRO_MACOSX
   #error something is wrong with the makefile
#endif

#include <OpenGL/gl.h>
#include <math.h>

#define PREFIX_I "al-osxgl INFO: "
#define PREFIX_W "al-osxgl WARNING: "

@class AllegroCocoaGLView;

static BITMAP *osx_gl_window_init(int, int, int, int, int);
static void osx_gl_window_exit(BITMAP *);

static BITMAP *osx_gl_full_init(int, int, int, int, int);
static void osx_gl_full_exit(BITMAP *);

static void gfx_cocoa_enable_acceleration(GFX_VTABLE *vtable);

GFX_DRIVER gfx_cocoagl_window =
{
    .id = GFX_COCOAGL_WINDOW,
    .name = empty_string,
    .desc = empty_string,
    .ascii_name = "Cocoa GL window",
    .init = osx_gl_window_init,
    .exit = osx_gl_window_exit,
    .set_mouse_sprite = osx_mouse_set_sprite,
    .show_mouse = osx_mouse_show,
    .hide_mouse = osx_mouse_hide,
    .move_mouse = osx_mouse_move,
    .linear = TRUE,
    .windowed = TRUE,
};

GFX_DRIVER gfx_cocoagl_full =
{
    .id = GFX_COCOAGL_FULLSCREEN,
    .name = empty_string,
    .desc = empty_string,
    .ascii_name = "Cocoa GL full",
    .init = osx_gl_full_init,
    .exit = osx_gl_full_exit,
    .set_mouse_sprite = osx_mouse_set_sprite,
    .show_mouse = osx_mouse_show,
    .hide_mouse = osx_mouse_hide,
    .move_mouse = osx_mouse_move,
    .linear = TRUE,
    .windowed = FALSE,
};


static float calc_aspect_ratio(NSSize size) {
    float display_aspect = ((float)size.width) / size.height;

#ifdef ALLEGRO_MACOSX_ALLOW_NON_SQUARE_PIXELS    
    /* if 320x200 or 640x400 - non square pixels */
    if ( ((size.width == 320) && (size.height == 200)) ||
        ((size.width == 640) && (size.height == 400)) ) {
        display_aspect = 4.0/3.0;
    }
#endif

    return display_aspect;
}

static NSSize adjust_display_size(NSSize size) 
{
    const float display_aspect = calc_aspect_ratio(size);
    return NSMakeSize (ceil(size.width), ceil(size.width / display_aspect));
}

static NSSize scale_display_size(NSSize size) 
{
    int display_w = size.width;
    int display_h = size.height;

    NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
    int w_scale = screenRect.size.width / display_w;
    int h_scale = screenRect.size.height / display_h;
    int scale = w_scale;
    if (h_scale < scale) { scale = h_scale; }
    if (scale < 1) { scale = 1; }

    display_w = display_w * scale;
    display_h = display_h * scale;

    return NSMakeSize (display_w, display_h);
}

static BITMAP *osx_gl_real_init(int w, int h, int v_w, int v_h, int color_depth, GFX_DRIVER * driver)
{
    bool is_fullscreen = driver->windowed == FALSE;

    GFX_VTABLE* vtable = _get_vtable(color_depth);

    if (color_depth != 32 && color_depth != 16)
        ustrzcpy(allegro_error, ALLEGRO_ERROR_SIZE, get_config_text("Unsupported color depth"));

    BITMAP *displayed_video_bitmap = create_bitmap_ex(color_depth, w, h);

    driver->w = w;
    driver->h = h;
    driver->vid_mem = w * h * BYTES_PER_PIXEL(color_depth);

    gfx_cocoa_enable_acceleration(vtable);

    // Run on the main thread because we're manipulating the API
    runOnMainQueueWithoutDeadlocking(^{

    const NSSize suggestedSize = NSMakeSize(w, h);
    const NSSize adjustedSize = adjust_display_size(suggestedSize);
    const NSSize scaledSize = scale_display_size(adjustedSize);

    // setup REAL window
    NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
    //if (is_fullscreen) {
      //  rect = [[NSScreen mainScreen] frame];
      //  styleMask = NSBorderlessWindowMask;
    //}

    osx_window = [[AllegroWindow alloc] initWithContentRect: (NSRect){.origin=NSZeroPoint, .size=scaledSize}
                                                  styleMask: styleMask
                                                    backing: NSBackingStoreBuffered
                                                      defer: NO
                                                     screen:[NSScreen mainScreen]];

    AllegroWindowDelegate *osx_window_delegate = [[AllegroWindowDelegate alloc] init];
    [osx_window setDelegate: (id<NSWindowDelegate>)osx_window_delegate];
    [osx_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    [osx_window center];

    set_window_title(osx_window_title);

    AllegroCocoaGLView *osx_gl_view = [[[AllegroCocoaGLView alloc] initWithFrame: (NSRect){.origin=NSZeroPoint, .size=scaledSize} windowed:driver->windowed backing: displayed_video_bitmap] autorelease];
    [osx_window setContentView: osx_gl_view];

    [osx_window setContentMinSize: adjustedSize];
    [osx_window setContentAspectRatio: adjustedSize];

    [osx_window setLevel: NSNormalWindowLevel];
    [osx_window orderFrontRegardless];
    [osx_window makeKeyAndOrderFront: nil];

    });

    // NS: this must be toggled after window and view has been completely configured. If not, the dock may interact
    // with the mouse cursor in weird ways, showing the system cursor if you hover where the dock would appear.
    // The use of 'dispatch_after' might not be necessary, but I wanted to let the runloop iterate a few times.
    if (is_fullscreen) {
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(50 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
        [osx_window toggleFullScreen:nil];
      });
    }

    osx_keyboard_focused(FALSE, 0);
    clear_keybuf();

    osx_skip_mouse_move = TRUE;
    osx_window_first_expose = TRUE;
    osx_gfx_mode = OSX_GFX_GL;

    return displayed_video_bitmap;
}

static BITMAP *osx_gl_window_init(int w, int h, int v_w, int v_h, int color_depth)
{
    BITMAP *bmp = osx_gl_real_init(w, h, v_w, v_h, color_depth, &gfx_cocoagl_window);
    return bmp;
}


static BITMAP *osx_gl_full_init(int w, int h, int v_w, int v_h, int color_depth)
{
    BITMAP *bmp = osx_gl_real_init(w, h, v_w, v_h, color_depth, &gfx_cocoagl_full);

    _unix_lock_mutex(osx_skip_events_processing_mutex);
    osx_skip_events_processing = FALSE;
    _unix_unlock_mutex(osx_skip_events_processing_mutex);

    return bmp;
}

static void osx_gl_window_exit(BITMAP *bmp)
{
    runOnMainQueueWithoutDeadlocking(^{
        if (osx_window) {
            [[osx_window contentView] stopRendering];
            
            [osx_window close];
            osx_window = nil;
        }
    });

    osx_gfx_mode = OSX_GFX_NONE;
}


static void osx_gl_full_exit(BITMAP *bmp)
{
    _unix_lock_mutex(osx_skip_events_processing_mutex);
    osx_skip_events_processing = TRUE;
    _unix_unlock_mutex(osx_skip_events_processing_mutex);

    osx_gl_window_exit(bmp);
}

static void gfx_cocoa_enable_acceleration(GFX_VTABLE *vtable)
{
    gfx_capabilities |= (GFX_HW_VRAM_BLIT | GFX_HW_MEM_BLIT);
}


/* NSOpenGLPixelFormat *init_pixel_format(int windowed)
 *
 * Generate a pixel format. First try and get all the 'suggested' settings.
 * If this fails, just get the 'required' settings,
 * or nil if no format can be found
 */
#define MAX_ATTRIBUTES           64

static NSOpenGLPixelFormat *init_pixel_format(int windowed)
{
    NSOpenGLPixelFormatAttribute attribs[MAX_ATTRIBUTES], *attrib;
   attrib=attribs;
    *attrib++ = kCGLPFAOpenGLProfile;
    *attrib++ = kCGLOGLPVersion_Legacy;
    *attrib++ = NSOpenGLPFADoubleBuffer;
    *attrib++ = NSOpenGLPFAColorSize;
    *attrib++ = 16;

//   if (!windowed) {
//      *attrib++ = NSOpenGLPFAScreenMask;
//      *attrib++ = CGDisplayIDToOpenGLDisplayMask(kCGDirectMainDisplay);
//   }
   *attrib = 0;

   NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes: attribs];

   return pf;
}

static CVReturn display_link_render_callback(CVDisplayLinkRef displayLink, const CVTimeStamp* now, const CVTimeStamp* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* displayLinkContext)
{
    @autoreleasepool {
        [((AllegroCocoaGLView *)displayLinkContext) render];
    }
    return kCVReturnSuccess;
}

struct MyVertex {
    GLfloat x;
    GLfloat y;
};

@interface AllegroCocoaGLView: NSOpenGLView {

    BITMAP *_displayed_video_bitmap;

    int _gameWidth;
    int _gameHeight;
    int _colourDepth;

    GLuint _osx_screen_texture;
    int _osx_screen_color_depth;
    GLuint _osx_texture_format;
    GLenum _osx_texture_storage;

    CVDisplayLinkRef _displayLink;

    struct MyVertex _gl_VertexCoords[4];
    struct MyVertex _gl_TextureCoords[4];
}

@end

@implementation AllegroCocoaGLView

// Used to override default cursor
- (void)resetCursorRects
{
    [super resetCursorRects];
    [self addCursorRect: [self visibleRect] cursor: osx_cursor];
    [osx_cursor setOnMouseEntered: YES];

    if (osx_mouse_tracking_rect != -1) {
      [self removeTrackingRect: osx_mouse_tracking_rect];
    }

    osx_mouse_tracking_rect = [self addTrackingRect: [self visibleRect]
                                                     owner: NSApp
                                                  userData: nil
                                              assumeInside: YES];

}

/* Custom view: when created, select a suitable pixel format */
- (id) initWithFrame: (NSRect) frame windowed:(BOOL)windowed backing: (BITMAP *) bmp
{
    _displayed_video_bitmap = bmp;
    _gameWidth = bmp->w;
    _gameHeight = bmp->h;
    _colourDepth = bmp->vtable->color_depth;

   NSOpenGLPixelFormat* pf = init_pixel_format(windowed);
   if (pf) {
        self = [super initWithFrame:frame pixelFormat: pf];
        [pf release];
        return self;
   }
   else
   {
        TRACE(PREFIX_W "Unable to find suitable pixel format\n");
   }

   return nil;
}

- (void)renewGState
{   
    // Called whenever graphics state updated (such as window resize)
    
    // OpenGL rendering is not synchronous with other rendering on the OSX.
    // Therefore, call disableScreenUpdatesUntilFlush so the window server
    // doesn't render non-OpenGL content in the window asynchronously from
    // OpenGL content, which could cause flickering.  (non-OpenGL content
    // includes the title bar and drawing done by the app with other APIs)
    [[self window] disableScreenUpdatesUntilFlush];

    [super renewGState];
}

- (void)stopRendering
{
    CVDisplayLinkStop(_displayLink);

    CVDisplayLinkRelease(_displayLink);
    _displayLink = nil;
    
    CGLLockContext([[self openGLContext] CGLContextObj]);
    @try {
        [[self openGLContext] makeCurrentContext];
        
        _displayed_video_bitmap = nil;
        
        [self delete_osx_screen_texture];
        
        [NSOpenGLContext clearCurrentContext];
    } @finally {
        CGLUnlockContext([[self openGLContext] CGLContextObj]);
    }

    if (osx_mouse_tracking_rect != -1) {
      [self removeTrackingRect: osx_mouse_tracking_rect];
    }
}

- (void)dealloc
{
    [self stopRendering];

    [super dealloc];

}

- (void)delete_osx_screen_texture
{
    if (_osx_screen_texture != 0)
    {
        glDeleteTextures(1, &_osx_screen_texture);
        _osx_screen_texture = 0;
    }  
}

- (void) prepareOpenGL 
{
    [super prepareOpenGL];
    
    /* Print out OpenGL version info */
    TRACE(PREFIX_I "OpenGL Version: %s\n", (AL_CONST char*)glGetString(GL_VERSION));
    TRACE(PREFIX_I "Vendor: %s\n", (AL_CONST char*)glGetString(GL_VENDOR));
    TRACE(PREFIX_I "Renderer: %s\n", (AL_CONST char*)glGetString(GL_RENDERER));

    // enable vsync
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

    [self osx_gl_setup];

    [[self openGLContext] flushBuffer];

    CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

    CVDisplayLinkSetOutputCallback(_displayLink, &display_link_render_callback, self);

    // Select the display link most optimal for the current renderer of an OpenGL context
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);

    CVDisplayLinkStart(_displayLink);
}

- (void) osx_gl_setup
{
    [self osx_gl_setup_arrays];

    glDisable(GL_DITHER);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glPixelZoom(1.0,1.0);

    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);

    [self reshape];

    glEnableClientState(GL_VERTEX_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINES);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(struct MyVertex), (const GLvoid*)&_gl_VertexCoords[0]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct MyVertex), (const GLvoid*)&_gl_TextureCoords[0]);

    glActiveTexture(GL_TEXTURE0);
    [self osx_gl_create_screen_texture];
}

- (void) osx_gl_create_screen_texture
{
    [self delete_osx_screen_texture];

    switch (_colourDepth) {
        case 16:
            _osx_texture_format = GL_RGB;
            _osx_texture_storage = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case 32:
            // this is ARGB, note the REV in storage which means bytes are reversed
            _osx_texture_format = GL_BGRA;
            _osx_texture_storage = GL_UNSIGNED_INT_8_8_8_8_REV;
            break;
        default:
            TRACE(PREFIX_I "unsupported color depth\n");
            return;
    }

    glGenTextures(1, &_osx_screen_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _osx_screen_texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA, _gameWidth, _gameHeight, 0, _osx_texture_format, _osx_texture_storage, NULL);
}

- (void) osx_gl_setup_arrays
{
    _gl_VertexCoords[0].x = 0;
    _gl_VertexCoords[0].y = 0;
    _gl_TextureCoords[0].x = 0;
    _gl_TextureCoords[0].y = _gameHeight;

    _gl_VertexCoords[1].x = _gameWidth;
    _gl_VertexCoords[1].y = 0;
    _gl_TextureCoords[1].x = _gameWidth;
    _gl_TextureCoords[1].y = _gameHeight;

    _gl_VertexCoords[2].x = 0;
    _gl_VertexCoords[2].y = _gameHeight;
    _gl_TextureCoords[2].x = 0;
    _gl_TextureCoords[2].y = 0;

    _gl_VertexCoords[3].x = _gameWidth;
    _gl_VertexCoords[3].y = _gameHeight;
    _gl_TextureCoords[3].x = _gameWidth;
    _gl_TextureCoords[3].y = 0;
}

- (void) drawRect: (NSRect) theRect
{
    [self render];
}


- (void) render
{
    [[self openGLContext] makeCurrentContext];
    
    CGLLockContext([[self openGLContext] CGLContextObj]);
    @try {
 
        if (!_displayed_video_bitmap) { return; }
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0,
                        0, 0, _gameWidth, _gameHeight,
                        _osx_texture_format, _osx_texture_storage, _displayed_video_bitmap->line[0]);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        CGLFlushDrawable([[self openGLContext] CGLContextObj]);
    } @finally {
        CGLUnlockContext([[self openGLContext] CGLContextObj]);
    }
}


- (void) reshape
{
    [super reshape];

    CGLLockContext([[self openGLContext] CGLContextObj]);
    @try {
        [[self openGLContext] makeCurrentContext];

        NSSize myNSWindowSize = [ self frame ].size;

        float aspect = calc_aspect_ratio(NSMakeSize(_gameWidth, _gameHeight));

        NSRect viewport;
        viewport.size = NSMakeSize(myNSWindowSize.height * aspect, myNSWindowSize.height);
        if (viewport.size.width > myNSWindowSize.width) {
            viewport.size = NSMakeSize(myNSWindowSize.width, myNSWindowSize.width/aspect);
        }
        viewport.origin = NSMakePoint((myNSWindowSize.width - viewport.size.width)/2, (myNSWindowSize.height - viewport.size.height)/2 );

        glViewport(viewport.origin.x, viewport.origin.y, viewport.size.width, viewport.size.height);
        glScissor(viewport.origin.x, viewport.origin.y, viewport.size.width, viewport.size.height);
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, _gameWidth - 1, 0, _gameHeight - 1, 0, 1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } @finally {
        CGLUnlockContext([[self openGLContext] CGLContextObj]);
    }

}

@end
