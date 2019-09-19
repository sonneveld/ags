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
#include "ac/dynobj/scriptset.h"

int ScriptSetBase::Dispose(const char *address, bool force)
{
    Clear();
    delete this;
    return 1;
}

const char *ScriptSetBase::GetType()
{
    return "StringSet";
}

int ScriptSetBase::Serialize(const char *address, char *buffer, int bufsize)
{
    size_t total_sz = CalcSerializeSize();
    if (bufsize < 0 || total_sz > (size_t)bufsize)
    {
        // buffer not big enough, ask for a bigger one
        return -((int)total_sz);
    }
    StartSerialize(buffer);
    SerializeInt(IsSorted());
    SerializeInt(IsCaseSensitive());
    SerializeContainer();
    return EndSerialize();
}

void ScriptSetBase::Unserialize(int index, const char *serializedData, int dataSize)
{
    // NOTE: we expect sorted/case flags are read by external reader;
    // this is awkward, but I did not find better design solution atm
    StartUnserialize(serializedData, dataSize);
    UnserializeContainer(serializedData);
    ManagedObjectInfo objinfo;
    objinfo.handle = index;
    objinfo.obj_type = kScValDynamicObject;
    objinfo.object_manager =  this;
    objinfo.address =  this;
    objinfo.buffer =  this;
    objinfo.buffer_size = sizeof(ScriptSetBase);
    ccRegisterUnserializedObject2(objinfo);
}
