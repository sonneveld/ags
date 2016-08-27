
//
//  sdlwrap.c
//  AGSKit
//
//  Created by Nick Sonneveld on 9/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include "sdlwrap.h"
#include <SDL2/SDL.h>
#include "allegro.h"
#include "allegro/internal/aintern.h"
#include <strings.h>
#include <ctype.h>



// --------------------------------------------------------------------------
// File
// --------------------------------------------------------------------------
char *canonicalize_filename(char *dest, const char *filename, int size)
{
    printf("STUB: canonicalize_filename\n");
    strcpy(dest, filename);
    return dest;
}
char *get_filename(const char *path) {
    
    const char *result = path;
    const char *p = path;
    
    while (*p) {
        if (*p == '/' || *p == '\\') {
            result = p+1;
        }
        p++;
    }
    printf("%s -> %s", path, result);
    return result;
}
int is_relative_filename(const char *filename)
{
    return filename[0] != '/' && filename[0] != '\\';
}
char *append_filename(char *dest, AL_CONST char *path, AL_CONST char *filename, int size)
{
    sprintf(dest, "%s/%s/", path, filename);
    return dest;
}
int file_exists(AL_CONST char *filename, int attrib, int *aret)
{
    return access( filename, F_OK ) != -1;
}
char *fix_filename_case(char *filename) { return filename; }
char *fix_filename_slashes(char *filename) { return filename; }


// File
int al_findfirst(AL_CONST char *pattern, struct al_ffblk *info, int attrib) { printf("STUB: al_findfirst\n");      return 0; }
int al_findnext(struct al_ffblk *info) { printf("STUB: al_findnext\n");      return 0; }
void al_findclose(struct al_ffblk *info) { printf("STUB: al_findclose\n");       }
int exists(AL_CONST char *filename) { printf("STUB: exists\n");      return 0; }
uint64_t file_size_ex(AL_CONST char *filename) { printf("STUB: file_size_ex\n");      return -1; }
char *make_relative_filename(char *dest, AL_CONST char *path, AL_CONST char *filename, int size) { printf("STUB: make_relative_filename\n");      return 0; }


//  fpack
//PACKFILE *__old_pack_fopen(AL_CONST char *filename, AL_CONST char *mode) { printf("STUB: __old_pack_fopen\n"); return 0; }
//int pack_fclose(PACKFILE *f) { printf("STUB: pack_fclose\n");      return 0; }
//int pack_feof(PACKFILE *f) { printf("STUB: pack_feof\n");      return 1; }
//PACKFILE *pack_fopen_vtable(AL_CONST PACKFILE_VTABLE *vtable, void *userdata) { printf("STUB: pack_fopen_vtable\n");      return 0; }
// long pack_fread(void *p, long n, PACKFILE *f) { printf("STUB: pack_fread\n");      return 0; }
//int pack_fseek(PACKFILE *f, int offset) { printf("STUB: pack_fseek\n");      return -1; }
//
//int pack_getc(PACKFILE *f) { printf("STUB: pack_getc\n");      return 0; }
//long pack_mgetl(PACKFILE *f) { printf("STUB: pack_mgetl\n");      return 0; }
//int pack_mgetw(PACKFILE *f) { printf("STUB: pack_mgetw\n");      return 0; }

