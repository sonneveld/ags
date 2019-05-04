#undef ALLEGRO_BCC32
#undef ALLEGRO_BEOS
#undef ALLEGRO_DJGPP
#undef ALLEGRO_DMC
#undef ALLEGRO_HAIKU
#undef ALLEGRO_MACOSX
#undef ALLEGRO_MINGW32
#undef ALLEGRO_MPW
#undef ALLEGRO_MSVC
#undef ALLEGRO_PSP
#undef ALLEGRO_QNX
#undef ALLEGRO_UNIX
#undef ALLEGRO_WATCOM

#undef ALLEGRO_MSVC_SDL2
#undef ALLEGRO_UNIX_SDL2
#define ALLEGRO_SDL2

#ifdef _WIN32
	#define ALLEGRO_MSVC
	#define ALLEGRO_MSVC_SDL2
#else
	#define ALLEGRO_UNIX
	#define ALLEGRO_UNIX_SDL2
#endif

/* These are always defined now. */
#define ALLEGRO_NO_ASM
#define ALLEGRO_USE_C
