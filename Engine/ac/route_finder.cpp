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
// New jump point search (JPS) A* pathfinder by Martin Sedlak.
//
//=============================================================================

#include "ac/route_finder.h"
#include "ac/common.h"   // quit()
#include "ac/movelist.h"     // MoveList
#include "ac/common_defines.h"
#include <string.h>
#include <math.h>
#include "gfx/bitmap.h"

#include "route_finder_jps.inl"

using AGS::Common::Bitmap;
namespace BitmapHelper = AGS::Common::BitmapHelper;

#define MAKE_INTCOORD(x,y) (((unsigned short)x << 16) | ((unsigned short)y))

const int MAXNAVPOINTS = MAXNEEDSTAGES;
int navpoints[MAXNAVPOINTS];
int num_navpoints;

fixed move_speed_x, move_speed_y;

extern void Display(const char *, ...);
extern void update_polled_stuff_if_runtime();

extern MoveList *mls;

Navigation nav;

void init_pathfinder()
{
}

Bitmap *wallscreen;
<<<<<<< HEAD
=======

// Show debug information on screen and wait for key press
// Incomplete: seems to rely on removed code.
//#define DEBUG_PATHFINDER

>>>>>>> SDL2: key/mouse events
const char *movelibcopyright = "PathFinder library v3.1 (c) 1998, 1999, 2001, 2002 Chris Jones.";
int lastcx, lastcy;

// check the copyright message is intact
#ifdef _MSC_VER
extern void winalert(char *, ...);
#endif

void sync_nav_wallscreen()
{
  // FIXME: this is dumb, but...
  nav.Resize(wallscreen->GetWidth(), wallscreen->GetHeight());

  for (int y=0; y<wallscreen->GetHeight(); y++)
    nav.SetMapRow(y, wallscreen->GetScanLine(y));
}

int can_see_from(int x1, int y1, int x2, int y2)
{
  lastcx = x1;
  lastcy = y1;

  if ((x1 == x2) && (y1 == y2))
    return 1;

  sync_nav_wallscreen();

  return !nav.TraceLine(x1, y1, x2, y2, lastcx, lastcy);
}

// new routing using JPS
int find_route_jps(int fromx, int fromy, int destx, int desty)
{
  sync_nav_wallscreen();

  static std::vector<int> path, cpath;
  path.clear();
  cpath.clear();

  if (nav.NavigateRefined(fromx, fromy, destx, desty, path, cpath) == Navigation::NAV_UNREACHABLE)
    return 0;

  num_navpoints = 0;

  // new behavior: cut path if too complex rather than abort with error message
  int count = std::min<int>((int)cpath.size(), MAXNAVPOINTS);

  for (int i = 0; i<count; i++)
  {
    int x, y;
    nav.UnpackSquare(cpath[i], x, y);

    navpoints[num_navpoints++] = MAKE_INTCOORD(x, y);
  }

  return 1;
}

void set_route_move_speed(int speed_x, int speed_y)
{
  // negative move speeds like -2 get converted to 1/2
  if (speed_x < 0) {
    move_speed_x = itofix(1) / (-speed_x);
  }
  else {
    move_speed_x = itofix(speed_x);
  }

  if (speed_y < 0) {
    move_speed_y = itofix(1) / (-speed_y);
  }
  else {
    move_speed_y = itofix(speed_y);
  }
}

// Calculates the X and Y per game loop, for this stage of the
// movelist
void calculate_move_stage(MoveList * mlsp, int aaa)
{
  // work out the x & y per move. First, opp/adj=tan, so work out the angle
  if (mlsp->pos[aaa] == mlsp->pos[aaa + 1]) {
    mlsp->xpermove[aaa] = 0;
    mlsp->ypermove[aaa] = 0;
    return;
  }

  short ourx = (mlsp->pos[aaa] >> 16) & 0x000ffff;
  short oury = (mlsp->pos[aaa] & 0x000ffff);
  short destx = ((mlsp->pos[aaa + 1] >> 16) & 0x000ffff);
  short desty = (mlsp->pos[aaa + 1] & 0x000ffff);

  // Special case for vertical and horizontal movements
  if (ourx == destx) {
    mlsp->xpermove[aaa] = 0;
    mlsp->ypermove[aaa] = move_speed_y;
    if (desty < oury)
      mlsp->ypermove[aaa] = -mlsp->ypermove[aaa];

    return;
  }

  if (oury == desty) {
    mlsp->xpermove[aaa] = move_speed_x;
    mlsp->ypermove[aaa] = 0;
    if (destx < ourx)
      mlsp->xpermove[aaa] = -mlsp->xpermove[aaa];

    return;
  }

  fixed xdist = itofix(abs(ourx - destx));
  fixed ydist = itofix(abs(oury - desty));

  fixed useMoveSpeed;

  if (move_speed_x == move_speed_y) {
    useMoveSpeed = move_speed_x;
  }
  else {
    // different X and Y move speeds
    // the X proportion of the movement is (x / (x + y))
    fixed xproportion = fixdiv(xdist, (xdist + ydist));

    if (move_speed_x > move_speed_y) {
      // speed = y + ((1 - xproportion) * (x - y))
      useMoveSpeed = move_speed_y + fixmul(xproportion, move_speed_x - move_speed_y);
    }
    else {
      // speed = x + (xproportion * (y - x))
      useMoveSpeed = move_speed_x + fixmul(itofix(1) - xproportion, move_speed_y - move_speed_x);
    }
  }

  fixed angl = fixatan(fixdiv(ydist, xdist));

  // now, since new opp=hyp*sin, work out the Y step size
  //fixed newymove = useMoveSpeed * fsin(angl);
  fixed newymove = fixmul(useMoveSpeed, fixsin(angl));

  // since adj=hyp*cos, work out X step size
  //fixed newxmove = useMoveSpeed * fcos(angl);
  fixed newxmove = fixmul(useMoveSpeed, fixcos(angl));

  if (destx < ourx)
    newxmove = -newxmove;
  if (desty < oury)
    newymove = -newymove;

  mlsp->xpermove[aaa] = newxmove;
  mlsp->ypermove[aaa] = newymove;
}


int find_route(short srcx, short srcy, short xx, short yy, Bitmap *onscreen, int movlst, int nocross, int ignore_walls)
{
  int i;

  wallscreen = onscreen;

  num_navpoints = 0;

  if (ignore_walls || can_see_from(srcx, srcy, xx, yy))
  {
    num_navpoints = 2;
    navpoints[0] = MAKE_INTCOORD(srcx, srcy);
    navpoints[1] = MAKE_INTCOORD(xx, yy);
  } else {
    if ((nocross == 0) && (wallscreen->GetPixel(xx, yy) == 0))
      return 0; // clicked on a wall

    find_route_jps(srcx, srcy, xx, yy);
  }

  if (!num_navpoints)
    return 0;

  // FIXME: really necessary?
  if (num_navpoints == 1)
    navpoints[num_navpoints++] = navpoints[0];

  assert(num_navpoints <= MAXNAVPOINTS);

//    Display("Route from %d,%d to %d,%d - %d stages", srcx,srcy,xx,yy,num_navpoints);

  int mlist = movlst;
  mls[mlist].numstage = num_navpoints;
  memcpy(&mls[mlist].pos[0], &navpoints[0], sizeof(int) * num_navpoints);
//    fprintf(stderr,"stages: %d\n",num_navpoints);

  for (i=0; i<num_navpoints-1; i++)
    calculate_move_stage(&mls[mlist], i);

<<<<<<< HEAD
  mls[mlist].fromx = srcx;
  mls[mlist].fromy = srcy;
  mls[mlist].onstage = 0;
  mls[mlist].onpart = 0;
  mls[mlist].doneflag = 0;
  mls[mlist].lastx = -1;
  mls[mlist].lasty = -1;
  return mlist;
=======
    for (aaa = 0; aaa < numstages - 1; aaa++) {
      calculate_move_stage(&mls[mlist], aaa);
    }

    mls[mlist].fromx = orisrcx;
    mls[mlist].fromy = orisrcy;
    mls[mlist].onstage = 0;
    mls[mlist].onpart = 0;
    mls[mlist].doneflag = 0;
    mls[mlist].lastx = -1;
    mls[mlist].lasty = -1;
#ifdef DEBUG_PATHFINDER
    for (;;) {    
        process_pending_events();
        SDL_Event kpEvent = getTextEventFromQueue();
        int kp = asciiFromEvent(kpEvent);
        if (kp > 0) { break; }
    }
#endif
    return mlist;
  } else {
    return 0;
  }

#ifdef DEBUG_PATHFINDER
  __unnormscreen();
#endif
>>>>>>> SDL2: key/mouse events
}
