#include "linden_common.h"

#include "nyxui_dock.h"
#include "llgl.h"
#include "llglheaders.h"
#include "llcontrol.h"
#include <functional>
#include <set>
#include <algorithm>
#include <cmath>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

extern LLControlGroup gSavedSettings;

namespace NyxUI
{
    bool loadFonts(const std::string& font_dir);

    namespace
    {
        std::vector<Panel> sPanels;
        std::vector<Workspace> sWorkspaces;
        std::vector<Command> sCommands;
        std::map<std::string, Dock::Layout> sLayouts;
        std::string sCurrent;
        std::string sSettingsPath;
        std::map<std::string, std::string> sSettings;
        StatusInfo sStatus;
        Hooks sHooks;
        LoginState sLogin;
        bool sDirty = false;
        double sLastSave = 0.0;

        struct Toast
        {
            std::string text;
            bool error = false;
            float age = 0.f;
        };
        std::vector<Toast> sToasts;

        char sChat[512] = {};
        char sUser[128] = {};
        char sPass[128] = {};
        int sGrid = 0;
        int sStart = 0;
        bool sRemember = true;

        std::map<std::string, std::vector<std::string>> sPendingDock;
        std::set<std::string> sChromeless;
        ImRect sLocationSlot;
        ImRect sShelfSlot;
        int sShelfFrame = -10;
        std::map<std::string, int> sWorkspaceBadges;

        int workspaceBadgeCount(const std::string& id)
        {
            auto it = sWorkspaceBadges.find(id);
            return it == sWorkspaceBadges.end() ? 0 : it->second;
        }

        std::string badgeText(int n)
        {
            return n > 99 ? std::string("99+") : std::to_string(n);
        }

        int countPanels(const Dock::Node* n)
        {
            if (!n) return 0;
            if (n->leaf)
            {
                int c = 0;
                for (const std::string& t : n->tabs)
                {
                    if (t != "world") ++c;
                }
                return c;
            }
            return countPanels(n->a.get()) + countPanels(n->b.get());
        }

        Dock::Layout& current()
        {
            Dock::Layout& l = sLayouts[sCurrent];
            if (!l.root)
            {
                for (const Workspace& w : sWorkspaces)
                {
                    if (w.id == sCurrent) l.root = Dock::parse(w.layout);
                }
                if (!l.root) l.root = std::make_unique<Dock::Node>();
            }
            if (sSettings["ui.layout_llui." + sCurrent] != "1")
            {
                for (const Workspace& w : sWorkspaces)
                    if (w.id == sCurrent) l.root = Dock::parse(w.layout);
                l.floating.clear();
                sSettings["ui.layout_llui." + sCurrent] = "1";
                sSettings["ui.layout_fix3." + sCurrent] = "1";
                sDirty = true;
            }
            // The 2026-10 shell redesign moved panels around in every workspace (and Places and Quick Actions
            // out of the dock): start each one from its new default once.
            if (sSettings["ui.layout_nyx3." + sCurrent] != "1")
            {
                for (const Workspace& w : sWorkspaces)
                    if (w.id == sCurrent) l.root = Dock::parse(w.layout);
                l.floating.clear();
                sSettings["ui.layout_nyx3." + sCurrent] = "1";
                sDirty = true;
            }
            if ((sCurrent == "worldedit" || sCurrent == "look" || sCurrent == "model") && sSettings["ui.layout_fix3." + sCurrent] != "1")
            {
                for (const Workspace& w : sWorkspaces)
                    if (w.id == sCurrent) l.root = Dock::parse(w.layout);
                l.floating.clear();
                sSettings["ui.layout_fix3." + sCurrent] = "1";
                sSettings.erase("ui.world_single");
                sDirty = true;
            }
            const std::string rev_key = "ui.layout_rev." + sCurrent;
            if (sSettings[rev_key] != "2")
            {
                bool all = true;
                for (const Workspace& w : sWorkspaces)
                {
                    if (w.id != sCurrent) continue;
                    std::unique_ptr<Dock::Node> def = Dock::parse(w.layout);
                    std::vector<std::vector<std::string>> groups;
                    std::function<void(Dock::Node*)> walk = [&](Dock::Node* n) {
                        if (!n) return;
                        if (n->leaf) { groups.push_back(n->tabs); return; }
                        walk(n->a.get());
                        walk(n->b.get());
                    };
                    walk(def.get());
                    std::function<Dock::Node*(Dock::Node*, const std::string&)> leafWith = [&](Dock::Node* n, const std::string& id) -> Dock::Node* {
                        if (!n) return nullptr;
                        if (n->leaf) return std::find(n->tabs.begin(), n->tabs.end(), id) != n->tabs.end() ? n : nullptr;
                        if (Dock::Node* r = leafWith(n->a.get(), id)) return r;
                        return leafWith(n->b.get(), id);
                    };
                    for (const auto& g : groups)
                    {
                        for (const std::string& id : g)
                        {
                            if (leafWith(l.root.get(), id)) continue;
                            bool floating = false;
                            for (const Dock::Floating& f : l.floating) floating |= f.id == id;
                            if (floating) continue;
                            if (!findPanel(id) && !(externalPanels().exists && externalPanels().exists(id))) { all = false; continue; }
                            Dock::Node* home = nullptr;
                            for (const std::string& sib : g)
                                if (!home && sib != id) home = leafWith(l.root.get(), sib);
                            if (home) home->tabs.push_back(id);
                            else Dock::dockPanel(l, id);
                        }
                    }
                }
                if (all)
                {
                    sSettings[rev_key] = "2";
                    sDirty = true;
                }
            }
            auto pending = sPendingDock.find(sCurrent);
            if (pending != sPendingDock.end())
            {
                for (const std::string& id : pending->second)
                {
                    if (findPanel(id)) Dock::dockPanel(l, id);
                }
                sPendingDock.erase(pending);
            }
            return l;
        }

        void loadState()
        {
            std::ifstream in(sSettingsPath);
            std::string line;
            while (std::getline(in, line))
            {
                if (line.empty()) continue;
                std::istringstream ss(line);
                std::string kind;
                ss >> kind;
                if (kind == "set")
                {
                    std::string k, v;
                    ss >> k;
                    std::getline(ss, v);
                    if (!v.empty() && v[0] == ' ') v.erase(0, 1);
                    sSettings[k] = v;
                }
                else if (kind == "layout")
                {
                    std::string ws, text;
                    ss >> ws >> text;
                    if (!ws.empty() && !text.empty()) sLayouts[ws].root = Dock::parse(text);
                }
                else if (kind == "float")
                {
                    std::string ws;
                    Dock::Floating f;
                    ss >> ws >> f.id >> f.pos.x >> f.pos.y >> f.size.x >> f.size.y;
                    if (!ws.empty() && !f.id.empty()) sLayouts[ws].floating.push_back(f);
                }
            }
            if (sSettings["ui.floats_docked"] != "1")
            {
                for (auto& kv : sLayouts)
                {
                    for (const Dock::Floating& f : kv.second.floating) sPendingDock[kv.first].push_back(f.id);
                    kv.second.floating.clear();
                }
                sSettings["ui.floats_docked"] = "1";
                sDirty = true;
            }
        }

        bool ctrl()
        {
            return ImGui::GetIO().KeyCtrl;
        }

        void shortcuts()
        {
            if (ImGui::IsKeyPressed(ImGuiKey_K) && ctrl()) openPalette();
            if (ImGui::IsKeyPressed(ImGuiKey_P) && ctrl() && ImGui::GetIO().KeyShift) openPalette();
            for (int i = 0; i < (int)sWorkspaces.size() && i < 9; ++i)
            {
                if (ctrl() && !ImGui::GetIO().KeyShift && ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false)) setWorkspace(sWorkspaces[i].id);
            }
        }

        void barWindow(const char* id, const ImVec2& pos, const ImVec2& size)
        {
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar(3);
        }

        std::vector<BarItem> sTopItems;
        std::vector<BarItem> sBottomItems;
        bool sFocusChat = false;
        bool sAutoSignIn = false;
        bool sAiChat = false;
        bool sChatActive = false;
        char sSearch[160] = {};
        ImRect sChatLogSlot;
        int sChatLogFrame = -10;
        int sLocationFrame = -10;

        bool panelShown(const std::string& id)
        {
            Dock::Layout& l = current();
            if (Dock::Node* leaf = Dock::findLeaf(l.root.get(), id)) return !leaf->tabs.empty() && leaf->tabs[leaf->active] == id;
            for (const Dock::Floating& f : l.floating)
            {
                if (f.id == id) return true;
            }
            return false;
        }

        bool itemActive(const BarItem& it)
        {
            if (it.command.empty()) return it.id == sCurrent;
            if (it.command.rfind("panel.", 0) == 0) return panelShown(it.command.substr(6));
            if (it.command.rfind("workspace.", 0) == 0) return it.command.substr(10) == sCurrent;
            return sHooks.commandActive && sHooks.commandActive(it.command);
        }

        void runItem(const BarItem& it)
        {
            if (it.command.empty()) setWorkspace(it.id);
            else runCommand(it.command);
        }

        std::string itemTip(const BarItem& it)
        {
            std::string tip = it.tip.empty() ? it.title : it.tip;
            if (!it.command.empty()) return tip;
            for (int i = 0; i < (int)sWorkspaces.size() && i < 9; ++i)
            {
                if (sWorkspaces[i].id == it.id) return tip + "  (Ctrl " + std::to_string(i + 1) + ")";
            }
            return tip;
        }

        std::vector<BarItem> topItems()
        {
            if (!sTopItems.empty()) return sTopItems;
            std::vector<BarItem> v;
            for (const Workspace& w : sWorkspaces) v.push_back({ w.id, w.title, w.icon, std::string(), std::string() });
            return v;
        }

        float spacedWidth(ImFont* f, float size, const char* s, float spacing)
        {
            float w = 0.f;
            for (const char* c = s; *c; ++c) w += f->CalcTextSizeA(size, FLT_MAX, 0.f, c, c + 1).x + (c[1] ? spacing : 0.f);
            return w;
        }

        // The butterfly and the spaced-out NYX / VIEWER wordmark; returns where it ends.
        float drawBrand(ImDrawList* dl, float x, float cy)
        {
            if (const Image* m = image("mark"))
            {
                const float s = 52.f;
                dl->AddImage(texRef(m->tex), ImVec2(x, cy - s * 0.5f), ImVec2(x + s, cy + s * 0.5f));
                x += s + 2.f;
            }
            ImFont* big = fonts().light ? fonts().light : fonts().body;
            const float bs = 26.f, ss = 9.5f, sp = 7.f, ssp = 4.4f;
            const char* word = "NYX";
            const char* sub = "VIEWER";
            const float ww = spacedWidth(big, bs, word, sp);
            const float sw = spacedWidth(fonts().body, ss, sub, ssp);
            const float block = std::max(ww, sw);
            const ImVec4 c0 = ImGui::ColorConvertU32ToFloat4(Theme::col(Theme::Accent2));
            const ImVec4 c1 = ImGui::ColorConvertU32ToFloat4(Theme::col(Theme::AccentHover));
            float lx = x + (block - ww) * 0.5f;
            int i = 0;
            for (const char* c = word; *c; ++c, ++i)
            {
                const float t = i / 2.f;
                const ImVec4 cc(0.3f + 0.7f * (c0.x + (c1.x - c0.x) * t), 0.3f + 0.7f * (c0.y + (c1.y - c0.y) * t), 0.3f + 0.7f * (c0.z + (c1.z - c0.z) * t), 1.f);
                dl->AddText(big, bs, ImVec2(lx, cy - 21.f), ImGui::ColorConvertFloat4ToU32(cc), c, c + 1);
                lx += big->CalcTextSizeA(bs, FLT_MAX, 0.f, c, c + 1).x + sp;
            }
            float sx = x + (block - sw) * 0.5f;
            for (const char* c = sub; *c; ++c)
            {
                dl->AddText(fonts().body, ss, ImVec2(sx, cy + 8.f), Theme::col(Theme::TextDim), c, c + 1);
                sx += fonts().body->CalcTextSizeA(ss, FLT_MAX, 0.f, c, c + 1).x + ssp;
            }
            return x + block;
        }

        void dot(ImDrawList* dl, const ImVec2& c, float r, ImU32 col)
        {
            dl->AddCircleFilled(c, r + 2.f, Theme::col(Theme::Surface), 16);
            dl->AddCircleFilled(c, r, col, 16);
        }

        ImU32 presenceColor()
        {
            if (!sStatus.online) return Theme::col(Theme::TextFaint);
            if (sStatus.presence == 1) return Theme::col(Theme::Warn);
            if (sStatus.presence == 2) return Theme::col(Theme::Bad);
            return Theme::col(Theme::Good);
        }

        void volumeSliders()
        {
            struct Level
            {
                const char* label;
                const char* key;
            };
            static const Level levels[] = { { "Sounds", "AudioLevelSFX" }, { "Music", "AudioLevelMusic" }, { "Voice", "AudioLevelVoice" } };
            for (const Level& l : levels)
            {
                int pct = (int)std::lround(gSavedSettings.getF32(l.key) * 100.f);
                ImGui::PushID(l.key);
                ImGui::SetNextItemWidth(170.f);
                if (ImGui::SliderInt("##level", &pct, 0, 100, (std::string(l.label) + "  %d%%").c_str())) gSavedSettings.setF32(l.key, pct / 100.f);
                ImGui::PopID();
            }
        }

        void accountMenu()
        {
            if (!ImGui::BeginPopup("##account")) return;
            ImGui::PushFont(fonts().strong, 15.f);
            ImGui::TextUnformatted(sStatus.name.empty() ? "You" : sStatus.name.c_str());
            ImGui::PopFont();
            if (!sStatus.region.empty()) ImGui::TextDisabled("%s", sStatus.region.c_str());
            if (!sStatus.parcel.empty()) ImGui::TextDisabled("%s", sStatus.parcel.c_str());
            if (!sStatus.balance.empty()) ImGui::TextDisabled("L$ %s", sStatus.balance.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Online", nullptr, sStatus.presence == 0, sStatus.online) && sHooks.setPresence) sHooks.setPresence(0);
            if (ImGui::MenuItem("Away", nullptr, sStatus.presence == 1, sStatus.online) && sHooks.setPresence) sHooks.setPresence(1);
            if (ImGui::MenuItem("Do not disturb", nullptr, sStatus.presence == 2, sStatus.online) && sHooks.setPresence) sHooks.setPresence(2);
            ImGui::Separator();
            if (ImGui::BeginMenu("Volume"))
            {
                volumeSliders();
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("My profile")) runCommand("profile.self");
            if (ImGui::MenuItem(Theme::dark() ? "Light theme" : "Dark theme")) runCommand("theme.toggle");
            if (ImGui::MenuItem("Sign out", nullptr, false, (bool)sHooks.signOut)) sHooks.signOut();
            ImGui::Separator();
            if (ImGui::MenuItem("Quit Nyx Storm", "Ctrl Q") && sHooks.quit) sHooks.quit();
            ImGui::EndPopup();
        }

        void topBar()
        {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const float H = Theme::TopBarHeight;
            barWindow("##topbar", ImVec2(0, 0), ImVec2(disp.x, H));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            FX::gradient(dl, ImVec2(0, 0), ImVec2(disp.x, H), Theme::col(Theme::SurfaceAlt, 0.97f), Theme::col(Theme::Surface, 0.97f), 0.f, true, 1.f);
            FX::gradient(dl, ImVec2(0, H - 1.f), ImVec2(disp.x, H), Theme::col(Theme::Border, 0.5f), Theme::col(Theme::Accent, 0.5f), 0.f, false, 1.f);
            const float cy = H * 0.5f;
            float brand_x = 10.f;
            if (sHooks.appMenu)
            {
                const float mb = 38.f;
                ImGui::SetCursorScreenPos(ImVec2(brand_x, cy - mb * 0.5f));
                if (iconButton("##appmenu_btn", Icon::Menu, "Menus", ImGui::IsPopupOpen("##appmenu"), mb)) ImGui::OpenPopup("##appmenu");
                ImGui::SetNextWindowPos(ImVec2(brand_x, H - 4.f));
                if (ImGui::BeginPopup("##appmenu"))
                {
                    sHooks.appMenu();
                    ImGui::EndPopup();
                }
                brand_x += mb + 8.f;
            }
            const float brand_end = drawBrand(dl, brand_x, cy);

            // Right-hand cluster: notifications, then the account menu.
            const float av = 44.f;
            const float ib = 38.f;
            const float ax = disp.x - 14.f - av;
            const float bell_x = ax - 12.f - ib, asst_x = bell_x;

            ImGui::SetCursorScreenPos(ImVec2(bell_x, cy - ib * 0.5f));
            if (iconButton("##bell", Icon::Bell, "Notifications", sCurrent == "notifications", ib)) setWorkspace(sCurrent == "notifications" ? std::string("home") : std::string("notifications"));
            if (workspaceBadgeCount("notifications") > 0) dot(dl, ImVec2(bell_x + ib - 10.f, cy - ib * 0.5f + 9.f), 4.5f, Theme::col(Theme::Bad));

            ImGui::SetCursorScreenPos(ImVec2(ax, cy - av * 0.5f));
            if (ImGui::InvisibleButton("##me", ImVec2(av, av))) ImGui::OpenPopup("##account");
            if (ImGui::IsItemHovered()) ImGui::SetItemTooltip("%s", sStatus.name.empty() ? "Your account" : sStatus.name.c_str());
            {
                const ImVec2 c(ax + av * 0.5f, cy);
                const float r = av * 0.5f;
                const Tex pt = sHooks.selfPortrait ? sHooks.selfPortrait() : 0;
                dl->AddCircleFilled(c, r, Theme::col(Theme::AccentHover, 0.9f), 40);
                if (pt)
                {
                    const ImVec2 ts = textureSize(pt);
                    ImVec2 uv0(0, 1), uv1(1, 0);
                    if (ts.x > ts.y && ts.y > 0.f)
                    {
                        const float cut = (1.f - ts.y / ts.x) * 0.5f;
                        uv0.x = cut;
                        uv1.x = 1.f - cut;
                    }
                    else if (ts.y > ts.x && ts.x > 0.f)
                    {
                        const float cut = (1.f - ts.x / ts.y) * 0.5f;
                        uv0.y = 1.f - cut;
                        uv1.y = cut;
                    }
                    dl->AddImageRounded(texRef(pt), c - ImVec2(r - 2.f, r - 2.f), c + ImVec2(r - 2.f, r - 2.f), uv0, uv1, IM_COL32_WHITE, r - 2.f);
                }
                else
                {
                    dl->AddCircleFilled(c, r - 2.f, Theme::col(Theme::Raised), 40);
                    Icon::draw(dl, Glow::Avatar, c, 26.f, IM_COL32_WHITE);
                }
                dot(dl, c + ImVec2(r * 0.72f, r * 0.72f), 5.f, presenceColor());
            }
            accountMenu();

            // Workspace tiles take what is left after the search box; past that they go into a More menu.
            const std::vector<BarItem> items = topItems();
            std::vector<float> want;
            float total = 0.f;
            for (const BarItem& it : items)
            {
                const float w = std::max(58.f, fonts().strong->CalcTextSizeA(12.5f, FLT_MAX, 0.f, it.title.c_str()).x + 18.f);
                want.push_back(w);
                total += w + 2.f;
            }
            const float tiles_x = brand_end + 16.f;
            const float right_edge = asst_x - 14.f;
            float search_w = right_edge - tiles_x - total - 18.f;
            const bool search_box = search_w >= 300.f;
            if (!search_box) search_w = 0.f;
            const float avail = right_edge - search_w - 14.f - tiles_x;
            const float scale = total > avail ? std::max(0.82f, avail / total) : 1.f;
            const float more_w = 50.f;
            int shown = (int)items.size();
            float used = 0.f;
            for (int i = 0; i < (int)items.size(); ++i)
            {
                used += want[i] * scale + 2.f;
                if (used > avail)
                {
                    shown = i;
                    break;
                }
            }
            if (shown < (int)items.size())
            {
                float u = 0.f;
                for (int i = 0; i < shown; ++i) u += want[i] * scale + 2.f;
                while (shown > 0 && u + more_w > avail) u -= want[--shown] * scale + 2.f;
            }
            const float th = H - 12.f;
            float x = tiles_x;
            for (int i = 0; i < shown; ++i)
            {
                const BarItem& it = items[i];
                const float w = want[i] * scale;
                ImGui::SetCursorScreenPos(ImVec2(x, 6.f));
                if (tileButton(it.id.c_str(), it.icon, it.title.c_str(), itemActive(it), ImVec2(w, th), 26.f, itemTip(it).c_str())) runItem(it);
                const int bn = it.command.empty() ? workspaceBadgeCount(it.id) : 0;
                if (bn > 0)
                {
                    const std::string bt = badgeText(bn);
                    const ImVec2 bs = fonts().strong->CalcTextSizeA(11.f, FLT_MAX, 0.f, bt.c_str());
                    const float bw = std::max(17.f, bs.x + 9.f);
                    const ImVec2 ba(x + w - bw - 3.f, 8.f);
                    dl->AddRectFilled(ba, ba + ImVec2(bw, 17.f), Theme::col(Theme::Bad), 8.5f);
                    dl->AddText(fonts().strong, 11.f, ImVec2(ba.x + (bw - bs.x) * 0.5f, ba.y + (17.f - bs.y) * 0.5f), IM_COL32_WHITE, bt.c_str());
                }
                x += w + 2.f;
            }
            if (shown < (int)items.size())
            {
                ImGui::SetCursorScreenPos(ImVec2(x, 6.f));
                bool any_on = false;
                for (int i = shown; i < (int)items.size(); ++i) any_on |= itemActive(items[i]);
                if (tileButton("##ws_more_tile", Icon::More, "More", any_on, ImVec2(more_w, th), 22.f)) ImGui::OpenPopup("##ws_more");
                if (ImGui::BeginPopup("##ws_more"))
                {
                    for (int i = shown; i < (int)items.size(); ++i)
                    {
                        if (ImGui::MenuItem(items[i].title.c_str(), nullptr, itemActive(items[i]))) runItem(items[i]);
                    }
                    ImGui::EndPopup();
                }
            }

            if (search_box)
            {
                sLocationSlot = ImRect(ImVec2(right_edge - search_w, cy - 18.f), ImVec2(right_edge, cy + 18.f));
                sLocationFrame = ImGui::GetFrameCount();
            }
            ImGui::End();
        }

        // Hold to talk; holding past 0.6 s latches the microphone on until the next click.
        bool voiceTile(const BarItem& it, const ImVec2& size, bool label)
        {
            static double s_down = -1.0;
            static bool s_latched = false;
            static bool s_held = false;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            tileButton(it.id.c_str(), it.icon, label ? it.title.c_str() : "", itemActive(it) || s_held || s_latched, size, 24.f,
                       s_latched ? "Mic stays on: click to turn it off" : "Hold to talk, hold longer to keep it on");
            const double now = ImGui::GetTime();
            if (ImGui::IsItemActivated())
            {
                if (s_latched)
                {
                    s_latched = false;
                    s_held = false;
                    runCommand("voice.off");
                }
                else
                {
                    s_down = now;
                    s_held = true;
                    runCommand("voice.on");
                }
            }
            if (s_held && ImGui::IsItemActive() && now - s_down >= 0.6) s_latched = true;
            if (ImGui::IsItemDeactivated() && s_held)
            {
                s_held = false;
                if (!s_latched) runCommand("voice.off");
            }
            if (s_latched) dl->AddCircleFilled(ImVec2(ImGui::GetItemRectMax().x - 9.f, ImGui::GetItemRectMin().y + 9.f), 4.f, Theme::col(Theme::Bad), 12);
            return false;
        }

        void signalBars(ImDrawList* dl, const ImVec2& base, int level)
        {
            for (int i = 0; i < 4; ++i)
            {
                const float bh = 6.f + i * 4.f;
                const ImVec2 a(base.x + i * 6.f, base.y - bh), b(base.x + i * 6.f + 4.f, base.y);
                if (i < level) FX::gradient(dl, a, b, Theme::col(Theme::Accent2), Theme::col(Theme::Accent), 1.5f, true, 1.f);
                else dl->AddRectFilled(a, b, Theme::col(Theme::TextFaint, 0.45f), 1.5f);
            }
        }

        void commandsMenu()
        {
            std::map<std::string, std::vector<const Command*>> groups;
            for (const Command& c : sCommands) groups[c.group.empty() ? "General" : c.group].push_back(&c);
            for (auto& g : groups)
            {
                if (ImGui::BeginMenu(g.first.c_str()))
                {
                    for (const Command* c : g.second)
                    {
                        if (ImGui::MenuItem(c->label.c_str(), c->shortcut.empty() ? nullptr : c->shortcut.c_str())) runCommand(c->id);
                    }
                    ImGui::EndMenu();
                }
            }
        }

        void chatInput(const ImRect& input);

        void bottomBar()
        {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const float h = Theme::BottomBarHeight;
            const float m = 8.f;
            barWindow("##bottombar", ImVec2(0, disp.y - h), ImVec2(disp.x, h));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float cy = disp.y - h * 0.5f;

            // Status cluster: connection, animation overrider, power menu.
            const float st_w = 144.f;
            const ImVec2 sa(disp.x - m - st_w, cy - 23.f), sb(disp.x - m, cy + 23.f);
            FX::glass(dl, sa, sb, 16.f, 0.9f, true);
            {
                std::string why;
                const int level = sHooks.signal ? sHooks.signal(why) : 4;
                ImGui::SetCursorScreenPos(ImVec2(sa.x + 8.f, cy - 17.f));
                if (ImGui::InvisibleButton("##signal", ImVec2(38.f, 34.f))) focusPanel("stats");
                if (ImGui::IsItemHovered())
                {
                    dl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Theme::col(Theme::Raised, 0.8f), 10.f);
                    ImGui::SetItemTooltip("%s", why.empty() ? "Connection" : why.c_str());
                }
                signalBars(dl, ImVec2(sa.x + 16.f, cy + 9.f), level);
                const ImVec2 ap(sa.x + 52.f, cy - 17.f);
                ImGui::SetCursorScreenPos(ap);
                const bool ao = sHooks.commandActive && sHooks.commandActive("ao.toggle");
                if (ImGui::InvisibleButton("##ao", ImVec2(38.f, 34.f))) runCommand("ao.toggle");
                const bool hov = ImGui::IsItemHovered();
                if (hov) ImGui::SetItemTooltip("%s", ao ? "Animation overrider is on: click to turn it off" : "Animation overrider is off: click to turn it on");
                const ImVec2 c = ap + ImVec2(19.f, 17.f);
                if (ao) FX::gradient(dl, c - ImVec2(15.f, 15.f), c + ImVec2(15.f, 15.f), Theme::col(Theme::Accent, 0.6f), Theme::col(Theme::Accent2, 0.4f), 15.f, true, 1.f);
                dl->AddCircle(c, 15.f, ao ? Theme::col(Theme::AccentHover) : Theme::col(hov ? Theme::Text : Theme::Border), 32, 1.5f);
                const ImVec2 ts = fonts().strong->CalcTextSizeA(13.f, FLT_MAX, 0.f, "AO");
                dl->AddText(fonts().strong, 13.f, c - ts * 0.5f, ao ? IM_COL32_WHITE : Theme::col(hov ? Theme::Text : Theme::TextDim), "AO");
                ImGui::SetCursorScreenPos(ImVec2(sa.x + 96.f, cy - 17.f));
                if (iconButton("##power", Icon::Power, "Menu, sign out and quit", false, 34.f)) ImGui::OpenPopup("##power_menu");
                if (ImGui::BeginPopup("##power_menu"))
                {
                    if (ImGui::BeginMenu("Commands"))
                    {
                        commandsMenu();
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Command palette", "Ctrl K")) openPalette();
                    if (ImGui::BeginMenu("Volume"))
                    {
                        volumeSliders();
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Sign out", nullptr, false, (bool)sHooks.signOut)) sHooks.signOut();
                    if (ImGui::MenuItem("Quit Nyx Storm", "Ctrl Q") && sHooks.quit) sHooks.quit();
                    ImGui::EndPopup();
                }
            }

            // The toolbar, centred, shrinking and then dropping its labels when the window is narrow.
            const float sep_w = 13.f;
            std::vector<float> want;
            float total = 16.f;
            ImRect chat_vp;
            const bool chat_ok = Dock::viewport(chat_vp) && chat_vp.GetWidth() >= 240.f && chat_vp.GetHeight() >= 160.f;
            for (const BarItem& it : sBottomItems)
            {
                const float w = it.id == "|" ? sep_w
                              : it.command == "chat.input" ? (chat_ok ? 380.f : 0.f)
                              : std::max(54.f, fonts().body->CalcTextSizeA(12.5f, FLT_MAX, 0.f, it.title.c_str()).x + 14.f);
                want.push_back(w);
                total += w + (it.id == "|" || w == 0.f ? 0.f : 2.f);
            }
            const float max_w = std::max(200.f, disp.x - 2.f * (st_w + m * 2.f + 6.f));
            float scale = 1.f;
            bool labels = true;
            if (total > max_w)
            {
                scale = max_w / total;
                labels = scale > 0.8f;
                if (!labels)
                {
                    total = 16.f;
                    for (size_t i = 0; i < want.size(); ++i)
                    {
                        if (sBottomItems[i].command == "chat.input") want[i] = chat_ok ? 240.f : 0.f;
                        else if (sBottomItems[i].id != "|") want[i] = 44.f;
                        total += want[i] + (sBottomItems[i].id == "|" || want[i] == 0.f ? 0.f : 2.f);
                    }
                    scale = std::min(1.f, max_w / total);
                }
            }
            const float bw = total * scale;
            const float x0 = std::max(m, (disp.x - bw) * 0.5f);
            const ImVec2 a(x0, disp.y - h + m - 1.f), b(x0 + bw, disp.y - m + 1.f);
            if (!sBottomItems.empty())
            {
                FX::glass(dl, a, b, 18.f, 0.9f, true);
                float x = a.x + 8.f * scale;
                const float th = b.y - a.y - 8.f;
                for (size_t i = 0; i < sBottomItems.size(); ++i)
                {
                    const BarItem& it = sBottomItems[i];
                    const float w = want[i] * scale;
                    if (it.id == "|")
                    {
                        dl->AddLine(ImVec2(x + w * 0.5f, cy - 16.f), ImVec2(x + w * 0.5f, cy + 16.f), Theme::col(Theme::Border), 1.f);
                        x += w;
                        continue;
                    }
                    if (it.command == "chat.input")
                    {
                        if (w > 0.f)
                        {
                            chatInput(ImRect(ImVec2(x + 4.f, cy - 18.f), ImVec2(x + w - 4.f, cy + 18.f)));
                            x += w + 2.f * scale;
                        }
                        continue;
                    }
                    ImGui::SetCursorScreenPos(ImVec2(x, a.y + 4.f));
                    if (it.command == "voice.talk") voiceTile(it, ImVec2(w, th), labels);
                    else if (tileButton(it.id.c_str(), it.icon, labels ? it.title.c_str() : "", false, ImVec2(w, th), 24.f, it.tip.empty() ? it.title.c_str() : it.tip.c_str())) runItem(it);
                    x += w + 2.f * scale;
                }
            }
            const float left_space = a.x - m * 2.f, right_space = sa.x - b.x - m * 2.f;
            if (right_space >= 320.f) sShelfSlot = ImRect(ImVec2(b.x + m, a.y), ImVec2(sa.x - m, b.y));
            else if (left_space >= 320.f) sShelfSlot = ImRect(ImVec2(m, a.y), ImVec2(a.x - m, b.y));
            else sShelfSlot = ImRect(a, a);
            sShelfFrame = ImGui::GetFrameCount();
            ImGui::End();
        }

        // Where the chat log, chat input and location bar go over the world view.
        void layoutOverlays()
        {
            ImRect vp;
            if (sLocationFrame == ImGui::GetFrameCount()) return;
            if (!Dock::viewport(vp) || vp.GetWidth() < 240.f || vp.GetHeight() < 160.f) return;
            const float m = 10.f;
            const float lw = std::clamp(vp.GetWidth() * 0.46f, 260.f, 560.f);
            sLocationSlot = ImRect(vp.Min + ImVec2(m, m), vp.Min + ImVec2(m + std::min(lw, vp.GetWidth() - 2.f * m), m + 40.f));
            sLocationFrame = ImGui::GetFrameCount();
        }

        bool chatInBar()
        {
            for (const BarItem& it : sBottomItems)
                if (it.command == "chat.input") return true;
            return false;
        }

        void chatInput(const ImRect& input);

        void chatBar()
        {
            ImRect vp;
            if (!Dock::viewport(vp) || vp.GetWidth() < 240.f || vp.GetHeight() < 160.f) return;
            const float m = 10.f, ih = 36.f;
            const bool show_log = settingBool("chat.overlay", true);
            const float log_w = std::clamp(vp.GetWidth() * 0.34f, 260.f, 440.f);
            const float log_h = std::clamp(vp.GetHeight() * 0.26f, 110.f, 210.f);
            if (chatInBar())
            {
                sChatLogSlot = ImRect(ImVec2(vp.Min.x + m, vp.Max.y - m - log_h), ImVec2(vp.Min.x + m + log_w, vp.Max.y - m));
                if (show_log) sChatLogFrame = ImGui::GetFrameCount();
                return;
            }
            ImRect input;
            if (show_log && vp.GetWidth() - log_w - m * 3.f >= 300.f)
            {
                sChatLogSlot = ImRect(ImVec2(vp.Min.x + m, vp.Max.y - m - log_h), ImVec2(vp.Min.x + m + log_w, vp.Max.y - m));
                input = ImRect(ImVec2(sChatLogSlot.Max.x + m, vp.Max.y - m - ih), ImVec2(std::min(vp.Max.x - m, sChatLogSlot.Max.x + m + 760.f), vp.Max.y - m));
            }
            else
            {
                const float w = std::min(vp.GetWidth() - m * 2.f, 760.f);
                input = ImRect(ImVec2(vp.Min.x + m, vp.Max.y - m - ih), ImVec2(vp.Min.x + m + w, vp.Max.y - m));
                sChatLogSlot = ImRect(ImVec2(vp.Min.x + m, input.Min.y - 8.f - log_h), ImVec2(vp.Min.x + m + std::min(log_w, w), input.Min.y - 8.f));
            }
            if (show_log) sChatLogFrame = ImGui::GetFrameCount();

            ImGui::SetNextWindowPos(input.Min);
            ImGui::SetNextWindowSize(input.GetSize());
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::Begin("##chatbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::PopStyleVar(2);
            chatInput(input);
            ImGui::End();
        }

        void chatInput(const ImRect& input)
        {
            const float ih = input.GetHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float r = ih * 0.5f;
            dl->AddRectFilled(input.Min, input.Max, Theme::col(Theme::Surface, sChatActive ? 0.94f : 0.8f), r);
            dl->AddRect(input.Min, input.Max, sChatActive ? Theme::col(Theme::Accent, 0.85f) : Theme::col(Theme::Border, 0.9f), r, 0, 1.f);
            const float cy = (input.Min.y + input.Max.y) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(input.Min.x + 4.f, cy - 14.f));
            if (iconButton("##aichat", sAiChat ? Icon::Robot : Icon::Message,
                           sAiChat ? "Talking to your assistant: click to chat with people nearby" : "Chatting nearby: click to talk to your assistant instead", sAiChat, 28.f))
                sAiChat = !sAiChat;
            const float send_x = input.Max.x - 4.f - 28.f;
            const float emo_x = send_x - 2.f - 28.f;
            const float fx = input.Min.x + 36.f;
            const float fw = std::max(40.f, emo_x - 6.f - fx);
            ImGui::PushFont(fonts().body, 14.f);
            ImGui::SetCursorScreenPos(ImVec2(fx, cy - ImGui::GetFontSize() * 0.5f - 5.f));
            if (sFocusChat)
            {
                ImGui::SetKeyboardFocusHere();
                sFocusChat = false;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 5.f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::SetNextItemWidth(fw);
            const bool enter = ImGui::InputTextWithHint("##chat", sAiChat ? "Ask your assistant\xE2\x80\xA6" : "Type a message\xE2\x80\xA6", sChat, sizeof(sChat), ImGuiInputTextFlags_EnterReturnsTrue);
            sChatActive = ImGui::IsItemActive();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::PopFont();
            auto send = [&]() {
                if (sChat[0] && sHooks.chat) sHooks.chat(sChat);
                sChat[0] = 0;
            };
            if (enter)
            {
                const bool had = sChat[0] != 0;
                send();
                if (had) ImGui::SetKeyboardFocusHere(-1);
            }
            ImGui::SetCursorScreenPos(ImVec2(emo_x, cy - 14.f));
            if (iconButton("##emote", 0xE76E, "Emoticons", false, 28.f)) ImGui::OpenPopup("##emotes");
            if (ImGui::BeginPopup("##emotes"))
            {
                static const char* kEmotes[] = { ":)", ":D", ";)", ":P", "<3", "^_^", "xD", ":O", ":(", "o/", "\\o/", "-_-", "lol", "brb", "ty!", "*hugs*" };
                for (int i = 0; i < (int)(sizeof(kEmotes) / sizeof(kEmotes[0])); ++i)
                {
                    if (i % 4) ImGui::SameLine();
                    if (ImGui::Button(kEmotes[i], ImVec2(52.f, 30.f)))
                    {
                        const size_t n = strlen(sChat);
                        snprintf(sChat + n, sizeof(sChat) - n, "%s%s ", (n && sChat[n - 1] != ' ') ? " " : "", kEmotes[i]);
                        sFocusChat = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
            }
            const ImVec2 sp(send_x, cy - 14.f);
            ImGui::SetCursorScreenPos(sp);
            if (ImGui::InvisibleButton("##send", ImVec2(28.f, 28.f))) send();
            const bool shov = ImGui::IsItemHovered();
            if (sChat[0]) FX::accent(dl, sp, sp + ImVec2(28.f, 28.f), 14.f, 1.f);
            else if (shov) dl->AddRectFilled(sp, sp + ImVec2(28.f, 28.f), Theme::col(Theme::Raised), 14.f);
            Icon::draw(dl, Icon::Send, sp + ImVec2(14.f, 14.f), 15.f, sChat[0] ? IM_COL32_WHITE : Theme::col(shov ? Theme::Text : Theme::TextDim));
        }

        void toasts(float dt)
        {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            float y = Theme::TopBarHeight + 8.f;
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            for (Toast& t : sToasts)
            {
                t.age += dt;
                const float a = std::clamp(std::min(t.age * 6.f, (5.f - t.age) * 2.f), 0.f, 1.f);
                const float slide = (1.f - std::min(1.f, t.age * 5.f)) * 30.f;
                const float w = 360.f;
                const ImVec2 ts = ImGui::CalcTextSize(t.text.c_str(), nullptr, false, w - 52.f);
                const float h = ts.y + 26.f;
                const ImVec2 p(disp.x - w - 18.f + slide, y);
                FX::shadow(fg, p, p + ImVec2(w, h), 14.f, a);
                fg->AddRectFilled(p, p + ImVec2(w, h), Theme::col(Theme::SurfaceAlt, 0.96f * a), 14.f);
                fg->AddRect(p, p + ImVec2(w, h), Theme::col(Theme::Border, a), 14.f);
                if (t.error) fg->AddRectFilled(p + ImVec2(12, 12), p + ImVec2(16, h - 12), Theme::col(Theme::Bad, a), 2.f);
                else FX::gradient(fg, p + ImVec2(12, 12), p + ImVec2(16, h - 12), Theme::col(Theme::Accent, a), Theme::col(Theme::Accent2, a), 2.f, true, a);
                fg->AddText(fonts().body, Theme::BodySize, p + ImVec2(28, 13), Theme::col(Theme::Text, a), t.text.c_str(), nullptr, w - 52.f);
                y += h + 10.f;
            }
            sToasts.erase(std::remove_if(sToasts.begin(), sToasts.end(), [](const Toast& t) { return t.age > 5.f; }), sToasts.end());
        }

        void appBackground(const ImRect& viewport, bool has_viewport);

        Tex sAuroraTex = 0;
        char sMfa[32] = {};
        char sRegion[96] = {};
        bool sShowPass = false;

        void makeAurora()
        {
            if (sAuroraTex) return;
            const int W = 512, H = 288;
            std::vector<unsigned char> px((size_t)W * H * 4);
            const bool dark = Theme::dark();
            const ImVec4 base = dark ? ImVec4(0.035f, 0.040f, 0.075f, 1.f) : ImVec4(0.93f, 0.93f, 0.97f, 1.f);
            struct Blob { float x, y, r; ImVec4 c; };
            const Blob blobs[] = {
                { 0.78f, 0.18f, 0.55f, ImVec4(0.45f, 0.30f, 0.95f, dark ? 0.55f : 0.35f) },
                { 0.18f, 0.88f, 0.60f, ImVec4(0.10f, 0.65f, 0.85f, dark ? 0.35f : 0.25f) },
                { 0.55f, 0.62f, 0.45f, ImVec4(0.85f, 0.30f, 0.75f, dark ? 0.22f : 0.15f) },
                { 0.05f, 0.10f, 0.40f, ImVec4(0.25f, 0.20f, 0.60f, dark ? 0.30f : 0.20f) },
            };
            unsigned int seed = 1234567u;
            for (int y = 0; y < H; ++y)
            {
                for (int x = 0; x < W; ++x)
                {
                    const float u = (float)x / (float)W;
                    const float v = (float)y / (float)H;
                    float r = base.x, g = base.y, bl = base.z;
                    for (const Blob& b : blobs)
                    {
                        const float dx = (u - b.x) * 1.78f;
                        const float dy = v - b.y;
                        const float d = std::sqrt(dx * dx + dy * dy) / b.r;
                        const float k = std::exp(-d * d * 2.2f) * b.c.w;
                        r += (b.c.x - r) * k;
                        g += (b.c.y - g) * k;
                        bl += (b.c.z - bl) * k;
                    }
                    seed = seed * 1664525u + 1013904223u;
                    const float n = ((float)((seed >> 8) & 0xFFFF) / 65535.f - 0.5f) / 255.f * 1.6f;
                    unsigned char* p = &px[((size_t)y * W + x) * 4];
                    p[0] = (unsigned char)std::clamp((r + n) * 255.f + 0.5f, 0.f, 255.f);
                    p[1] = (unsigned char)std::clamp((g + n) * 255.f + 0.5f, 0.f, 255.f);
                    p[2] = (unsigned char)std::clamp((bl + n) * 255.f + 0.5f, 0.f, 255.f);
                    p[3] = 255;
                }
            }
            sAuroraTex = createTexture(W, H, px.data());
        }

        void appBackground(const ImRect& vp, bool has_vp)
        {
            makeAurora();
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            const ImTextureRef tex = texRef(sAuroraTex);
            auto quad = [&](ImVec2 a, ImVec2 b) {
                if (b.x <= a.x || b.y <= a.y) return;
                bg->AddImage(tex, a, b, ImVec2(a.x / disp.x, a.y / disp.y), ImVec2(b.x / disp.x, b.y / disp.y), IM_COL32(150, 150, 165, 255));
            };
            if (!has_vp)
            {
                quad(ImVec2(0, 0), disp);
                return;
            }
            quad(ImVec2(0, 0), ImVec2(disp.x, vp.Min.y));
            quad(ImVec2(0, vp.Max.y), disp);
            quad(ImVec2(0, vp.Min.y), ImVec2(vp.Min.x, vp.Max.y));
            quad(ImVec2(vp.Max.x, vp.Min.y), ImVec2(disp.x, vp.Max.y));
            ImDrawList* fg = bg;
            const float r = Theme::Radius;
            fg->AddRect(vp.Min, vp.Max, Theme::col(Theme::Border, 0.7f), r, 0, 1.f);
        }

        std::string groupDigits(double v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f", v);
            std::string s = buf, out;
            int n = 0;
            for (int i = (int)s.size() - 1; i >= 0; --i)
            {
                out.insert(out.begin(), s[i]);
                if (++n % 3 == 0 && i > 0) out.insert(out.begin(), ',');
            }
            return out;
        }

        void loginLiveCorner(ImDrawList* dl)
        {
            const ImVec2 ca(24.f, 24.f);
            const float cw = 300.f;
            const int extra = (int)std::min<size_t>(sLogin.incidents.size(), 2) + (int)std::min<size_t>(sLogin.maintenances.size(), 2);
            const ImVec2 cb(ca.x + cw, ca.y + 132.f + extra * 20.f);
            FX::glow(dl, ImVec2(ca.x + cw * 0.4f, ca.y + 50.f), cw * 0.6f, Theme::col(Theme::Accent), 0.25f);
            FX::glass(dl, ca, cb, 18.f, 0.82f, true);
            const float t = (float)ImGui::GetTime();
            const float shown = sLogin.online >= 0 ? FX::anim(ImGui::GetID("##online"), (float)sLogin.online, 2.5f) : -1.f;
            const ImU32 live = Theme::col(Theme::Good);
            const float pulse = 0.5f + 0.5f * std::sin(t * 3.f);
            FX::glow(dl, ImVec2(ca.x + 24.f, ca.y + 26.f), 12.f + 5.f * pulse, live, 0.6f + 0.4f * pulse);
            dl->AddCircleFilled(ImVec2(ca.x + 24.f, ca.y + 26.f), 4.5f, live, 16);
            dl->AddText(fonts().body, 13.f, ImVec2(ca.x + 38.f, ca.y + 18.f), Theme::col(Theme::TextDim), "Residents online now");
            const std::string big = shown >= 0.f ? groupDigits(shown) : std::string("-");
            const int v0 = dl->VtxBuffer.Size;
            dl->AddText(fonts().light, 42.f, ImVec2(ca.x + 18.f, ca.y + 38.f), IM_COL32(255, 255, 255, 255), big.c_str());
            const ImVec2 bs = fonts().light->CalcTextSizeA(42.f, FLT_MAX, 0.f, big.c_str());
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, ImVec2(ca.x + 18.f, 0), ImVec2(ca.x + 18.f + std::max(bs.x, 1.f), 0), IM_COL32(255, 255, 255, 255), ImGui::ColorConvertFloat4ToU32(ImVec4(0.75f, 0.88f, 1.f, 1.f)));

            const float py = ca.y + 92.f;
            ImU32 sc = Theme::col(Theme::TextFaint);
            if (sLogin.gridLevel == 0) sc = Theme::col(Theme::Good);
            else if (sLogin.gridLevel == 1) sc = Theme::col(Theme::Warn);
            else if (sLogin.gridLevel >= 2) sc = Theme::col(Theme::Bad);
            const std::string status = sLogin.gridStatus.empty() ? std::string("Checking the grid...") : sLogin.gridStatus;
            const ImVec2 sts = fonts().strong->CalcTextSizeA(12.5f, FLT_MAX, 0.f, status.c_str());
            const ImVec2 pa(ca.x + 16.f, py), pb(ca.x + 16.f + sts.x + 34.f, py + 26.f);
            ImVec4 scv = ImGui::ColorConvertU32ToFloat4(sc);
            scv.w = 0.14f;
            dl->AddRectFilled(pa, pb, ImGui::ColorConvertFloat4ToU32(scv), 13.f);
            dl->AddCircleFilled(ImVec2(pa.x + 14.f, py + 13.f), 4.f, sc, 12);
            dl->AddText(fonts().strong, 12.5f, ImVec2(pa.x + 24.f, py + 6.f), sc, status.c_str());
            float ly = py + 36.f;
            for (size_t i = 0; i < sLogin.incidents.size() && i < 2; ++i, ly += 20.f)
            {
                dl->PushClipRect(ImVec2(ca.x + 16.f, ly), ImVec2(cb.x - 14.f, ly + 20.f), true);
                dl->AddText(fonts().body, 12.5f, ImVec2(ca.x + 18.f, ly), Theme::col(Theme::Warn), sLogin.incidents[i].c_str());
                dl->PopClipRect();
            }
            for (size_t i = 0; i < sLogin.maintenances.size() && i < 2; ++i, ly += 20.f)
            {
                const std::string m = "Planned: " + sLogin.maintenances[i];
                dl->PushClipRect(ImVec2(ca.x + 16.f, ly), ImVec2(cb.x - 14.f, ly + 20.f), true);
                dl->AddText(fonts().body, 12.5f, ImVec2(ca.x + 18.f, ly), Theme::col(Theme::TextDim), m.c_str());
                dl->PopClipRect();
            }
        }

        const float kNewsRowH = 196.f;

        void loginNewsRow(const ImVec2& disp, float y)
        {
            const float row_w = std::min(disp.x - 80.f, 1180.f);
            const float x = (disp.x - row_w) * 0.5f;
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            pdl->AddText(fonts().strong, 13.f, ImVec2(x + 4.f, y), Theme::col(Theme::Accent2), "WHAT'S NEW IN SECOND LIFE");
            const float card_w = 300.f;
            const float card_h = 142.f;
            ImGui::SetCursorScreenPos(ImVec2(x, y + 24.f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, Theme::vec(Theme::Accent, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, Theme::vec(Theme::Accent, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 6.f));
            ImGui::BeginChild("##newsrow", ImVec2(row_w, card_h + 26.f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (ImGui::IsWindowHovered())
            {
                const ImGuiIO& io = ImGui::GetIO();
                const float wheel = io.MouseWheelH != 0.f ? -io.MouseWheelH : io.MouseWheel;
                if (wheel != 0.f) ImGui::SetScrollX(ImGui::GetScrollX() - wheel * 120.f);
            }
            if (sLogin.news.empty())
            {
                const ImVec2 c = ImGui::GetCursorScreenPos();
                dl->AddText(fonts().body, 14.f, ImVec2(c.x + 4.f, c.y + 8.f), Theme::col(Theme::TextFaint), "Loading the latest news...");
            }
            for (size_t i = 0; i < sLogin.news.size(); ++i)
            {
                const NewsItem& n = sLogin.news[i];
                if (i) ImGui::SameLine(0.f, 14.f);
                const ImVec2 a = ImGui::GetCursorScreenPos();
                ImGui::PushID((int)i);
                const bool click = ImGui::InvisibleButton("##news", ImVec2(card_w, card_h));
                const float hv = FX::anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.f : 0.f);
                ImGui::PopID();
                const ImVec2 ta(a.x, a.y + 4.f - hv * 3.f), tb(a.x + card_w, a.y + card_h - hv * 3.f);
                if (hv > 0.01f) FX::glow(dl, ImVec2((ta.x + tb.x) * 0.5f, tb.y), card_w * 0.5f, Theme::col(Theme::Accent), hv * 0.6f);
                FX::glass(dl, ta, tb, 16.f, 0.78f + 0.12f * hv, true);
                if (hv > 0.01f) dl->AddRect(ta, tb, Theme::col(Theme::Accent, 0.5f * hv), 16.f);
                const float tw = card_w - 32.f;
                dl->PushClipRect(ta, ImVec2(tb.x, tb.y - 12.f), true);
                dl->AddText(fonts().body, 12.f, ImVec2(ta.x + 16.f, ta.y + 14.f), Theme::col(Theme::TextFaint), n.date.c_str());
                const ImVec2 ts = fonts().strong->CalcTextSizeA(14.5f, FLT_MAX, tw, n.title.c_str());
                dl->AddText(fonts().strong, 14.5f, ImVec2(ta.x + 16.f, ta.y + 32.f), Theme::col(Theme::Text), n.title.c_str(), nullptr, tw);
                dl->AddText(fonts().body, 12.5f, ImVec2(ta.x + 16.f, ta.y + 38.f + ts.y), Theme::col(Theme::TextDim), n.summary.c_str(), nullptr, tw);
                dl->PopClipRect();
                if (click && sHooks.openUrl && !n.link.empty()) sHooks.openUrl(n.link);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        }

        std::vector<std::string> recentUsers()
        {
            std::vector<std::string> out;
            auto it = sSettings.find("login.recent");
            if (it == sSettings.end()) return out;
            std::string cur;
            for (char c : it->second)
            {
                if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
                else cur += c;
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        }

        void rememberUser(const std::string& u)
        {
            std::vector<std::string> r = recentUsers();
            r.erase(std::remove(r.begin(), r.end(), u), r.end());
            r.insert(r.begin(), u);
            if (r.size() > 6) r.resize(6);
            std::string joined;
            for (size_t i = 0; i < r.size(); ++i) joined += (i ? "," : "") + r[i];
            sSettings["login.recent"] = joined;
            sSettings["login.user"] = u;
            sDirty = true;
        }

        void chip(const char* label, int idx, int* cur, float w)
        {
            const bool on = *cur == idx;
            ImGui::PushStyleColor(ImGuiCol_Button, on ? Theme::vec(Theme::Accent, 0.9f) : Theme::vec(Theme::Raised, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? Theme::vec(Theme::AccentHover) : Theme::vec(Theme::Raised));
            ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(1, 1, 1, 1) : Theme::vec(Theme::TextDim));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f);
            if (ImGui::Button(label, ImVec2(w, 32.f))) *cur = idx;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }

        void loginScreen()
        {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            makeAurora();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(disp);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::vec(Theme::Base));
            ImGui::Begin("##login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float t = (float)ImGui::GetTime();
            const float drift = std::sin(t * 0.05f) * 0.03f;
            dl->AddImage(texRef(sAuroraTex), ImVec2(0, 0), disp, ImVec2(0.02f + drift, 0.02f), ImVec2(0.98f + drift, 0.98f));

            const float card_w = 420.f;
            const float card_x = (disp.x - card_w) * 0.5f;
            loginLiveCorner(dl);
            std::vector<const char*> grids;
            for (const std::string& g : sLogin.grids) grids.push_back(g.c_str());
            if (grids.empty()) grids.push_back("Second Life");
            sGrid = std::clamp(sGrid, 0, (int)grids.size() - 1);

            static float sCardH = 470.f;
            const float brand_h = 120.f;
            const float card_y = std::max(brand_h + 20.f, (disp.y - sCardH - brand_h - kNewsRowH) * 0.5f + brand_h);
            {
                const float cx = disp.x * 0.5f;
                const float by = card_y - brand_h + 6.f;
                if (const Image* wm = image("wordmark"))
                {
                    const float wh = 96.f;
                    const float ww = wm->w * wh / std::max(1.f, wm->h);
                    FX::glow(dl, ImVec2(cx, by + 40.f), ww * 0.45f, Theme::col(Theme::Accent), 0.35f);
                    dl->AddImage(texRef(wm->tex), ImVec2(cx - ww * 0.5f, by - 8.f), ImVec2(cx + ww * 0.5f, by - 8.f + wh));
                }
                else
                {
                    ImFont* lf = fonts().light;
                    const ImVec2 ts = lf->CalcTextSizeA(56.f, FLT_MAX, 0.f, "Nyxstorm");
                    dl->AddText(lf, 56.f, ImVec2(cx - ts.x * 0.5f, by + 34.f - ts.y * 0.5f), Theme::col(Theme::Text), "Nyxstorm");
                }
            }
            for (int i = 0; i < 6; ++i)
            {
                const float g = 6.f + i * 6.f;
                dl->AddRectFilled(ImVec2(card_x - g * 0.5f, card_y - g * 0.3f + 10.f), ImVec2(card_x + card_w + g * 0.5f, card_y + sCardH + g), IM_COL32(0, 0, 0, 10), 22.f + g * 0.5f);
            }
            ImGui::SetCursorScreenPos(ImVec2(card_x, card_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::vec(Theme::Surface, 0.94f));
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::vec(Theme::Accent, 0.22f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 22.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 30));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
            ImGui::BeginChild("##card", ImVec2(card_w, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
            ImGui::PushFont(fonts().strong, 24.f);
            ImGui::TextUnformatted("Sign in");
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 10));

            const float fw = ImGui::GetContentRegionAvail().x;
            sectionHeader("Username");
            const std::vector<std::string> recent = recentUsers();
            textField("##user", "first.last or username", sUser, sizeof(sUser), Icon::Contact, false, recent.empty() ? fw : fw - 40.f);
            if (!recent.empty())
            {
                ImGui::SameLine(0, 6);
                if (iconButton("##recent", Icon::ChevronDown, "Recent accounts", false, 34.f)) ImGui::OpenPopup("##recentpop");
                if (ImGui::BeginPopup("##recentpop"))
                {
                    for (const std::string& u : recent)
                    {
                        if (ImGui::Selectable(u.c_str())) strncpy(sUser, u.c_str(), sizeof(sUser) - 1);
                    }
                    ImGui::EndPopup();
                }
            }
            // Saved passwords and MFA trust are per grid, so the lookup follows the grid chip too.
            static std::string sSavedFor;
            static int sSaved = 0;
            const std::string saved_key = std::string(sUser) + "\n" + grids[sGrid];
            if (sSavedFor != saved_key)
            {
                sSavedFor = saved_key;
                sSaved = (sUser[0] && sHooks.savedLogin) ? sHooks.savedLogin(sUser, grids[sGrid]) : 0;
            }
            const bool saved_pass = (sSaved & 1) != 0;
            const bool trusted = (sSaved & 2) != 0;
            ImGui::Dummy(ImVec2(0, 4));
            sectionHeader("Password");
            bool enter = textField("##pass", saved_pass ? "Saved password" : "Password", sPass, sizeof(sPass), Icon::Lock, !sShowPass, fw - 40.f);
            ImGui::SameLine(0, 6);
            if (iconButton("##eye", Icon::View, sShowPass ? "Hide password" : "Show password", sShowPass, 34.f)) sShowPass = !sShowPass;
            ImGui::Dummy(ImVec2(0, 4));
            sectionHeader("Two-factor code");
            enter |= textField("##mfa", trusted ? "Not needed, this computer is trusted" : "6-digit code, if your account uses one", sMfa, sizeof(sMfa), Icon::Lock, false, fw);
            ImGui::Dummy(ImVec2(0, 8));
            sectionHeader("Start at");
            const float cw = (fw - 16.f) / 3.f;
            chip("Last location", 0, &sStart, cw);
            ImGui::SameLine(0, 8);
            chip("Home", 1, &sStart, cw);
            ImGui::SameLine(0, 8);
            chip("A region", 2, &sStart, cw);
            if (sStart == 2)
            {
                ImGui::Dummy(ImVec2(0, 2));
                textField("##region", "Region name", sRegion, sizeof(sRegion), Icon::MapPin, false, fw);
            }
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::Checkbox("Remember me", &sRemember);
            ImGui::Dummy(ImVec2(0, 8));

            const bool can = !sLogin.busy && sUser[0] && (sPass[0] || saved_pass);
            ImGui::BeginDisabled(!can);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
            const bool go = primaryButton(sLogin.busy ? "Signing in\xE2\x80\xA6" : "Sign in", ImVec2(-1, 48)) || (enter && can) || (sAutoSignIn && can);
            if (go) sAutoSignIn = false;
            ImGui::PopStyleVar();
            ImGui::EndDisabled();
            if (go && can && sHooks.signIn)
            {
                LoginRequest r;
                r.user = sUser;
                r.password = sPass;
                r.grid = grids[sGrid];
                r.start = sStart == 0 ? "last" : (sStart == 1 ? "home" : "region");
                r.region = sRegion;
                r.mfa = sMfa;
                r.remember = sRemember;
                setSettingBool("login.remember", sRemember);
                if (sRemember) rememberUser(sUser);
                sDirty = true;
                sLogin.error = false;
                sLogin.busy = true;
                sLogin.message = "Connecting to Second Life";
                sHooks.signIn(r);
                memset(sPass, 0, sizeof(sPass));
                sSavedFor.clear();
                memset(sMfa, 0, sizeof(sMfa));
            }
            if (sLogin.busy)
            {
                ImGui::Dummy(ImVec2(0, 4));
                const float ph = (float)std::fmod(ImGui::GetTime() * 0.6, 1.0);
                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                cdl->AddRectFilled(p, p + ImVec2(fw, 4.f), Theme::col(Theme::Raised), 2.f);
                const float bx = p.x + fw * ph * 1.3f - fw * 0.3f;
                cdl->AddRectFilled(ImVec2(std::max(p.x, bx), p.y), ImVec2(std::min(p.x + fw, bx + fw * 0.3f), p.y + 4.f), Theme::col(Theme::Accent), 2.f);
                ImGui::Dummy(ImVec2(fw, 8.f));
            }
            if (!sLogin.message.empty())
            {
                ImGui::Dummy(ImVec2(0, 2));
                if (sLogin.error)
                {
                    const ImVec2 p = ImGui::GetCursorScreenPos();
                    const ImVec2 ts = ImGui::CalcTextSize(sLogin.message.c_str(), nullptr, false, fw - 24.f);
                    ImGui::GetWindowDrawList()->AddRectFilled(p, p + ImVec2(fw, ts.y + 18.f), Theme::col(Theme::Bad, 0.14f), 10.f);
                    ImGui::SetCursorScreenPos(p + ImVec2(12, 9));
                    ImGui::PushTextWrapPos(p.x + fw - 12.f);
                    ImGui::PushStyleColor(ImGuiCol_Text, Theme::vec(Theme::Bad));
                    ImGui::TextUnformatted(sLogin.message.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorScreenPos(p + ImVec2(0, ts.y + 22.f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Theme::vec(Theme::TextDim));
                    ImGui::PushTextWrapPos(0.f);
                    ImGui::TextUnformatted(sLogin.message.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                }
            }
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::vec(Theme::TextDim));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
            auto link = [](const char* t) { return ImGui::Selectable(t, false, 0, ImGui::CalcTextSize(t)); };
            const std::string grid_label = std::string("Grid: ") + grids[sGrid];
            if (link(grid_label.c_str())) ImGui::OpenPopup("##gridpop");
            if (ImGui::BeginPopup("##gridpop"))
            {
                for (int i = 0; i < (int)grids.size(); ++i)
                {
                    if (ImGui::Selectable(grids[i], i == sGrid)) sGrid = i;
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine(0, 18);
            if (link("Forgot password") && sHooks.openUrl) sHooks.openUrl("https://secondlife.com/my/account/request.php");
            ImGui::SameLine(0, 18);
            if (link("New account") && sHooks.openUrl) sHooks.openUrl("https://join.secondlife.com/");
            ImGui::PopStyleColor(3);
            sCardH = ImGui::GetWindowHeight();
            ImGui::EndChild();
            ImGui::PopStyleVar(4);
            ImGui::PopStyleColor(2);
            loginNewsRow(disp, card_y + sCardH + 28.f);

            dl->AddText(fonts().body, 13.f, ImVec2(20.f, disp.y - 28.f), Theme::col(Theme::TextFaint), "Nyxstorm 0.1  \xC2\xB7  Third-party viewer for Second Life");
            ImGui::End();
        }
    }

    bool init(const std::string& font_dir, const std::string& settings_dir)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        sSettingsPath = settings_dir + "/nyx_ui.txt";
        loadState();
        loadFonts(font_dir);
        Theme::apply(settingBool("ui.dark", true));
        ImGui::GetStyle().FontScaleMain = settingFloat("ui.scale", 1.f);
        const auto it = sSettings.find("login.user");
        if (it != sSettings.end()) strncpy(sUser, it->second.c_str(), sizeof(sUser) - 1);
        sRemember = settingBool("login.remember", true);
        sCurrent = sSettings.count("ui.workspace") ? sSettings["ui.workspace"] : std::string("home");
        return glInit();
    }

    void shutdown()
    {
        saveState();
        glShutdown();
        ImGui::DestroyContext();
    }

    void saveState()
    {
        if (sSettingsPath.empty()) return;
        // Write a temporary file and swap it in, so a crash mid-write can't wipe the layouts.
        const std::string tmp = sSettingsPath + ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) return;
            sSettings["ui.workspace"] = sCurrent;
            for (const auto& kv : sSettings) out << "set " << kv.first << " " << kv.second << "\n";
            for (const auto& kv : sLayouts)
            {
                if (!kv.second.root) continue;
                out << "layout " << kv.first << " " << Dock::serialize(kv.second.root.get()) << "\n";
                for (const Dock::Floating& f : kv.second.floating)
                {
                    out << "float " << kv.first << " " << f.id << " " << f.pos.x << " " << f.pos.y << " " << f.size.x << " " << f.size.y << "\n";
                }
            }
            out.flush();
            if (!out) return;
        }
        std::error_code ec;
        std::filesystem::rename(tmp, sSettingsPath, ec);
        if (ec) return;
        sDirty = false;
    }

    bool settingBool(const std::string& key, bool def)
    {
        auto it = sSettings.find(key);
        return it == sSettings.end() ? def : it->second == "1";
    }

    void setSettingBool(const std::string& key, bool v)
    {
        sSettings[key] = v ? "1" : "0";
        sDirty = true;
    }

    float settingFloat(const std::string& key, float def)
    {
        auto it = sSettings.find(key);
        return it == sSettings.end() ? def : (float)atof(it->second.c_str());
    }

    void setSettingFloat(const std::string& key, float v)
    {
        sSettings[key] = std::to_string(v);
        sDirty = true;
    }

    std::string settingString(const std::string& key, const std::string& def)
    {
        auto it = sSettings.find(key);
        return it == sSettings.end() ? def : it->second;
    }

    void setSettingString(const std::string& key, const std::string& v)
    {
        std::string clean = v;
        for (char& c : clean)
        {
            if (c == '\n' || c == '\r') c = ' ';
        }
        sSettings[key] = clean;
        sDirty = true;
    }

    void registerPanel(const Panel& p)
    {
        for (Panel& e : sPanels)
        {
            if (e.id == p.id)
            {
                e = p;
                return;
            }
        }
        sPanels.push_back(p);
        registerCommand({ "panel." + p.id, "Open " + p.title, "Panels", "", [id = p.id]() { focusPanel(id); } });
    }

    const Panel* findPanel(const std::string& id)
    {
        for (const Panel& p : sPanels)
        {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    void setPanelChromeless(const std::string& id)
    {
        sChromeless.insert(id);
    }

    bool panelChromeless(const std::string& id)
    {
        return sChromeless.count(id) != 0;
    }

    void focusPanel(const std::string& id)
    {
        if (!findPanel(id) && !(externalPanels().exists && externalPanels().exists(id))) return;
        Dock::Layout& l = current();
        if (Dock::Node* leaf = Dock::findLeaf(l.root.get(), id))
        {
            leaf->active = (int)(std::find(leaf->tabs.begin(), leaf->tabs.end(), id) - leaf->tabs.begin());
            return;
        }
        for (const Dock::Floating& f : l.floating)
        {
            if (f.id == id)
            {
                ImGui::SetWindowFocus(("##float_" + id).c_str());
                return;
            }
        }
        Dock::dockPanel(l, id);
        sDirty = true;
    }

    bool panelOpen(const std::string& id)
    {
        return Dock::contains(current(), id);
    }

    bool panelInWorkspaceLayout(const std::string& id)
    {
        for (const Workspace& w : sWorkspaces)
        {
            size_t at = 0;
            while ((at = w.layout.find(id, at)) != std::string::npos)
            {
                const char before = at > 0 ? w.layout[at - 1] : '[';
                const char after = at + id.size() < w.layout.size() ? w.layout[at + id.size()] : ']';
                if ((before == '[' || before == '|') && (after == ']' || after == '|' || after == '@')) return true;
                at += id.size();
            }
        }
        return false;
    }

    void closePanel(const std::string& id)
    {
        Dock::removePanel(current(), id);
        sDirty = true;
    }

    void registerWorkspace(const Workspace& w)
    {
        sWorkspaces.push_back(w);
        const int n = (int)sWorkspaces.size();
        registerCommand({ "workspace." + w.id, "Go to " + w.title, "Workspaces", n <= 9 ? "Ctrl " + std::to_string(n) : std::string(), [id = w.id]() { setWorkspace(id); } });
    }

    void setWorkspace(const std::string& id)
    {
        sCurrent = id;
        sDirty = true;
        Dock::Layout& l = current();
        if (l.floating.empty() && countPanels(l.root.get()) == 0) resetWorkspaceLayout(id);
    }

    const std::string& workspace()
    {
        return sCurrent;
    }

    const std::vector<Workspace>& workspaces()
    {
        return sWorkspaces;
    }

    void resetWorkspaceLayout(const std::string& id)
    {
        for (const Workspace& w : sWorkspaces)
        {
            if (w.id == id)
            {
                sLayouts[id].root = Dock::parse(w.layout);
                sLayouts[id].floating.clear();
                sDirty = true;
            }
        }
    }

    void registerCommand(const Command& c)
    {
        for (Command& e : sCommands)
        {
            if (e.id == c.id)
            {
                e = c;
                return;
            }
        }
        sCommands.push_back(c);
    }

    void runCommand(const std::string& id)
    {
        for (const Command& c : sCommands)
        {
            if (c.id == id && c.run)
            {
                c.run();
                return;
            }
        }
    }

    const std::vector<Command>& commands()
    {
        return sCommands;
    }

    StatusInfo& status()
    {
        return sStatus;
    }

    Hooks& hooks()
    {
        return sHooks;
    }

    LoginState& login()
    {
        return sLogin;
    }

    void setWorkspaceBadge(const std::string& id, int count)
    {
        if (count > 0) sWorkspaceBadges[id] = count;
        else sWorkspaceBadges.erase(id);
    }

    bool shelfSlot(ImRect& out)
    {
        if (ImGui::GetFrameCount() - sShelfFrame > 1) return false;
        out = sShelfSlot;
        return true;
    }

    void toast(const std::string& text, bool error)
    {
        sToasts.push_back({ text, error, 0.f });
        if (sToasts.size() > 5) sToasts.erase(sToasts.begin());
    }

    namespace
    {
        float sPixelScale = 1.f;
    }

    void setPixelScale(float scale)
    {
        sPixelScale = scale > 0.1f ? scale : 1.f;
    }

    float pixelScale()
    {
        return sPixelScale;
    }

    void beginFrame(float width, float height, float dt)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(width, height);
        io.DisplayFramebufferScale = ImVec2(sPixelScale, sPixelScale);
        io.DeltaTime = dt > 0.f ? dt : 1.f / 60.f;
        ImGui::NewFrame();
    }

    void drawShell(bool signed_in)
    {
        const float dt = ImGui::GetIO().DeltaTime;
        if (!signed_in)
        {
            loginScreen();
            toasts(dt);
            return;
        }
        shortcuts();
        const bool bare = workspace() == "model";
        if (!bare) topBar();
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const ImRect area = bare ? ImRect(ImVec2(0, 0), disp)
                                 : ImRect(ImVec2(Theme::Gap, Theme::TopBarHeight + Theme::Gap * 0.8f), ImVec2(disp.x - Theme::Gap, disp.y - Theme::BottomBarHeight));
        Dock::Layout& l = current();
        const std::string before = Dock::serialize(l.root.get());
        const size_t floats = l.floating.size();
        {
            ImRect vp;
            const bool has_vp = Dock::viewport(vp);
            appBackground(vp, has_vp);
        }
        Dock::draw(l, area);
        if (before != Dock::serialize(l.root.get()) || floats != l.floating.size()) sDirty = true;
        if (!bare)
        {
            layoutOverlays();
            chatBar();
            bottomBar();
        }
        drawPalette();
        toasts(dt);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) sDirty = true;
        const double now = ImGui::GetTime();
        if (sDirty && now - sLastSave > 2.0)
        {
            saveState();
            sLastSave = now;
        }
    }

    ImDrawData* endFrame()
    {
        ImGui::Render();
        return ImGui::GetDrawData();
    }

    void focusChat()
    {
        sFocusChat = true;
    }

    void autoSignIn()
    {
        sAutoSignIn = true;
    }

    bool aiChatMode()
    {
        return sAiChat;
    }

    bool locationSlot(float& x0, float& y0, float& x1, float& y1)
    {
        if (ImGui::GetFrameCount() - sLocationFrame > 1 || sLocationSlot.GetWidth() < 120.f) return false;
        x0 = sLocationSlot.Min.x;
        y0 = sLocationSlot.Min.y;
        x1 = sLocationSlot.Max.x;
        y1 = sLocationSlot.Max.y;
        return true;
    }

    bool chatLogSlot(float& x0, float& y0, float& x1, float& y1)
    {
        if (ImGui::GetFrameCount() - sChatLogFrame > 1) return false;
        x0 = sChatLogSlot.Min.x;
        y0 = sChatLogSlot.Min.y;
        x1 = sChatLogSlot.Max.x;
        y1 = sChatLogSlot.Max.y;
        return true;
    }

    void setTopBar(const std::vector<BarItem>& items)
    {
        sTopItems = items;
    }

    void setBottomBar(const std::vector<BarItem>& items)
    {
        sBottomItems = items;
    }

    bool chatFieldActive()
    {
        return sChatActive;
    }

    bool viewportRect(float& x, float& y, float& w, float& h)
    {
        ImRect r;
        if (!Dock::viewport(r)) return false;
        x = r.Min.x;
        y = r.Min.y;
        w = r.GetWidth();
        h = r.GetHeight();
        return true;
    }
}
