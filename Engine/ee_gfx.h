#ifndef __AGS_EE_GFX_COLLATED_H
#define __AGS_EE_GFX_COLLATED_H

#include "cn_gfx.h"

#include "gfx/gfxdefines.h"
#include "gfx/ali3dexception.h"
#include "gfx/ali3dsw.h"
#include "gfx/blender.h"
#include "gfx/ddb.h"
#include "gfx/gfx_util.h"
#include "gfx/gfxdriverbase.h"
#include "gfx/gfxdriverfactory.h"
#include "gfx/gfxdriverfactorybase.h"
#include "gfx/gfxfilter.h"
#include "gfx/gfxfilter_aad3d.h"
#include "gfx/gfxfilter_aaogl.h"
#include "gfx/gfxfilter_allegro.h"
#include "gfx/gfxfilter_d3d.h"
#include "gfx/gfxfilter_hqx.h"
#include "gfx/gfxfilter_ogl.h"
#include "gfx/gfxfilter_scaling.h"
#include "gfx/gfxmodelist.h"
#include "gfx/graphicsdriver.h"

#if defined(WINDOWS_VERSION) || defined(LINUX_VERSION) || defined(ANDROID_VERSION) || defined(IOS_VERSION)
#include "gfx/ali3dogl.h"
#include "gfx/ogl_headers.h"
#endif

#ifdef _WIN32
#include "platform/windows/gfx/ali3dd3d.h"
#endif

#endif
