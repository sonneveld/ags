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

#ifndef __AC_GUIINV_H
#define __AC_GUIINV_H

#include <vector>
#include "gui/guiobject.h"

struct GUIInv:public GUIObject
{
  int isover;
  int charId;   // whose inventory (-1 = current player)
  int itemWidth, itemHeight;
  int topIndex;

  int itemsPerLine, numLines;  // not persisted

  void WriteToFile(Common::Stream *out) override;
  void ReadFromFile(Common::Stream *in, GuiVersion gui_version) override;

  void CalculateNumCells();

  void Resized() override {
    CalculateNumCells();
  }

  int CharToDisplay();

  // This function has distinct implementations in Engine and Editor
  void Draw(Common::Bitmap *ds) override;

  void MouseOver() override
  {
    isover = 1;
  }

  void MouseLeave() override
  {
    isover = 0;
  }

  void MouseUp() override
  {
    if (isover)
      activated = 1;
  }

  GUIInv() {
    isover = 0;
    numSupportedEvents = 0;
    charId = -1;
    itemWidth = 40;
    itemHeight = 22;
    topIndex = 0;
    CalculateNumCells();
  }
};

extern std::vector<GUIInv> guiinv;
extern int numguiinv;

#endif // __AC_GUIINV_H
