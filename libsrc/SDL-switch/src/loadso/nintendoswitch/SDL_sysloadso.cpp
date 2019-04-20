/*
Simple DirectMedia Layer
Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_LOADSO_NINTENDO_SWITCH

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent library loading routines                           */

#include <stdio.h>

#include "SDL.h"

#include <nn/os.h>
#include <nn/ro.h>
#include <nn/fs.h>


static void *
allocPages(const size_t bytes, size_t *outbytes)
{
    const int64_t buflen = nn::util::align_up(bytes, nn::os::MemoryPageSize);
    void *buffer = NULL;
    if (posix_memalign(&buffer, nn::os::MemoryPageSize, buflen) != 0) {
        return NULL;
    }
    if (outbytes) {
        *outbytes = buflen;
    }
    return buffer;
}


/* I would like to use SDL_LoadFile(), but I need these aligned to memory pages. */
static void *
loadFile(const char *fname, size_t *_buflen)
{
    nn::Result rc;
    nn::fs::FileHandle file;

    rc = nn::fs::OpenFile(&file, fname, nn::fs::OpenMode_Read);
    if (rc.IsFailure()) {
        SDL_SetError("Failed to open file '%s', err=%d", fname, rc.GetDescription());
        return NULL;
    }

    int64_t filelen = 0;
    rc = nn::fs::GetFileSize(&filelen, file);
    if (rc.IsFailure()) {
        nn::fs::CloseFile(file);
        SDL_SetError("Failed to get size of file '%s', err=%d", fname, rc.GetDescription());
        return NULL;
    }

    size_t buflen = 0;
    void *buffer = allocPages(filelen, &buflen);
    if (buffer == NULL) {
        nn::fs::CloseFile(file);
        SDL_OutOfMemory();
        return NULL;
    }

    size_t readlen = 0;
    rc = nn::fs::ReadFile(&readlen, file, 0, buffer, buflen);
    nn::fs::CloseFile(file);
    if (rc.IsFailure() || (readlen != ((size_t) filelen))) {
        SDL_SetError("Failed read all of file '%s', err=%d", fname, rc.GetDescription());
        free(buffer);
        return NULL;
    }

    /* zero out any padding. */
    if (buflen > readlen) {
        SDL_memset(((Uint8 *) buffer) + readlen, '\0', buflen - readlen);
    }

    if (_buflen) {
        *_buflen = buflen;
    }

    return buffer;
}


static SDL_atomic_t ro_lib_refcount;

typedef struct
{
    char *sofile;
    void *nrrimage;
    size_t nrrlen;
    void *nroimage;
    size_t nrolen;
    void *bssimage;
    size_t bsslen;
    nn::ro::Module module;
    nn::ro::RegistrationInfo reginfo;
} ModuleObject;


void *
SDL_LoadObject(const char *sofile)
{
    const size_t sofilelen = SDL_strlen(sofile);
    const size_t maxstrlen = sofilelen + 5;
    nn::Result rc;
    char *extension = NULL;
    SDL_bool incremented = SDL_FALSE;
    SDL_bool registered = SDL_FALSE;
    ModuleObject *obj = NULL;

    obj = (ModuleObject *) SDL_calloc(1, sizeof (ModuleObject));
    if (!obj) {
        SDL_OutOfMemory();
        goto failed;
    }

    obj->sofile = (char *) SDL_malloc(maxstrlen);
    if (!obj->sofile) {
        SDL_OutOfMemory();
        goto failed;
    }
    SDL_strlcpy(obj->sofile, sofile, maxstrlen);

    extension = SDL_strrchr(obj->sofile, '.');
    if (extension) {
        *extension = '\0';  /* chop off any filename extension. */
    } else {
        extension = obj->sofile + sofilelen;  /* point to end of string. */
    }

    SDL_strlcpy(extension, ".nrr", 5);
    obj->nrrimage = loadFile(obj->sofile, &obj->nrrlen);
    if (!obj->nrrimage) {
        goto failed;
    }

    SDL_strlcpy(extension, ".nro", 5);
    obj->nroimage = loadFile(obj->sofile, &obj->nrolen);
    if (!obj->nroimage) {
        goto failed;
    }

    *extension = '\0';

    if (SDL_AtomicIncRef(&ro_lib_refcount) == 0) {
        nn::ro::Initialize();
    }
    incremented = SDL_TRUE;

    rc = nn::ro::RegisterModuleInfo(&obj->reginfo, obj->nrrimage);
    if (rc.IsFailure()) {        
        SDL_SetError("Failed to register module info: err=%d", rc.GetDescription());
        goto failed;
    }
    registered = SDL_TRUE;

    rc = nn::ro::GetBufferSize(&obj->bsslen, obj->nroimage);
    if (rc.IsFailure()) {
        SDL_SetError("Failed to get BSS size for object: err=%d", rc.GetDescription());
        goto failed;
    }

    if (obj->bsslen > 0) {  /* can be zero when no data; bssimage==NULL, then. */
        obj->bssimage = allocPages(obj->bsslen, &obj->bsslen);
        if (!obj->bssimage) {
            SDL_OutOfMemory();
            goto failed;
        }
        SDL_memset(obj->bssimage, '\0', obj->bsslen);
    }

    rc = nn::ro::LoadModule(&obj->module, obj->nroimage, obj->bssimage, obj->bsslen, nn::ro::BindFlag_Now);
    if (rc.IsFailure()) {
        SDL_SetError("Failed to load module: err=%d", rc.GetDescription());
        goto failed;
    }

    return obj;

failed:
    if (registered) {
        nn::ro::UnregisterModuleInfo(&obj->reginfo);
    }

    if (incremented) {
        if (SDL_AtomicDecRef(&ro_lib_refcount)) {
            nn::ro::Finalize();
        }
    }

    if (obj) {
        /* the "image" fields use posix_memalign(), so don't use SDL_free here */
        free(obj->bssimage);
        free(obj->nroimage);
        free(obj->nrrimage);
        SDL_free(obj->sofile);
        SDL_free(obj);
    }

    return NULL;
}

void *
SDL_LoadFunction(void *handle, const char *name)
{
    uintptr_t addr = 0;
    if (handle == NULL) {
        SDL_SetError("Invalid module handle");
    } else {
        ModuleObject *obj = (ModuleObject *) handle;
        const nn::Result rc = nn::ro::LookupModuleSymbol(&addr, &obj->module, name);
        if (rc.IsFailure()) {
            SDL_SetError("Failed loading %s: err=%d", name, rc.GetDescription());
            return NULL;
        }
    }
    return (void *) addr;
}

void
SDL_UnloadObject(void *handle)
{
    ModuleObject *obj = (ModuleObject *)handle;
    if (obj == NULL) {
        return;
    }

    nn::ro::UnloadModule(&obj->module);
    nn::ro::UnregisterModuleInfo(&obj->reginfo);

    if (SDL_AtomicDecRef(&ro_lib_refcount)) {
        nn::ro::Finalize();
    }

    /* the "image" fields use posix_memalign(), so don't use SDL_free here */
    free(obj->bssimage);
    free(obj->nroimage);
    free(obj->nrrimage);
    SDL_free(obj->sofile);
    SDL_free(obj);
}

#endif /* SDL_LOADSO_NINTENDO_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */
