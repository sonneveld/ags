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

#ifndef __CC_MANAGEDOBJECTPOOL_H
#define __CC_MANAGEDOBJECTPOOL_H

#include <vector>
#include <queue>
#include <unordered_map>

#include "ac/dynobj/cc_dynamicobject.h"   // ICCDynamicObject

namespace AGS { namespace Common { class Stream; }}
using namespace AGS; // FIXME later

struct ManagedObjectPool final {
private:
    // TODO: find out if we can make handle size_t
    struct ManagedObject {

        ManagedObjectInfo info;

        int refCount;

        bool isUsed() const { return info.obj_type != kScValUndefined; }

        ManagedObject();
        ManagedObject(ManagedObjectInfo &info);
    };

    int objectCreationCounter;  // used to do garbage collection every so often

    int32_t nextHandle {}; // TODO: manage nextHandle's going over INT32_MAX !
    std::queue<int32_t> available_ids;
    std::vector<ManagedObject> objects;
    std::unordered_map<const char *, int32_t> handleByAddress;

    void Init(int32_t theHandle, const char *theAddress, ICCDynamicObject *theCallback, ScriptValueType objType);
    int Remove(ManagedObject &o, bool force = false); 

    void RunGarbageCollection();

public:

    int32_t AddRef(int32_t handle);
    int CheckDispose(int32_t handle);
    int32_t SubRef(int32_t handle);
    int GetInfoByHandle(ManagedObjectInfo &info, int32_t handle);
    int GetInfoByAddress(ManagedObjectInfo &info, void *address);
    int RemoveObject(const char *address);
    void RunGarbageCollectionIfAppropriate();
    int AddObject(ManagedObjectInfo &info);
    int AddUnserializedObject(ManagedObjectInfo &info);
    void WriteToDisk(Common::Stream *out);
    int ReadFromDisk(Common::Stream *in, ICCObjectReader *reader);
    void reset();
    ManagedObjectPool();

    const char* disableDisposeForObject {nullptr};
};

extern ManagedObjectPool pool;

#ifdef DEBUG_MANAGED_OBJECTS
#define ManagedObjectLog(...) Debug::Printf(kDbgGroup_ManObj, kDbgMsg_Debug, __VA_ARGS__)
#else
#define ManagedObjectLog(...)
#endif

#endif // __CC_MANAGEDOBJECTPOOL_H
