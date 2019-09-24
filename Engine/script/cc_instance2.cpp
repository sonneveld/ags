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

#include "script/cc_instance.h"

#include <stdint.h>
#include <list>
// #include <string.h>
// #include "ac/common.h"
#include "ac/dynobj/cc_dynamicarray.h"
#include "ac/dynobj/cc_dynamicobject.h"
#include "ac/dynobj/managedobjectpool.h"
// #include "gui/guidefines.h"
#include "script/cc_error.h"
// #include "debug/debug_log.h"
// #include "debug/out.h"
#include "script/cc_options.h"
// #include "script/script.h"
#include "script/script_runtime.h"
#include "script/systemimports.h"
// #include "util/bbop.h"
// #include "util/stream.h"
// #include "util/misc.h"
// #include "util/textstreamwriter.h"
// #include "ac/dynobj/scriptstring.h"
#include "ac/dynobj/scriptuserobject.h"
// #include "ac/statobj/agsstaticobject.h"
// #include "ac/statobj/staticarray.h"
// #include "util/memory.h"
// #include "util/string_utils.h" // linux strnicmp definition

#include "tinyalloc.h"


/*

TODO

instruction logging like dosbox/debug
https://github.com/svn2github/dosbox/blob/master/src/debug/debug.cpp#L2522
06571FBF    2E8F06A4 1F11111111 2E8F06A4 2E8F06A4    POPREG    pop ax    
AX:%08x BX:%08x CX:%08x DX:%08x MAR:%08x OP:%08x SP:%08x (line= base=  callobj=0/1 callargs=xx )
registers= flags= otherstate= MAR=0x2344,handle(if exists)


reuse mapped memory addresses


SAVE all registers when calling an external C function, in case it calls more scripts


string cache, save memory
- be careful when unloading rooms though!

look up if function needs object..

Sc_String_Format does some weird things with parameters

replace cc_error with exceptions?

support notify_alive and infinite loop count checks

*/

const auto CC_EXP_STACK_SIZE = 8192;

// #define DEBUG_MACHINE

// [IKM] 2012-10-21:
// NOTE: This is temporary solution (*sigh*, one of many) which allows certain
// exported functions return value as a RuntimeScriptValue object;
// Of 2012-12-20: now used only for plugin exports
RuntimeScriptValue GlobalReturnValue;



ccInstance::~ccInstance() { }



bool tiny_heap_init = false;
void *tiny_heap_base = nullptr;
void *tiny_heap_limit = nullptr;

int alloc_count = 0;

void init_tiny_heap()
{
    if (tiny_heap_init) { return; }

    tiny_heap_base = malloc(64*1024*1024);
    assert(tiny_heap_base);
    tiny_heap_limit = (char*)tiny_heap_base + 64*1024*1024;

    auto initok = ta_init(tiny_heap_base, tiny_heap_limit, 64*1024, 16, 16);
    assert(initok);

    auto checkok = ta_check();
    assert(checkok);

    tiny_heap_init = true;

    printf("tiny alloc init! base=%p limit=%p\n", tiny_heap_base, tiny_heap_limit);
}


void *tiny_alloc(size_t n) 
{
    init_tiny_heap();

    auto result = ta_alloc(n);
    // auto result = ta_calloc(1, n);
    if (!result) {
        printf("%p: alloc %zd bytes - total items %d\n",result, n, alloc_count);
        assert(result);
    }

    // auto checkok = ta_check();
    // assert(checkok);

    alloc_count ++;
    // printf("%p: alloc %d bytes - total items %d\n",result, n, alloc_count);

    return result;
}

void tiny_free(void *p)
{
    if (!p) { return; }
    init_tiny_heap();

    ta_free(p);
    alloc_count --;
}







using reg_t = uint32_t;
const char *regnames[] = { "null", "sp", "mar", "ax", "bx", "cx", "op", "dx" };


// #define FIXUP_GLOBALDATA  1     // code[fixup] += &globaldata[0]
// #define FIXUP_FUNCTION    2     // code[fixup] += &code[0]
// #define FIXUP_STRING      3     // code[fixup] += &strings[0]
// #define FIXUP_IMPORT      4     // code[fixup] = &imported_thing[code[fixup]]
// #define FIXUP_DATADATA    5     // globaldata[fixup] += &globaldata[0]
// #define FIXUP_STACK       6     // code[fixup] += &stack[0]

struct ccFixup2
{
    int type;
    int value;
};


struct ccExport2
{
    AGS::Common::String name;
    int type;
    uint32_t vaddr;
};

struct ccSection2
{
    AGS::Common::String name;
    int offset;
};


struct ccScript2
{
    std::vector<char> globaldata;
    std::vector<intptr_t> code;
    std::vector<char> strings;
    std::vector<ccFixup2> fixups;
    std::vector<ccImport2> imports;
    std::vector<ccExport2> exports;
    std::vector<ccSection2> sections;
};






// Running instance of the script
struct ccInstanceExperiment : public ccInstance
{
public:
    ccInstanceExperiment();
    ~ccInstanceExperiment()  override;

    // for manipulating the global data.
    void OverrideGlobalData(std::vector<char> data) override;
    std::vector<char> GetGlobalData() override;

    // Create a runnable instance of the same script, sharing global memory
    ccInstance *Fork()  override;

    // Specifies that when the current function returns to the script, it
    // will stop and return from CallInstance
    void    Abort() override;
    // Aborts instance, then frees the memory later when it is done with
    void    AbortAndDestroy() override;
    
    // Get the address of an exported symbol (function or variable) in the script
    RuntimeScriptValue GetSymbolAddress(const char *symname) override;

    PScript instanceof;

    char *globaldata;
    size_t globaldata_size;
    char *code;
    char *strings;

    uint32_t globaldata_vaddr;
    uint32_t code_vaddr;
    uint32_t strings_vaddr;


    // std::vector<ccImport2> imports;
    std::vector<ccExport2> exports;

};


ccInstanceExperiment *current_instance = nullptr; 



ccInstanceExperiment::ccInstanceExperiment() {}
ccInstanceExperiment::~ccInstanceExperiment()  {}

// for manipulating the global data.
void ccInstanceExperiment::OverrideGlobalData(std::vector<char> data) 
{
    auto &script = *this->instanceof;

    assert(data.size() == this->globaldata_size);
    memcpy(this->globaldata, data.data(), data.size());

    for (int i = 0; i < script.numfixups; i++) {
        long fixup = script.fixups[i];
        switch (script.fixuptypes[i]) {
            case FIXUP_DATADATA:
            {
                printf("FIXUP\n");
                auto p = reinterpret_cast<uint32_t *>(this->globaldata + fixup);
                *p += this->globaldata_vaddr;
            }
            default:
                break;
        }
    }
}


std::vector<char> ccInstanceExperiment::GetGlobalData() 
{ 
    auto &script = *this->instanceof;

    auto result = std::vector<char>(this->globaldata, this->globaldata+this->globaldata_size);

    for (int i = 0; i < script.numfixups; i++) {
        long fixup = script.fixups[i];
        switch (script.fixuptypes[i]) {
            case FIXUP_DATADATA:
            {
                printf("UNFIXUP\n");

                auto p = reinterpret_cast<uint32_t *>(result.data() + fixup);
                *p -= this->globaldata_vaddr;
            }
            default:
                break;
        }
    }

    return result;
}

// Create a runnable instance of the same script, sharing global memory
ccInstance *ccInstanceExperiment::Fork()  
{ 
    assert(0);
    // return ccInstanceExperimentCreate(this->instanceof, this);
    return nullptr;
}

// Specifies that when the current function returns to the script, it
// will stop and return from CallInstance
void    ccInstanceExperiment::Abort() {
    assert(0);
}
// Aborts instance, then frees the memory later when it is done with
void    ccInstanceExperiment::AbortAndDestroy() {
    assert(0);
}



// We assume that these addresses are references to managed objects
// with NO body
struct RealMemoryAddressInfo
{
    void *addr;
    uint32_t vaddr;
    int use_count;
};

std::list<RealMemoryAddressInfo> address_maps {};
using map_interator_t = std::list<RealMemoryAddressInfo>::iterator;
std::unordered_map<void *, map_interator_t> map_by_real_addr {};
std::unordered_map<uint32_t, map_interator_t> map_by_vaddr {};
uint32_t next_map_addr = 0x20000000;

char err_buff[1000];

uint32_t ccExecutor::AddRealMemoryAddress(const void *addr)
{
    if (addr == nullptr) { return 0; }

    // if (addr & 0xC0000000 == 0)
    if (addr >= tiny_heap_base && addr < tiny_heap_limit)
    {
        // don't track if in tiny heap
        return (char*)addr - (char*)tiny_heap_base;
    }

    auto fit = map_by_real_addr.find((void*)addr);
    if (fit != map_by_real_addr.end()) {
        auto it = fit->second;
        it->use_count++;
        return it->vaddr;
    }

    auto vaddr = next_map_addr;
    next_map_addr += 64;

    address_maps.push_back({(void*)addr, vaddr, 1});
    auto it = address_maps.end();
    it--;
    map_by_real_addr[(void*)addr] = it;
    map_by_vaddr[vaddr] = it;

    // printf("mapped count: %d\n", address_maps.size());
    return vaddr;
}

void ccExecutor::RemoveRealMemoryAddress(const void *addr)
{
    if (addr == nullptr) { return; }

    if (addr >= tiny_heap_base && addr < tiny_heap_limit) {
        // don't track if in tiny heap
        return;
    }

    auto fit = map_by_real_addr.find((void*)addr);
    if (fit == map_by_real_addr.end()) { return; }

    auto it = fit->second;
    it->use_count --;
    if (it->use_count <= 0) {
        map_by_real_addr.erase(it->addr);
        map_by_vaddr.erase(it->vaddr);
        address_maps.erase(it);
    }

}

inline void *ToRealMemoryAddress(uint32_t vaddr)
{
    if (vaddr == 0) { return nullptr; }

    // mapped addresses start from 0x20000000
    if ((vaddr & 0xF0000000) == 0)
    // if (vaddr < ((char*)tiny_heap_limit - (char*)tiny_heap_base)) 
    {
        return (char*)tiny_heap_base + vaddr;
    }

    auto fit = map_by_vaddr.find(vaddr);
    if (fit == map_by_vaddr.end()) { 
        // sprintf(err_buff, "could not find virtual addr %08x", vaddr);
        // throw std::runtime_error(err_buff);
        return nullptr;
    }

    auto it = fit->second;

    return it->addr;

}

// as an optimisation, this only returns addresses within heap (code, stack, strings, objects with data)
inline void *ToRealMemoryAddressFromHeap(uint32_t vaddr)
{
    if (vaddr == 0) { return nullptr; }
    assert ((vaddr & 0xF0000000) == 0);
    // mapped addresses start from 0x20000000
    return (char*)tiny_heap_base + vaddr;
}

inline uint32_t ToVirtualAddress(const void *addr)
{
    if (addr == nullptr) { return 0; }

    if (addr >= tiny_heap_base && addr < tiny_heap_limit) {
        // don't track if in tiny heap
        return (char*)addr - (char*)tiny_heap_base;
    }

    auto fit = map_by_real_addr.find((void*)addr);
    if (fit == map_by_real_addr.end()) { 
        sprintf(err_buff, "could not find read addr %p", addr);
        throw std::runtime_error(err_buff);
    }

    auto it = fit->second;

    return it->vaddr;
}

inline uint32_t ToVirtualAddressFromHeap(const void *addr)
{
    if (addr == nullptr) { return 0; }
    assert (addr >= tiny_heap_base && addr < tiny_heap_limit);
    // don't track if in tiny heap
    return (char*)addr - (char*)tiny_heap_base;
}

ccExecutor::ccExecutor() 
{
    // reset registers
    std::fill(registers, registers+CC_NUM_REGISTERS, 0);

    this->pc = 0;

    // this->stack.resize(1024*8);
    this->stack = (char*)tiny_alloc(8*1024);

    // this->stacktop = AddMemoryWindow(stack, 8*1024, false);
    this->stacktop = ToVirtualAddressFromHeap(this->stack);
    registers[SREG_SP] = this->stacktop;

}


ccInstance *ccExecutor::LoadScript(PScript scri)
{

    // allocate and copy all the memory with data, code and strings across
    auto cinst = new ccInstanceExperiment();
    
    cinst->instanceof = scri;

    // create own memory space
    // cinst->globaldata.insert(cinst->globaldata.end(), scri->globaldata, scri->globaldata+scri->globaldatasize);
    cinst->globaldata_size = scri->globaldatasize;
    cinst->globaldata = (char*)tiny_alloc(scri->globaldatasize);
    memcpy(cinst->globaldata, scri->globaldata, scri->globaldatasize);
    // for (int i = 0; i < 16*1024; i++) {
    //     cinst->globaldata.push_back(0);
    // }
    // assert(cinst->globaldata.size() == scri->globaldatasize);
    // cinst->code.resize(scri->codesize * 4);
    cinst->code = (char*)tiny_alloc(scri->codesize * 4);
    // memcpy(cinst->code, scri->globaldata, scri->codesize);
    auto cptr = reinterpret_cast<uint32_t *>(cinst->code);
    for (int i = 0; i < scri->codesize; i++) {
        *(cptr++) = (intptr_t)scri->code[i];
    }

    // TODO
    // just use the pointer to the strings since they don't change
    // cinst->strings.insert(cinst->strings.end(), scri->strings, scri->strings+scri->stringssize);
    cinst->strings = (char*)tiny_alloc(scri->stringssize);
    memcpy(cinst->strings, scri->strings, scri->stringssize);

    // cinst->globaldata_vaddr = AddMemoryWindow(cinst->globaldata.data(), cinst->globaldata.size(), false);
    cinst->globaldata_vaddr = ToVirtualAddressFromHeap(cinst->globaldata);
    // cinst->code_vaddr = AddMemoryWindow(cinst->code.data(), cinst->code.size(), false);
    cinst->code_vaddr = ToVirtualAddressFromHeap(cinst->code);
    // cinst->strings_vaddr = AddMemoryWindow(cinst->strings.data(), cinst->strings.size(), false);
    cinst->strings_vaddr = ToVirtualAddressFromHeap(cinst->strings);


#ifdef DEBUG_MACHINE
    printf("global:%08x to %08x  code:%08x strings:%08x\n", cinst->globaldata_vaddr, cinst->globaldata_vaddr+scri->globaldatasize, cinst->code_vaddr, cinst->strings_vaddr);
#endif

    // used by CALLAS for other instances. I think assumes load order.
#if 0
  // find a LoadedInstance slot for it
  for (i = 0; i < MAX_LOADED_INSTANCES; i++) {
    if (loadedInstances[i] == NULL) {
      loadedInstances[i] = cinst;
      cinst->loadedInstanceId = i;
      break;
    }
    if (i == MAX_LOADED_INSTANCES - 1) {
      cc_error("too many active instances");
      ccFreeInstance(cinst);
      return NULL;
    }
  }
#endif

  // find the real address of all the imports
    // cinst->imports.reserve(scri->numimports);

    // for (int i = 0; i < scri->numimports; i++) 
    // {
    //     if (scri->imports[i] == nullptr || strlen(scri->imports[i]) == 0) { continue; }
    //     imports.push_back( {scri->imports[i], 0});
    // }

    cptr = reinterpret_cast<uint32_t *>(cinst->code);

    for (int i = 0; i < scri->numfixups; i++) {
    long fixup = scri->fixups[i];
    switch (scri->fixuptypes[i]) {
        case FIXUP_GLOBALDATA:
#ifdef DEBUG_MACHINE
            printf("FIXUP_GLOBALDATA %x : ", cinst->code_vaddr + fixup*4);
#endif
            cptr[fixup] += cinst->globaldata_vaddr;
            break;
        case FIXUP_FUNCTION:
#ifdef DEBUG_MACHINE
            printf("FIXUP_FUNCTION %x : ", cinst->code_vaddr + fixup*4);
#endif
            //cptr[fixup] += cinst->code_vaddr;
            break;
        case FIXUP_STRING:
#ifdef DEBUG_MACHINE
            printf("FIXUP_STRING %x : ", cinst->code_vaddr + fixup*4);
#endif
            cptr[fixup] += cinst->strings_vaddr;
            break;
        case FIXUP_IMPORT:
        {
#ifdef DEBUG_MACHINE
            printf("FIXUP_IMPORT %x : ", cinst->code_vaddr + fixup*4);
#endif

            auto import_index = cptr[fixup];
            auto import_name = scri->imports[import_index];
            auto system_index = simp.get_index_of(import_name);
            auto sym = simp.getByIndex(system_index);

            switch(sym->Value.Type) {
                case kScValData:
                case kScValStringLiteral:
                case kScValStaticObject:
                case kScValStaticArray:
                case kScValDynamicObject:
                case kScValPluginObject:
                {
#ifdef DEBUG_MACHINE
                    printf("MEM %s : ", import_name);
#endif
                    // auto vaddr = AddMemoryWindow(sym->Value.Ptr, 1024*1024, false);
                    assert(sym->Value.Ptr != nullptr);
                    cptr[fixup] = ToVirtualAddress(sym->Value.Ptr);
                    break;
                }

                case kScMachineDataAddress:
#ifdef DEBUG_MACHINE
                    printf("DATA : ");
#endif
                    assert(sym->Value.IValue != 0);

                    cptr[fixup] = sym->Value.IValue;
                    break;

                default:
#ifdef DEBUG_MACHINE
                    printf("SYM : ");
#endif
                    cptr[fixup] = system_index | 0x40000000;
            }

            break;
        }
        case FIXUP_DATADATA:
        {
            auto p = reinterpret_cast<uint32_t *>(cinst->globaldata + fixup);
            *p += cinst->globaldata_vaddr;
        }
        case FIXUP_STACK:
            assert(0);
            // cptr[fixup] += this->stack_vaddr;
            break;

        default:
            break;

    }
#ifdef DEBUG_MACHINE
    printf("\n");
#endif
  }



    cinst->exports.reserve(scri->numexports);

    // find the real address of the exports
    for (int i = 0; i < scri->numexports; i++) {
        int32_t etype = (scri->export_addr[i] >> 24L) & 0x000ff;
        int32_t eaddr = (scri->export_addr[i] & 0x00ffffff);
        if (etype == EXPORT_DATA) {
            cinst->exports.push_back( {scri->exports[i], etype, cinst->globaldata_vaddr + eaddr});
        } else {
            cinst->exports.push_back( {scri->exports[i], etype, cinst->code_vaddr + eaddr*4});
        }
    }

    // used for global scripts!!
    if (ccGetOption(SCOPT_AUTOIMPORT) != 0) {
        // import all the exported stuff from this script
        for (const auto &e : cinst->exports) {

            if (e.type == EXPORT_DATA) {
                RuntimeScriptValue rsv;
                rsv.SetInt32(e.vaddr);
                rsv.Type = kScMachineDataAddress;
                ccAddExternalScriptSymbol(e.name, rsv, cinst);
                //printf("export %s\n", e.name.GetCStr());
            } else {
                RuntimeScriptValue rsv;
                rsv.SetInt32(e.vaddr);
                rsv.Type = kScMachineFunctionAddress;
                ccAddExternalScriptSymbol(e.name, rsv, cinst);
                //printf("export %s\n", e.name.GetCStr());
            }

        }
    }

    // loadedInstances.push_back(cinst);

    return cinst;
}


void ccExecutor::UnloadScript(ccInstance *instance)
{
    if (instance == nullptr) { return; }

    auto inst = (ccInstanceExperiment*)instance;

    simp.RemoveScriptExports(inst);

    inst->exports.clear();

    tiny_free(inst->globaldata);
    inst->globaldata = nullptr;
    inst->globaldata_vaddr = 0;

    tiny_free(inst->code);
    inst->code = nullptr;
    inst->code_vaddr = 0;

    tiny_free(inst->strings);
    inst->strings = nullptr;
    inst->strings_vaddr = 0;

    inst->instanceof = nullptr;

    delete instance;
}

// Get the address of an exported symbol (function or variable) in the script
RuntimeScriptValue ccInstanceExperiment::GetSymbolAddress(const char *symname) 
{
    char altName[200];
    sprintf(altName, "%s$", symname);

    RuntimeScriptValue rval_null;

    for (const auto &e : this->exports) {
        if (strcmp(e.name.GetCStr(), symname) == 0){
            // only care if exists
            rval_null.SetData((char*)(1), 0);
            return rval_null;
        }
        // mangled function name
        if (strncmp(e.name.GetCStr(), altName, strlen(altName)) == 0){
            // only care if exists
            rval_null.SetData((char*)(1), 0);
            return rval_null;
        }
    }

    return rval_null;
}


// Tells whether this instance is in the process of executing the byte-code
bool    ccExecutor::IsBeingRun()  { 
    assert(0);
    return false;
}

#if 0
inline RuntimeScriptValue RuntimeScriptValueFromMachineValue(uint32_t machvalue)
{
    RuntimeScriptValue result;

    result.SetInt32(machvalue);

        try {
            auto realaddrr = ToRealMemoryAddress(e);
            if (realaddrr != nullptr) {
                callparams[cindex].Ptr = (char*)realaddrr;
                // v.SetData((char*)realaddrr, 1024);
            }
        } catch (const std::runtime_error& error) { }
}
#endif

inline uint32_t MachineValueFromRuntimeScriptValue(const RuntimeScriptValue &value)
{
    switch(value.Type) {

        // returned in the case of void functions
        case kScValUndefined:
            return 0;

        case kScValInteger:
        case kScValFloat:
        case kScValPluginArg:
        case kScMachineDataAddress:
            return reinterpret_cast<const uint32_t&>(value.IValue);

        case kScValData:
        case kScValStringLiteral:
        case kScValStaticObject:
        case kScValStaticArray:
        case kScValDynamicObject:
        case kScValPluginObject:
            return ToVirtualAddress(value.Ptr);

        default:
            throw std::runtime_error("unsupported RuntimeScriptValue type");
    }
}


// stack_values assumed to be in reverse parameter order
int ccExecutor::CallScriptFunctionDirect(uint32_t vaddr, int32_t num_values, const uint32_t *stack_values)
{
    ccError = 0;

    auto old_pc = pc;
    auto old_sreg_op = registers[SREG_OP];

    stackframes.push_back({});
    assert(stackframes.back().next_call_needs_object == 0);
    stackframes.back().funcstart = vaddr;

    auto stackptr = reinterpret_cast<uint32_t*>( ToRealMemoryAddressFromHeap(registers[SREG_SP]) );

    // NOTE: Pushing parameters to stack in reverse order
    for (int i = 0; i < num_values; i++) {
        *stackptr = stack_values[i];
        registers[SREG_SP] += 4;
        stackptr++;
    }

    // return address on stack, '0' will tell Run to return.
    *stackptr = 0;
    registers[SREG_SP] += 4;
    stackptr++;

    pc = vaddr;

    int reterr = Run();

    registers[SREG_SP] -= 4 * num_values;

    // restore OP
    registers[SREG_OP] = old_sreg_op;
    pc = old_pc;

    if (stackframes.size() <= 0) {
        pool.RunGarbageCollectionIfAppropriate();
    }

    if (reterr)
        return -6;

    return ccError;
}


// Call an exported function in the script
int ccExecutor::CallScriptFunction(ccInstance *sci_, const char *funcname, int32_t num_params, const RuntimeScriptValue *params) 
{ 
    auto sci = (ccInstanceExperiment *)sci_;

    ccError = 0;

    uint32_t startat = 0;
    char mangledName[200];
    sprintf(mangledName, "%s$", funcname);

    for (const auto &e : sci->exports) {

        const char *thisExportName = e.name.GetCStr();
        int match = 0;

        // check for a mangled name match
        if (strncmp(thisExportName, mangledName, strlen(mangledName)) == 0) {
            // found, compare the number of parameters
            match = 1;
        }
        // check for an exact match (if the script was compiled with
        // an older version)
        if ((match == 1) || (strcmp(thisExportName, funcname) == 0)) {
            //int32_t etype = (instanceof->export_addr[k] >> 24L) & 0x000ff;
            // startat = (instanceof->export_addr[k] & 0x00ffffff);
            startat = e.vaddr;
            break;
        }
    }

    if (startat == 0) {
        cc_error("function '%s' not found", funcname);
        return -2;
    }

    // reverse order
    uint32_t stack_values[num_params];
    for (int i = 0; i < num_params; i++) {
        stack_values[i] = MachineValueFromRuntimeScriptValue(params[num_params - i - 1]);
    }

    return CallScriptFunctionDirect(startat, num_params, stack_values);

}

void ccExecutor::Abort() {
    assert(0);
}



// inline uint32_t read_code(const uint8_t * &code, uint32_t &pc)
// {
//     auto value = code[0] | (code[1]<<8) | (code[2]<<16) | (code[3]<<24);
//     code += 4;
//     pc += 4;
//     return value;
// }

template <class T>
inline T read_code_t(const uint8_t * &code, uint32_t &pc)
{
    static_assert(sizeof(T) == sizeof(uint32_t), "Type must be same size as uint32_t");
    const auto value = (code[0]) | (code[1]<<8) | (code[2]<<16) | (code[3]<<24);
    const auto value_t = reinterpret_cast<const T &>(value);
    code += 4;
    pc += 4;
    return value_t;
}


template <class T>
inline T read_reg_t(uint32_t registers[], reg_t reg)
{
    static_assert(sizeof(T) == sizeof(uint32_t), "Type must be same size as uint32_t");
    const auto value_raw = registers[reg];
    const auto value = reinterpret_cast<const T &>(value_raw);
    return value;
}

template <class T>
inline void write_reg_t(uint32_t registers[], reg_t reg, T value)
{
    static_assert(sizeof(T) == sizeof(uint32_t), "Type must be same size as uint32_t");
    const auto value_raw = reinterpret_cast<const uint32_t&>(value);
    registers[reg] = value_raw;
}


int ccExecutor::GetReturnValue() 
{  
    return read_reg_t<int32_t>(registers, SREG_AX);
}


void dump_codes(const uint8_t *cptr, int num_args)
{
    auto codeptr2 = reinterpret_cast<const uint32_t *>(cptr);

    int size = num_args + 1;
    for (int i = 0; i < 4; i++) {
        if (i < size)
            printf("%08x ", codeptr2[i]);
        else
            printf("         ");
    }
    printf("   ");
}

static bool extra_line = false;

void ccExecutor::DisassembleInstruction()
{
    char fbuf[1024];

    auto pc_ = this->pc;
    auto codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc_));
    assert(codeptr2 != 0);

    if (extra_line) {
        printf("\n");
    }
    extra_line = false;

    printf("%08x    ", pc_);

    auto op = read_code_t<uint32_t>(codeptr2, pc_);


    switch(op) {


        // debug
        // ------------------------------------------------------------------------------------------------

        case SCMD_LINENUM: //      36    // debug info - source code line number
        {
            dump_codes(codeptr2, 1);
            auto arg_line = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "line %d", arg_line);
            break;
        }

        case SCMD_CHECKBOUNDS: //  46    // check reg1 is between 0 and arg2
        {
            dump_codes(codeptr2, 2);
            const auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "assert 0 <= %s < %d", regnames[arg_reg], arg_lit);
            break;
        }


        // data transfer
        // ------------------------------------------------------------------------------------------------


        case SCMD_LITTOREG: //     6     // set reg1 to literal value arg2
        {
            dump_codes(codeptr2, 2);
            const auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg_lit = read_code_t<uint32_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %d", regnames[arg_reg], arg_lit);
            break;
        }


        case SCMD_REGTOREG: //     3     // reg2 = reg1
        {
            dump_codes(codeptr2, 2);
            const auto arg_reg_src = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg_reg_dest = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s", regnames[arg_reg_dest], regnames[arg_reg_src]);
            break;
        }


        // arithmetic
        // ------------------------------------------------------------------------------------------------

        case SCMD_ADD: //          1     // reg1 += arg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "%s += %d", regnames[arg1_reg], arg2_lit);
            break;
        }
        case SCMD_SUB: //          2     // reg1 -= arg2
        {           
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "%s -= %d", regnames[arg1_reg], arg2_lit);
            break;
        }
        case SCMD_MUL: //          32    // reg1 *= arg2
        {           
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "%s *= %d", regnames[arg1_reg], arg2_lit);
            break;
        }

        case SCMD_ADDREG: //       11    // reg1 += reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s += %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_SUBREG: //       12    // reg1 -= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s -= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_MULREG: //       9     // reg1 *= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s *= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_DIVREG: //       10    // reg1 /= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s /= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_MODREG: //       40    // reg1 %= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s %%= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_FADD: //         53    // reg1 += arg2 (int)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s += %d", regnames[arg1_reg], arg2_lit);
            break;
        }

        case SCMD_FSUB: //         54    // reg1 -= arg2 (int)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s -= %d", regnames[arg1_reg], arg2_lit);
            break;
        }
        case SCMD_FMULREG: //      55    // reg1 *= reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s *= (f)%s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_FDIVREG: //      56    // reg1 /= reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s /= (f)%s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_FADDREG: //      57    // reg1 += reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s += (f)%s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_FSUBREG: //      58    // reg1 -= reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "(f)%s -= (f)%s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }


        // binary
        // ------------------------------------------------------------------------------------------------

        case SCMD_BITAND: //       13    // bitwise  reg1 & reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s &= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_BITOR: //        14    // bitwise  reg1 | reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s |= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_XORREG: //       41    // reg1 ^= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s ^= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_SHIFTLEFT: //    43    // reg1 = reg1 << reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s <<= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_SHIFTRIGHT: //   44    // reg1 = reg1 >> reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s >>= %s", regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }


        // logic
        // ------------------------------------------------------------------------------------------------
        case SCMD_AND: //          21    // (reg1!=0) && (reg2!=0) -> reg1
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s && %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_OR: //           22    // (reg1!=0) || (reg2!=0) -> reg1
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s || %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_NOTREG: //       42    // reg1 = !reg1
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = !%s", regnames[arg1_reg], regnames[arg1_reg]);
            break;
        }


        // comparison
        // ------------------------------------------------------------------------------------------------

        case SCMD_ISEQUAL: //      15    // reg1 == reg2   reg1=1 if true, =0 if not
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s == %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_NOTEQUAL: //     16    // reg1 != reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s != %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_GREATER: //      17    // reg1 > reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s > %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_LESSTHAN: //     18    // reg1 < reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s < %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_GTE: //          19    // reg1 >= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s >= %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_LTE: //          20    // reg1 <= reg2
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s <= %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_FGREATER: //     59    // reg1 > reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = %s > %s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_FLESSTHAN: //    60    // reg1 < reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = (f)%s < (f)%s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_FGTE: //         61    // reg1 >= reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = (f)%s >= (f)%s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }
        case SCMD_FLTE: //         62    // reg1 <= reg2 (float)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = (f)%s <= (f)%s", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }


        // stack
        // ------------------------------------------------------------------------------------------------

        case SCMD_PUSHREG: //      29    // m[sp]=reg1; sp++
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "push %s", regnames[arg_reg]);
            break;
        }

        case SCMD_POPREG: //       30    // sp--; reg1=m[sp]
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "pop %s", regnames[arg_reg]);
            break;
        }

        case SCMD_LOADSPOFFS: //   51    // MAR = SP - arg1 (optimization for local var access)
        {
            dump_codes(codeptr2, 1);
            auto arg1_lit = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "mar = sp - %d", arg1_lit);
            break;
        }


        // MAR memory
        // ------------------------------------------------------------------------------------------------

        case SCMD_CHECKNULL: //    52    // error if MAR==0
        {
            dump_codes(codeptr2, 0);
            sprintf(fbuf, "assert mar==0");
            break;

        }

        case SCMD_MEMREAD: //      7     // reg1 = m[MAR] (4 bytes)
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = dword ptr [mar]", regnames[arg1_reg]);
            break;

        }
        case SCMD_MEMREADB: //     24    // reg1 = m[MAR] (1 byte)
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = byte ptr [mar]", regnames[arg1_reg]);
            break;
        }
        case SCMD_MEMREADW: //     25    // reg1 = m[MAR] (2 bytes)
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = word ptr [mar]", regnames[arg1_reg]);
            break;
        }


        case SCMD_WRITELIT: //     4     // m[MAR] = arg2 (copy arg1 bytes)
        {
            dump_codes(codeptr2, 2);
            const auto arg1_size = read_code_t<uint32_t>(codeptr2, pc_);
            const auto arg2_data = read_code_t<uint32_t>(codeptr2, pc_);

            switch (arg1_size) 
            {
                case 1:
                    sprintf(fbuf, "byte ptr [mar] = %d", arg2_data);
                    break;
                case 2:
                    sprintf(fbuf, "word ptr [mar] = %d", arg2_data);
                    break;
                case 4:
                    sprintf(fbuf, "dword ptr [mar] = %d", arg2_data);
                    break;
                default:
                    assert(0);
            }

            break;
        }
        case SCMD_MEMWRITE: //     8     // m[MAR] = reg1
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "dword ptr [mar] = %s", regnames[arg1_reg]);
            break;
        }
        case SCMD_MEMWRITEB: //    26    // m[MAR] = reg1 (1 byte)
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "byte ptr [mar] = %s", regnames[arg1_reg]);
            break;

        }
        case SCMD_MEMWRITEW: //    27    // m[MAR] = reg1 (2 bytes)
        {
            dump_codes(codeptr2, 1);
            const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "word ptr [mar] = %s", regnames[arg1_reg]);
            break;
        }
        case SCMD_ZEROMEMORY: //   63    // m[MAR]..m[MAR+(arg1-1)] = 0
        {
            dump_codes(codeptr2, 1);
            const auto arg1_count = read_code_t<uint32_t>(codeptr2, pc_);
            sprintf(fbuf, "memset(mar, '\\0', %d", arg1_count);
            break;
        }


        // MAR memory handles
        // ------------------------------------------------------------------------------------------------

        /*
            registers hold the object address

            but MAR only holds the handle (so we can save/restore)
        */

        case SCMD_MEMINITPTR: //   50    // m[MAR] = reg1 (but don't free old one)
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "hndl ptr [mar] = retain(%s)", regnames[arg_reg]);
            break;
        }

        // for keeping track of memory handles
        case SCMD_MEMWRITEPTR: //  47    // m[MAR] = reg1 (adjust ptr addr)
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "release(hndl ptr [mar]); hndl ptr [mar] = retain(%s)", regnames[arg_reg]);
            break;

        }
        case SCMD_MEMREADPTR: //   48    // reg1 = m[MAR] (adjust ptr addr)
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = addr(hndl ptr [mar])", regnames[arg_reg]);
            break;
        }
        case SCMD_MEMZEROPTR: //   49    // m[MAR] = 0    (blank ptr)
        {
            dump_codes(codeptr2, 0);
            sprintf(fbuf, "release(hndl ptr [mar]); hndl ptr [mar] = 0");
            break;
        }

        case SCMD_MEMZEROPTRND: // 69    // m[MAR] = 0    (blank ptr, no dispose if = ax)
        {
            dump_codes(codeptr2, 0);
            sprintf(fbuf, "release(hndl ptr [mar]); hndl ptr [mar] = 0; ax == no dispose");
            break;
        }


        // control transfer
        // ------------------------------------------------------------------------------------------------

        case SCMD_LOOPCHECKOFF: // 68    // no loop checking for this function
        {
            dump_codes(codeptr2, 0);
            sprintf(fbuf, "disable loop check");
            break;
        }

        case SCMD_JMP: //          31    // jump to arg1
        {
            dump_codes(codeptr2, 1);
            auto arg_rel = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "jmp %+d", arg_rel);
            
            extra_line = true;
            break;
        }

        case SCMD_JZ: //           28    // jump if ax==0 to arg1
        {
            dump_codes(codeptr2, 1);
            auto arg_rel = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "if (ax == 0) jmp %+d", arg_rel);
            extra_line = true;
            break;
        }

        case SCMD_JNZ: //          70    // jump to arg1 if ax!=0
        {
            dump_codes(codeptr2, 1);
            auto arg_rel = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "if (ax != 0) jmp %+d", arg_rel);
            extra_line = true;
            break;
        }


        // sub routines
        // ------------------------------------------------------------------------------------------------

        case SCMD_THISBASE: //     38    // current relative address
        {
            dump_codes(codeptr2, 1);
            auto arg_base = read_code_t<uint32_t>(codeptr2, pc_);
            sprintf(fbuf, "base = %08x", arg_base);
            break;
        }

        case SCMD_NUMFUNCARGS: //  39    // number of arguments for ext func call
        {
            dump_codes(codeptr2, 1);
            auto arg_numfuncs = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "num_args = %d // next call", arg_numfuncs);
            break;
        }

        case SCMD_CALLOBJ: //      45    // next call is member function of reg1
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "op = %s // next call", regnames[arg_reg]);
            break;
        }

        case SCMD_CALL: //         23    // jump to subroutine at reg1
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "call %s", regnames[arg_reg]);
            extra_line = true;
            break;
        }

        case SCMD_RET: //          5     // return from subroutine
        {
            dump_codes(codeptr2, 0);
            sprintf(fbuf, "return");
            extra_line = true;
            break;
        }

        case SCMD_PUSHREAL: //     34    // push reg1 onto real stack
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "farpush %s", regnames[arg_reg]);
            break;
        }

        case SCMD_SUBREALSTACK: // 35    // decrement stack ptr by literal
        {
            dump_codes(codeptr2, 1);
            auto arg_offset = read_code_t<int32_t>(codeptr2, pc_);
            sprintf(fbuf, "(far)sp -= %d", arg_offset);
            break;
        }

        case SCMD_CALLAS: //       37    // call external script function
        {
            // this is not actually generated, I think.
            assert(0);
            break;
        }

        case SCMD_CALLEXT: //      33    // call external (imported) function reg1
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "callext %s", regnames[arg_reg]);
            extra_line = true;
            break;
        }



        // strings
        // ------------------------------------------------------------------------------------------------

        case SCMD_CREATESTRING: // 64    // reg1 = new String(reg1)
        {
            dump_codes(codeptr2, 1);
            auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = new String(%s)", regnames[arg_reg], regnames[arg_reg]);

            break;
        }

        case SCMD_STRINGSEQUAL: // 65    // (char*)reg1 == (char*)reg2   reg1=1 if true, =0 if not
        {
            dump_codes(codeptr2, 2);
            auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);

            sprintf(fbuf, "%s = streq(%s, %s)", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);

            break;
        }
        case SCMD_STRINGSNOTEQ: // 66    // (char*)reg1 != (char*)reg2
        {
            dump_codes(codeptr2, 2);
            auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            auto arg2_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "%s = strneq(%s, %s)", regnames[arg1_reg], regnames[arg1_reg], regnames[arg2_reg]);
            break;
        }

        case SCMD_CHECKNULLREG: // 67    // error if reg1 == NULL
        {
            dump_codes(codeptr2, 1);
            const auto arg_reg = read_code_t<reg_t>(codeptr2, pc_);
            sprintf(fbuf, "assert %s != nullptr", regnames[arg_reg]);
            break;
        }


        // dyn objects
        // ------------------------------------------------------------------------------------------------


        case SCMD_NEWARRAY: //     72    // reg1 = new array of reg1 elements, each of size arg2 (arg3=managed type?)
        {
            dump_codes(codeptr2, 3);
            auto arg1 = read_code_t<int32_t>(codeptr2, pc_); // num elements
            auto arg2 = read_code_t<int32_t>(codeptr2, pc_); // element size
            auto arg3 = read_code_t<int32_t>(codeptr2, pc_); // element type, 1 == managed.

            auto managed = arg3==1 ? "managed " : "";
            sprintf(fbuf, "%s = new Array[%d](%s%d bytes)", regnames[arg1], registers[arg1], managed, arg2);
            break;
        }

        case SCMD_DYNAMICBOUNDS: // 71   // check reg1 is between 0 and m[MAR-4]
        {
            dump_codes(codeptr2, 1);
            auto arg1 = read_code_t<reg_t>(codeptr2, pc_); 
            sprintf(fbuf, "assert 0 < %s < arraysize(mar)", regnames[arg1]);
            break;
        }

        case SCMD_NEWUSEROBJECT: // 73   // reg1 = new user object of arg2 size
        {
            dump_codes(codeptr2, 2);
            auto arg1_reg = read_code_t<reg_t>(codeptr2, pc_);
            auto arg2 = read_code_t<int32_t>(codeptr2, pc_); // element size
            sprintf(fbuf, "%s = new UserObject(%d bytes)", regnames[arg1_reg], arg2);
            break;
        }


        // default
        // ------------------------------------------------------------------------------------------------

        default:
            sprintf(fbuf, "ERROR UNABLE TO DECODE");
    }


    printf("%-70s   ", fbuf);

    printf("ax:%08x bx:%08x cx:%08x dx:%08x mar:%08x op:%08x sp:%08x ",
        registers[SREG_AX],
        registers[SREG_BX],
        registers[SREG_CX],
        registers[SREG_DX],
        registers[SREG_MAR],
        registers[SREG_OP],
        registers[SREG_SP]
        );

    auto &stackframe = stackframes.back();

    printf("farstk:%-2zd callobj:%-1d callnargs%-2d ", stackframe.callstack.size(), stackframe.next_call_needs_object, stackframe.num_args_to_func);

    printf("\n");
}


int ccExecutor::Run()
{
    auto *pctr = ToRealMemoryAddressFromHeap(pc);
    const uint8_t *codeptr2 = reinterpret_cast<uint8_t *>(pctr);


    for(;;) {

        assert(pc != 0);

#ifdef DEBUG_MACHINE
        DisassembleInstruction();
#endif

        auto op = read_code_t<uint32_t>(codeptr2, pc);


        switch(op) {


            // debug
            // ------------------------------------------------------------------------------------------------

            case SCMD_LINENUM: //      36    // debug info - source code line number
            {
                auto arg_line = read_code_t<int32_t>(codeptr2, pc);
                break;
            }

            case SCMD_CHECKBOUNDS: //  46    // check reg1 is between 0 and arg2
            {

                const auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg_lit = read_code_t<int32_t>(codeptr2, pc);

                auto value = read_reg_t<int32_t>(registers, arg_reg);

                assert(value >= 0 && value < arg_lit);
                break;
            }


            // data transfer
            // ------------------------------------------------------------------------------------------------


            case SCMD_LITTOREG: //     6     // set reg1 to literal value arg2
            {
                const auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg_lit = read_code_t<uint32_t>(codeptr2, pc);

                write_reg_t(registers, arg_reg, arg_lit);
                break;
            }


            case SCMD_REGTOREG: //     3     // reg2 = reg1
            {
                const auto arg_reg_src = read_code_t<reg_t>(codeptr2, pc);
                const auto arg_reg_dest = read_code_t<reg_t>(codeptr2, pc);

                const auto value = read_reg_t<uint32_t>(registers, arg_reg_src);

                write_reg_t(registers, arg_reg_dest, value);

                break;
            }


            // arithmetic
            // ------------------------------------------------------------------------------------------------

            case SCMD_ADD: //          1     // reg1 += arg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) + arg2_lit;
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_SUB: //          2     // reg1 -= arg2
            {           
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) - arg2_lit;
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_MUL: //          32    // reg1 *= arg2
            {           
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) * arg2_lit;
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_ADDREG: //       11    // reg1 += reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) + read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_SUBREG: //       12    // reg1 -= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) - read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_MULREG: //       9     // reg1 *= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) * read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_DIVREG: //       10    // reg1 /= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) / read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_MODREG: //       40    // reg1 %= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) % read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_FADD: //         53    // reg1 += arg2 (int)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) + arg2_lit;
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_FSUB: //         54    // reg1 -= arg2 (int)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_lit = read_code_t<int32_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) - arg2_lit;
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_FMULREG: //      55    // reg1 *= reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) * read_reg_t<float>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_FDIVREG: //      56    // reg1 /= reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) / read_reg_t<float>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_FADDREG: //      57    // reg1 += reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) + read_reg_t<float>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_FSUBREG: //      58    // reg1 -= reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) - read_reg_t<float>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }



            // binary
            // ------------------------------------------------------------------------------------------------

            case SCMD_BITAND: //       13    // bitwise  reg1 & reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) & read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_BITOR: //        14    // bitwise  reg1 | reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) | read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_XORREG: //       41    // reg1 ^= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) ^ read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }

            case SCMD_SHIFTLEFT: //    43    // reg1 = reg1 << reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) << read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_SHIFTRIGHT: //   44    // reg1 = reg1 >> reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) >> read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t(registers, arg1_reg, value);
                break;
            }


            // logic
            // ------------------------------------------------------------------------------------------------
            case SCMD_AND: //          21    // (reg1!=0) && (reg2!=0) -> reg1
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) && read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_OR: //           22    // (reg1!=0) || (reg2!=0) -> reg1
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) || read_reg_t<uint32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_NOTREG: //       42    // reg1 = !reg1
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<uint32_t>(registers, arg1_reg) == 0;
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }

            // comparison
            // ------------------------------------------------------------------------------------------------

            case SCMD_ISEQUAL: //      15    // reg1 == reg2   reg1=1 if true, =0 if not
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) == read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_NOTEQUAL: //     16    // reg1 != reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) != read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_GREATER: //      17    // reg1 > reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) > read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_LESSTHAN: //     18    // reg1 < reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) < read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_GTE: //          19    // reg1 >= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) >= read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_LTE: //          20    // reg1 <= reg2
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<int32_t>(registers, arg1_reg) <= read_reg_t<int32_t>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_FGREATER: //     59    // reg1 > reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) > read_reg_t<float>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }

            case SCMD_FLESSTHAN: //    60    // reg1 < reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) < read_reg_t<float>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_FGTE: //         61    // reg1 >= reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) >= read_reg_t<float>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_FLTE: //         62    // reg1 <= reg2 (float)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);
                const auto value = read_reg_t<float>(registers, arg1_reg) <= read_reg_t<float>(registers, arg2_reg);
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }



            // stack
            // ------------------------------------------------------------------------------------------------


            case SCMD_PUSHREG: //      29    // m[sp]=reg1; sp++
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                auto stackptr = reinterpret_cast<uint32_t*>( ToRealMemoryAddressFromHeap(registers[SREG_SP]) );
                *stackptr = registers[arg_reg];
                registers[SREG_SP] += 4;

                break;
            }


            case SCMD_POPREG: //       30    // sp--; reg1=m[sp]
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                registers[SREG_SP] -= 4;
                auto stackptr = reinterpret_cast<uint32_t*>( ToRealMemoryAddressFromHeap(registers[SREG_SP]) );
                registers[arg_reg] = *stackptr;

                break;

            }


            case SCMD_LOADSPOFFS: //   51    // MAR = SP - arg1 (optimization for local var access)
            {
                auto arg1_lit = read_code_t<int32_t>(codeptr2, pc);

                registers[SREG_MAR] = registers[SREG_SP] - arg1_lit;
                break;
            }



            // MAR memory
            // ------------------------------------------------------------------------------------------------

            case SCMD_CHECKNULL: //    52    // error if MAR==0
            {
                assert(registers[SREG_MAR] != 0);
                auto ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(ptr != nullptr);
                break;
            }

            case SCMD_MEMREAD: //      7     // reg1 = m[MAR] (4 bytes)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto src_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(src_ptr != nullptr);
                uint32_t value = src_ptr[0] | (src_ptr[1]<<8) | (src_ptr[2] << 16) | (src_ptr[3] << 24);
                write_reg_t(registers, arg1_reg, value);
                break;
            }
            case SCMD_MEMREADB: //     24    // reg1 = m[MAR] (1 byte)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto src_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(src_ptr != nullptr);
                uint8_t value = *src_ptr;
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }
            case SCMD_MEMREADW: //     25    // reg1 = m[MAR] (2 bytes)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto src_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(src_ptr != nullptr);
                uint16_t value_raw = src_ptr[0] | (src_ptr[1]<<8);
                int32_t value = reinterpret_cast<int16_t&>(value_raw);
                // sign extended
                write_reg_t<uint32_t>(registers, arg1_reg, value);
                break;
            }


            case SCMD_WRITELIT: //     4     // m[MAR] = arg2 (copy arg1 bytes)
            {
                const auto arg1_size = read_code_t<uint32_t>(codeptr2, pc);
                const auto arg2_data = read_code_t<uint32_t>(codeptr2, pc);

                auto dest_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(dest_ptr != nullptr);

                // 4 bytes is the only size ever used.
                // if 2 or 1 bytes are requested.. should we sign extend?
                switch(arg1_size) {
                    case 4:
                        dest_ptr[3] = (arg2_data >> 24) & 0xFF;
                    case 3:
                        dest_ptr[2] = (arg2_data >> 16) & 0xFF;
                    case 2:
                        dest_ptr[1] = (arg2_data >> 8) & 0xFF;
                    case 1:
                        dest_ptr[0] = arg2_data & 0xFF;
                    case 0:
                        break;
                    default:
                        throw std::runtime_error("too big?");
                }

                break;
            }
            case SCMD_MEMWRITE: //     8     // m[MAR] = reg1
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto value = read_reg_t<uint32_t>(registers, arg1_reg);
                auto dest_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(dest_ptr != nullptr);
                dest_ptr[0] = (value >> 0) & 0xFF ;
                dest_ptr[1] = (value >> 8) & 0xFF ;
                dest_ptr[2] = (value >> 16) & 0xFF ;
                dest_ptr[3] = (value >> 24) & 0xFF ;
                break;
            }
            case SCMD_MEMWRITEB: //    26    // m[MAR] = reg1 (1 byte)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto dest_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(dest_ptr != nullptr);
                auto value = read_reg_t<uint32_t>(registers, arg1_reg);
                dest_ptr[0] = value & 0xFF;
                break;
            }
            case SCMD_MEMWRITEW: //    27    // m[MAR] = reg1 (2 bytes)
            {
                const auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto dest_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(dest_ptr != nullptr);
                auto value = read_reg_t<uint32_t>(registers, arg1_reg);
                dest_ptr[0] = value & 0xFF;
                dest_ptr[1] = (value >> 8) & 0xFF;
                break;
            }
            case SCMD_ZEROMEMORY: //   63    // m[MAR]..m[MAR+(arg1-1)] = 0
            {
                const auto arg1_count = read_code_t<uint32_t>(codeptr2, pc);
                auto dest_ptr = (uint8_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(dest_ptr != nullptr);
                memset(dest_ptr, 0, arg1_count);
                break;
            }


            // MAR memory handles
            // ------------------------------------------------------------------------------------------------

            /*
                registers hold the object address

                but MAR only holds the handle (so we can save/restore)
            */

            case SCMD_MEMINITPTR: //   50    // m[MAR] = reg1 (but don't free old one)
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                auto handleptr = (uint32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(handleptr != nullptr);

                auto objptr = (const char*)ToRealMemoryAddress(registers[arg_reg]);

                if (objptr != nullptr) {
                    ManagedObjectInfo objinfo;
                    auto err = ccGetObjectInfoFromAddress(objinfo, (void*)objptr);
                    assert(err == 0);
                    *handleptr = objinfo.handle;
                    ccAddObjectReference(objinfo.handle);
                } else {
                    *handleptr = 0;
                }   

                break;
            }

            // for keeping track of memory handles
            case SCMD_MEMWRITEPTR: //  47    // m[MAR] = reg1 (adjust ptr addr)
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                auto handleptr = (uint32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(handleptr != nullptr);
                auto oldhandle = *handleptr;

                auto objptr = (const char*)ToRealMemoryAddress(registers[arg_reg]);

                if (objptr != nullptr) {
                    ManagedObjectInfo objinfo;
                    auto err = ccGetObjectInfoFromAddress(objinfo, (void*)objptr);
                    assert(err == 0);
                    *handleptr = objinfo.handle;
                    ccAddObjectReference(objinfo.handle);
                } else {
                    *handleptr = 0;
                }   

                if (oldhandle != 0) {
                    ccReleaseObjectReference(oldhandle);
                }

                // this->AddObjectReference(...);

                break;

            }
            case SCMD_MEMREADPTR: //   48    // reg1 = m[MAR] (adjust ptr addr)
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                auto handleptr = (uint32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(handleptr != nullptr);
                auto handle = *handleptr;

                if (handle != 0) {
                    ManagedObjectInfo objinfo;
                    auto err = ccGetObjectInfoFromHandle(objinfo, handle);
                    assert(err == 0);
                    assert(objinfo.address != nullptr);
                    registers[arg_reg] = ToVirtualAddress(objinfo.address);
                } else {
                    registers[arg_reg] = 0;
                }



                break;
            }
            case SCMD_MEMZEROPTR: //   49    // m[MAR] = 0    (blank ptr)
            {
                auto handleptr = (uint32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(handleptr != nullptr);
                auto handle = *handleptr;
                if (handle != 0) {
                    ccReleaseObjectReference(handle);
                }
                *handleptr = 0;
                break;
            }

            case SCMD_MEMZEROPTRND: // 69    // m[MAR] = 0    (blank ptr, no dispose if = ax)
            {
                // WHY AX?
                // Because SCMD_MEMZEROPTRND is called when exiting the outer most loop of a function
                // ax might be a pointer or it might just be a normal number, depends on 
                // what the function is returning.

                auto handleptr = (uint32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]);
                assert(handleptr != nullptr);
                auto handle = *handleptr;

                // don't do the Dispose check for the object being returned -- this is
                // for returning a String (or other pointer) from a custom function.
                // Note: we might be freeing a dynamic array which contains the DisableDispose
                // object, that will be handled inside the recursive call to SubRef.

                if (handle != 0) {
                    // try {
                       pool.disableDisposeForObject = (char*)ToRealMemoryAddress(registers[SREG_AX]);
                    // } catch (const std::runtime_error& error) {}
                    ccReleaseObjectReference(handle);
                    pool.disableDisposeForObject = nullptr;
                }
                *handleptr = 0;
                break;
            }


            // control transfer
            // ------------------------------------------------------------------------------------------------

            case SCMD_LOOPCHECKOFF: // 68    // no loop checking for this function
            {
#ifdef DEBUG_MACHINE
                printf("SCMD_LOOPCHECKOFF: TODO\n");
#endif
                break;
            }

            case SCMD_JMP: //          31    // jump to arg1
            {
                auto arg_rel = read_code_t<int32_t>(codeptr2, pc);
                pc += arg_rel*4;
                codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc));
                break;
            }

            case SCMD_JZ: //           28    // jump if ax==0 to arg1
            {
                auto arg_rel = read_code_t<int32_t>(codeptr2, pc);

                if (registers[SREG_AX] == 0) {
                    pc += arg_rel*4;
                    codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc));
                }
                break;
            }

            case SCMD_JNZ: //          70    // jump to arg1 if ax!=0
            {
                auto arg_rel = read_code_t<int32_t>(codeptr2, pc);

                if (registers[SREG_AX] != 0) {
                    pc += arg_rel*4;
                    codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc));
                }
                break;
            }


            // sub routines
            // ------------------------------------------------------------------------------------------------

            case SCMD_THISBASE: //     38    // current relative address
            {
                auto arg_base = read_code_t<uint32_t>(codeptr2, pc);

                auto &stackframe = stackframes.back();

                stackframe.thisbase = arg_base;
#ifdef DEBUG_MACHINE
                printf("SCMD_THISBASE: base:0x%08x baseadj:0x%08x funcstart:0x%08x\n", arg_base, arg_base*4, stackframe.funcstart);
#endif
                break;
            }

            case SCMD_NUMFUNCARGS: //  39    // number of arguments for ext func call
            {
                auto arg_numfuncs = read_code_t<int32_t>(codeptr2, pc);

                auto &stackframe = stackframes.back();

                stackframe.num_args_to_func = arg_numfuncs;

                break;
            }

            case SCMD_CALLOBJ: //      45    // next call is member function of reg1
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                auto &stackframe = stackframes.back();

                // set the OP register
                if (registers[arg_reg] == 0) {
                    cc_error("!Null pointer referenced");
                    assert(0);
                    return -1;
                }
                registers[SREG_OP] = registers[arg_reg];

                stackframe.next_call_needs_object = 1;
                break;

            }

            case SCMD_CALL: //         23    // jump to subroutine at reg1
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                auto &stackframe = stackframes.back();

                // not used here, but collect and clear.
                auto need_object = stackframe.next_call_needs_object;
                stackframe.next_call_needs_object = 0; // clear

                auto func_args_count = stackframe.num_args_to_func;
                stackframe.num_args_to_func = -1;

                #ifdef DEBUG_MACHINE
                printf("SCMD_CALL: num_args_to_func=%d next_call_needs_object=%d\n", func_args_count, need_object);
                #endif

                // PUSH_CALL_STACK(inst);

                auto stackptr = reinterpret_cast<uint32_t*>( ToRealMemoryAddressFromHeap(registers[SREG_SP]) );
                *stackptr = pc;
                registers[SREG_SP] += 4;

                if (stackframe.thisbase == 0) {
                    assert(0 /* lookup fixup */); 
                    pc = 4 * read_reg_t<uint32_t>(registers, arg_reg);
                } else {
                    // TODO, does base differ from func start at all?
                    pc = stackframe.funcstart - 4*stackframe.thisbase + 4*read_reg_t<uint32_t>(registers, arg_reg);
                    // pc = 4*read_reg_t<uint32_t>(registers, arg_reg);;
                }

                codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc));

                stackframes.push_back({});
                stackframes.back().thisbase = 0;
                stackframes.back().funcstart = pc;

                break;
            }

            case SCMD_RET: //          5     // return from subroutine
            {
                registers[SREG_SP] -= 4;
                auto stackptr = reinterpret_cast<uint32_t*>( ToRealMemoryAddressFromHeap(registers[SREG_SP]) );

                stackframes.pop_back();

                pc = *stackptr;
                codeptr2 = reinterpret_cast<const uint8_t *>(ToRealMemoryAddressFromHeap(pc));

                if (pc == 0) { 
                    //returnValue = registers[SREG_AX];
                    return 0; 
                }

                break;
            }

            case SCMD_PUSHREAL: //     34    // push reg1 onto real stack
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                stackframes.back().callstack.push_back(registers[arg_reg]);
                break;
            }

            case SCMD_SUBREALSTACK: // 35    // decrement stack ptr by literal
            {
                auto arg_offset = read_code_t<int32_t>(codeptr2, pc);

                // NOTE: no need to do this, as SCMD_CALLEXT will do it instead
                // if (stackframes.back().was_just_callas >= 0) {
                //     registers[SREG_SP] -= arg_offset*4;
                // }
                // stackframes.back().was_just_callas = -1;

                while (arg_offset--) {
                    stackframes.back().callstack.pop_back();
                }

                break;
            }

            case SCMD_CALLAS: //       37    // call external script function
            {
                // this is not actually generated, I think.
                assert(0);
                break;
            }

            case SCMD_CALLEXT: //      33    // call external (imported) function reg1
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                auto extfunc = registers[arg_reg] & 0x3FFFFFFF;

                auto sysimp = simp.getByIndex(extfunc);

#ifdef DEBUG_MACHINE
                printf("SCMD_CALLEXT begin:%s\n", sysimp->Name.GetCStr());
#endif

                if (sysimp->Value.Type == kScMachineFunctionAddress) {
                    // DON'T SET AX since the script function itself will set it.
                    CallExternalScriptFunction(sysimp);
                } else {
                    // CallScriptFunction to a real 'C' code function
                    registers[SREG_AX] = CallExternalFunction(sysimp);
                }

                #ifdef DEBUG_MACHINE
                printf("SCMD_CALLEXT   end:%s\n", sysimp->Name.GetCStr());
                #endif

                break;
            }



            // strings
            // ------------------------------------------------------------------------------------------------

            case SCMD_CREATESTRING: // 64    // reg1 = new String(reg1)
            {
                auto arg_reg = read_code_t<reg_t>(codeptr2, pc);

                if (stringClassImpl == nullptr) {
                    cc_error("No string class implementation set, but opcode was used");
                    assert(0);
                    return -1;
                }

                auto fromstr = ToRealMemoryAddressFromHeap(registers[arg_reg]);
                auto str = (char *)stringClassImpl->CreateString((const char*)fromstr).second;
                registers[arg_reg] = ToVirtualAddressFromHeap(str);

#ifdef DEBUG_MACHINE
                printf("SCMD_CREATESTRING: \"%s\"\n",  str);
#endif
                break;
            }

            case SCMD_STRINGSEQUAL: // 65    // (char*)reg1 == (char*)reg2   reg1=1 if true, =0 if not
            {
                auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);

                auto arg1_ptr = (char *)ToRealMemoryAddressFromHeap(registers[arg1_reg]);
                auto arg2_ptr = (char *)ToRealMemoryAddressFromHeap(registers[arg2_reg]);

                if (strcmp(arg1_ptr, arg2_ptr) == 0)
                    registers[arg1_reg] = 1;
                else
                    registers[arg1_reg] = 0;

                break;
            }
            case SCMD_STRINGSNOTEQ: // 66    // (char*)reg1 != (char*)reg2
            {
                auto arg1_reg = read_code_t<reg_t>(codeptr2, pc);
                auto arg2_reg = read_code_t<reg_t>(codeptr2, pc);

                auto arg1_ptr = (char *)ToRealMemoryAddressFromHeap(registers[arg1_reg]);
                auto arg2_ptr = (char *)ToRealMemoryAddressFromHeap(registers[arg2_reg]);

                if (strcmp(arg1_ptr, arg2_ptr) != 0)
                    registers[arg1_reg] = 1;
                else
                    registers[arg1_reg] = 0;
                break;
            }

            case SCMD_CHECKNULLREG: // 67    // error if reg1 == NULL
            {
                const auto arg_reg = read_code_t<reg_t>(codeptr2, pc);
                auto value = read_reg_t<uint32_t>(registers, arg_reg);
                assert(value != 0);
                break;
            }


            // dyn objects
            // ------------------------------------------------------------------------------------------------


            case SCMD_NEWARRAY: //     72    // reg1 = new array of reg1 elements, each of size arg2 (arg3=managed type?)
            {
                auto arg1 = read_code_t<int32_t>(codeptr2, pc); // num elements
                auto arg2 = read_code_t<int32_t>(codeptr2, pc); // element size
                auto arg3 = read_code_t<int32_t>(codeptr2, pc); // element type, 1 == managed.

                auto num_elements = registers[arg1];
                if (num_elements < 1 || num_elements > 1000000) {
                    cc_error("invalid size for dynamic array; requested: %d, range: 1..1000000", num_elements);
                    assert(0);
                    return -1;
                }

                auto ref = globalDynamicArray.Create(num_elements, arg2, arg3 == 1);

                registers[arg1] = ToVirtualAddressFromHeap(ref.second);

                break;
            }

            case SCMD_DYNAMICBOUNDS: // 71   // check reg1 is between 0 and m[MAR-4]
            {
                auto arg1_reg = read_code_t<reg_t>(codeptr2, pc); // reg of num elements
                auto array_header = (int32_t *)ToRealMemoryAddressFromHeap(registers[SREG_MAR]) - 2;
                auto numElements = array_header[0] & (~ARRAY_MANAGED_TYPE_FLAG);
                auto sizeInBytes = array_header[1];
                assert(registers[arg1_reg] >= 0 && registers[arg1_reg] < sizeInBytes);
                break;
            }

            case SCMD_NEWUSEROBJECT: // 73   // reg1 = new user object of arg2 size
            {
                auto arg1_reg = read_code_t<reg_t>(codeptr2, pc); // dest to write ptr
                auto arg2_size = read_code_t<int32_t>(codeptr2, pc); // size
                assert(arg2_size >= 0);
                auto ref = ScriptUserObject::CreateManaged(arg2_size);
                registers[arg1_reg] = ToVirtualAddressFromHeap(ref.second);
                break;
            }


            // default
            // ------------------------------------------------------------------------------------------------

            default:
            assert(0);
        }


    }

}


// CallScriptFunction to a real 'C' code function
uint32_t ccExecutor::CallExternalFunction(const ScriptImport *sysimp)
{
    auto &stackframe = stackframes.back();
    // stackframe.was_just_callas = -1;

    auto need_object = stackframe.next_call_needs_object;
    stackframe.next_call_needs_object = 0; // clear

    auto func_args_count = stackframe.num_args_to_func;
    stackframe.num_args_to_func = -1;

#ifdef DEBUG_MACHINE
    printf("SCMD_CALLEXT: num_args_to_func=%d next_call_needs_object=%d\n", func_args_count, need_object);
#endif

    if (func_args_count < 0) {
        func_args_count = stackframe.callstack.size();
    }

    RuntimeScriptValue callparams[func_args_count];

    // reverse call stack into correct order
    auto cit = stackframe.callstack.rbegin();
    for (int i = 0; i < func_args_count; i++) {
        callparams[i].SetInt32(*cit);

        // try {
            auto realaddrr = ToRealMemoryAddress(*cit);
            callparams[i].Ptr = (char*)realaddrr;
        // } catch (const std::runtime_error& error) {
            // It might just be an integer that does not map to an address
        // }

        ++cit;
    }

    RuntimeScriptValue return_value;

    // ok, set int AND the pointer if possible.

    if (need_object && sysimp->Value.Type == kScValObjectFunction)
    {
        // member function call
        auto obj = ToRealMemoryAddress(registers[SREG_OP]);
        return_value = sysimp->Value.ObjPfn(obj, callparams, func_args_count);
    }
    else if (!need_object && sysimp->Value.Type == kScValStaticFunction)
    {
        return_value = sysimp->Value.SPfn(callparams, func_args_count);
    }
    else
    {
        cc_error("invalid pointer type for function call: %d", sysimp->Value.Type);
        assert(0);
    }

    return MachineValueFromRuntimeScriptValue(return_value);
}

uint32_t ccExecutor::CallExternalScriptFunction(const ScriptImport *sysimp)
{
    auto &stackframe = stackframes.back();

    auto need_object = stackframe.next_call_needs_object;
    stackframe.next_call_needs_object = 0; // clear

    auto func_args_count = stackframe.num_args_to_func;
    stackframe.num_args_to_func = -1;

#ifdef DEBUG_MACHINE
    printf("SCMD_CALLEXT: num_args_to_func=%d next_call_needs_object=%d\n", func_args_count, need_object);
#endif

    // If there are nested CALLAS calls, the stack might
    // contain 2 calls worth of parameters, so only
    // push args for this call
    if (func_args_count < 0) { 
        func_args_count = stackframe.callstack.size(); 
    }

    uint32_t *stack_values = stackframe.callstack.data() + stackframe.callstack.size() - func_args_count;

    return CallScriptFunctionDirect(sysimp->Value.IValue, func_args_count, stack_values);
}



ccExecutor coreExecutor;
