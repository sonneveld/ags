//
//  sdlwrap_scratch.c
//  AGSKit
//
//  Created by Nick Sonneveld on 20/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include <stdio.h>

#include "allegro.h"


/* a block of temporary working memory */
void *_scratch_mem = NULL;
int _scratch_mem_size = 0;


void _grow_scratch_mem (int size)
{
    if (size > _scratch_mem_size) {
        size = (size+1023) & 0xFFFFFC00;
        _scratch_mem = realloc(_scratch_mem, size);
        _scratch_mem_size = size;
    }
}