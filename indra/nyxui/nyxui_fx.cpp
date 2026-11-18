#include "linden_common.h"

#include "nyxui.h"
#include "imgui_internal.h"

#include <cmath>

namespace NyxUI
{
    namespace FX
    {
        void shadow(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float strength)
        {
            const int steps = 10;
            for (int i = steps; i >= 1; --i)
            {
                const float e = (float)i * 2.4f;
                const float alpha = 0.035f * strength * (1.f - (float)i / (float)(steps + 1));
                dl->AddRectFilled(ImVec2(a.x - e, a.y - e * 0.5f + 4.f), ImVec2(b.x + e, b.y + e + 6.f), IM_COL32(0, 0, 0, (int)(alpha * 255.f)), rounding + e);
            }
        }

        void gradient(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 c0, ImU32 c1, float rounding, bool vertical, float alpha)
        {
            if (b.x <= a.x || b.y <= a.y) return;
            const int v0 = dl->VtxBuffer.Size;
            dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 255), rounding);
            const int v1 = dl->VtxBuffer.Size;
            const ImVec4 f0 = ImGui::ColorConvertU32ToFloat4(c0);
            const ImVec4 f1 = ImGui::ColorConvertU32ToFloat4(c1);
            const float len = vertical ? (b.y - a.y) : (b.x - a.x);
            for (int i = v0; i < v1; ++i)
            {
                ImDrawVert& v = dl->VtxBuffer[i];
                const float vertex_alpha = (float)((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
                float t = ((vertical ? v.pos.y - a.y : v.pos.x - a.x)) / len;
                t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
                ImVec4 c(f0.x + (f1.x - f0.x) * t, f0.y + (f1.y - f0.y) * t, f0.z + (f1.z - f0.z) * t, (f0.w + (f1.w - f0.w) * t) * alpha * vertex_alpha);
                v.col = ImGui::ColorConvertFloat4ToU32(c);
            }
        }

        void accent(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float alpha)
        {
            gradient(dl, a, b, Theme::col(Theme::Accent), Theme::col(Theme::Accent2), rounding, false, alpha);
        }

        void glass(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float alpha, bool with_shadow)
        {
            if (with_shadow) shadow(dl, a, b, rounding, 1.f);
            dl->AddRectFilled(a, b, Theme::col(Theme::Surface, alpha), rounding);
            const float hh = std::min(46.f, (b.y - a.y) * 0.5f);
            gradient(dl, a, ImVec2(b.x, a.y + hh), IM_COL32(255, 255, 255, Theme::dark() ? 14 : 40), IM_COL32(255, 255, 255, 0), rounding, true, 1.f);
            dl->AddRect(a, b, Theme::col(Theme::Border, 0.85f), rounding, 0, 1.f);
            dl->AddLine(ImVec2(a.x + rounding, a.y + 0.5f), ImVec2(b.x - rounding, a.y + 0.5f), IM_COL32(255, 255, 255, Theme::dark() ? 22 : 90), 1.f);
        }

        void glow(ImDrawList*, const ImVec2&, float, ImU32, float)
        {
        }

        void ring(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, ImU32 col)
        {
            dl->AddRect(a, b, col, rounding, 0, 1.5f);
        }

        float anim(ImGuiID id, float target, float speed)
        {
            ImGuiStorage* st = ImGui::GetStateStorage();
            float* v = st->GetFloatRef(id, target);
            const float dt = ImGui::GetIO().DeltaTime;
            *v += (target - *v) * std::min(1.f, dt * speed);
            if (std::fabs(target - *v) < 0.01f) *v = target;
            return *v;
        }
    }
}
