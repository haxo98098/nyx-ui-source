#pragma once

#include "indra_constants.h"
#include "llrect.h"
#include "stdtypes.h"

// The Nyx interface shell around the world view: workspace tabs, the button toolbar, the location
// bar and the status cluster, drawn with NyxUI. Firestorm's own windows and menus stay Firestorm's:
// the tabs dock its floaters into their layouts, the toolbar commits its menu items, and the menu
// button at the top left shows its whole menu bar. Off (NyxShell) gives the classic layout.
namespace FSNyxShell
{
    // Signed in, the setting on, the UI showing and not in mouselook.
    bool active();
    // Draws the shell; LLViewerWindow::draw calls it before the root view, so floaters, menus and
    // popups land on top.
    void draw();
    // A Firestorm floater, menu, popup or top control under this point (scaled UI coordinates).
    bool overFirestormUI(S32 x, S32 y);
    // Raw window pixels, y from the top. True when the shell takes the event.
    bool mouseButton(S32 raw_x, S32 raw_y, EMouseClickType type, bool down, bool over_ui);
    void mouseMove(S32 raw_x, S32 raw_y);
    bool wheel(F32 dx, F32 dy, bool over_ui);
    bool key(KEY key, MASK mask, bool down);
    bool unicodeChar(llwchar uni_char);
    // Typing, Enter or "/" started chat: the shell's chat field takes it. False when the shell is
    // off, so the caller opens the classic chat bar. text goes into the field first.
    bool startChat(const char* text = nullptr);
    // Where the world is drawn, raw window pixels with the origin at the bottom left.
    bool worldRect(LLRect& raw);
}
