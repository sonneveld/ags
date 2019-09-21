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

#include "ac/dynobj/scriptviewframe.h"

int ScriptViewFrame::Dispose(const char *address, bool force) {
    // always dispose a ViewFrame
    delete this;
    return 1;
}

const char *ScriptViewFrame::GetType() {
    return "ViewFrame";
}

int ScriptViewFrame::Serialize(const char *address, char *buffer, int bufsize) {
    StartSerialize(buffer);
    SerializeInt(view);
    SerializeInt(loop);
    SerializeInt(frame);
    return EndSerialize();
}

void ScriptViewFrame::Unserialize(int index, const char *serializedData, int dataSize) {
    StartUnserialize(serializedData, dataSize);
    view = UnserializeInt();
    loop = UnserializeInt();
    frame = UnserializeInt();
    ManagedObjectInfo objinfo;
    objinfo.handle = index;
    objinfo.obj_type = kScValDynamicObject;
    objinfo.object_manager =  this;
    objinfo.address =  this;
    objinfo.buffer =  nullptr;
    objinfo.buffer_size = 0;
    ccRegisterUnserializedObject2(objinfo); // NO DATA
}

ScriptViewFrame::ScriptViewFrame(int p_view, int p_loop, int p_frame) {
    view = p_view;
    loop = p_loop;
    frame = p_frame;
}

ScriptViewFrame::ScriptViewFrame() {
    view = -1;
    loop = -1;
    frame = -1;
}
