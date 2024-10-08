// -*- C++ -*-
// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef __VBA_GAMEBOYCHEATLIST_H__
#define __VBA_GAMEBOYCHEATLIST_H__

#include "../gb/gbCheats.h"
#include "cheatlist.h"

namespace VBA
{

class GameBoyCheatListDialog : public CheatListDialog
{
public:
  GameBoyCheatListDialog(GtkDialog* _pstDialog, const Glib::RefPtr<Gtk::Builder>& refBuilder);

protected:
  void vAddCheat(Glib::ustring sDesc, ECheatType type, Glib::RefPtr<Gtk::TextBuffer> buffer);
  bool vCheatListOpen(const char *file);
  void vCheatListSave(const char *file);
  void vRemoveCheat(int index);
  void vRemoveAllCheats();
  void vToggleCheat(int index, bool enable);
  void vUpdateList(int previous = 0);
};

} // namespace VBA


#endif
