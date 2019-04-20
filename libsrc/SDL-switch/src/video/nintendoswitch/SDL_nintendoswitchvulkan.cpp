/*
Simple DirectMedia Layer
Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_VULKAN && SDL_VIDEO_DRIVER_NINTENDO_SWITCH

#include "SDL_nintendoswitchvideo.h"

extern "C" {
#include "SDL_assert.h"
#include "SDL_loadso.h"
#include "SDL_nintendoswitchvulkan.h"
#include "SDL_syswm.h"

/* we do VK_NO_PROTOTYPES, but we statically link this on the Switch. */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName);
}


int NINTENDOSWITCH_Vulkan_LoadLibrary(_THIS, const char *path)
{
    VkExtensionProperties *extensions = NULL;
    Uint32 i, extensionCount = 0;
    SDL_bool hasSurfaceExtension = SDL_FALSE;
    SDL_bool hasNnViSurfaceExtension = SDL_FALSE;

    /* Load the Vulkan loader library */
    if (!path)
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    if (!path)
        path = "libvulkan.so";
    SDL_strlcpy(_this->vulkan_config.loader_path, path,
        SDL_arraysize(_this->vulkan_config.loader_path));
    _this->vulkan_config.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (PFN_vkEnumerateInstanceExtensionProperties) vkGetInstanceProcAddr(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!_this->vulkan_config.vkEnumerateInstanceExtensionProperties)
        goto fail;
    extensions = SDL_Vulkan_CreateInstanceExtensionsList(
        (PFN_vkEnumerateInstanceExtensionProperties)
        _this->vulkan_config.vkEnumerateInstanceExtensionProperties,
        &extensionCount);
    if (!extensions)
        goto fail;
    for (i = 0; i < extensionCount; i++)
    {
        if (SDL_strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0)
            hasSurfaceExtension = SDL_TRUE;
        else if (SDL_strcmp(VK_NN_VI_SURFACE_EXTENSION_NAME, extensions[i].extensionName) == 0)
            hasNnViSurfaceExtension = SDL_TRUE;
    }
    SDL_free(extensions);
    if (!hasSurfaceExtension)
    {
        SDL_SetError("Installed Vulkan doesn't implement the "
            VK_KHR_SURFACE_EXTENSION_NAME " extension");
        goto fail;
    }
    else if (!hasNnViSurfaceExtension)
    {
        SDL_SetError("Installed Vulkan doesn't implement the "
            VK_NN_VI_SURFACE_EXTENSION_NAME "extension");
        goto fail;
    }
    return 0;

fail:
    return -1;
}

void NINTENDOSWITCH_Vulkan_UnloadLibrary(_THIS)
{
}

SDL_bool NINTENDOSWITCH_Vulkan_GetInstanceExtensions(_THIS,
    SDL_Window *window,
    unsigned *count,
    const char **names)
{
    static const char *const extensionsForSwitch[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_NN_VI_SURFACE_EXTENSION_NAME
    };
    if (!_this->vulkan_config.vkGetInstanceProcAddr)
    {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }
    return SDL_Vulkan_GetInstanceExtensions_Helper(
        count, names, SDL_arraysize(extensionsForSwitch),
        extensionsForSwitch);
}

SDL_bool NINTENDOSWITCH_Vulkan_CreateSurface(_THIS,
    SDL_Window *window,
    VkInstance instance,
    VkSurfaceKHR *surface)
{
    if (!_this->vulkan_config.vkGetInstanceProcAddr)
    {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }

    SDL_WindowData *windowData = (SDL_WindowData *)window->driverdata;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr;
    PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN =
        (PFN_vkCreateViSurfaceNN)vkGetInstanceProcAddr(
        (VkInstance)instance,
            "vkCreateViSurfaceNN");
    VkViSurfaceCreateInfoNN createInfo;
    VkResult result;

    if (!vkCreateViSurfaceNN)
    {
        SDL_SetError(VK_NN_VI_SURFACE_EXTENSION_NAME
            " extension is not enabled in the Vulkan instance.");
        return SDL_FALSE;
    }
    SDL_zero(createInfo);
    createInfo.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = windowData->vi_window;
    result = vkCreateViSurfaceNN(instance, &createInfo,
        NULL, surface);
    if (result != VK_SUCCESS)
    {
        SDL_SetError("vkCreateViSurfaceNN failed: %s",
            SDL_Vulkan_GetResultString(result));
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

#endif

/* vi: set ts=4 sw=4 expandtab: */

