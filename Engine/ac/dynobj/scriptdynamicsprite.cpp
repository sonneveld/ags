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

#include "ac/dynobj/scriptdynamicsprite.h"
#include "ac/dynamicsprite.h"

int ScriptDynamicSprite::Dispose(const char *address, bool force) {
    // always dispose
    if ((slot) && (!force))
        free_dynamic_sprite(slot);

    delete this;
    return 1;
}

const char *ScriptDynamicSprite::GetType() {
    return "DynamicSprite";
}

int ScriptDynamicSprite::Serialize(const char *address, char *buffer, int bufsize) {
    StartSerialize(buffer);
    SerializeInt(slot);
    return EndSerialize();
}

void ScriptDynamicSprite::Unserialize(int index, const char *serializedData, int dataSize) {
    StartUnserialize(serializedData, dataSize);
    slot = UnserializeInt();
    ManagedObjectInfo objinfo;
    objinfo.handle = index;
    objinfo.obj_type = kScValDynamicObject;
    objinfo.object_manager =  this;
    objinfo.address =  this;
    objinfo.buffer =  nullptr;
    objinfo.buffer_size = 0;
    ccRegisterUnserializedObject2(objinfo); // NO DATA
}

ScriptDynamicSprite::ScriptDynamicSprite(int theSlot) {
    slot = theSlot;

    ManagedObjectInfo objinfo;
    objinfo.obj_type = kScValDynamicObject;
    objinfo.object_manager = this;
    objinfo.address = this;
    objinfo.buffer = nullptr;
    objinfo.buffer_size = 0;
    ccRegisterManagedObject2(objinfo); // NO DATA
}

ScriptDynamicSprite::ScriptDynamicSprite() {
    slot = 0;
}
