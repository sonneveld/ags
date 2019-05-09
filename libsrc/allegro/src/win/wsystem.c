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
 *      Main system driver for the Windows library.
 *
 *      By Stefan Schimanski.
 *
 *      Window close button support by Javier Gonzalez.
 *
 *      See readme.txt for copyright information.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include "allegro/platform/aintwin.h"

#ifndef ALLEGRO_WINDOWS
#error something is wrong with the makefile
#endif

/* DMC requires a DllMain() function, or else the DLL hangs. */
#if !defined(ALLEGRO_STATICLINK) && defined(ALLEGRO_DMC)
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason, LPVOID lpReserved)
{
   return TRUE;
}
#endif



/* list the available drivers */
_DRIVER_INFO _system_driver_list[] =
{
#ifdef ALLEGRO_SDL2
   {  SYSTEM_SDL2,    &system_sdl2,   TRUE  },
#endif

   {  SYSTEM_NONE,      &system_none,     FALSE },
   {  0,                NULL,             0     }
};


/* general vars */
HINSTANCE allegro_inst = NULL;
HANDLE allegro_thread = NULL;
CRITICAL_SECTION allegro_critical_section;
int _dx_ver;

/* internals */
static RECT wnd_rect;





/* win_err_str:
 *  Returns a error string for a window error.
 */
char *win_err_str(long err)
{
   static char msg[256];

   FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPTSTR)&msg, 0, NULL);

   return msg;
}



/* thread_safe_trace:
 *  Outputs internal trace message.
 */
void thread_safe_trace(char *msg,...)
{
   char buf[256];
   va_list ap;

   /* todo, some day: use vsnprintf (C99) */
   va_start(ap, msg);
   vsprintf(buf, msg, ap);
   va_end(ap);

   OutputDebugString(buf);  /* thread safe */
}
