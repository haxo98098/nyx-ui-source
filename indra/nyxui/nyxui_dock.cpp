#include "linden_common.h"

#include "nyxui_dock.h"

#include <cstdio>
#include <cstring>

namespace NyxUI
{
    namespace
    {
        ExternalPanels& externals()
        {
            static ExternalPanels e;
            return e;
        }
    }

    void setExternalPanels(const ExternalPanels& e)
    {
        externals() = e;
    }

    const ExternalPanels& externalPanels()
    {
        return externals();
    }

    namespace Dock
    {
        namespace
        {
            constexpr const char* kPayload = "NYX_TAB";

            bool isExternal(const std::string& id)
            {
                const ExternalPanels& e = externals();
                return !id.empty() && !findPanel(id) && e.exists && e.exists(id);
            }

            std::string panelTitle(const std::string& id)
            {
                if (const Panel* p = findPanel(id)) return p->title;
                const ExternalPanels& e = externals();
                return isExternal(id) && e.title ? e.title(id) : id;
            }
            constexpr float kStrip = 38.f;
            constexpr float kTileStrip = 66.f;

            float stripHeight(const Node* n)
            {
                if (n->tabs.size() <= 1) return 0.f;
                return kStrip;
            }

            struct Pending
            {
                enum Kind { None, Move, Float, Close, Dock } kind = None;
                std::string panel;
                Node* target = nullptr;
                int zone = 0;
            };

            Pending sPending;
            ImRect sViewport;
            bool sHasViewport = false;

            struct Parser
            {
                const std::string& s;
                size_t i = 0;
                void ws() { while (i < s.size() && isspace((unsigned char)s[i])) ++i; }
                std::unique_ptr<Node> node()
                {
                    ws();
                    auto n = std::make_unique<Node>();
                    if (i >= s.size()) return n;
                    const char c = s[i];
                    if (c == '[')
                    {
                        ++i;
                        if (i < s.size() && s[i] == '*')
                        {
                            n->tiles = true;
                            ++i;
                        }
                        std::string tok;
                        while (i < s.size() && s[i] != ']')
                        {
                            if (s[i] == '|')
                            {
                                if (!tok.empty()) n->tabs.push_back(tok);
                                tok.clear();
                            }
                            else tok += s[i];
                            ++i;
                        }
                        ++i;
                        const size_t at = tok.find('@');
                        if (at != std::string::npos)
                        {
                            n->active = atoi(tok.c_str() + at + 1);
                            tok = tok.substr(0, at);
                        }
                        if (!tok.empty()) n->tabs.push_back(tok);
                        n->active = llclamp(n->active, 0, std::max(0, (int)n->tabs.size() - 1));
                        return n;
                    }
                    if (c == 'h' || c == 'v')
                    {
                        n->leaf = false;
                        n->horizontal = c == 'h';
                        ++i;
                        const size_t start = i;
                        while (i < s.size() && s[i] != '(') ++i;
                        n->ratio = llclamp((float)atof(s.substr(start, i - start).c_str()), 0.05f, 0.95f);
                        ++i;
                        n->a = node();
                        ws();
                        if (i < s.size() && s[i] == ',') ++i;
                        n->b = node();
                        ws();
                        if (i < s.size() && s[i] == ')') ++i;
                        return n;
                    }
                    ++i;
                    return n;
                }
            };

            void collapse(std::unique_ptr<Node>& n)
            {
                if (!n || n->leaf) return;
                collapse(n->a);
                collapse(n->b);
                const bool ea = n->a->leaf && n->a->tabs.empty();
                const bool eb = n->b->leaf && n->b->tabs.empty();
                if (ea && !eb) n = std::move(n->b);
                else if (eb && !ea) n = std::move(n->a);
                else if (ea && eb)
                {
                    n->leaf = true;
                    n->a.reset();
                    n->b.reset();
                    n->tabs.clear();
                }
            }

            void removeFrom(Node* n, const std::string& panel, const Node* keep)
            {
                if (!n) return;
                if (n->leaf)
                {
                    if (n == keep) return;
                    for (size_t i = 0; i < n->tabs.size(); ++i)
                    {
                        if (n->tabs[i] == panel)
                        {
                            n->tabs.erase(n->tabs.begin() + i);
                            n->active = llclamp(n->active, 0, std::max(0, (int)n->tabs.size() - 1));
                            return;
                        }
                    }
                    return;
                }
                removeFrom(n->a.get(), panel, keep);
                removeFrom(n->b.get(), panel, keep);
            }

            bool isViewportLeaf(const Node* n)
            {
                if (!n->leaf || n->tabs.size() != 1) return false;
                const Panel* p = findPanel(n->tabs[0]);
                return p && p->viewport;
            }

            void layout(Node* n, const ImRect& r)
            {
                n->rect = r;
                if (n->leaf) return;
                const float g = Theme::Gap;
                if (n->horizontal)
                {
                    const float w = r.GetWidth() - g;
                    const float wa = std::floor(w * n->ratio);
                    layout(n->a.get(), ImRect(r.Min, ImVec2(r.Min.x + wa, r.Max.y)));
                    layout(n->b.get(), ImRect(ImVec2(r.Min.x + wa + g, r.Min.y), r.Max));
                }
                else
                {
                    const float h = r.GetHeight() - g;
                    const float ha = std::floor(h * n->ratio);
                    layout(n->a.get(), ImRect(r.Min, ImVec2(r.Max.x, r.Min.y + ha)));
                    layout(n->b.get(), ImRect(ImVec2(r.Min.x, r.Min.y + ha + g), r.Max));
                }
            }

            std::string ptrId(const char* prefix, const void* p)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s%p", prefix, p);
                return buf;
            }

            bool dragging()
            {
                const ImGuiPayload* pl = ImGui::GetDragDropPayload();
                return pl && pl->IsDataType(kPayload);
            }

            void dragSource(const std::string& panel)
            {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    char buf[96] = {};
                    strncpy(buf, panel.c_str(), sizeof(buf) - 1);
                    ImGui::SetDragDropPayload(kPayload, buf, sizeof(buf));
                    const Panel* p = findPanel(panel);
                    ImGui::PushFont(fonts().strong, 14.f);
                    ImGui::Text("%s  %s", p ? Icon::glyph(p->icon) : "", p ? p->title.c_str() : panel.c_str());
                    ImGui::PopFont();
                    caption("Drop on a panel edge to split, or on its centre to add a tab");
                    ImGui::EndDragDropSource();
                }
            }

            int zoneAt(const ImRect& r, const ImVec2& m)
            {
                const float fx = (m.x - r.Min.x) / std::max(1.f, r.GetWidth());
                const float fy = (m.y - r.Min.y) / std::max(1.f, r.GetHeight());
                const float e = 0.25f;
                const float dl = fx, dr = 1.f - fx, dt = fy, db = 1.f - fy;
                const float m0 = std::min(std::min(dl, dr), std::min(dt, db));
                if (m0 > e) return 0;
                if (m0 == dl) return 1;
                if (m0 == dr) return 2;
                if (m0 == dt) return 3;
                return 4;
            }

            ImRect zoneRect(const ImRect& r, int z)
            {
                const float hw = r.GetWidth() * 0.5f, hh = r.GetHeight() * 0.5f;
                switch (z)
                {
                case 1: return ImRect(r.Min, ImVec2(r.Min.x + hw, r.Max.y));
                case 2: return ImRect(ImVec2(r.Max.x - hw, r.Min.y), r.Max);
                case 3: return ImRect(r.Min, ImVec2(r.Max.x, r.Min.y + hh));
                case 4: return ImRect(ImVec2(r.Min.x, r.Max.y - hh), r.Max);
                default: return r;
                }
            }

            void dropTarget(Node* n, const ImRect& r)
            {
                if (!dragging()) return;
                const ImGuiID id = ImGui::GetID(ptrId("drop", n).c_str());
                if (ImGui::BeginDragDropTargetCustom(r, id))
                {
                    const int z = isViewportLeaf(n) ? (zoneAt(r, ImGui::GetMousePos()) == 0 ? 1 : zoneAt(r, ImGui::GetMousePos())) : zoneAt(r, ImGui::GetMousePos());
                    const ImRect zr = zoneRect(r, z);
                    ImDrawList* fg = ImGui::GetForegroundDrawList();
                    fg->AddRectFilled(zr.Min + ImVec2(4, 4), zr.Max - ImVec2(4, 4), Theme::col(Theme::Accent, 0.18f), 10.f);
                    fg->AddRect(zr.Min + ImVec2(4, 4), zr.Max - ImVec2(4, 4), Theme::col(Theme::Accent, 0.9f), 10.f, 0, 2.f);
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(kPayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
                    {
                        sPending.kind = Pending::Move;
                        sPending.panel = (const char*)pl->Data;
                        sPending.target = n;
                        sPending.zone = z;
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            // Double-click floats a tab, dragging moves it, right-click offers float and close.
            void tabBehaviour(Node* n, int i, bool hov)
            {
                if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    sPending.kind = Pending::Float;
                    sPending.panel = n->tabs[i];
                }
                dragSource(n->tabs[i]);
                if (ImGui::BeginPopupContextItem("##tabctx"))
                {
                    if (ImGui::MenuItem("Float")) { sPending.kind = Pending::Float; sPending.panel = n->tabs[i]; }
                    if (ImGui::MenuItem("Close")) { sPending.kind = Pending::Close; sPending.panel = n->tabs[i]; }
                    ImGui::EndPopup();
                }
            }

            void leafMenu(Node* n, const ImVec2& at)
            {
                ImGui::SetCursorScreenPos(at);
                if (iconButton("##more", Icon::More, "Panel options", false, 30.f)) ImGui::OpenPopup("##leafmore");
                if (ImGui::BeginPopup("##leafmore"))
                {
                    const std::string cur = n->tabs.empty() ? std::string() : n->tabs[n->active];
                    if (ImGui::MenuItem("Float this panel", nullptr, false, !cur.empty())) { sPending.kind = Pending::Float; sPending.panel = cur; }
                    if (ImGui::MenuItem("Close this panel", nullptr, false, !cur.empty())) { sPending.kind = Pending::Close; sPending.panel = cur; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Reset workspace layout")) resetWorkspaceLayout(workspace());
                    ImGui::EndPopup();
                }
            }

            void tabStrip(Node* n, const ImRect& r)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float x = r.Min.x + 8.f;
                const float y = r.Min.y + 6.f;
                const float h = Theme::TabHeight - 4.f;
                for (int i = 0; i < (int)n->tabs.size(); ++i)
                {
                    const Panel* p = findPanel(n->tabs[i]);
                    const std::string title = panelTitle(n->tabs[i]);
                    const bool active = i == n->active;
                    ImGui::PushFont(active ? fonts().strong : fonts().body, 14.f);
                    const ImVec2 ts = ImGui::CalcTextSize(title.c_str());
                    ImGui::PopFont();
                    const bool with_icon = p && n->tabs.size() > 1;
                    const float w = 12.f + (with_icon ? 18.f : 0.f) + ts.x + 12.f;
                    if (x + w > r.Max.x - 40.f && i > 0) break;
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    ImGui::PushID(i);
                    if (ImGui::InvisibleButton("##tab", ImVec2(w, h))) n->active = i;
                    const bool hov = ImGui::IsItemHovered();
                    tabBehaviour(n, i, hov);
                    ImGui::PopID();
                    const ImVec2 a(x, y), b(x + w, y + h);
                    if (active)
                    {
                        dl->AddRectFilled(a, b, Theme::col(Theme::Raised, 0.55f), 9.f);
                        FX::accent(dl, ImVec2(a.x + 10.f, b.y + 3.f), ImVec2(b.x - 10.f, b.y + 5.5f), 2.f, 1.f);
                    }
                    else if (hov) dl->AddRectFilled(a, b, Theme::col(Theme::Raised, 0.35f), 9.f);
                    if (with_icon)
                    {
                        Icon::draw(dl, p->icon, ImVec2(x + 20.f, y + h * 0.5f), 15.f, active ? Theme::col(Theme::Accent) : Theme::col(Theme::TextFaint));
                    }
                    ImGui::PushFont(active ? fonts().strong : fonts().body, 14.f);
                    dl->AddText(ImVec2(x + (with_icon ? 30.f : 12.f), y + (h - ts.y) * 0.5f), active ? Theme::col(Theme::Text) : Theme::col(Theme::TextDim), title.c_str());
                    ImGui::PopFont();
                    x += w + 4.f;
                }
                leafMenu(n, ImVec2(r.Max.x - 36.f, y - 1.f));
                FX::gradient(dl, ImVec2(r.Min.x + 12.f, r.Min.y + kStrip - 1.f), ImVec2(r.Max.x - 12.f, r.Min.y + kStrip), Theme::col(Theme::Border, 0.1f), Theme::col(Theme::Border, 0.9f), 0.f, false, 1.f);
            }

            void drawLeaf(Node* n)
            {
                const ImRect r = n->rect;
                if (r.GetWidth() < 8.f || r.GetHeight() < 8.f) return;
                if (isViewportLeaf(n))
                {
                    sViewport = r;
                    sHasViewport = true;
                    if (dragging())
                    {
                        ImGui::SetNextWindowPos(r.Min);
                        ImGui::SetNextWindowSize(r.GetSize());
                        ImGui::Begin(ptrId("##vpdrop", n).c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
                        dropTarget(n, r);
                        ImGui::End();
                    }
                    return;
                }
                const std::string active_id = n->tabs.empty() ? std::string() : n->tabs[n->active];
                if (isExternal(active_id) && n->tabs.size() <= 1)
                {
                    if (externals().place) externals().place(active_id, r.Min.x, r.Min.y, r.GetWidth(), r.GetHeight());
                    return;
                }
                if (isExternal(active_id))
                {
                    const ImRect strip_r(r.Min, ImVec2(r.Max.x, r.Min.y + stripHeight(n)));
                    ImGui::SetNextWindowPos(strip_r.Min);
                    ImGui::SetNextWindowSize(strip_r.GetSize());
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Theme::Radius);
                    ImGui::Begin(ptrId("##leaf", n).c_str(), nullptr,
                                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    ImGui::PopStyleVar(2);
                    FX::glass(ImGui::GetWindowDrawList(), strip_r.Min, strip_r.Max, Theme::Radius, 0.9f, false);
                    tabStrip(n, strip_r);
                    dropTarget(n, r);
                    ImGui::End();
                    if (externals().place) externals().place(active_id, r.Min.x, r.Min.y + stripHeight(n), r.GetWidth(), r.GetHeight() - stripHeight(n));
                    return;
                }
                FX::shadow(ImGui::GetBackgroundDrawList(), r.Min, r.Max, Theme::Radius, 1.f);
                ImGui::SetNextWindowPos(r.Min);
                ImGui::SetNextWindowSize(r.GetSize());
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Theme::Radius);
                ImGui::Begin(ptrId("##leaf", n).c_str(), nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PopStyleVar(2);
                FX::glass(ImGui::GetWindowDrawList(), r.Min, r.Max, Theme::Radius, 0.9f, false);
                const bool chromeless = n->tabs.size() == 1 && panelChromeless(n->tabs[0]);
                const float strip = chromeless ? 0.f : stripHeight(n);
                if (!chromeless && strip > 0.f) tabStrip(n, r);
                ImGui::SetCursorScreenPos(ImVec2(r.Min.x, r.Min.y + strip));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, chromeless ? ImVec2(4, 2) : ImVec2(16, 12));
                if (ImGui::BeginChild("##content", ImVec2(r.GetWidth(), r.GetHeight() - strip), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    if (!n->tabs.empty())
                    {
                        const Panel* p = findPanel(n->tabs[n->active]);
                        if (p && p->draw) p->draw();
                        else emptyState(Icon::Info, "Missing panel", n->tabs[n->active].c_str());
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
                dropTarget(n, r);
                ImGui::End();
            }

            void drawSplitters(Node* n)
            {
                if (!n || n->leaf) return;
                const ImRect ra = n->a->rect, rb = n->b->rect;
                ImRect gap = n->horizontal ? ImRect(ImVec2(ra.Max.x, n->rect.Min.y), ImVec2(rb.Min.x, n->rect.Max.y))
                                           : ImRect(ImVec2(n->rect.Min.x, ra.Max.y), ImVec2(n->rect.Max.x, rb.Min.y));
                gap.Expand(n->horizontal ? ImVec2(2, 0) : ImVec2(0, 2));
                ImGui::SetNextWindowPos(gap.Min);
                ImGui::SetNextWindowSize(gap.GetSize());
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
                ImGui::Begin(ptrId("##split", n).c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
                ImGui::PopStyleVar(2);
                ImGui::SetCursorScreenPos(gap.Min);
                ImGui::InvisibleButton("##grip", gap.GetSize());
                const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
                if (hot) ImGui::SetMouseCursor(n->horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
                if (ImGui::IsItemActive())
                {
                    const ImVec2 m = ImGui::GetMousePos();
                    const float total = n->horizontal ? n->rect.GetWidth() : n->rect.GetHeight();
                    const float pos = n->horizontal ? m.x - n->rect.Min.x : m.y - n->rect.Min.y;
                    const float min_px = 180.f;
                    n->ratio = llclamp(pos / std::max(1.f, total), min_px / std::max(min_px * 2.f, total), 1.f - min_px / std::max(min_px * 2.f, total));
                }
                if (hot)
                {
                    const ImVec2 c = gap.GetCenter();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    if (n->horizontal) FX::gradient(dl, ImVec2(c.x - 1.5f, gap.Min.y + 24.f), ImVec2(c.x + 1.5f, gap.Max.y - 24.f), Theme::col(Theme::Accent), Theme::col(Theme::Accent2), 2.f, true, 1.f);
                    else FX::gradient(dl, ImVec2(gap.Min.x + 24.f, c.y - 1.5f), ImVec2(gap.Max.x - 24.f, c.y + 1.5f), Theme::col(Theme::Accent), Theme::col(Theme::Accent2), 2.f, false, 1.f);
                }
                ImGui::End();
                drawSplitters(n->a.get());
                drawSplitters(n->b.get());
            }

            void drawLeaves(Node* n)
            {
                if (!n) return;
                if (n->leaf)
                {
                    drawLeaf(n);
                    return;
                }
                drawLeaves(n->a.get());
                drawLeaves(n->b.get());
            }

            void drawFloating(Layout& l)
            {
                const ImVec2 disp = ImGui::GetIO().DisplaySize;
                for (Floating& f : l.floating)
                {
                    const Panel* p = findPanel(f.id);
                    if (!p) continue;
                    if (!f.placed)
                    {
                        if (f.size.x < 100.f) f.size = ImVec2(420.f, 520.f);
                        if (f.pos.x <= 0.f && f.pos.y <= 0.f) f.pos = ImVec2((disp.x - f.size.x) * 0.5f, (disp.y - f.size.y) * 0.4f);
                        ImGui::SetNextWindowPos(f.pos);
                        ImGui::SetNextWindowSize(f.size);
                        f.placed = true;
                    }
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Theme::Radius);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(260, 200));
                    ImGui::Begin(("##float_" + f.id).c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
                    ImGui::PopStyleVar(4);
                    const ImVec2 wp = ImGui::GetWindowPos();
                    const ImVec2 ws = ImGui::GetWindowSize();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->PushClipRectFullScreen();
                    FX::shadow(dl, wp, wp + ws, Theme::Radius, 1.6f);
                    dl->PopClipRect();
                    FX::glass(dl, wp, wp + ws, Theme::Radius, 0.95f, false);
                    ImGui::SetCursorScreenPos(wp);
                    ImGui::InvisibleButton("##head", ImVec2(ws.x - 80.f, kStrip));
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.f))
                    {
                        ImGui::SetWindowPos(wp + ImGui::GetIO().MouseDelta);
                    }
                    dragSource(f.id);
                    Icon::draw(dl, p->icon, ImVec2(wp.x + 22.f, wp.y + 19.f), 16.f, Theme::col(Theme::Accent));
                    ImGui::PushFont(fonts().strong, 14.f);
                    dl->AddText(ImVec2(wp.x + 36.f, wp.y + 10.f), Theme::col(Theme::Text), p->title.c_str());
                    ImGui::PopFont();
                    ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - 72.f, wp.y + 4.f));
                    if (iconButton("##dock", Icon::Pin, "Dock into this workspace", false, 30.f)) { sPending.kind = Pending::Dock; sPending.panel = f.id; }
                    ImGui::SameLine(0, 4);
                    if (iconButton("##close", Icon::Close, "Close", false, 30.f)) { sPending.kind = Pending::Close; sPending.panel = f.id; }
                    FX::accent(dl, ImVec2(wp.x + 14.f, wp.y + kStrip - 2.f), ImVec2(wp.x + ws.x - 14.f, wp.y + kStrip - 0.5f), 1.f, 0.7f);
                    ImGui::SetCursorScreenPos(ImVec2(wp.x, wp.y + kStrip));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
                    if (ImGui::BeginChild("##content", ImVec2(ws.x, ws.y - kStrip), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar))
                    {
                        if (p->draw) p->draw();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    f.pos = ImGui::GetWindowPos();
                    f.size = ImGui::GetWindowSize();
                    ImGui::End();
                }
            }

            void apply(Layout& l)
            {
                Pending p = sPending;
                sPending = Pending();
                if (p.kind == Pending::None || p.panel.empty()) return;
                if ((p.kind == Pending::Close || p.kind == Pending::Float) && isExternal(p.panel))
                {
                    removePanel(l, p.panel);
                    if (externals().undock) externals().undock(p.panel);
                    return;
                }
                if (p.kind == Pending::Close)
                {
                    removePanel(l, p.panel);
                    return;
                }
                if (p.kind == Pending::Float)
                {
                    floatPanel(l, p.panel);
                    return;
                }
                if (p.kind == Pending::Dock)
                {
                    dockPanel(l, p.panel);
                    return;
                }
                if (p.kind == Pending::Move && p.target)
                {
                    Node* t = p.target;
                    if (!t->leaf) return;
                    Node* keep = t;
                    if (p.zone == 0)
                    {
                        if (std::find(t->tabs.begin(), t->tabs.end(), p.panel) == t->tabs.end()) t->tabs.push_back(p.panel);
                        t->active = (int)(std::find(t->tabs.begin(), t->tabs.end(), p.panel) - t->tabs.begin());
                    }
                    else
                    {
                        auto old = std::make_unique<Node>();
                        old->tabs = t->tabs;
                        old->active = t->active;
                        old->tiles = t->tiles;
                        t->tiles = false;
                        old->tabs.erase(std::remove(old->tabs.begin(), old->tabs.end(), p.panel), old->tabs.end());
                        old->active = llclamp(old->active, 0, std::max(0, (int)old->tabs.size() - 1));
                        auto fresh = std::make_unique<Node>();
                        fresh->tabs.push_back(p.panel);
                        keep = fresh.get();
                        t->leaf = false;
                        t->tabs.clear();
                        t->horizontal = p.zone == 1 || p.zone == 2;
                        const bool first = p.zone == 1 || p.zone == 3;
                        t->ratio = isViewportLeaf(old.get()) ? (first ? 0.26f : 0.74f) : 0.5f;
                        if (first) { t->a = std::move(fresh); t->b = std::move(old); }
                        else { t->a = std::move(old); t->b = std::move(fresh); }
                    }
                    removeFrom(l.root.get(), p.panel, keep);
                    l.floating.erase(std::remove_if(l.floating.begin(), l.floating.end(), [&](const Floating& f) { return f.id == p.panel; }), l.floating.end());
                    collapse(l.root);
                }
            }
        }

        std::unique_ptr<Node> parse(const std::string& text)
        {
            Parser p{ text };
            return p.node();
        }

        std::string serialize(const Node* n)
        {
            if (!n) return "[]";
            if (n->leaf)
            {
                std::string s = n->tiles ? "[*" : "[";
                for (size_t i = 0; i < n->tabs.size(); ++i)
                {
                    if (i) s += "|";
                    s += n->tabs[i];
                }
                if (n->active > 0) s += "@" + std::to_string(n->active);
                return s + "]";
            }
            char r[32];
            snprintf(r, sizeof(r), "%.3f", n->ratio);
            return std::string(n->horizontal ? "h" : "v") + r + "(" + serialize(n->a.get()) + "," + serialize(n->b.get()) + ")";
        }

        Node* findLeaf(Node* n, const std::string& panel)
        {
            if (!n) return nullptr;
            if (n->leaf) return std::find(n->tabs.begin(), n->tabs.end(), panel) != n->tabs.end() ? n : nullptr;
            if (Node* f = findLeaf(n->a.get(), panel)) return f;
            return findLeaf(n->b.get(), panel);
        }

        Node* largestLeaf(Node* n, bool allow_viewport)
        {
            if (!n) return nullptr;
            if (n->leaf) return (!allow_viewport && isViewportLeaf(n)) ? nullptr : n;
            Node* a = largestLeaf(n->a.get(), allow_viewport);
            Node* b = largestLeaf(n->b.get(), allow_viewport);
            if (!a) return b;
            if (!b) return a;
            return a->rect.GetArea() >= b->rect.GetArea() ? a : b;
        }

        bool contains(const Layout& l, const std::string& panel)
        {
            if (findLeaf(l.root.get(), panel)) return true;
            for (const Floating& f : l.floating)
            {
                if (f.id == panel) return true;
            }
            return false;
        }

        void removePanel(Layout& l, const std::string& panel)
        {
            const Panel* p = findPanel(panel);
            if (p && p->viewport) return;
            removeFrom(l.root.get(), panel, nullptr);
            l.floating.erase(std::remove_if(l.floating.begin(), l.floating.end(), [&](const Floating& f) { return f.id == panel; }), l.floating.end());
            collapse(l.root);
        }

        void floatPanel(Layout& l, const std::string& panel)
        {
            const Panel* p = findPanel(panel);
            if (!p || p->viewport) return;
            Node* leaf = findLeaf(l.root.get(), panel);
            Floating f;
            f.id = panel;
            if (leaf)
            {
                f.pos = leaf->rect.Min + ImVec2(40.f, 40.f);
                f.size = ImVec2(std::max(360.f, leaf->rect.GetWidth()), std::max(320.f, leaf->rect.GetHeight() * 0.8f));
            }
            removeFrom(l.root.get(), panel, nullptr);
            collapse(l.root);
            for (const Floating& e : l.floating)
            {
                if (e.id == panel) return;
            }
            l.floating.push_back(f);
        }

        void dockPanel(Layout& l, const std::string& panel)
        {
            l.floating.erase(std::remove_if(l.floating.begin(), l.floating.end(), [&](const Floating& f) { return f.id == panel; }), l.floating.end());
            if (findLeaf(l.root.get(), panel)) return;
            Node* t = largestLeaf(l.root.get(), false);
            if (!t)
            {
                Node* v = largestLeaf(l.root.get(), true);
                if (!v) return;
                auto old = std::make_unique<Node>();
                old->tabs = v->tabs;
                auto fresh = std::make_unique<Node>();
                fresh->tabs.push_back(panel);
                v->leaf = false;
                v->tabs.clear();
                v->horizontal = true;
                v->ratio = 0.74f;
                v->a = std::move(old);
                v->b = std::move(fresh);
                return;
            }
            t->tabs.push_back(panel);
            t->active = (int)t->tabs.size() - 1;
        }

        void draw(Layout& l, const ImRect& area)
        {
            sHasViewport = false;
            if (!l.root) l.root = std::make_unique<Node>();
            if (externals().frameBegin) externals().frameBegin();
            layout(l.root.get(), area);
            drawSplitters(l.root.get());
            drawLeaves(l.root.get());
            drawFloating(l);
            apply(l);
            if (externals().frameEnd) externals().frameEnd();
        }

        bool viewport(ImRect& out)
        {
            out = sViewport;
            return sHasViewport;
        }
    }
}
