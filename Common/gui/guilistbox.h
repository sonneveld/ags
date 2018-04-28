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

#ifndef __AC_GUILISTBOX_H
#define __AC_GUILISTBOX_H

#include <vector>
#include "gui/guiobject.h"
#include "util/string.h"

struct GUIListBox:public GUIObject
{
  AGS::Common::String items[MAX_LISTBOX_ITEMS];
  short saveGameIndex[MAX_LISTBOX_ITEMS];
  int numItems, selected, topItem, mousexp, mouseyp;
  int rowheight, num_items_fit;
  int font, textcol, backcol, exflags;
  int selectedbgcol;
  int alignment, reserved1;
  void WriteToFile(Common::Stream *out) override;
  void ReadFromFile(Common::Stream *in, GuiVersion gui_version) override;
  int  AddItem(const char *toadd);
  int  InsertItem(int index, const char *toadd);
  void SetItemText(int index, const char *newtext);
  void RemoveItem(int index);
  void Clear();
  void Draw(Common::Bitmap *ds) override;
  int  IsInRightMargin(int x);
  int  GetIndexFromCoordinates(int x, int y);
  void ChangeFont(int newFont);
  int MouseDown() override;
  
  void MouseMove(int nx, int ny) override
  {
    mousexp = nx - x;
    mouseyp = ny - y;
  }

  void Resized() override;

  void reset()
  {
    GUIObject::init();
    mousexp = 0;
    mouseyp = 0;
    activated = 0;
    numItems = 0;
    topItem = 0;
    selected = 0;
    font = 0;
    textcol = 0;
    selectedbgcol = 16;
    backcol = 7;
    exflags = 0;
    rowheight = 0;
    num_items_fit = 0;
    alignment = GALIGN_LEFT;
    numSupportedEvents = 1;
    supportedEvents[0] = "SelectionChanged";
    supportedEventArgs[0] = "GUIControl *control";
  }

  GUIListBox() {
    reset();
  }

  virtual ~GUIListBox()
  {
    for (int i = 0; i < numItems; i++)
      items[i].Free();
  }

private:
  int numItemsTemp;

  void Draw_items_fix();
  void Draw_items_unfix();
  void Draw_set_oritext(char *oritext, const char *text);
};

extern std::vector<GUIListBox> guilist;
extern int numguilist;

#endif // __AC_GUILISTBOX_H
