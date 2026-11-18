#include "linden_common.h"

#include "nyxui.h"

#include <map>
#include <sys/stat.h>

namespace
{
    bool sDark = true;
    ImVec4 sPalette[NyxUI::Theme::RoleCount];
    NyxUI::Fonts sFonts;

    ImVec4 hex(unsigned rgb, float a = 1.f)
    {
        return ImVec4(((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, a);
    }

    bool exists(const std::string& p)
    {
        struct stat st;
        return !p.empty() && stat(p.c_str(), &st) == 0;
    }

    std::string pick(std::initializer_list<std::string> paths)
    {
        for (const std::string& p : paths)
        {
            if (exists(p)) return p;
        }
        return std::string();
    }

    ImFont* load(const std::string& path, float size, bool merge_icons, const std::string& icon_path)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFont* f = nullptr;
        if (!path.empty())
        {
            ImFontConfig cfg;
            cfg.RasterizerDensity = NyxUI::pixelScale();
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            f = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &cfg);
        }
        if (!f) f = io.Fonts->AddFontDefault();
        if (merge_icons && !icon_path.empty())
        {
            ImFontConfig icfg;
            icfg.RasterizerDensity = NyxUI::pixelScale();
            icfg.MergeMode = true;
            icfg.GlyphOffset = ImVec2(0.f, 2.f);
            icfg.GlyphMinAdvanceX = size;
            io.Fonts->AddFontFromFileTTF(icon_path.c_str(), size, &icfg);
        }
        return f;
    }
}

namespace NyxUI
{
    const Fonts& fonts()
    {
        return sFonts;
    }

    namespace Icon
    {
        const char* glyph(unsigned cp)
        {
            cp = plain(cp);
            static std::map<unsigned, std::string> cache;
            auto it = cache.find(cp);
            if (it != cache.end()) return it->second.c_str();
            std::string s;
            if (cp < 0x80) s += (char)cp;
            else if (cp < 0x800)
            {
                s += (char)(0xC0 | (cp >> 6));
                s += (char)(0x80 | (cp & 0x3F));
            }
            else
            {
                s += (char)(0xE0 | (cp >> 12));
                s += (char)(0x80 | ((cp >> 6) & 0x3F));
                s += (char)(0x80 | (cp & 0x3F));
            }
            return cache.emplace(cp, s).first->second.c_str();
        }
    }

    namespace Theme
    {
        ImVec4 vec(Role r, float alpha)
        {
            ImVec4 c = sPalette[r];
            c.w *= alpha;
            return c;
        }

        ImU32 col(Role r, float alpha)
        {
            return ImGui::ColorConvertFloat4ToU32(vec(r, alpha));
        }

        bool dark()
        {
            return sDark;
        }

        void apply(bool dark_mode)
        {
            sDark = dark_mode;
            if (dark_mode)
            {
                sPalette[Base] = hex(0x151517);
                sPalette[Surface] = hex(0x202023);
                sPalette[SurfaceAlt] = hex(0x303033);
                sPalette[Raised] = hex(0x414144);
                sPalette[Border] = hex(0x505055);
                sPalette[Text] = hex(0xEDF1F5);
                sPalette[TextDim] = hex(0xB9C3CD);
                sPalette[TextFaint] = hex(0xA1ADB9);
                sPalette[Accent] = hex(0xABA1F2);
                sPalette[AccentHover] = hex(0xC8BFFF);
                sPalette[AccentSoft] = hex(0x555078);
                sPalette[Accent2] = hex(0xD4CBFF);
                sPalette[Good] = hex(0x34D399);
                sPalette[Warn] = hex(0xFBBF24);
                sPalette[Bad] = hex(0xF7616F);
            }
            else
            {
                sPalette[Base] = hex(0xDFE3EE);
                sPalette[Surface] = hex(0xF8F9FC);
                sPalette[SurfaceAlt] = hex(0xFFFFFF);
                sPalette[Raised] = hex(0xE8EBF3);
                sPalette[Border] = hex(0xD3D8E4);
                sPalette[Text] = hex(0x151821);
                sPalette[TextDim] = hex(0x4E5568);
                sPalette[TextFaint] = hex(0x8A90A0);
                sPalette[Accent] = hex(0x355E85);
                sPalette[AccentHover] = hex(0x284E72);
                sPalette[AccentSoft] = hex(0x355E85, 0.12f);
                sPalette[Accent2] = hex(0x557A9D);
                sPalette[Good] = hex(0x10B981);
                sPalette[Warn] = hex(0xD97706);
                sPalette[Bad] = hex(0xE11D48);
            }

            ImGuiStyle& s = ImGui::GetStyle();
            s.WindowPadding = ImVec2(16, 14);
            s.FramePadding = ImVec2(11, 8);
            s.CellPadding = ImVec2(8, 6);
            s.ItemSpacing = ImVec2(9, 9);
            s.ItemInnerSpacing = ImVec2(8, 6);
            s.IndentSpacing = 18;
            s.ScrollbarSize = 8;
            s.GrabMinSize = 14;
            s.WindowBorderSize = 1;
            s.ChildBorderSize = 0;
            s.PopupBorderSize = 1;
            s.FrameBorderSize = 1;
            s.TabBorderSize = 1;
            s.WindowRounding = 12;
            s.ChildRounding = 8;
            s.FrameRounding = 6;
            s.PopupRounding = 10;
            s.ScrollbarRounding = 8;
            s.GrabRounding = 9;
            s.TabRounding = 7;
            s.WindowTitleAlign = ImVec2(0.f, 0.5f);
            s.SeparatorTextBorderSize = 1;
            s.AntiAliasedLines = true;
            s.AntiAliasedFill = true;

            ImVec4* c = s.Colors;
            c[ImGuiCol_Text] = vec(Text);
            c[ImGuiCol_TextDisabled] = vec(TextFaint);
            c[ImGuiCol_WindowBg] = vec(Surface);
            c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_PopupBg] = vec(SurfaceAlt, 0.97f);
            c[ImGuiCol_Border] = vec(Border, 0.9f);
            c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_FrameBg] = vec(Raised, dark_mode ? 0.7f : 1.f);
            c[ImGuiCol_FrameBgHovered] = vec(Raised);
            c[ImGuiCol_FrameBgActive] = vec(AccentSoft);
            c[ImGuiCol_TitleBg] = vec(Surface);
            c[ImGuiCol_TitleBgActive] = vec(SurfaceAlt);
            c[ImGuiCol_TitleBgCollapsed] = vec(Surface);
            c[ImGuiCol_MenuBarBg] = vec(Surface);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_ScrollbarGrab] = vec(Border);
            c[ImGuiCol_ScrollbarGrabHovered] = vec(TextFaint);
            c[ImGuiCol_ScrollbarGrabActive] = vec(Accent);
            c[ImGuiCol_CheckMark] = vec(Accent);
            c[ImGuiCol_SliderGrab] = vec(Accent);
            c[ImGuiCol_SliderGrabActive] = vec(Accent2);
            c[ImGuiCol_Button] = vec(SurfaceAlt);
            c[ImGuiCol_ButtonHovered] = vec(Raised);
            c[ImGuiCol_ButtonActive] = vec(AccentSoft);
            c[ImGuiCol_Header] = vec(AccentSoft);
            c[ImGuiCol_HeaderHovered] = vec(Raised);
            c[ImGuiCol_HeaderActive] = vec(AccentSoft);
            c[ImGuiCol_Separator] = vec(Border);
            c[ImGuiCol_SeparatorHovered] = vec(Accent);
            c[ImGuiCol_SeparatorActive] = vec(AccentHover);
            c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_ResizeGripHovered] = vec(Accent, 0.5f);
            c[ImGuiCol_ResizeGripActive] = vec(Accent);
            c[ImGuiCol_Tab] = vec(SurfaceAlt);
            c[ImGuiCol_TabHovered] = vec(Raised);
            c[ImGuiCol_TabSelected] = vec(AccentSoft);
            c[ImGuiCol_TabSelectedOverline] = vec(Accent);
            c[ImGuiCol_TabDimmed] = vec(SurfaceAlt, 0.8f);
            c[ImGuiCol_TabDimmedSelected] = vec(Raised);
            c[ImGuiCol_PlotLines] = vec(Accent);
            c[ImGuiCol_PlotHistogram] = vec(Accent);
            c[ImGuiCol_TableHeaderBg] = vec(SurfaceAlt);
            c[ImGuiCol_TableBorderStrong] = vec(Border);
            c[ImGuiCol_TableBorderLight] = vec(Border, 0.6f);
            c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_TableRowBgAlt] = vec(Raised, 0.3f);
            c[ImGuiCol_TextSelectedBg] = vec(Accent, 0.35f);
            c[ImGuiCol_DragDropTarget] = vec(Accent);
            c[ImGuiCol_NavCursor] = vec(Accent);
            c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, dark_mode ? 0.55f : 0.3f);
        }
    }

    bool loadFonts(const std::string& font_dir)
    {
        const std::string win = "C:\\Windows\\Fonts\\";
        const std::string body = pick({ win + "segoeui.ttf", font_dir + "/InterVariableFont.ttf", font_dir + "/DejaVuSans.ttf" });
        const std::string strong = pick({ win + "seguisb.ttf", win + "segoeuib.ttf", body });
        const std::string light = pick({ win + "segoeuisl.ttf", body });
        const std::string icons = pick({ win + "SegoeIcons.ttf", win + "segmdl2.ttf" });
        sFonts.body = load(body, Theme::BodySize, true, icons);
        sFonts.strong = load(strong, Theme::BodySize, true, icons);
        sFonts.light = load(light, Theme::HeroSize, false, std::string());
        sFonts.icons = icons.empty() ? sFonts.body : load(icons, 16.f, false, std::string());
        ImGui::GetIO().FontDefault = sFonts.body;
        return true;
    }
}
