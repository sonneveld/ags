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
#include <vector>
#include <string.h>
#include "ac/dynobj/managedobjectpool.h"
#include "ac/dynobj/cc_dynamicarray.h" // globalDynamicArray, constants
#include "debug/out.h"
#include "util/string_utils.h"               // fputstring, etc
#include "script/cc_error.h"
#include "script/script_common.h"
#include "util/stream.h"
#include "script/cc_instance.h"

using namespace AGS::Common;

const auto OBJECT_CACHE_MAGIC_NUMBER = 0xa30b;
const auto SERIALIZE_BUFFER_SIZE = 10240;
const auto GARBAGE_COLLECTION_INTERVAL = 1024;
const auto RESERVED_SIZE = 2048;

ManagedObjectPool::ManagedObject::ManagedObject() : info(), refCount(0) {}
ManagedObjectPool::ManagedObject::ManagedObject(ManagedObjectInfo &info) : info(info), refCount(0) {}

int ManagedObjectPool::Remove(ManagedObject &o, bool force) {
    if (!o.isUsed()) { return 1; } // already removed

    bool canBeRemovedFromPool = o.info.object_manager->Dispose((const char*)o.info.address, force) != 0;
    if (!(canBeRemovedFromPool || force)) { return 0; }

    auto handle = o.info.handle;
    available_ids.push(o.info.handle);

    handleByAddress.erase((const char*)o.info.address);
    o = ManagedObject();

    coreExecutor.RemoveMemoryWindow(o.info.buffer, o.info.buffer_size);

    ManagedObjectLog("Line %d Disposed managed object handle=%d", currentline, handle);

    return 1;
}

int32_t ManagedObjectPool::AddRef(int32_t handle) {
    if (handle < 0 || (size_t)handle >= objects.size()) { return 0; }
    auto & o = objects[handle];
    if (!o.isUsed()) { return 0; }

    o.refCount += 1;
    ManagedObjectLog("Line %d AddRef: handle=%d new refcount=%d", currentline, o.info.handle, o.refCount);
    return o.refCount;
}

int ManagedObjectPool::CheckDispose(int32_t handle) {
    if (handle < 0 || (size_t)handle >= objects.size()) { return 1; }
    auto & o = objects[handle];
    if (!o.isUsed()) { return 1; }
    if (o.refCount >= 1) { return 0; }
    return Remove(o);
}

int32_t ManagedObjectPool::SubRef(int32_t handle) {
    if (handle < 0 || (size_t)handle >= objects.size()) { return 0; }
    auto & o = objects[handle];
    if (!o.isUsed()) { return 0; }

    o.refCount--;
    auto newRefCount = o.refCount;
    auto canBeDisposed = (o.info.address != disableDisposeForObject);
    if (canBeDisposed) {
        CheckDispose(handle);
    }
    // object could be removed at this point, don't use any values.
    ManagedObjectLog("Line %d SubRef: handle=%d new refcount=%d canBeDisposed=%d", currentline, handle, newRefCount, canBeDisposed);
    return newRefCount;
}

int ManagedObjectPool::GetInfoByHandle(ManagedObjectInfo &info, int32_t handle) 
{
    if (handle < 0 || (size_t)handle >= objects.size()) { return -1; }

    auto & o = objects[handle];
    if (!o.isUsed()) { return -1; }
    info = o.info;

    return 0;
}
int ManagedObjectPool::GetInfoByAddress(ManagedObjectInfo &info, void *address)
{
    if (address == nullptr) { return -1; }
    auto it = handleByAddress.find((const char*)address);
    if (it == handleByAddress.end()) { return -1; }

    auto &o = objects[it->second];
    if (!o.isUsed()) { return -1; }
    info = o.info;

    return 0;
}

int ManagedObjectPool::RemoveObject(const char *address) {
    if (address == nullptr) { return 0; }
    auto it = handleByAddress.find(address);
    if (it == handleByAddress.end()) { return 0; }

    auto & o = objects[it->second];
    return Remove(o, true);
}

void ManagedObjectPool::RunGarbageCollectionIfAppropriate()
{
    if (objectCreationCounter <= GARBAGE_COLLECTION_INTERVAL) { return; }
    RunGarbageCollection();
    objectCreationCounter = 0;
}

void ManagedObjectPool::RunGarbageCollection()
{
    for (int i = 1; i < nextHandle; i++) {
        auto & o = objects[i];
        if (!o.isUsed()) { continue; }
        if (o.refCount < 1) {
            Remove(o);
        }
    }
    ManagedObjectLog("Ran garbage collection");
}

int ManagedObjectPool::AddObject(ManagedObjectInfo &info) 
{
    int32_t handle;

    if (!available_ids.empty()) {
        handle = available_ids.front();
        available_ids.pop();
    } else {
        handle = nextHandle++;
        if ((size_t)handle >= objects.size()) {
           objects.resize(objects.size() * 2, ManagedObject());
        }
    }

    auto & o = objects[handle];
    if (o.isUsed()) { cc_error("used: %d", handle); return 0; }

    info.handle = handle;

    o = ManagedObject(info);

    handleByAddress.insert({(const char *)o.info.address, o.info.handle});
    objectCreationCounter++;

    // printf("memwindow adding obj\n");
    coreExecutor.AddMemoryWindow(o.info.buffer, o.info.buffer_size, false);

    ManagedObjectLog("Allocated managed object handle=%d, type=%s", o.info.handle, o.info.object_manager->GetType());
    return o.info.handle;
}


int ManagedObjectPool::AddUnserializedObject(ManagedObjectInfo &info) 
{
    if (info.handle < 0) { cc_error("Attempt to assign invalid handle: %d", info.handle); return 0; }
    if ((size_t)info.handle >= objects.size()) {
        objects.resize(objects.size() * 2, ManagedObject());
    }

    auto & o = objects[info.handle];
    if (o.isUsed()) { cc_error("bad save. used: %d", o.info.handle); return 0; }

    o = ManagedObject(info);

    handleByAddress.insert({(const char*)info.address, o.info.handle});

    // printf("memwindow adding obj\n");
    coreExecutor.AddMemoryWindow(o.info.buffer, o.info.buffer_size, false);

    ManagedObjectLog("Allocated unserialized managed object handle=%d, type=%s", o.info.handle, o.info.object_manager->GetType());
    return o.info.handle;
}

void ManagedObjectPool::WriteToDisk(Stream *out) {

    // use this opportunity to clean up any non-referenced pointers
    RunGarbageCollection();

    std::vector<char> serializeBuffer;
    serializeBuffer.resize(SERIALIZE_BUFFER_SIZE);

    out->WriteInt32(OBJECT_CACHE_MAGIC_NUMBER);
    out->WriteInt32(2);  // version

    int size = 0;
    for (int i = 1; i < nextHandle; i++) {
        auto const & o = objects[i];
        if (o.isUsed()) { 
            size += 1;
        }
    }
    out->WriteInt32(size);

    for (int i = 1; i < nextHandle; i++) {
        auto const & o = objects[i];
        if (!o.isUsed()) { continue; }

        // handle
        out->WriteInt32(o.info.handle);
        // write the type of the object
        StrUtil::WriteCStr((char*)o.info.object_manager->GetType(), out);
        // now write the object data
        int bytesWritten = o.info.object_manager->Serialize((const char*)o.info.address, &serializeBuffer.front(), serializeBuffer.size());
        if ((bytesWritten < 0) && ((size_t)(-bytesWritten) > serializeBuffer.size()))
        {
            // buffer not big enough, re-allocate with requested size
            serializeBuffer.resize(-bytesWritten);
            bytesWritten = o.info.object_manager->Serialize((const char*)o.info.address, &serializeBuffer.front(), serializeBuffer.size());
        }
        assert(bytesWritten >= 0);
        out->WriteInt32(bytesWritten);
        out->Write(&serializeBuffer.front(), bytesWritten);
        out->WriteInt32(o.refCount);

        ManagedObjectLog("Wrote handle = %d", o.info.handle);
    }
}

int ManagedObjectPool::ReadFromDisk(Stream *in, ICCObjectReader *reader) {
    if (in->ReadInt32() != OBJECT_CACHE_MAGIC_NUMBER) {
        cc_error("Data was not written by ccSeralize");
        return -1;
    }

    char typeNameBuffer[200];
    std::vector<char> serializeBuffer;
    serializeBuffer.resize(SERIALIZE_BUFFER_SIZE);

    auto version = in->ReadInt32();

    switch (version) {
        case 1:
            {
                // IMPORTANT: numObjs is "nextHandleId", which is why we iterate from 1 to numObjs-1
                int numObjs = in->ReadInt32();
                for (int i = 1; i < numObjs; i++) {
                    StrUtil::ReadCStr(typeNameBuffer, in, sizeof(typeNameBuffer));
                    if (typeNameBuffer[0] != 0) {
                        size_t numBytes = in->ReadInt32();
                        if (numBytes > serializeBuffer.size()) {
                            serializeBuffer.resize(numBytes);
                        }
                        in->Read(&serializeBuffer.front(), numBytes);
                        if (strcmp(typeNameBuffer, CC_DYNAMIC_ARRAY_TYPE_NAME) == 0) {
                            globalDynamicArray.Unserialize(i, &serializeBuffer.front(), numBytes);
                        } else {
                            reader->Unserialize(i, typeNameBuffer, &serializeBuffer.front(), numBytes);
                        }
                        objects[i].refCount = in->ReadInt32();
                        ManagedObjectLog("Read handle = %d", objects[i].info.handle);
                    }
                }
            }
            break;
        case 2:
            {
                // This is actually number of objects written.
                int objectsSize = in->ReadInt32();
                for (int i = 0; i < objectsSize; i++) {
                    auto handle = in->ReadInt32();
                    assert (handle >= 1);
                    StrUtil::ReadCStr(typeNameBuffer, in, sizeof(typeNameBuffer));
                    assert (typeNameBuffer[0] != 0);
                    size_t numBytes = in->ReadInt32();
                    if (numBytes > serializeBuffer.size()) {
                        serializeBuffer.resize(numBytes);
                    }
                    in->Read(&serializeBuffer.front(), numBytes);
                    if (strcmp(typeNameBuffer, CC_DYNAMIC_ARRAY_TYPE_NAME) == 0) {
                        globalDynamicArray.Unserialize(handle, &serializeBuffer.front(), numBytes);
                    } else {
                        reader->Unserialize(handle, typeNameBuffer, &serializeBuffer.front(), numBytes);
                    }
                    objects[handle].refCount = in->ReadInt32();
                    ManagedObjectLog("Read handle = %d", objects[i].info.handle);
                }
            }
            break;
        default:
            cc_error("Invalid data version: %d", version);
            return -1;
    }

    // re-adjust next handles. (in case saved in random order)
    while (!available_ids.empty()) { available_ids.pop(); }
    nextHandle = 1;

    for (const auto &o : objects) {
        if (o.isUsed()) { 
            nextHandle = o.info.handle + 1;
        }
    }
    for (int i = 1; i < nextHandle; i++) {
        if (!objects[i].isUsed()) {
            available_ids.push(i);
        }
    }

    return 0;
}

// de-allocate all objects
void ManagedObjectPool::reset() {
    for (int i = 1; i < nextHandle; i++) {
        auto & o = objects[i];
        if (!o.isUsed()) { continue; }
        Remove(o, true);
    }
    while (!available_ids.empty()) { available_ids.pop(); }
    nextHandle = 1;
}

ManagedObjectPool::ManagedObjectPool() : objectCreationCounter(0), nextHandle(1), available_ids(), objects(RESERVED_SIZE, ManagedObject()), handleByAddress() {
    handleByAddress.reserve(RESERVED_SIZE);
}

ManagedObjectPool pool;
