//
//  sdlwrap_filepack.cpp
//  AGSKit
//
//  Created by Nick Sonneveld on 5/10/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include <SDL2/SDL.h>

#include <stdio.h>

#include "ac/gamesetup.h"
#include "util/misc.h"
#include "core/assetmanager.h"
#include "main/game_file.h"
#include "debug/debug_log.h"

struct fp_window {
    FILE *fp;
    size_t offset;
    size_t len;
};

static Sint64 SDLCALL
stdio_size(SDL_RWops * context)
{
    struct fp_window *d = (struct fp_window *)(context->hidden.unknown.data1);
    return d->len;
}

static Sint64 SDLCALL
stdio_seek(SDL_RWops * context, Sint64 offset, int whence)
{
    struct fp_window *d = (struct fp_window *)(context->hidden.unknown.data1);

    switch (whence) {
        case RW_SEEK_SET:
        {
            long fseek_offset = d->offset + offset;
            fseek(d->fp, fseek_offset, RW_SEEK_SET);
            break;
        }
        case RW_SEEK_CUR:
        {
            long actual_offset = ftell(d->fp);
            long rel_offset = actual_offset - d->offset;
            long fseek_offset = d->offset + rel_offset + offset;
            fseek(d->fp, fseek_offset, RW_SEEK_SET);
            break;
        }
        case RW_SEEK_END:
        {
            long fseek_offset = d->offset + d->len + offset;
            fseek(d->fp, fseek_offset, RW_SEEK_SET);
            break;
        }
    }

    return ftell(d->fp) - d->offset;
}

static size_t SDLCALL
stdio_read(SDL_RWops * context, void *ptr, size_t size, size_t maxnum)
{
    struct fp_window *d = (struct fp_window *)(context->hidden.unknown.data1);
    
    size_t remaining = (d->len - (ftell(d->fp) - d->offset)) / size;
    size_t nread;
    
    if (remaining < maxnum) {
        maxnum = remaining;
    }
    
    nread = fread(ptr, size, maxnum, d->fp);
    if (nread == 0 && ferror(d->fp)) {
        SDL_Error(SDL_EFREAD);
    }

    return nread;
}

static size_t SDLCALL
stdio_write(SDL_RWops * context, const void *ptr, size_t size, size_t num)
{
    SDL_SetError("writing not supported.");
    return 0;
}

static int SDLCALL
stdio_close(SDL_RWops * context)
{
    int status = 0;
    if (context) {
        struct fp_window *d = (struct fp_window *)(context->hidden.unknown.data1);
        if (fclose(d->fp) != 0) {
            status = SDL_Error(SDL_EFWRITE);
        }
        free(context->hidden.unknown.data1);
        context->hidden.unknown.data1 = nullptr;
        SDL_FreeRW(context);
    }
    return status;
}

SDL_RWops *SDL_RWFromWindowFP(FILE * fp, size_t offset, size_t len)
{
    SDL_RWops *rwops = NULL;
    
    rwops = SDL_AllocRW();
    if (!rwops) { return NULL; }
    
    rwops->size = stdio_size;
    rwops->seek = stdio_seek;
    rwops->read = stdio_read;
    rwops->write = stdio_write;
    rwops->close = stdio_close;
    
    struct fp_window *d = (struct fp_window *)malloc(sizeof(struct fp_window));
    d->fp = fp;
    d-> offset = offset;
    d->len = len;
    
    rwops->hidden.unknown.data1 = d;
    
    rwops->type = SDL_RWOPS_UNKNOWN;

    stdio_seek(rwops, 0, RW_SEEK_SET);

    return rwops;
}

SDL_RWops *SDL_RWFromAgsAsset(const char *filnam1, const char *modd1)
{
    char  *filnam = (char *)filnam1;
    char  *modd = (char *)modd1;
    int   needsetback = 0;
    
    if (filnam[0] == '~') {
        // ~ signals load from specific data file, not the main default one
        char gfname[80];
        int ii = 0;
        
        filnam++;
        while (filnam[0]!='~') {
            gfname[ii] = filnam[0];
            filnam++;
            ii++;
        }
        filnam++;
        
        gfname[ii] = '\0';
        
        
        char *libname = ci_find_file(usetup.data_files_dir, gfname);
        if (Common::AssetManager::SetDataFile(libname) != Common::kAssetNoError)
        {
            // Hack for running in Debugger
            free(libname);
            libname = ci_find_file("Compiled", gfname);
            Common::AssetManager::SetDataFile(libname);
        }
        free(libname);
        
        needsetback = 1;
    }
    
    // if the file exists, override the internal file
    bool file_exists = Common::File::TestReadFile(filnam);
    
    if ((Common::AssetManager::GetAssetOffset(filnam)<1) || (file_exists)) {
        if (needsetback) Common::AssetManager::SetDataFile(game_file_name);
        return SDL_RWFromFile(filnam, modd);
    }
    else {
        FILE *_my_temppack=fopen(Common::AssetManager::GetLibraryForAsset(filnam), modd);
        
        SDL_RWops *result = SDL_RWFromWindowFP (_my_temppack, Common::AssetManager::GetAssetOffset(filnam), Common::AssetManager::GetAssetSize(filnam));
        
        if (needsetback)
            Common::AssetManager::SetDataFile(game_file_name);
        return result;
    }
}