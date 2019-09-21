
#include "script/cc_error.h"
#include "script/runtimescriptvalue.h"
#include "ac/dynobj/cc_dynamicobject.h"
#include "ac/statobj/staticobject.h"
#include "util/memory.h"

#include <string.h> // for memcpy()

using namespace AGS::Common;

//
// NOTE to future optimizers: I am using 'this' ptr here to better
// distinguish Runtime Values.
//

// TODO: test again if stack entry really can hold an offset itself

// TODO: use endian-agnostic method to access global vars
