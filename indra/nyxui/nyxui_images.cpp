#include "linden_common.h"

#include "nyxui.h"
#include "imgui_internal.h"

#include <map>

namespace NyxUI
{
    namespace
    {
        std::map<std::string, Image> sImages;

        struct Atlas
        {
            Tex tex = 0;
            int cell = 0;
            int cols = 0;
            float w = 0.f;
            float h = 0.f;
            std::map<unsigned, int> cells;

            bool find(unsigned cp, ImVec2& uv0, ImVec2& uv1, float pad) const
            {
                auto it = tex ? cells.find(cp) : cells.end();
                if (it == cells.end()) return false;
                const int i = it->second;
                const float cx = (float)((i % cols) * cell);
                const float cy = (float)((i / cols) * cell);
                uv0 = ImVec2((cx + pad) / w, (cy + pad) / h);
                uv1 = ImVec2((cx + cell - pad) / w, (cy + cell - pad) / h);
                return true;
            }
        };

        Atlas sLine;
        Atlas sColor;
    }

    void setImage(const std::string& name, Tex tex, float w, float h)
    {
        sImages[name] = Image{ tex, w, h };
    }

    const Image* image(const std::string& name)
    {
        auto it = sImages.find(name);
        return (it == sImages.end() || !it->second.tex) ? nullptr : &it->second;
    }

    void setIconAtlas(Tex tex, int cell, int cols, float atlas_w, float atlas_h, const std::map<unsigned, int>& cells)
    {
        sLine = Atlas{ tex, cell, cols, atlas_w, atlas_h, cells };
    }

    void setColorIconAtlas(Tex tex, int cell, int cols, float atlas_w, float atlas_h, const std::map<unsigned, int>& cells)
    {
        sColor = Atlas{ tex, cell, cols, atlas_w, atlas_h, cells };
    }

    namespace Icon
    {
        unsigned plain(unsigned cp)
        {
            if (!Glow::is(cp)) return cp;
            static const std::map<unsigned, unsigned> m = {
                { Glow::Home, Home }, { Glow::Inventory, Package }, { Glow::Build, Model }, { Glow::Edit, Edit }, { Glow::Move, Walk },
                { Glow::Rotate, Repair }, { Glow::Scale, Crop }, { Glow::Texture, Brush }, { Glow::Appearance, Contact }, { Glow::Avatar, Contact },
                { Glow::People, People }, { Glow::Chat, Chat }, { Glow::Voice, Mic }, { Glow::Gestures, People }, { Glow::Animations, Walk },
                { Glow::Profile, Contact }, { Glow::Camera, CameraCtl }, { Glow::Snapshot, Camera }, { Glow::Phototools, Camera }, { Glow::Mouselook, View },
                { Glow::Fly, Airplane }, { Glow::Walk, Walk }, { Glow::Run, Walk }, { Glow::Sit, Contact }, { Glow::Map, MapPin }, { Glow::Minimap, MapPin },
                { Glow::Radar, Globe }, { Glow::Teleport, Send }, { Glow::Land, Layers }, { Glow::Landmark, Library }, { Glow::Search, Search },
                { Glow::Destinations, Star }, { Glow::Settings, Settings }, { Glow::Debug, Repair }, { Glow::Statistics, Info }, { Glow::Lighting, Lightbulb },
                { Glow::Materials, Layers }, { Glow::Weather, Lightbulb }, { Glow::Browser, Globe }, { Glow::Help, Info },
            };
            auto it = m.find(cp);
            return it == m.end() ? Info : it->second;
        }

        bool hasImage(unsigned cp)
        {
            if (Glow::is(cp) && sColor.tex && sColor.cells.count(cp)) return true;
            cp = plain(cp);
            return sLine.tex && sLine.cells.count(cp);
        }

        void draw(ImDrawList* dl, unsigned cp, const ImVec2& center, float size, ImU32 col)
        {
            ImVec2 uv0, uv1;
            if (Glow::is(cp))
            {
                if (sColor.find(cp, uv0, uv1, 4.f))
                {
                    // The sheet's glyphs sit inside a margin of glow, so draw them a little larger than line icons.
                    const float h = size * 0.58f;
                    const ImU32 tint = IM_COL32(255, 255, 255, (col >> IM_COL32_A_SHIFT) & 0xFF);
                    dl->AddImage(texRef(sColor.tex), ImVec2(center.x - h, center.y - h), ImVec2(center.x + h, center.y + h), uv0, uv1, tint);
                    return;
                }
                cp = plain(cp);
            }
            if (sLine.find(cp, uv0, uv1, 8.f))
            {
                const float h = size * 0.5f;
                dl->AddImage(texRef(sLine.tex), ImVec2(center.x - h, center.y - h), ImVec2(center.x + h, center.y + h), uv0, uv1, col);
                return;
            }
            if (cp == Fire)
            {
                const float h = size * 0.5f;
                const float x = center.x, y = center.y;
                auto P = [&](float u, float v) { return ImVec2(x + u * h, y + v * h); };
                dl->PathLineTo(P(0.f, 0.95f));
                dl->PathBezierCubicCurveTo(P(-0.72f, 0.95f), P(-0.85f, 0.25f), P(-0.45f, -0.2f));
                dl->PathBezierCubicCurveTo(P(-0.42f, 0.1f), P(-0.25f, 0.2f), P(-0.15f, 0.1f));
                dl->PathBezierCubicCurveTo(P(-0.3f, -0.35f), P(-0.05f, -0.75f), P(0.1f, -1.f));
                dl->PathBezierCubicCurveTo(P(0.15f, -0.6f), P(0.8f, -0.35f), P(0.72f, 0.3f));
                dl->PathBezierCubicCurveTo(P(0.68f, 0.75f), P(0.38f, 0.95f), P(0.f, 0.95f));
                dl->PathFillConcave(col);
                const ImU32 a = col & IM_COL32_A_MASK;
                const ImU32 core = a | IM_COL32((int)(((col >> IM_COL32_R_SHIFT) & 0xFF) * 0.35f), (int)(((col >> IM_COL32_G_SHIFT) & 0xFF) * 0.35f), (int)(((col >> IM_COL32_B_SHIFT) & 0xFF) * 0.35f), 0);
                dl->PathLineTo(P(0.f, 0.8f));
                dl->PathBezierCubicCurveTo(P(-0.35f, 0.8f), P(-0.45f, 0.45f), P(-0.25f, 0.2f));
                dl->PathBezierCubicCurveTo(P(-0.1f, 0.35f), P(0.05f, 0.1f), P(0.05f, -0.15f));
                dl->PathBezierCubicCurveTo(P(0.25f, 0.05f), P(0.42f, 0.3f), P(0.36f, 0.52f));
                dl->PathBezierCubicCurveTo(P(0.3f, 0.72f), P(0.18f, 0.8f), P(0.f, 0.8f));
                dl->PathFillConcave(core);
                return;
            }
            if (cp == 0xF011 || cp == 0xF012)
            {
                const float h = size * 0.5f;
                const float x = center.x, y = center.y;
                auto P = [&](float u, float v) { return ImVec2(x + u * h, y + v * h); };
                const float th = std::max(1.5f, size * 0.09f);
                if (cp == 0xF011)
                {
                    dl->AddCircleFilled(P(0.f, -0.32f), h * 0.2f, col, 16);
                    dl->AddRectFilled(P(-0.3f, -0.06f), P(0.3f, 0.42f), col, h * 0.22f, ImDrawFlags_RoundCornersTop);
                    dl->PathArcTo(center, h * 0.86f, -IM_PI * 0.3f, IM_PI * 1.3f, 28);
                    dl->PathStroke(col, 0, th);
                    const ImVec2 e = P(std::cos(-IM_PI * 0.3f) * 0.86f, std::sin(-IM_PI * 0.3f) * 0.86f);
                    dl->AddTriangleFilled(e + ImVec2(-h * 0.08f, -h * 0.34f), e + ImVec2(h * 0.3f, h * 0.06f), e + ImVec2(-h * 0.24f, h * 0.12f), col);
                }
                else
                {
                    dl->AddCircleFilled(P(0.1f, -0.72f), h * 0.18f, col, 16);
                    dl->AddLine(P(0.05f, -0.48f), P(-0.05f, 0.18f), col, th * 1.3f);
                    dl->AddLine(P(0.03f, -0.36f), P(0.55f, -0.85f), col, th);
                    dl->AddLine(P(0.03f, -0.3f), P(-0.55f, -0.12f), col, th);
                    dl->AddLine(P(-0.55f, -0.12f), P(-0.72f, -0.42f), col, th);
                    dl->AddLine(P(-0.05f, 0.18f), P(0.45f, 0.5f), col, th);
                    dl->AddLine(P(0.45f, 0.5f), P(0.42f, 0.95f), col, th);
                    dl->AddLine(P(-0.05f, 0.18f), P(-0.38f, 0.92f), col, th);
                }
                return;
            }
            ImGui::PushFont(fonts().icons, size);
            const char* g = glyph(cp);
            const ImVec2 ts = ImGui::CalcTextSize(g);
            dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f + 1.f), col, g);
            ImGui::PopFont();
        }

        void item(unsigned cp, float size, ImU32 col)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float lh = ImGui::GetTextLineHeight();
            draw(ImGui::GetWindowDrawList(), cp, ImVec2(p.x + size * 0.5f, p.y + lh * 0.5f), size, col);
            ImGui::Dummy(ImVec2(size, lh));
        }
    }
}
