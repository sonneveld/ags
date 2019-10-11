/*         ______   ___    ___
*        /\  _  \ /\_ \  /\_ \
*        \ \ \\ \\//\ \ \//\ \      __     __   _ __   ___
*         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
*          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
*           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
*            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
*                                           /\____/
*                                           \_/__/
*
*      Display driver.
*
*      By JJS for the Adventure Game Studio runtime port.
*      Based on the Allegro PSP port.
*
*      See readme.txt for copyright information.
*/

#include "glm/glm.hpp"
#include "glm/ext.hpp"

#define ALLEGRO_NO_FIX_CLASS

extern "C" {

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintand.h"

#include <GLES2/gl2.h>
#include <android/log.h>


/* Software version of some blitting methods */
static void (*_orig_blit) (BITMAP * source, BITMAP * dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);

static BITMAP *android_display_init(int w, int h, int v_w, int v_h, int color_depth);
static void gfx_android_enable_acceleration(GFX_VTABLE *vtable);
static void android_hw_blit(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height);

void android_render();
void android_initialize_opengl();


/* Options controlled by the application */
int psp_gfx_scaling = 1;
int psp_gfx_smoothing = 1;


unsigned int android_screen_texture = 0;
unsigned int android_screen_width = 320;
unsigned int android_screen_height = 200;
unsigned int android_screen_physical_width = 320;
unsigned int android_screen_physical_height = 200;
unsigned int android_screen_color_depth = 32;
int android_screen_texture_width = 0;
int android_screen_texture_height = 0;
float android_screen_ar = 1.0f;
float android_device_ar = 1.0f;

static GLuint android_shader_program = 0;

int android_screen_initialized = 0;

GLfloat android_vertices[] =
{
   0, 0,
   320,  0,
   0,  200,
   320,  200
};

GLfloat android_texture_coordinates[] =
{
   0, 200.0f / 256.0f,
   320.0f / 512.0f, 200.0f / 256.0f,
   0, 0,
   320.0f / 512.0f, 0
};


GFX_DRIVER gfx_android =
{
   GFX_ANDROID,
   empty_string,
   empty_string,
   "Android gfx driver",
   android_display_init,         /* AL_METHOD(struct BITMAP *, init, (int w, int h, int v_w, int v_h, int color_depth)); */
   NULL,                         /* AL_METHOD(void, exit, (struct BITMAP *b)); */
   NULL,                         /* AL_METHOD(int, scroll, (int x, int y)); */
   NULL,                         /* AL_METHOD(void, vsync, (void)); */
   NULL,                         /* AL_METHOD(void, set_palette, (AL_CONST struct RGB *p, int from, int to, int retracesync)); */
   NULL,                         /* AL_METHOD(int, request_scroll, (int x, int y)); */
   NULL,                         /* AL_METHOD(int, poll_scroll, (void)); */
   NULL,                         /* AL_METHOD(void, enable_triple_buffer, (void)); */
   NULL,                         /* AL_METHOD(struct BITMAP *, create_video_bitmap, (int width, int height)); */
   NULL,                         /* AL_METHOD(void, destroy_video_bitmap, (struct BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(int, show_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(int, request_video_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(BITMAP *, create_system_bitmap, (int width, int height)); */
   NULL,                         /* AL_METHOD(void, destroy_system_bitmap, (BITMAP *bitmap)); */
   NULL,                         /* AL_METHOD(int, set_mouse_sprite, (BITMAP *sprite, int xfocus, int yfocus)); */
   NULL,                         /* AL_METHOD(int, show_mouse, (BITMAP *bmp, int x, int y)); */
   NULL,                         /* AL_METHOD(void, hide_mouse, (void)); */
   NULL,                         /* AL_METHOD(void, move_mouse, (int x, int y)); */
   NULL,                         /* AL_METHOD(void, drawing_mode, (void)); */
   NULL,                         /* AL_METHOD(void, save_video_state, (void)); */
   NULL,                         /* AL_METHOD(void, restore_video_state, (void)); */
   NULL,                         /* AL_METHOD(void, set_blender_mode, (int mode, int r, int g, int b, int a)); */
   NULL,                         /* AL_METHOD(int, fetch_mode_list, (void)); */
   0, 0,                         /* physical (not virtual!) screen size */
   TRUE,                         /* true if video memory is linear */
   0,                            /* bank size, in bytes */
   0,                            /* bank granularity, in bytes */
   0,                            /* video memory size, in bytes */
   0,                            /* physical address of video memory */
   FALSE                         /* true if driver runs windowed */
};


static BITMAP *android_display_init(int w, int h, int v_w, int v_h, int color_depth)
{
   GFX_VTABLE* vtable = _get_vtable(color_depth);

   /* Do the final screen blit to a 32 bit bitmap in palette mode */
   if (color_depth == 8)
      color_depth = 32;

   android_screen_width = w;
   android_screen_height = h;
   android_screen_color_depth = color_depth;

   displayed_video_bitmap = create_bitmap_ex(color_depth, w, h);
   gfx_android_enable_acceleration(vtable);

   android_create_screen(w, h, color_depth);

   return displayed_video_bitmap;
}


static void gfx_android_enable_acceleration(GFX_VTABLE *vtable)
{
   /* Keep the original blitting methods */
   _orig_blit = vtable->blit_to_self;

   /* Accelerated blits. */
   vtable->blit_from_memory = android_hw_blit;
   vtable->blit_to_memory = android_hw_blit;
   vtable->blit_from_system = android_hw_blit;
   vtable->blit_to_system = android_hw_blit;
   vtable->blit_to_self = android_hw_blit;

   _screen_vtable.blit_from_memory = android_hw_blit;
   _screen_vtable.blit_to_memory = android_hw_blit;
   _screen_vtable.blit_from_system = android_hw_blit;
   _screen_vtable.blit_to_system = android_hw_blit;
   _screen_vtable.blit_to_self = android_hw_blit;

   gfx_capabilities |= (GFX_HW_VRAM_BLIT | GFX_HW_MEM_BLIT);
}


/* All blitting goes through here, signal a screen update if the blit target is the screen bitmap */
static void android_hw_blit(BITMAP *source, BITMAP *dest, int source_x, int source_y, int dest_x, int dest_y, int width, int height)
{
   _orig_blit(source, dest, source_x, source_y, dest_x, dest_y, width, height);

   if (dest == displayed_video_bitmap)
      android_render();
}


static const char *default_vertex_shader_src =
   "#version 100\n"
   "uniform mat4 u_transform;\n"
   "attribute vec2 a_Position;\n"
   "attribute vec2 a_TexCoord;\n"
   "varying vec2 v_TexCoord;\n"
   "void main() {  \n"
   "  v_TexCoord = a_TexCoord;\n"
   "  gl_Position = u_transform * vec4(a_Position.xy, 0.0, 1.0);\n"
   "}\n";

static const char *default_fragment_shader_src =
   "#version 100\n"
   "precision mediump float;\n"
   "uniform sampler2D u_texture;\n"
   "varying vec2 v_TexCoord;\n"
   "void main()\n"
   "{\n"
   "  gl_FragColor = texture2D(u_texture, v_TexCoord);\n"
   "}";



void OutputShaderError(GLuint obj_id, const char *obj_name, int is_shader)
{
  GLint log_len;
  if (is_shader)
    glGetShaderiv(obj_id, GL_INFO_LOG_LENGTH, &log_len);
  else
    glGetProgramiv(obj_id, GL_INFO_LOG_LENGTH, &log_len);

  char errorLog[log_len];
  if (log_len > 0)
  {
    if (is_shader)
      glGetShaderInfoLog(obj_id, log_len, &log_len, &errorLog[0]);
    else
      glGetProgramInfoLog(obj_id, log_len, &log_len, &errorLog[0]);
  }

    __android_log_print(ANDROID_LOG_ERROR, "OutputShaderError", "ERROR: OpenGL: %s %s:", obj_name, is_shader ? "failed to compile" : "failed to link");
  if (log_len > 0)
  {
      __android_log_print(ANDROID_LOG_ERROR, "OutputShaderError", "----------------------------------------");
      __android_log_print(ANDROID_LOG_ERROR, "OutputShaderError","%s", &errorLog[0]);
      __android_log_print(ANDROID_LOG_ERROR, "OutputShaderError", "----------------------------------------");
  }
  else
  {
      __android_log_print(ANDROID_LOG_ERROR, "OutputShaderError", "Shader info log was empty.");
  }
}


GLuint CreateShaderProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
  GLint result;

  GLint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE)
  {
    OutputShaderError(vertex_shader, "vertex", 1);
    return 0;
  }

  GLint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
  if (result == GL_FALSE)
  {
    OutputShaderError(fragment_shader, "fragment", 1);
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if(result == GL_FALSE)
  {
    OutputShaderError(program, "program", 0);
    return 0;
  }

  glDetachShader(program, vertex_shader);
  glDeleteShader(vertex_shader);

  glDetachShader(program, fragment_shader);
  glDeleteShader(fragment_shader);

  return program;
}


int android_get_next_power_of_2(int value)
{
   int test = 1;

   while (test < value)
      test *= 2;

   return test;
}


/* Create the texture that holds the screen bitmap */
void android_create_screen_texture(int width, int height, int color_depth)
{
   char* empty;

   android_screen_texture_width = android_get_next_power_of_2(width);
   android_screen_texture_height = android_get_next_power_of_2(height);

   empty = (char*)malloc(android_screen_texture_width * android_screen_texture_height * color_depth / 8);
   memset(empty, 0, android_screen_texture_width * android_screen_texture_height * color_depth / 8);

   if (android_screen_texture != 0)
      glDeleteTextures(1, &android_screen_texture);

   glGenTextures(1, &android_screen_texture);
   glBindTexture(GL_TEXTURE_2D, android_screen_texture);

   if (color_depth == 16)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, android_screen_texture_width, android_screen_texture_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, empty);
   else
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, android_screen_texture_width, android_screen_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, empty);

   free(empty);
}


/* Set the values for the texture coord. and vertex arrays */
void android_create_arrays()
{
   if (psp_gfx_scaling == 1)
   {
      if (android_device_ar <= android_screen_ar)
      {
         android_vertices[2] = android_vertices[6] = android_screen_physical_width - 1;
         android_vertices[5] = android_vertices[7] = android_screen_physical_width * ((float)android_screen_height / (float)android_screen_width);
         
         android_mouse_setup(
            0, 
            android_screen_physical_width - 1, 
            (android_screen_physical_height - android_vertices[5]) / 2, 
            android_screen_physical_height - ((android_screen_physical_height - android_vertices[5]) / 2), 
            (float)android_screen_width / (float)android_screen_physical_width, 
            (float)android_screen_height / android_vertices[5]);
      }
      else
      {
         android_vertices[2] = android_vertices[6] = android_screen_physical_height * ((float)android_screen_width / (float)android_screen_height);
         android_vertices[5] = android_vertices[7] = android_screen_physical_height - 1;
         
         android_mouse_setup(
            (android_screen_physical_width - android_vertices[2]) / 2,
            android_screen_physical_width - ((android_screen_physical_width - android_vertices[2]) / 2),
            0,
            android_screen_physical_height - 1,
            (float)android_screen_width / android_vertices[2], 
            (float)android_screen_height / (float)android_screen_physical_height);
      }
   }
   else if (psp_gfx_scaling == 2)
   {
      android_vertices[2] = android_vertices[6] = android_screen_physical_width - 1;
      android_vertices[5] = android_vertices[7] = android_screen_physical_height - 1;
      
      android_mouse_setup(
         0, 
         android_screen_physical_width - 1, 
         0, 
         android_screen_physical_width - 1, 
         (float)android_screen_width / (float)android_screen_physical_width, 
         (float)android_screen_height / (float)android_screen_physical_height);	  
   }   
   else
   {
      android_vertices[0] = android_vertices[4] = android_screen_width * (-0.5f);
      android_vertices[2] = android_vertices[6] = android_screen_width * 0.5f;
      android_vertices[5] = android_vertices[7] = android_screen_height * 0.5f;
      android_vertices[1] = android_vertices[3] = android_screen_height * (-0.5f);
      
      android_mouse_setup(
         (android_screen_physical_width - android_screen_width) / 2,
         android_screen_physical_width - ((android_screen_physical_width - android_screen_width) / 2),
         (android_screen_physical_height - android_screen_height) / 2, 
         android_screen_physical_height - ((android_screen_physical_height - android_screen_height) / 2), 
         1.0f,
         1.0f);
   }

   android_texture_coordinates[1] = android_texture_coordinates[3] = (float)android_screen_height / (float)android_screen_texture_height;
   android_texture_coordinates[2] = android_texture_coordinates[6] = (float)android_screen_width / (float)android_screen_texture_width;
}


/* Called from the Java app to set up the screen */
void android_initialize_renderer(JNIEnv* env, jobject self, jint screen_width, jint screen_height)
{
   android_screen_physical_width = screen_width;
   android_screen_physical_height = screen_height;
   android_screen_initialized = 0;
}


void android_initialize_opengl()
{
   android_screen_ar = (float)android_screen_width / (float)android_screen_height;
   android_device_ar = (float)android_screen_physical_width / (float)android_screen_physical_height;

   android_shader_program = CreateShaderProgram(default_vertex_shader_src, default_fragment_shader_src);

   android_create_screen_texture(android_screen_width, android_screen_height, android_screen_color_depth);
   android_create_arrays();
}



void android_render()
{
   if (!android_screen_initialized)
   {
      android_initialize_opengl();
      android_screen_initialized = 1;
   }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glViewport(0, 0, android_screen_physical_width, android_screen_physical_height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);


   glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, android_screen_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (psp_gfx_smoothing)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    if (android_screen_color_depth == 16)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, android_screen_width, android_screen_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, displayed_video_bitmap->line[0]);
    else
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, android_screen_width, android_screen_height, GL_RGBA, GL_UNSIGNED_BYTE, displayed_video_bitmap->line[0]);


    auto transform = glm::ortho(0.0f, ((float)android_screen_physical_width) - 1.0f, 0.0f, ((float)android_screen_physical_height) - 1.0f, -1.0f, 1.0f);

    if (psp_gfx_scaling == 1)
    {
        if (android_device_ar <= android_screen_ar)
            transform = glm::translate(transform, {0.0f, (android_screen_physical_height - android_vertices[5] - 1) / 2.0f, 0.0f});
        else
            transform = glm::translate(transform, {(android_screen_physical_width - android_vertices[2] - 1) / 2.0f, 0.0f, 0.0f});
    }
    else if (psp_gfx_scaling == 0)
    {
        transform = glm::translate(transform, {android_screen_physical_width / 2.0f, android_screen_physical_height / 2.0f, 0.0f});
    }


    glUseProgram(android_shader_program);
    glUniformMatrix4fv(glGetUniformLocation(android_shader_program, "u_transform"), 1, GL_FALSE, glm::value_ptr(transform));
    glUniform1i(glGetUniformLocation(android_shader_program, "u_texture"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(0);
    GLint a_Position = glGetAttribLocation(android_shader_program, "a_Position");
    glVertexAttribPointer(a_Position, 2, GL_FLOAT, GL_FALSE, 0, android_vertices);

    glEnableVertexAttribArray(1);
    GLint a_TexCoord = glGetAttribLocation(android_shader_program, "a_TexCoord");
    glVertexAttribPointer(a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, android_texture_coordinates);


    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   glFinish();

    GLenum err;
    for (;;) {
        err = glGetError();
        if (err == GL_NO_ERROR) { break; }
        __android_log_print(ANDROID_LOG_ERROR, "agfx::_render", "glerror: %d", err);
    }

   android_swap_buffers();
}


}
