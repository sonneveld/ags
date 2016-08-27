//
//  sdlwrap_maths.c
//  AGSKit
//
//  Created by Nick Sonneveld on 20/08/2016.
//  Copyright © 2016 Adventure Game Studio. All rights reserved.
//

#include <stdio.h>

#include <limits.h>
#include <string.h>
#include <math.h>

#include "allegro.h"
#include "allegro/internal/aintern.h"
#include <SDL2/SDL.h>



/* ftofix and fixtof are used in generic C versions of fixmul and fixdiv */
 fixed ftofix (double x)
{
    if (x > 32767.0) {
        *allegro_errno = ERANGE;
        return 0x7FFFFFFF;
    }
    
    if (x < -32767.0) {
        *allegro_errno = ERANGE;
        return -0x7FFFFFFF;
    }
    
    return (fixed)(x * 65536.0 + (x < 0 ? -0.5 : 0.5));
}


 double fixtof (fixed x)
{
    return (double)x / 65536.0;
}


fixed fixatan(fixed x)
{
    return ftofix(atan(fixtof(x)));
}

fixed fixcos(fixed x)
{
    return ftofix(cos(fixtof(x)));
}

fixed fixsin(fixed x)
{
    return ftofix(sin(fixtof(x)));
}


 fixed fixdiv (fixed x, fixed y)
{
    if (y == 0) {
        *allegro_errno = ERANGE;
        return (x < 0) ? -0x7FFFFFFF : 0x7FFFFFFF;
    }
    else
        return ftofix(fixtof(x) / fixtof(y));
}


 fixed fixmul (fixed x, fixed y)
          {
              return ftofix(fixtof(x) * fixtof(y));
          }

 fixed itofix (int x)
{
    return x << 16;
}


 int fixtoi (fixed x)
{
    return fixfloor(x) + ((x & 0x8000) >> 15);
}

 int fixfloor (fixed x)
{
    /* (x >> 16) is not portable */
    if (x >= 0)
        return (x >> 16);
    else
        return ~((~x) >> 16);
}



