#include "linden_common.h"

#include "nyxui.h"
#include "imgui_internal.h"

namespace NyxUI
{
    using namespace Theme;

    bool iconButton(const char* id, unsigned icon, const char* tip, bool active, float size)
    {
        ImGui::PushID(id);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##b", ImVec2(size, size));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const float h = FX::anim(ImGui::GetItemID(), hovered ? 1.f : 0.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 b(p.x + size, p.y + size);
        const float r = size * 0.32f;
        if (active)
        {
            FX::accent(dl, p, b, r, 0.9f);
        }
        else if (h > 0.01f)
        {
            dl->AddRectFilled(p, b, col(Raised, 0.9f * h), r);
            dl->AddRect(p, b, col(Border, h), r);
        }
        if (held) dl->AddRectFilled(p, b, col(AccentSoft), r);
        const ImU32 gc = active ? IM_COL32(255, 255, 255, 255) : (hovered ? col(Text) : col(TextDim));
        Icon::draw(dl, icon, ImVec2(p.x + size * 0.5f, p.y + size * 0.5f - h), size * 0.52f, gc);
        if (hovered && tip && *tip)
        {
            ImGui::SetItemTooltip("%s", tip);
        }
        ImGui::PopID();
        return pressed;
    }

    bool pillButton(const char* id, unsigned icon, const char* label, bool active)
    {
        ImGui::PushID(id);
        const char* g = Icon::glyph(icon);
        ImGui::PushFont(fonts().icons, 15.f);
        const ImVec2 is = ImGui::CalcTextSize(g);
        ImGui::PopFont();
        ImGui::PushFont(active ? fonts().strong : fonts().body, 14.f);
        const ImVec2 ls = ImGui::CalcTextSize(label);
        const float h = 34.f;
        const float w = 14.f + is.x + 8.f + ls.x + 16.f;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##p", ImVec2(w, h));
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (active) FX::accent(dl, p, ImVec2(p.x + w, p.y + h), h * 0.5f, 0.95f);
        else if (hovered) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(Raised, 0.8f), h * 0.5f);
        const ImU32 tc = active ? IM_COL32(255, 255, 255, 255) : (hovered ? col(Text) : col(TextDim));
        if (icon) Icon::draw(dl, icon, ImVec2(p.x + 14.f + is.x * 0.5f, p.y + h * 0.5f), 15.f, tc);
        dl->AddText(ImVec2(p.x + 14.f + is.x + 8.f, p.y + (h - ls.y) * 0.5f), tc, label);
        ImGui::PopFont();
        ImGui::PopID();
        return pressed;
    }

    bool textTabs(const char* id, const char* const* labels, int n, int& sel)
    {
        ImGui::PushID(id);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const float tw = w / std::max(1, n), h = 32.f;
        const int before = sel;
        dl->AddRectFilled(p, p + ImVec2(w, h), col(Raised, 0.35f), 9.f);
        for (int i = 0; i < n; ++i)
        {
            const ImVec2 a(p.x + tw * i, p.y);
            ImGui::SetCursorScreenPos(a);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##tt", ImVec2(tw, h))) sel = i;
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool on = sel == i;
            if (on)
            {
                dl->AddRectFilled(a + ImVec2(2.f, 2.f), a + ImVec2(tw - 2.f, h - 2.f), col(Raised, 0.95f), 8.f);
                FX::accent(dl, a + ImVec2(tw * 0.22f, h - 3.f), a + ImVec2(tw * 0.78f, h - 1.f), 1.f, 1.f);
            }
            ImFont* f = on ? fonts().strong : fonts().body;
            const ImVec2 ts = f->CalcTextSizeA(12.5f, FLT_MAX, 0.f, labels[i]);
            ImGui::PushClipRect(a, a + ImVec2(tw, h), true);
            dl->AddText(f, 12.5f, a + ImVec2(std::max(2.f, (tw - ts.x) * 0.5f), (h - ts.y) * 0.5f), on || hov ? col(Text) : col(TextDim), labels[i]);
            ImGui::PopClipRect();
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h));
        ImGui::Dummy(ImVec2(w, 6.f));
        ImGui::PopID();
        return sel != before;
    }

    bool tileButton(const char* id, unsigned icon, const char* label, bool active, const ImVec2& size, float icon_size, const char* tip)
    {
        ImGui::PushID(id);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##t", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const float h = FX::anim(ImGui::GetItemID(), hovered ? 1.f : 0.f);
        const float on = FX::anim(ImGui::GetID("##on"), active ? 1.f : 0.f, 12.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 b = p + size;
        const float r = std::min(12.f, size.y * 0.2f);
        if (on > 0.01f)
        {
            FX::gradient(dl, p, b, col(Accent, 0.55f * on), col(Accent2, 0.28f * on), r, true, 1.f);
            dl->AddRect(p, b, col(AccentHover, 0.85f * on), r, 0, 1.2f);
            FX::gradient(dl, p, ImVec2(b.x, p.y + size.y * 0.45f), IM_COL32(255, 255, 255, (int)(34 * on)), IM_COL32(255, 255, 255, 0), r, true, 1.f);
        }
        if (h > 0.01f && on < 0.99f)
        {
            dl->AddRectFilled(p, b, col(Raised, 0.75f * h * (1.f - on)), r);
            dl->AddRect(p, b, col(Border, h * (1.f - on)), r);
        }
        if (held) dl->AddRectFilled(p, b, col(AccentSoft), r);
        const bool has_label = label && *label;
        ImGui::PushFont(active ? fonts().strong : fonts().body, 12.5f);
        const ImVec2 ls = has_label ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
        const float stack = icon_size + (has_label ? 4.f + ls.y : 0.f);
        const float top = p.y + (size.y - stack) * 0.5f;
        const ImU32 ic = IM_COL32(255, 255, 255, hovered || active ? 255 : 228);
        Icon::draw(dl, icon, ImVec2(p.x + size.x * 0.5f, top + icon_size * 0.5f - h), icon_size, Glow::is(icon) ? ic : (active ? IM_COL32(255, 255, 255, 255) : col(hovered ? Text : TextDim)));
        if (has_label)
        {
            const ImU32 tc = active ? IM_COL32(255, 255, 255, 255) : col(hovered ? Text : TextDim);
            ImGui::PushClipRect(p, b, true);
            dl->AddText(ImVec2(p.x + std::max(2.f, (size.x - ls.x) * 0.5f), top + icon_size + 4.f), tc, label);
            ImGui::PopClipRect();
        }
        ImGui::PopFont();
        if (hovered && tip && *tip) ImGui::SetItemTooltip("%s", tip);
        ImGui::PopID();
        return pressed;
    }

    static bool styledButton(const char* label, const ImVec2& size, bool primary)
    {
        ImGui::PushFont(primary ? fonts().strong : fonts().body, 0.f);
        const ImVec2 ls = ImGui::CalcTextSize(label, nullptr, true);
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 sz(size.x > 0.f ? size.x : (size.x < 0.f ? avail.x + size.x + 1.f : ls.x + 36.f), size.y > 0.f ? size.y : ImGui::GetFrameHeight() + 6.f);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton(label, sz);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
        const float h = FX::anim(ImGui::GetItemID(), hovered && !disabled ? 1.f : 0.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 b = p + sz;
        const float r = std::min(12.f, sz.y * 0.36f);
        if (primary)
        {
            FX::accent(dl, p, b, r, disabled ? 0.35f : 1.f);
            FX::gradient(dl, p, ImVec2(b.x, p.y + sz.y * 0.5f), IM_COL32(255, 255, 255, (int)(26 + 18 * h)), IM_COL32(255, 255, 255, 0), r, true, 1.f);
            if (held) dl->AddRectFilled(p, b, IM_COL32(0, 0, 0, 40), r);
        }
        else
        {
            dl->AddRectFilled(p, b, col(Raised, 0.35f + 0.45f * h), r);
            dl->AddRect(p, b, h > 0.5f ? col(Accent, 0.6f) : col(Border), r, 0, 1.f);
            if (held) dl->AddRectFilled(p, b, col(AccentSoft), r);
        }
        const ImU32 tc = primary ? IM_COL32(255, 255, 255, disabled ? 140 : 255) : col(disabled ? TextFaint : Text);
        const char* end = ImGui::FindRenderedTextEnd(label);
        dl->AddText(ImVec2(p.x + (sz.x - ls.x) * 0.5f, p.y + (sz.y - ls.y) * 0.5f), tc, label, end);
        ImGui::PopFont();
        return pressed;
    }

    bool primaryButton(const char* label, const ImVec2& size)
    {
        return styledButton(label, size, true);
    }

    bool ghostButton(const char* label, const ImVec2& size)
    {
        return styledButton(label, size, false);
    }

    bool textField(const char* id, const char* hint, char* buf, size_t len, unsigned icon, bool password, float width)
    {
        ImGui::PushID(id);
        const float w = width < 0.f ? ImGui::GetContentRegionAvail().x : width;
        const float pad_left = icon ? 36.f : 13.f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad_left, 10.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, vec(Raised, dark() ? 0.55f : 1.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, vec(Raised, dark() ? 0.75f : 1.f));
        ImGui::SetNextItemWidth(w);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (password) flags |= ImGuiInputTextFlags_Password;
        const bool entered = ImGui::InputTextWithHint("##f", hint, buf, len, flags);
        const bool active = ImGui::IsItemActive();
        const float f = FX::anim(ImGui::GetItemID(), active ? 1.f : 0.f, 16.f);
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (f > 0.02f) FX::ring(dl, a, b, 11.f, col(Accent, f));
        else dl->AddRect(a, b, col(Border), 11.f, 0, 1.f);
        if (icon)
        {
            Icon::draw(dl, icon, ImVec2(p.x + 21.f, p.y + (b.y - a.y) * 0.5f), 16.f, active ? col(Accent) : col(TextFaint));
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return entered;
    }

    void sectionHeader(const char* text)
    {
        ImGui::Dummy(ImVec2(0, 3));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 3.f), ImVec2(p.x + 3.f, p.y + 13.f), col(Accent), 2.f);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 9.f, p.y));
        ImGui::PushFont(fonts().strong, 11.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, vec(TextDim));
        std::string up(text);
        for (char& c : up) c = (char)toupper((unsigned char)c);
        ImGui::TextUnformatted(up.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void caption(const char* text)
    {
        ImGui::PushFont(fonts().body, SmallSize);
        ImGui::PushStyleColor(ImGuiCol_Text, vec(TextDim));
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void emptyState(unsigned icon, const char* title, const char* body)
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float block_w = std::min(avail.x - 24.f, 320.f);
        const float top = std::max(12.f, avail.y * 0.28f - 40.f);
        ImGui::Dummy(ImVec2(0, top));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 base = ImGui::GetCursorScreenPos();
        const float cx = base.x + avail.x * 0.5f;
        const ImVec2 c(cx, base.y + 30.f);
        FX::glow(dl, c, 64.f, col(Accent), 0.8f);
        FX::gradient(dl, ImVec2(c.x - 30.f, c.y - 30.f), ImVec2(c.x + 30.f, c.y + 30.f), col(Accent, 0.35f), col(Accent2, 0.25f), 30.f, false, 1.f);
        dl->AddCircle(c, 30.f, col(Accent, 0.5f), 48, 1.f);
        Icon::draw(dl, icon, c, 28.f, IM_COL32(255, 255, 255, 235));
        ImGui::Dummy(ImVec2(0, 74.f));
        ImGui::PushFont(fonts().strong, 17.f);
        const ImVec2 ts = ImGui::CalcTextSize(title);
        ImGui::SetCursorPosX(std::max(0.f, (avail.x - ts.x) * 0.5f));
        ImGui::TextUnformatted(title);
        ImGui::PopFont();
        ImGui::PushFont(fonts().body, SmallSize);
        ImGui::PushStyleColor(ImGuiCol_Text, vec(TextDim));
        const float x0 = std::max(0.f, (avail.x - block_w) * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x0);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + block_w);
        const ImVec2 bs = ImGui::CalcTextSize(body, nullptr, false, block_w);
        if (bs.x < block_w) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (block_w - bs.x) * 0.5f);
        ImGui::TextUnformatted(body);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    bool toggleRow(const char* label, const char* detail, bool* v)
    {
        ImGui::PushID(label);
        const float w = ImGui::GetContentRegionAvail().x;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = detail && *detail ? 46.f : 32.f;
        const bool pressed = ImGui::InvisibleButton("##row", ImVec2(w, h));
        if (pressed) *v = !*v;
        const bool hov = ImGui::IsItemHovered();
        const float k = FX::anim(ImGui::GetItemID(), *v ? 1.f : 0.f, 18.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (hov) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(Raised, 0.45f), 10.f);
        dl->AddText(fonts().body, BodySize, ImVec2(p.x + 10.f, p.y + (detail && *detail ? 6.f : (h - BodySize) * 0.5f - 1.f)), col(Text), label);
        if (detail && *detail) dl->AddText(fonts().body, SmallSize, ImVec2(p.x + 10.f, p.y + 26.f), col(TextFaint), detail);
        const float sw = 40.f, sh = 22.f;
        const ImVec2 s0(p.x + w - sw - 10.f, p.y + (h - sh) * 0.5f);
        dl->AddRectFilled(s0, ImVec2(s0.x + sw, s0.y + sh), col(Border), sh * 0.5f);
        if (k > 0.01f) FX::accent(dl, s0, ImVec2(s0.x + sw, s0.y + sh), sh * 0.5f, k);
        const float kx = s0.x + sh * 0.5f + (sw - sh) * k;
        dl->AddCircleFilled(ImVec2(kx, s0.y + sh * 0.5f + 1.f), sh * 0.5f - 2.f, IM_COL32(0, 0, 0, 50), 24);
        dl->AddCircleFilled(ImVec2(kx, s0.y + sh * 0.5f), sh * 0.5f - 3.f, IM_COL32(255, 255, 255, 255), 24);
        ImGui::PopID();
        return pressed;
    }

    bool beginCard(const char* id, const ImVec2& size)
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->ChannelsSplit(2);
        ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        const bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding | (size.y == 0.f ? ImGuiChildFlags_AutoResizeY : 0));
        ImGui::GetStateStorage()->SetFloat(ImGui::GetID("##cardx"), p.x);
        ImGui::GetStateStorage()->SetFloat(ImGui::GetID("##cardy"), p.y);
        return open;
    }

    void endCard()
    {
        const ImVec2 sz = ImGui::GetWindowSize();
        const float x = ImGui::GetStateStorage()->GetFloat(ImGui::GetID("##cardx"));
        const float y = ImGui::GetStateStorage()->GetFloat(ImGui::GetID("##cardy"));
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->ChannelsSetCurrent(0);
        const ImVec2 a(x, y), b(x + sz.x, y + sz.y);
        dl->AddRectFilled(a, b, col(SurfaceAlt, 0.85f), 14.f);
        FX::gradient(dl, a, ImVec2(b.x, a.y + std::min(40.f, sz.y)), col(Accent, 0.08f), IM_COL32(0, 0, 0, 0), 14.f, true, 1.f);
        dl->AddRect(a, b, col(Border, 0.8f), 14.f);
        dl->ChannelsMerge();
    }
}
