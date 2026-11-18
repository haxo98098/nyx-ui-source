#include "linden_common.h"
#include "indra_constants.h"

#include "nyxui.h"
#include "imgui_internal.h"

namespace
{
    ImGuiKey mapKey(unsigned k)
    {
        if (k >= 'A' && k <= 'Z') return (ImGuiKey)(ImGuiKey_A + (k - 'A'));
        if (k >= '0' && k <= '9') return (ImGuiKey)(ImGuiKey_0 + (k - '0'));
        if (k >= KEY_F1 && k <= KEY_F12) return (ImGuiKey)(ImGuiKey_F1 + (k - KEY_F1));
        switch (k)
        {
        case KEY_RETURN: return ImGuiKey_Enter;
        case KEY_LEFT: return ImGuiKey_LeftArrow;
        case KEY_RIGHT: return ImGuiKey_RightArrow;
        case KEY_UP: return ImGuiKey_UpArrow;
        case KEY_DOWN: return ImGuiKey_DownArrow;
        case KEY_ESCAPE: return ImGuiKey_Escape;
        case KEY_BACKSPACE: return ImGuiKey_Backspace;
        case KEY_DELETE: return ImGuiKey_Delete;
        case KEY_HOME: return ImGuiKey_Home;
        case KEY_END: return ImGuiKey_End;
        case KEY_PAGE_UP: return ImGuiKey_PageUp;
        case KEY_PAGE_DOWN: return ImGuiKey_PageDown;
        case KEY_INSERT: return ImGuiKey_Insert;
        case KEY_TAB: return ImGuiKey_Tab;
        case KEY_HYPHEN: return ImGuiKey_Minus;
        case KEY_EQUALS: return ImGuiKey_Equal;
        case KEY_CAPSLOCK: return ImGuiKey_CapsLock;
        case ' ': return ImGuiKey_Space;
        case ',': return ImGuiKey_Comma;
        case '.': return ImGuiKey_Period;
        case '/': return ImGuiKey_Slash;
        case ';': return ImGuiKey_Semicolon;
        case '\'': return ImGuiKey_Apostrophe;
        case '[': return ImGuiKey_LeftBracket;
        case ']': return ImGuiKey_RightBracket;
        case '\\': return ImGuiKey_Backslash;
        case '`': return ImGuiKey_GraveAccent;
        default: return ImGuiKey_None;
        }
    }
}

namespace NyxUI
{
    void onMouseMove(float x, float y)
    {
        ImGui::GetIO().AddMousePosEvent(x, y);
    }

    void onMouseButton(int button, bool down)
    {
        ImGui::GetIO().AddMouseButtonEvent(button, down);
    }

    void onWheel(float dx, float dy)
    {
        ImGui::GetIO().AddMouseWheelEvent(dx, dy);
    }

    void onKey(unsigned key, bool down, unsigned mask)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, (mask & MASK_CONTROL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mask & MASK_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mask & MASK_ALT) != 0);
        if (key == KEY_CONTROL) { io.AddKeyEvent(ImGuiKey_LeftCtrl, down); return; }
        if (key == KEY_SHIFT) { io.AddKeyEvent(ImGuiKey_LeftShift, down); return; }
        if (key == KEY_ALT) { io.AddKeyEvent(ImGuiKey_LeftAlt, down); return; }
        const ImGuiKey k = mapKey(key);
        if (k != ImGuiKey_None) io.AddKeyEvent(k, down);
    }

    void onChar(unsigned codepoint)
    {
        if (codepoint >= 32 && codepoint != 127) ImGui::GetIO().AddInputCharacter(codepoint);
    }

    void onFocus(bool focused)
    {
        ImGui::GetIO().AddFocusEvent(focused);
    }

    bool wantsMouse()
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool wantsKeyboard()
    {
        const ImGuiContext* g = ImGui::GetCurrentContext();
        return g && (ImGui::GetIO().WantTextInput || g->ActiveId != 0);
    }
}
