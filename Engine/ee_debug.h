#ifndef __AGS_EE_DEBUG_COLLATED_H
#define __AGS_EE_DEBUG_COLLATED_H

#include "cn_debug.h"

#include "debug/agseditordebugger.h"
#include "debug/consoleoutputtarget.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "debug/dummyagsdebugger.h"
#include "debug/filebasedagsdebugger.h"
#include "debug/logfile.h"
#include "debug/messagebuffer.h"

#ifdef _WIN32
#include "platform/windows/debug/namedpipesagsdebugger.h"
#endif

#endif
