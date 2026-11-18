#include "linden_common.h"

#include "nyxui.h"
#include "imgui_internal.h"

#include <cctype>

namespace NyxUI
{
    namespace
    {
        bool sOpen = false;
        bool sFocus = false;
        char sQuery[128] = {};
        int sSel = 0;

        int score(const std::string& text, const char* q)
        {
            if (!*q) return 1;
            int s = 0, run = 0;
            size_t i = 0;
            for (const char* p = q; *p; ++p)
            {
                const char c = (char)tolower((unsigned char)*p);
                bool found = false;
                while (i < text.size())
                {
                    const char t = (char)tolower((unsigned char)text[i++]);
                    if (t == c)
                    {
                        found = true;
                        run = (i >= 2 && run > 0) ? run + 1 : 1;
                        s += 1 + run * 2 + (i == 1 ? 6 : 0);
                        break;
                    }
                    run = 0;
                }
                if (!found) return 0;
            }
            return s;
        }
    }

    void openPalette()
    {
        sOpen = true;
        sFocus = true;
        sQuery[0] = 0;
        sSel = 0;
    }

    void drawPalette()
    {
        if (!sOpen) return;
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const float w = std::min(620.f, disp.x - 40.f);
        ImGui::SetNextWindowPos(ImVec2((disp.x - w) * 0.5f, disp.y * 0.14f));
        ImGui::SetNextWindowSize(ImVec2(w, 0));
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::vec(Theme::SurfaceAlt));
        ImGui::Begin("##palette", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        if (sFocus)
        {
            ImGui::SetKeyboardFocusHere();
            sFocus = false;
        }
        const bool enter = textField("##q", "Type a command, panel or workspace\xE2\x80\xA6", sQuery, sizeof(sQuery), Icon::Search);

        struct Hit
        {
            const Command* c;
            int s;
        };
        std::vector<Hit> hits;
        for (const Command& c : commands())
        {
            const int s = std::max(score(c.label, sQuery), score(c.group + " " + c.label, sQuery) / 2);
            if (s > 0) hits.push_back({ &c, s });
        }
        std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.s > b.s; });
        if (hits.size() > 12) hits.resize(12);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) sSel = std::min(sSel + 1, (int)hits.size() - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) sSel = std::max(sSel - 1, 0);
        sSel = std::clamp(sSel, 0, std::max(0, (int)hits.size() - 1));

        std::string run;
        ImGui::Dummy(ImVec2(0, 2));
        for (int i = 0; i < (int)hits.size(); ++i)
        {
            const Command& c = *hits[i].c;
            ImGui::PushID(i);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float rw = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##row", ImVec2(rw, 34.f))) run = c.id;
            if (ImGui::IsItemHovered()) sSel = i;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (i == sSel) dl->AddRectFilled(p, p + ImVec2(rw, 34.f), Theme::col(Theme::AccentSoft), 8.f);
            dl->AddText(fonts().body, Theme::BodySize, p + ImVec2(12, 8), Theme::col(Theme::Text), c.label.c_str());
            const ImVec2 gs = fonts().body->CalcTextSizeA(Theme::SmallSize, FLT_MAX, 0.f, c.group.c_str());
            float rx = p.x + rw - 12.f;
            if (!c.shortcut.empty())
            {
                const ImVec2 ss = fonts().body->CalcTextSizeA(Theme::SmallSize, FLT_MAX, 0.f, c.shortcut.c_str());
                rx -= ss.x + 12.f;
                dl->AddRectFilled(ImVec2(rx, p.y + 7.f), ImVec2(rx + ss.x + 12.f, p.y + 27.f), Theme::col(Theme::Raised), 6.f);
                dl->AddText(fonts().body, Theme::SmallSize, ImVec2(rx + 6.f, p.y + 9.f), Theme::col(Theme::TextDim), c.shortcut.c_str());
                rx -= 10.f;
            }
            dl->AddText(fonts().body, Theme::SmallSize, ImVec2(rx - gs.x, p.y + 9.f), Theme::col(Theme::TextFaint), c.group.c_str());
            ImGui::PopID();
        }
        if (hits.empty()) caption("Nothing matches.");
        if (enter && !hits.empty()) run = hits[sSel].c->id;
        const bool outside = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || outside) sOpen = false;
        ImGui::End();
        if (!run.empty())
        {
            sOpen = false;
            runCommand(run);
        }
    }
}
