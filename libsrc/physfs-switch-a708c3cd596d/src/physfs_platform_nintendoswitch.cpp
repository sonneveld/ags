/*
 * Nintendo Switch support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_NINTENDO_SWITCH

#include "physfs_internal.h"

#include <nn/fs.h>
#include <nn/os.h>
#include <nn/account.h>

static char *NintendoSwitch_ROMCache = NULL;
static nn::account::UserHandle *NintendoSwitch_UserHandle = NULL;

int PHYSFS_NintendoSwitch_CommitSaveData(const char *mountpoint)
{
    const nn::Result rc = nn::fs::Commit(mountpoint);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
}

int __PHYSFS_platformInit(void)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    if (NintendoSwitch_ROMCache != NULL) 
    {
        nn::fs::Unmount("rom");
        allocator.Free(NintendoSwitch_ROMCache);
        NintendoSwitch_ROMCache = NULL;
    } /* if */

    if (NintendoSwitch_UserHandle != NULL)
    {
        nn::fs::Unmount("save");
        nn::account::CloseUser(*NintendoSwitch_UserHandle);
        delete NintendoSwitch_UserHandle;
        NintendoSwitch_UserHandle = NULL;
    } /* if */
} /* __PHYSFS_platformDeinit */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    if (!NintendoSwitch_ROMCache)
    {
        size_t cachelen = 0;
        nn::fs::QueryMountRomCacheSize(&cachelen);
        BAIL_IF(cachelen == 0, PHYSFS_ERR_OS_ERROR, NULL);
        char *cachebuf = (char *) allocator.Malloc(cachelen);
        BAIL_IF(!cachebuf, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

        const nn::Result rc = nn::fs::MountRom("rom", cachebuf, cachelen);
        if (rc.IsFailure())
        {
            allocator.Free(cachebuf);
            BAIL(PHYSFS_ERR_OS_ERROR, NULL);
        } /* if */

        NintendoSwitch_ROMCache = cachebuf;
    } /* if */

    char *retval = (char *) __PHYSFS_strdup("rom:/");
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return retval;
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcUserDir(void)
{
    if (!NintendoSwitch_UserHandle)
    {
        nn::account::Initialize();
        nn::account::UserHandle *handle = new nn::account::UserHandle;
        nn::Result rc;

        bool openUserSuccess = nn::account::TryOpenPreselectedUser(handle);
        BAIL_IF(!openUserSuccess, PHYSFS_ERR_OS_ERROR, NULL);

        nn::account::Uid uid;
        rc = nn::account::GetUserId(&uid, *handle);
        if (rc.IsFailure())
        {
            delete handle;
            BAIL(PHYSFS_ERR_OS_ERROR, NULL);
        } /* if */

        rc = nn::fs::EnsureSaveData(uid);
        if (rc.IsFailure())
        {
            delete handle;
            BAIL(PHYSFS_ERR_OS_ERROR, NULL);
        } /* if */

        rc = nn::fs::MountSaveData("save", uid);
        if (rc.IsFailure())
        {
            delete handle;
            BAIL(PHYSFS_ERR_OS_ERROR, NULL);
        } /* if */

        NintendoSwitch_UserHandle = handle;
    } /* if */
    
    char *retval = __PHYSFS_strdup("save:/");
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return retval;
} /* __PHYSFS_platformCalcUserDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    return __PHYSFS_platformCalcUserDir();
} /* __PHYSFS_platformCalcPrefDir */


PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
    nn::fs::DirectoryHandle dir;
    nn::Result rc = nn::fs::OpenDirectory(&dir, dirname, nn::fs::OpenDirectoryMode_All);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, PHYSFS_ENUM_ERROR);
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

    while (retval == PHYSFS_ENUM_OK)
    {
        int64_t val = 0;
        nn::fs::DirectoryEntry ent;
        rc = nn::fs::ReadDirectory(&val, &ent, dir, 1);
        if (rc.IsFailure())
        {
            nn::fs::CloseDirectory(dir);
            BAIL(PHYSFS_ERR_OS_ERROR, PHYSFS_ENUM_ERROR);
        } /* if */

        if (val == 0)
            break;  /* end of listing. */

        retval = callback(callbackdata, origdir, ent.name);
        if (retval == PHYSFS_ENUM_ERROR)
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    } /* while */

    nn::fs::CloseDirectory(dir);

    return retval;
} /* __PHYSFS_platformEnumerate */


int __PHYSFS_platformMkDir(const char *path)
{
    const nn::Result rc = nn::fs::CreateDirectory(path);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
} /* __PHYSFS_platformMkDir */


typedef struct
{
    nn::fs::FileHandle handle;
    int64_t offset;
    PHYSFS_uint8 readahead[8 * 1024];
    size_t readahead_pos;
    size_t readahead_len;
} File;

static File *doOpen(const char *filename, int mode)
{
    nn::Result rc;
    int created = 0;
    if (mode & nn::fs::OpenMode_Write)
    {
        nn::fs::DirectoryEntryType t;
        rc = nn::fs::GetEntryType(&t, filename);
        if (nn::fs::ResultPathNotFound().Includes(rc))
        {
            rc = nn::fs::CreateFile(filename, 0);
            BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, NULL);
            created = 1;
        } /* if */
    } /* if */

    File *retval = (File *) allocator.Malloc(sizeof (File));
    if (!retval)
    {
        if (created)
            nn::fs::DeleteFile(filename);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    memset(retval, '\0', sizeof (*retval));

    rc = nn::fs::OpenFile(&retval->handle, filename, mode);
    if (rc.IsFailure())
    {
        if (created)
            nn::fs::DeleteFile(filename);
        allocator.Free(retval);
        BAIL(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    return retval;
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, nn::fs::OpenMode_Read);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    File *retval = doOpen(filename, nn::fs::OpenMode_Write | nn::fs::OpenMode_AllowAppend);
    BAIL_IF_ERRPASS(!retval, NULL);

    nn::fs::FileHandle &fh = retval->handle;
    const nn::Result rc = nn::fs::SetFileSize(fh, 0);
    if (rc.IsFailure())
    {
        nn::fs::CloseFile(fh);
        allocator.Free(retval);
        BAIL(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    return retval;
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    File *retval = doOpen(filename, nn::fs::OpenMode_Write | nn::fs::OpenMode_AllowAppend);
    BAIL_IF_ERRPASS(!retval, NULL);
    nn::fs::FileHandle &fh = retval->handle;
    nn::Result rc = nn::fs::GetFileSize(&retval->offset, fh);
    if (rc.IsFailure())
    {
        nn::fs::CloseFile(fh);
        allocator.Free(retval);
        BAIL(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    return retval;
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    BAIL_IF(!__PHYSFS_ui64FitsAddressSpace(len), PHYSFS_ERR_INVALID_ARGUMENT, -1);
    File *f = (File *) opaque;
    size_t br = 0;

    while (len > 0)
    {
        size_t cpy = f->readahead_len;

        if (f->readahead_len > 0)
        {
            if (((size_t)len) < cpy)
                cpy = (size_t)len;

            memcpy(buffer, f->readahead + f->readahead_pos, cpy);
            f->offset += cpy;
            f->readahead_len -= cpy;
            f->readahead_pos += cpy;
            len -= cpy;
            br += cpy;
            buffer = ((PHYSFS_uint8 *)buffer) + cpy;
        }
        else if (len > sizeof(f->readahead))
        {
            const nn::Result rc = nn::fs::ReadFile(&cpy, f->handle, f->offset, buffer, len);
            BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, (br > 0) ? (PHYSFS_sint64)br : -1);
            f->offset += cpy;
            f->readahead_pos = 0;
            f->readahead_len = 0;
            len -= cpy;
            br += cpy;
        }
        else 
        {
            const nn::Result rc = nn::fs::ReadFile(&f->readahead_len, f->handle, f->offset, f->readahead, sizeof(f->readahead));
            BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, (br > 0) ? (PHYSFS_sint64)br : -1);
            f->readahead_pos = 0;
            cpy = f->readahead_len;
            if (!cpy)  /* out of data. */
                return (PHYSFS_sint64)br;
        }
    } /* if */

    return (PHYSFS_sint64) br;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    File *f = (File *) opaque;
    const nn::Result rc = nn::fs::WriteFile(f->handle, f->offset, buffer, (size_t) len, nn::fs::WriteOption::MakeValue(0));
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, -1);
    f->offset += (int64_t) len;
    return (PHYSFS_sint64) len;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    File *f = (File *) opaque;
    const PHYSFS_uint64 start = (PHYSFS_uint64) f->offset;
    if (pos == start)
        return 1;  /* already there, nothing to do. */
    else if (pos > start)
    {
        /* can we save _some_ of our readahead? */
        const PHYSFS_uint64 diff = pos - start;
        if (diff < f->readahead_len)
        {
            f->offset += diff;
            f->readahead_len -= diff;
            f->readahead_pos += diff;
            return 1;
        } /* if */
    } /* else if */

    f->offset = (int64_t) pos;
    f->readahead_len = 0;  /* dump the readahead cache. */
    f->readahead_pos = 0;
    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    File *f = (File *) opaque;
    return f->offset;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    File *f = (File *) opaque;
    int64_t size = 0;
    const nn::Result rc = nn::fs::GetFileSize(&size, f->handle);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, -1);
    return (PHYSFS_sint64) size;
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    File *f = (File *) opaque;
    if (nn::fs::GetFileOpenMode(f->handle) == nn::fs::OpenMode_Write)
        BAIL_IF(nn::fs::FlushFile(f->handle).IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    __PHYSFS_platformFlush(opaque);  /* force a flush to avoid possible asserts in the SDK. */
    File *f = (File *) opaque;
    nn::fs::CloseFile(f->handle);  /* we don't check this. You should have used flush! */
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    nn::fs::DirectoryEntryType enttype;
    nn::Result rc = nn::fs::GetEntryType(&enttype, path);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    if (enttype == nn::fs::DirectoryEntryType_Directory)
        rc = nn::fs::DeleteDirectory(path);
    else
        rc = nn::fs::DeleteFile(path);
    BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
} /* __PHYSFS_platformDelete */


int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    nn::fs::DirectoryEntryType t;
    if ((strcmp(fname, "rom:") == 0) || (strcmp(fname, "save:") == 0))
        t = nn::fs::DirectoryEntryType_Directory;
    else
    {
        nn::Result rc = nn::fs::GetEntryType(&t, fname);
        BAIL_IF(rc.IsFailure(), PHYSFS_ERR_OS_ERROR, 0);
    } /* else */

    if (t == nn::fs::DirectoryEntryType_File)
    {
        void *f = __PHYSFS_platformOpenRead(fname);
        BAIL_IF_ERRPASS(!f, 0);
        const PHYSFS_sint64 len = __PHYSFS_platformFileLength(f);
        __PHYSFS_platformClose(f);
        BAIL_IF_ERRPASS(len < 0, 0);
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = len;
    } /* if */

    else
    {
        assert(t == nn::fs::DirectoryEntryType_Directory);
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    st->modtime = -1;
    st->createtime = -1;
    st->accesstime = -1;
    st->readonly = (strncmp(fname, "save:", 5) != 0);

    return 1;
} /* __PHYSFS_platformStat */


void *__PHYSFS_platformGetThreadID(void)
{
    return nn::os::GetCurrentThread();
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    nn::os::MutexType *mutex = (nn::os::MutexType *) allocator.Malloc(sizeof (nn::os::MutexType));
    BAIL_IF(!mutex, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    nn::os::InitializeMutex(mutex, true, 0);
    return mutex;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    nn::os::MutexType *m = (nn::os::MutexType *) mutex;
    nn::os::FinalizeMutex(m);
    allocator.Free(m);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    nn::os::MutexType *m = (nn::os::MutexType *) mutex;
    nn::os::LockMutex(m);
    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    nn::os::MutexType *m = (nn::os::MutexType *) mutex;
    nn::os::UnlockMutex(m);
} /* __PHYSFS_platformReleaseMutex */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    /* Obviously no CDs on a Switch. */
} /* __PHYSFS_platformDetectAvailableCDs */

#endif  /* PHYSFS_PLATFORM_NINTENDO_SWITCH */

/* end of physfs_platform_nintendoswitch.cpp ... */

