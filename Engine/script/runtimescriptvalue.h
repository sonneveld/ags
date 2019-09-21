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
// Runtime script value struct
//
//=============================================================================
#ifndef __AGS_EE_SCRIPT__RUNTIMESCRIPTVALUE_H
#define __AGS_EE_SCRIPT__RUNTIMESCRIPTVALUE_H

#include "script/script_api.h"

struct ICCStaticObject;
struct StaticArray;
struct ICCDynamicObject;

enum ScriptValueType
{
    kScValUndefined,    // to detect errors
    kScValInteger,      // as strictly 32-bit integer (for integer math)
    kScValFloat,        // as float (for floating point math), 32-bit
    kScValPluginArg,    // an 32-bit value, passed to a script function when called
                        // directly by plugin; is allowed to represent object pointer
    kScValData,         // as a container for randomly sized data (usually array)
    kScValStringLiteral,// as a pointer to literal string (array of chars)
    kScValStaticObject, // as a pointer to static global script object
    kScValStaticArray,  // as a pointer to static global array (of static or dynamic objects)
    kScValDynamicObject,// as a pointer to managed script object
    kScValPluginObject, // as a pointer to object managed by plugin (similar to
                        // kScValDynamicObject, but has backward-compatible limitations)
    kScValStaticFunction,// as a pointer to static function
    kScValPluginFunction,// temporary workaround for plugins (unsafe function ptr)
    kScValObjectFunction,// as a pointer to object member function, gets object pointer as
                        // first parameter

    kScMachineFunctionAddress,  // virtual machine address to function
    kScMachineDataAddress       // virtual machine address to data
};

struct RuntimeScriptValue
{
public:
    RuntimeScriptValue()
    {
        Type        = kScValUndefined;
        IValue      = 0;
        Ptr         = nullptr;
        Size        = 0;
    }

    RuntimeScriptValue(int32_t val)
    {
        Type        = kScValInteger;
        IValue      = val;
        Ptr         = nullptr;
        Size        = 4;
    }

    ScriptValueType Type;
    // The 32-bit value used for integer/float math and for storing
    // variable/element offset relative to object (and array) address
    union
    {
        int32_t     IValue; // access Value as int32 type
        float       FValue;	// access Value as float type
    };
    // Pointer is used for storing... pointers - to objects, arrays,
    // functions and stack entries (other RSV)
    union
    {
        char                *Ptr;   // generic data pointer
        ScriptAPIFunction   *SPfn;  // access ptr as a pointer to Script API Static Function
        ScriptAPIObjectFunction *ObjPfn; // access ptr as a pointer to Script API Object Function
    };
    // TODO: separation to Ptr and MgrPtr is only needed so far as there's
    // a separation between Script*, Dynamic* and game entity classes.
    // Once those classes are merged, it will no longer be needed.
    union
    {
        StaticArray         *StcArr;// static array manager
    };
    // The "real" size of data, either one stored in I/FValue,
    // or the one referenced by Ptr. Used for calculating stack
    // offsets.
    // Original AGS scripts always assumed pointer is 32-bit.
    // Therefore for stored pointers Size is always 4 both for x32
    // and x64 builds, so that the script is interpreted correctly.
    int             Size;

    inline bool IsValid() const
    {
        return Type != kScValUndefined;
    }
    inline bool IsNull() const
    {
        return Ptr == nullptr && IValue == 0;
    }
    
    inline bool GetAsBool() const
    {
        return !IsNull();
    }
    inline char* GetPtrWithOffset() const
    {
        return Ptr + IValue;
    }
    
    inline RuntimeScriptValue &Invalidate()
    {
        Type    = kScValUndefined;
        IValue   = 0;
        Ptr     = nullptr;
        Size    = 0;
        return *this;
    }
    inline RuntimeScriptValue &SetInt32(int32_t val)
    {
        Type    = kScValInteger;
        IValue  = val;
        Ptr     = nullptr;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetFloat(float val)
    {
        Type    = kScValFloat;
        FValue  = val;
        Ptr     = nullptr;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetInt32AsBool(bool val)
    {
        return SetInt32(val ? 1 : 0);
    }
    inline RuntimeScriptValue &SetPluginArgument(int32_t val)
    {
        Type    = kScValPluginArg;
        IValue  = val;
        Ptr     = nullptr;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetData(char *data, int size)
    {
        Type    = kScValData;
        IValue  = 0;
        Ptr     = data;
        Size    = size;
        return *this;
    }
    // TODO: size?
    inline RuntimeScriptValue &SetStringLiteral(const char *str)
    {
        Type    = kScValStringLiteral;
        IValue  = 0;
        Ptr     = const_cast<char *>(str);
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetStaticObject(void *object, ICCStaticObject *manager)
    {
        Type    = kScValStaticObject;
        IValue  = 0;
        Ptr     = (char*)object;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetStaticArray(void *object, StaticArray *manager)
    {
        Type    = kScValStaticArray;
        IValue  = 0;
        Ptr     = (char*)object;
        StcArr  = manager;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetDynamicObject(void *object, ICCDynamicObject *manager)
    {
        Type    = kScValDynamicObject;
        IValue  = 0;
        Ptr     = (char*)object;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetPluginObject(void *object, ICCDynamicObject *manager)
    {
        Type    = kScValPluginObject;
        IValue  = 0;
        Ptr     = (char*)object;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetStaticFunction(ScriptAPIFunction *pfn)
    {
        Type    = kScValStaticFunction;
        IValue  = 0;
        SPfn    = pfn;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetPluginFunction(void *pfn)
    {
        Type    = kScValPluginFunction;
        IValue  = 0;
        Ptr     = (char*)pfn;
        Size    = 4;
        return *this;
    }
    inline RuntimeScriptValue &SetObjectFunction(ScriptAPIObjectFunction *pfn)
    {
        Type    = kScValObjectFunction;
        IValue  = 0;
        ObjPfn  = pfn;
        Size    = 4;
        return *this;
    }

};

#endif // __AGS_EE_SCRIPT__RUNTIMESCRIPTVALUE_H
