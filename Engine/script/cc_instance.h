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
// 'C'-style script interpreter
//
//=============================================================================

#ifndef __CC_INSTANCE_H
#define __CC_INSTANCE_H

#include <memory>
#include <unordered_map>

#include "script/script_common.h"
#include "script/cc_script.h"  // ccScript
#include "script/nonblockingscriptfunction.h"
#include "util/string.h"

using namespace AGS;

#define INSTF_SHAREDATA     1
#define INSTF_ABORTED       2
#define INSTF_FREE          4
#define INSTF_RUNNING       8   // set by main code to confirm script isn't stuck
#define CC_STACK_SIZE       250
#define CC_STACK_DATA_SIZE  (1000 * sizeof(int32_t))
#define MAX_CALL_STACK      100

// 256 because we use 8 bits to hold instance number
#define MAX_LOADED_INSTANCES 256

#define INSTANCE_ID_SHIFT 24LL
#define INSTANCE_ID_MASK  0x00000000000000ffLL
#define INSTANCE_ID_REMOVEMASK 0x0000000000ffffffLL

struct ccInstance;
struct ScriptImport;

struct ScriptVariable
{
    ScriptVariable()
    {
        ScAddress   = -1; // address = 0 is valid one, -1 means undefined
    }

    int32_t             ScAddress;  // original 32-bit relative data address, written in compiled script;
                                    // if we are to use Map or HashMap, this could be used as Key
    RuntimeScriptValue  RValue;
};

struct ScriptPosition
{
    ScriptPosition()
        : Line(0)
    {
    }

    ScriptPosition(const Common::String &section, int32_t line)
        : Section(section)
        , Line(line)
    {
    }

    Common::String  Section;
    int32_t         Line;
};

struct ccInstance
{
public:
    virtual ~ccInstance() = 0;

    // Create a runnable instance of the same script, sharing global memory
    virtual ccInstance *Fork() = 0;

    virtual void OverrideGlobalData(const char *data, int size) = 0;
    virtual void GetGlobalData(const char *&data, int &size) = 0;

    // Specifies that when the current function returns to the script, it
    // will stop and return from CallInstance
    virtual void    Abort() = 0;
    // Aborts instance, then frees the memory later when it is done with
    virtual void    AbortAndDestroy() = 0;
    
    // Call an exported function in the script
    virtual int     CallScriptFunction(const char *funcname, int32_t num_params, const RuntimeScriptValue *params) = 0;

    virtual int GetReturnValue() = 0;

    // Get the address of an exported symbol (function or variable) in the script
    // NOTE: only used to check for existance?
    virtual RuntimeScriptValue GetSymbolAddress(const char *symname) = 0;
    
    // Tells whether this instance is in the process of executing the byte-code
    virtual bool    IsBeingRun() const = 0;
};

// returns the currently executing instance, or NULL if none
ccInstance *ccInstanceGetCurrentInstance(void);
// create a runnable instance of the supplied script
ccInstance *ccInstanceCreateFromScript(PScript script);
ccInstance *ccInstanceCreateEx(PScript scri, ccInstance * joined);

#endif // __CC_INSTANCE_H
