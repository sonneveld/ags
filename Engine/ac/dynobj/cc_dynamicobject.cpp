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
//
// C-Script run-time interpreter (c) 2001 Chris Jones
//
// You must DISABLE OPTIMIZATIONS AND REGISTER VARIABLES in your compiler
// when compiling this, or strange results can happen.
//
// There is a problem with importing functions on 16-bit compilers: the
// script system assumes that all parameters are passed as 4 bytes, which
// ints are not on 16-bit systems. Be sure to define all parameters as longs,
// or join the 21st century and switch to DJGPP or Visual C++.
//
//=============================================================================

//#define DEBUG_MANAGED_OBJECTS

#include <stdlib.h>
#include <string.h>
#include "ac/dynobj/cc_dynamicobject.h"
#include "ac/dynobj/managedobjectpool.h"
#include "debug/out.h"
#include "script/cc_error.h"
#include "script/script_common.h"
#include "util/stream.h"

using namespace AGS::Common;

ICCStringClass *stringClassImpl = nullptr;

// set the class that will be used for dynamic strings
void ccSetStringClassImpl(ICCStringClass *theClass) {
    stringClassImpl = theClass;
}

// register a memory handle for the object and allow script
// pointers to point to it
int32_t ccRegisterManagedObject2(ManagedObjectInfo &info)
{
    pool.AddObject(info);
    assert(info.handle > 0);

    ManagedObjectLog("Register managed object type '%s' handle=%d addr=%08X",
        ((info.object_manager == NULL) ? "(unknown)" : info.object_manager->GetType()), info.handle, info.address);

    return info.handle;
}

// register a de-serialized object
int32_t ccRegisterUnserializedObject2(ManagedObjectInfo &info) 
{
    pool.AddUnserializedObject(info);
    return info.handle;
}

// unregister a particular object
int ccUnRegisterManagedObject(const void *object) {
    return pool.RemoveObject((const char*)object);
}

// remove all registered objects
void ccUnregisterAllObjects() {
    pool.reset();
}

// serialize all objects to disk
void ccSerializeAllObjects(Stream *out) {
    pool.WriteToDisk(out);
}

// un-serialise all objects (will remove all currently registered ones)
int ccUnserializeAllObjects(Stream *in, ICCObjectReader *callback) {
    return pool.ReadFromDisk(in, callback);
}

// dispose the object if RefCount==0
void ccAttemptDisposeObject(int32_t handle) {
    pool.CheckDispose(handle);
}

// translate between object handles and memory addresses
int ccGetObjectInfoFromAddress(ManagedObjectInfo &info, void *address)
{
    if (address == nullptr)
        return -1;

    auto result = pool.GetInfoByAddress(info, address);
    if (result != 0) { return result; }

    ManagedObjectLog("Line %d WritePtr: %08X to %d", currentline, address, info.handle);

    if (info.handle == 0) {
        cc_error("Pointer cast failure: the object being pointed to is not in the managed object pool");
        return -1;
    }

    return 0;
}

int ccGetObjectInfoFromHandle(ManagedObjectInfo &info, int32_t handle)
{
    if (handle == 0) {
        return -1;
    }
    auto result = pool.GetInfoByHandle(info, handle);
    if (result) { return result; }

    ManagedObjectLog("Line %d ReadPtr: %d to %08X", currentline, handle, info.address);

    if (info.address == nullptr) {
        cc_error("Error retrieving pointer: invalid handle %d", handle);
        return -1;
    }
    return 0;
}

int ccAddObjectReference(int32_t handle) {
    if (handle == 0)
        return 0;

    return pool.AddRef(handle);
}

int ccReleaseObjectReference(int32_t handle) {
    if (handle == 0)
        return 0;

    ManagedObjectInfo info;
    if (pool.GetInfoByHandle(info, handle) != 0) {
        cc_error("Error releasing pointer: invalid handle %d", handle);
        return -1;
    }

    return pool.SubRef(handle);
}
