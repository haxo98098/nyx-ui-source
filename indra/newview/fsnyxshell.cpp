#include "llviewerprecompiledheaders.h"

#include <cctype>

#include "fsnyxshell.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "nyxui.h"

#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llvoavatarself.h"
#include "llcallbacklist.h"
#include "llframetimer.h"
#include "llviewerstats.h"
#include "llkeyboard.h"
#include "llviewercamera.h"
#include "llselectmgr.h"
#include "llagent.h"
#include "llcontrol.h"
#include "lldir.h"
#include "llfile.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfloatertools.h"
#include "llbutton.h"
#include "llfloatersnapshot.h"
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llviewerobject.h"
#include "llinventorybridge.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "llpanelmaininventory.h"
#include "llsidepanelinventory.h"
#include "lldraghandle.h"
#include "llviewerinventory.h"
#include "llmutelist.h"
#include "llimview.h"
#include "fsfloaterim.h"
#include "lldate.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "llfocusmgr.h"
#include "llimagepng.h"
#include <array>
#include <optional>
#include "llimagebmp.h"
#include "llimagejpeg.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llrender.h"
#include "llstartup.h"
#include "lltooldraganddrop.h"
#include "llviewerassettype.h"
#include "fsmesheditinput.h"
#include "lltoolbrush.h"
#include "lltoolselectland.h"
#include "llslider.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llweb.h"
#include "llwin32headers.h"
#include "llwindow.h"
#include "pipeline.h"
#include "llrootview.h"
#include "llstatusbar.h"
#include "llteleporthistory.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvocache.h"
#include "lldatapacker.h"
#include "lldrawable.h"
#include <unordered_set>
#include "llviewerstats.h"
#include "fsfloaternearbychat.h"
#include "fsradar.h"
#include "llchatentry.h"
#include "fsnyx.h"
#include "llmultifloater.h"
#include "llmodaldialog.h"
#include "llrender2dutils.h"
#include "rlvactions.h"
#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llviewertexture.h"
#include "llparcel.h"
#include "llpanelland.h"
#include "llfloaterland.h"
#include "lltabcontainer.h"
#include "fsstormpanels.h"
#include "aoengine.h"
#include "fsdynamictheme.h"
#include "llgl.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llviewernetwork.h"
#include "llviewerobjectlist.h"
#include "lltoolcomp.h"
#include "lltoolface.h"
#include "qtoolalign.h"
#include "lltoolmgr.h"
#include "indra_constants.h"

#include <ctime>
#include <fstream>
#include <map>
#include <set>

// The Nyx interface shell is gone: the interface is Firestorm's own, in the Nyx skin. What is left
// here are the three Nyx panels that never were Firestorm windows - Ani's settings, the Nyx
// assistant and the Nyx browser. Each is a floating window of its own, opened from the menu bar
// (Nyx menu) and open for as long as its setting is on:
//   NyxWindowAni, NyxWindowAssistant, NyxWindowBrowser
// The FSNyxShell interface (fsnyxshell.h) is unchanged, so the viewer window still hands draw and
// input here; with no panel window open, all of it is a no-op.

bool nyxBrowserKey(U32 key, U32 mask, bool down);
bool nyxBrowserChar(U32 cp);
void nyxBrowserPanel();
void nyxAssistantPanel();
void nyxAniSettingsPanel();
void nyxBrainSettingsPanel();
void fsJustMyLandPickParcels(bool on);
bool fsJustMyLandPicking();

namespace FSStormWorkspace { bool showBuildTools(); }

using namespace NyxUI;

void nyxToast(const std::string& text, bool error);
// Portals live with the other world code.
LLSD nyxPortalsList();
bool nyxPortalAddHere(const std::string& label, const std::string& dest_slurl);
void nyxPortalRemove(const std::string& id);
void nyxPortalsTick();

namespace
{
    void changesPanel();
    void noticesPanel();
    void assistantWorkspacePanel();
    void layoutsPanel();
    void viewfinderPanel();
    void inventoryPanel();
    void conversationsPanel();
    void libraryPanel();
    void portalsPanel();
    bool stormRoutePortals();

    struct PanelWindow
    {
        const char* id;        // "ani", "assistant", "browser"
        const char* title;
        const char* setting;
        ImVec2 size;           // first size
        ImVec2 min_size;
        void (*body)();
        bool focus = false;    // bring to the front on the next frame
        bool was_open = false;
    };

    PanelWindow sWindows[] = {
        { "ani", "Ani", "NyxWindowAni", ImVec2(460.f, 640.f), ImVec2(380.f, 360.f), []() { nyxAniSettingsPanel(); } },
        { "assistant", "Nyx Assistant", "NyxWindowAssistant", ImVec2(440.f, 620.f), ImVec2(360.f, 320.f), []() { assistantWorkspacePanel(); } },
        { "browser", "Nyx Browser", "NyxWindowBrowser", ImVec2(980.f, 680.f), ImVec2(480.f, 320.f), []() { nyxBrowserPanel(); } },
        { "changes", "What I Changed", "NyxWindowChanges", ImVec2(640.f, 520.f), ImVec2(460.f, 260.f), []() { changesPanel(); } },
        { "notices", "Notices", "NyxWindowNotices", ImVec2(460.f, 560.f), ImVec2(340.f, 240.f), []() { noticesPanel(); } },
        { "layouts", "Window Layouts", "NyxWindowLayouts", ImVec2(440.f, 330.f), ImVec2(400.f, 300.f), []() { layoutsPanel(); } },
        { "viewfinder", "Photo Viewfinder", "NyxViewfinder", ImVec2(320.f, 340.f), ImVec2(280.f, 170.f), []() { viewfinderPanel(); } },
        { "inventory", "Inventory Lens", "NyxWindowInventory", ImVec2(760.f, 560.f), ImVec2(520.f, 280.f), []() { inventoryPanel(); } },
        { "conversations", "Conversations", "NyxWindowConversations", ImVec2(420.f, 560.f), ImVec2(320.f, 240.f), []() { conversationsPanel(); } },
        { "library", "Quick Library", "NyxWindowLibrary", ImVec2(560.f, 420.f), ImVec2(420.f, 200.f), []() { libraryPanel(); } },
        { "portals", "Portals", "NyxWindowPortals", ImVec2(440.f, 380.f), ImVec2(380.f, 240.f), []() { portalsPanel(); } },
    };

    bool sStarted = false;
    bool sFailed = false;
    LLTimer sFrameTimer;
    bool sHeld[3] = { false, false, false };

    void studioDraw(float width, float height);
    void studioChrome(bool hide);
    bool studioEnabled();
    bool studioKey(KEY key, MASK mask, bool down);
    void studioOpenConversation(const LLUUID& id);

    F32 scale()
    {
        return gViewerWindow ? llmax(0.5f, gViewerWindow->getDisplayScale().mV[VX]) : 1.f;
    }

    bool windowWanted(const PanelWindow& w)
    {
        return gSavedSettings.controlExists(w.setting) && gSavedSettings.getBOOL(w.setting);
    }

    bool anyWanted()
    {
        for (const PanelWindow& w : sWindows)
            if (windowWanted(w)) return true;
        return false;
    }

    // ---- Brand and icons -----------------------------------------------------------------------

    Tex uploadPng(const std::string& path, float& w, float& h)
    {
        LLPointer<LLImagePNG> png = new LLImagePNG;
        if (!png->load(path)) return 0;
        LLPointer<LLImageRaw> raw = new LLImageRaw;
        if (!png->decode(raw, 0.f)) return 0;
        if (raw->getComponents() != 4)
        {
            LLPointer<LLImageRaw> rgba = new LLImageRaw(raw->getWidth(), raw->getHeight(), 4);
            rgba->copy(raw);
            raw = rgba;
        }
        const S32 iw = raw->getWidth();
        const S32 ih = raw->getHeight();
        // Decoded images are bottom row first; interface textures are top row first.
        std::vector<U8> rows((size_t)iw * ih * 4);
        const U8* src = raw->getData();
        for (S32 y = 0; y < ih; ++y) memcpy(&rows[(size_t)y * iw * 4], src + (size_t)(ih - 1 - y) * iw * 4, (size_t)iw * 4);
        w = (float)iw;
        h = (float)ih;
        return createTexture(iw, ih, rows.data(), true);
    }

    bool readAtlas(const std::string& txt, int& cell, int& cols, std::map<unsigned, int>& cells)
    {
        std::ifstream f(txt);
        std::string tag, name;
        int rows = 0, idx = 0;
        unsigned cp = 0;
        if (!(f >> tag >> cell >> tag >> cols >> tag >> rows) || cell <= 0 || cols <= 0) return false;
        while (f >> cp >> idx >> name) cells[cp] = idx;
        return true;
    }

    void loadBrand()
    {
        const std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "nyxui") + gDirUtilp->getDirDelimiter();
        float w = 0.f, h = 0.f;
        if (Tex t = uploadPng(dir + "nyx_wordmark.png", w, h)) setImage("wordmark", t, w, h);
        if (Tex t = uploadPng(dir + "nyx_mark.png", w, h)) setImage("mark", t, w, h);
        if (Tex t = uploadPng(dir + "nyxstorm_emblem.png", w, h)) setImage("storm_emblem", t, w, h);
        if (Tex t = uploadPng(dir + "nyx_studio_world.png", w, h)) setImage("studio_world", t, w, h);
        int cell = 0, cols = 0;
        std::map<unsigned, int> cells;
        if (readAtlas(dir + "nyx_glow_icons.txt", cell, cols, cells))
        {
            if (Tex t = uploadPng(dir + "nyx_glow_icons.png", w, h)) setColorIconAtlas(t, cell, cols, w, h, cells);
        }
        cells.clear();
        if (readAtlas(dir + "nyx_icons.txt", cell, cols, cells))
        {
            if (Tex t = uploadPng(dir + "nyx_icons.png", w, h)) setIconAtlas(t, cell, cols, w, h, cells);
        }
    }

    bool start()
    {
        if (sStarted) return true;
        if (sFailed) return false;
        setPixelScale(scale());
        const std::string font_dir = gDirUtilp->getExecutableDir() + gDirUtilp->getDirDelimiter() + "fonts";
        const std::string user_dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "");
        if (!init(font_dir, user_dir) || !glInit())
        {
            LL_WARNS("NyxWindows") << "The Nyx panel windows failed to start" << LL_ENDL;
            sFailed = true;
            return false;
        }
        loadBrand();
        hooks().openUrl = [](const std::string& url) { LLWeb::loadURL(url); };
        sStarted = true;
        return true;
    }

    // One of the panel windows is under this point (interface pixels).
    bool windowsOwn(float x, float y)
    {
        ImGuiContext* g = ImGui::GetCurrentContext();
        if (!g) return false;
        for (ImGuiWindow* w : g->Windows)
        {
            if (!w->Active || w->Hidden || (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoMouseInputs))) continue;
            if (ImRect(w->Pos, w->Pos + w->Size).Contains(ImVec2(x, y))) return true;
        }
        return false;
    }

    void letGo()
    {
        if (!ImGui::GetCurrentContext()) return;
        ImGui::ClearActiveID();
        ImGui::FocusWindow(nullptr);
    }


    // ---- Viewer tools --------------------------------------------------------------------------
    // Four small tools that live beside the panels: what you changed in the settings, a notice
    // centre, saved window layouts, and a photo viewfinder. Plus "streaming mode", a switch with
    // no window.

    std::string valueText(const LLSD& v)
    {
        if (v.isArray())
        {
            std::string s;
            for (LLSD::array_const_iterator it = v.beginArray(); it != v.endArray(); ++it) s += (s.empty() ? "" : ", ") + valueText(*it);
            return s;
        }
        if (v.isBoolean()) return v.asBoolean() ? "on" : "off";
        if (v.isReal()) return llformat("%.4g", v.asReal());
        if (v.isMap()) return "(several values)";
        std::string s = v.asString();
        if (s.size() > 60) s = s.substr(0, 57) + "...";
        return s.empty() ? "(empty)" : s;
    }

    LLControlVariable* findControl(const std::string& name, bool account)
    {
        LLControlGroup& g = account ? gSavedPerAccountSettings : gSavedSettings;
        return g.controlExists(name) ? g.getControl(name).get() : nullptr;
    }

    // -- What I changed --

    struct Changed
    {
        std::string name, comment, now, def;
        bool account = false;
        bool bookkeeping = false;
    };
    std::vector<Changed> sChanged;
    F64 sChangedAt = -10.0;

    bool isBookkeeping(const std::string& n)
    {
        static const char* prefix[] = { "Last", "First", "NyxWindow", "NyxViewfinder", "Warn", "Window", "FSInternal", "Session", "NextLogin",
                                        "CurrentGrid", "LoginLocation", "UserLogin", "Floater", "Saved", "Recent", "Version", "Crash", "Stats" };
        for (const char* p : prefix)
            if (n.compare(0, strlen(p), p) == 0) return true;
        return n.find("Rect") != std::string::npos || n.find("Position") != std::string::npos || n.find("Timestamp") != std::string::npos;
    }

    struct CollectChanged : public LLControlGroup::ApplyFunctor
    {
        bool account = false;
        void apply(const std::string& name, LLControlVariable* c) override
        {
            if (!c || !c->isPersisted() || c->isHiddenFromSettingsEditor()) return;
            Changed row;
            row.now = valueText(c->getValue());
            row.def = valueText(c->getDefault());
            if (row.now == row.def) return;
            row.name = name;
            row.comment = c->getComment();
            row.account = account;
            row.bookkeeping = row.comment.empty() || isBookkeeping(name);
            sChanged.push_back(row);
        }
    };

    void collectChanged()
    {
        sChanged.clear();
        CollectChanged f;
        gSavedSettings.applyToAll(&f);
        f.account = true;
        gSavedPerAccountSettings.applyToAll(&f);
        std::sort(sChanged.begin(), sChanged.end(), [](const Changed& a, const Changed& b) { return LLStringUtil::compareInsensitive(a.name, b.name) < 0; });
        sChangedAt = LLTimer::getElapsedSeconds();
    }

    bool containsNoCase(const std::string& hay, const std::string& needle)
    {
        if (needle.empty()) return true;
        std::string h = hay, n = needle;
        LLStringUtil::toLower(h);
        LLStringUtil::toLower(n);
        return h.find(n) != std::string::npos;
    }

    void changesPanel()
    {
        static char filter[96] = "";
        static bool all = false;
        static F64 arm_reset = -10.0;
        const F64 now = LLTimer::getElapsedSeconds();
        if (now - sChangedAt > 1.5) collectChanged();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 210.f);
        ImGui::InputTextWithHint("##chg_filter", "Find a setting", filter, sizeof(filter));
        ImGui::SameLine();
        ImGui::Checkbox("Housekeeping too", &all);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Also list what the viewer keeps for itself:\nlast-used values, window places and the like.");

        std::vector<const Changed*> rows;
        for (const Changed& c : sChanged)
        {
            if (c.bookkeeping && !all) continue;
            if (!containsNoCase(c.name, filter) && !containsNoCase(c.comment, filter)) continue;
            rows.push_back(&c);
        }
        ImGui::TextDisabled("%d setting%s differ from a fresh install", (int)rows.size(), rows.size() == 1 ? "" : "s");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150.f);
        const bool armed = now - arm_reset < 4.0;
        if (ImGui::SmallButton(armed ? "Click again to reset all" : "Reset all listed") && !rows.empty())
        {
            if (armed)
            {
                for (const Changed* c : rows)
                    if (LLControlVariable* v = findControl(c->name, c->account)) v->resetToDefault(true);
                arm_reset = -10.0;
                collectChanged();
                return;
            }
            arm_reset = now;
        }
        ImGui::Separator();

        std::string reset_name;
        bool reset_account = false;
        if (ImGui::BeginTable("##chg_rows", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable,
                              ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Now", ImGuiTableColumnFlags_WidthStretch, 1.f);
            ImGui::TableSetupColumn("Fresh install", ImGuiTableColumnFlags_WidthStretch, 1.f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 56.f);
            ImGui::TableHeadersRow();
            for (const Changed* c : rows)
            {
                ImGui::TableNextRow();
                ImGui::PushID(c->name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(c->name.c_str());
                if (ImGui::IsItemHovered() && !c->comment.empty())
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(360.f);
                    ImGui::TextUnformatted(c->comment.c_str());
                    if (c->account) ImGui::TextDisabled("Kept per account.");
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(c->now.c_str());
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", c->def.c_str());
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Reset"))
                {
                    reset_name = c->name;
                    reset_account = c->account;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (!reset_name.empty())
        {
            if (LLControlVariable* v = findControl(reset_name, reset_account)) v->resetToDefault(true);
            collectChanged();
        }
    }

    // -- Notice centre --

    // Notification text carries toast markup (<nolink>, <icon>, <b>, <i>, <a href>...).
    // ImGui prints it raw, so strip tag-looking spans. A literal "<3" or "a < b" survives.
    std::string plainNotice(const std::string& t)
    {
        std::string out;
        out.reserve(t.size());
        for (size_t i = 0; i < t.size(); ++i)
        {
            if (t[i] == '<' && i + 1 < t.size() && (std::isalpha((unsigned char)t[i + 1]) || t[i + 1] == '/'))
            {
                const size_t end = t.find('>', i);
                if (end != std::string::npos && end - i < 120)
                {
                    i = end;
                    continue;
                }
            }
            out += t[i];
        }
        return out;
    }

    struct Notice
    {
        std::string text;
        std::string when;
    };
    struct Sender
    {
        std::string name;
        LLUUID id;
        bool object = false;
        bool hidden = false;   // this session only
        int unseen = 0;
        std::vector<Notice> notices;
    };
    std::vector<Sender> sSenders;
    LLNotificationChannelPtr sNoticeCentreChannel;
    boost::signals2::connection sNoticeCentreConnection;
    int sNoticeUnseen = 0;

    bool onCentreNotice(const LLSD& payload)
    {
        if (payload["sigtype"].asString() != "add") return false;
        LLNotificationPtr n = LLNotifications::instance().find(payload["id"].asUUID());
        if (!n) return false;
        std::string text = plainNotice(n->getMessage());
        LLStringUtil::trim(text);
        if (text.empty()) return false;
        if (text.size() > 700) text = text.substr(0, 700) + "...";
        const LLSD subs = n->getSubstitutions();
        const LLSD pl = n->getPayload();
        Sender who;
        for (const char* k : { "NAME", "OBJECTNAME", "FROM_NAME", "TITLE" })
        {
            if (who.name.empty() && subs.has(k)) who.name = subs[k].asString();
        }
        if (pl.has("from_id")) who.id = pl["from_id"].asUUID();
        if (who.id.isNull() && pl.has("object_id"))
        {
            who.id = pl["object_id"].asUUID();
            who.object = true;
        }
        if (who.name.empty() && pl.has("from_name")) who.name = pl["from_name"].asString();
        if (who.name.empty()) who.name = "Nyx Storm";
        if (who.name.size() > 60) who.name = who.name.substr(0, 57) + "...";

        Sender* dst = nullptr;
        for (Sender& s : sSenders)
            if (s.name == who.name && (s.id == who.id || s.id.isNull() || who.id.isNull())) dst = &s;
        if (!dst)
        {
            sSenders.push_back(who);
            dst = &sSenders.back();
        }
        if (dst->id.isNull()) dst->id = who.id;
        Notice note;
        note.text = text;
        time_t t = time(nullptr);
        char buf[16] = "";
        if (struct tm* lt = localtime(&t)) strftime(buf, sizeof(buf), "%H:%M", lt);
        note.when = buf;
        dst->notices.push_back(note);
        if (dst->notices.size() > 60) dst->notices.erase(dst->notices.begin());
        if (!dst->hidden)
        {
            ++dst->unseen;
            ++sNoticeUnseen;
        }
        return false;
    }

    void noticeHook()
    {
        if (sNoticeCentreChannel) return;
        sNoticeCentreChannel = new LLNotificationChannel("NyxNoticeCentre", "Visible", [](LLNotificationPtr) { return true; });
        sNoticeCentreConnection = LLNotifications::instance().getChannel("NyxNoticeCentre")->connectChanged(&onCentreNotice);
    }

    void noticesPanel()
    {
        static char filter[96] = "";
        static bool show_hidden = false;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 250.f);
        ImGui::InputTextWithHint("##ntc_filter", "Find in notices", filter, sizeof(filter));
        ImGui::SameLine();
        ImGui::Checkbox("Hidden senders", &show_hidden);
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear all")) sSenders.clear();
        sNoticeUnseen = 0;
        ImGui::Separator();
        if (sSenders.empty())
        {
            ImGui::TextDisabled("Nothing yet. Notices from this session collect here, by who sent them.");
            return;
        }
        ImGui::BeginChild("##ntc_list", ImGui::GetContentRegionAvail());
        int erase = -1;
        // Newest sender first.
        for (int i = (int)sSenders.size() - 1; i >= 0; --i)
        {
            Sender& s = sSenders[i];
            if (s.hidden && !show_hidden) continue;
            bool match = containsNoCase(s.name, filter);
            if (!match)
                for (const Notice& n : s.notices) match = match || containsNoCase(n.text, filter);
            if (!match) continue;
            ImGui::PushID(i);
            const std::string head = llformat("%s   (%d)%s###snd", s.name.c_str(), (int)s.notices.size(), s.unseen > 0 ? "  new" : "");
            const bool open = ImGui::CollapsingHeader(head.c_str(), s.unseen > 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            if (open)
            {
                s.unseen = 0;
                if (ImGui::SmallButton(s.hidden ? "Show again" : "Hide until I log out")) s.hidden = !s.hidden;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stops counting this sender as new here.\nNothing is blocked; the notices still arrive.");
                if (s.id.notNull() && s.id != gAgentID)
                {
                    ImGui::SameLine();
                    const bool muted = LLMuteList::getInstance()->isMuted(s.id);
                    if (ImGui::SmallButton(muted ? "Unblock" : "Block sender"))
                    {
                        LLMute mute(s.id, s.name, s.object ? LLMute::OBJECT : LLMute::AGENT);
                        if (muted) LLMuteList::getInstance()->remove(mute);
                        else LLMuteList::getInstance()->add(mute);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Puts this sender on your block list, the same as Block in their profile.");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear")) erase = i;
                for (int k = (int)s.notices.size() - 1; k >= 0; --k)
                {
                    const Notice& n = s.notices[k];
                    if (!containsNoCase(n.text, filter) && !containsNoCase(s.name, filter)) continue;
                    ImGui::TextDisabled("%s", n.when.c_str());
                    ImGui::SameLine(58.f);
                    ImGui::PushTextWrapPos(0.f);
                    ImGui::TextUnformatted(n.text.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::Spacing();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (erase >= 0) sSenders.erase(sSenders.begin() + erase);
    }

    // -- Window layouts --

    const int LAYOUT_SLOTS = 4;
    LLSD sLayouts;
    bool sLayoutsLoaded = false;

    std::string layoutFile()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "nyx_window_layouts.xml");
    }

    void layoutsLoad()
    {
        if (sLayoutsLoaded) return;
        sLayoutsLoaded = true;
        llifstream in(layoutFile().c_str());
        if (in.is_open()) LLSDSerialize::fromXML(sLayouts, in);
        if (!sLayouts.isMap()) sLayouts = LLSD::emptyMap();
    }

    void layoutsWrite()
    {
        llofstream out(layoutFile().c_str());
        if (out.is_open()) LLSDSerialize::toPrettyXML(sLayouts, out);
    }

    int layoutSave(int slot, const std::string& label)
    {
        layoutsLoad();
        if (!gFloaterView) return 0;
        LLSD wins = LLSD::emptyArray();
        for (LLView* v : *gFloaterView->getChildList())
        {
            LLFloater* f = dynamic_cast<LLFloater*>(v);
            if (!f || !f->getVisible() || f->isMinimized() || f->getInstanceName().empty()) continue;
            if (!f->getKey().isUndefined()) continue;   // a conversation, a profile: belongs to a moment, not to a layout
            const LLRect r = f->getRect();
            LLSD w;
            w["name"] = f->getInstanceName();
            w["left"] = r.mLeft;
            w["bottom"] = r.mBottom;
            w["width"] = r.getWidth();
            w["height"] = r.getHeight();
            wins.append(w);
        }
        LLSD entry;
        entry["label"] = label;
        entry["windows"] = wins;
        sLayouts[llformat("%d", slot)] = entry;
        layoutsWrite();
        return (int)wins.size();
    }

    bool layoutApply(int slot, bool close_others)
    {
        layoutsLoad();
        const std::string key = llformat("%d", slot);
        if (!sLayouts.has(key) || !gFloaterView) return false;
        const LLSD wins = sLayouts[key]["windows"];
        std::set<std::string> wanted;
        for (LLSD::array_const_iterator it = wins.beginArray(); it != wins.endArray(); ++it)
        {
            const std::string name = (*it)["name"].asString();
            if (name.empty()) continue;
            wanted.insert(name);
            LLFloater* f = LLFloaterReg::findInstance(name);
            if (!f || !f->getVisible()) f = LLFloaterReg::showInstance(name);
            if (!f) continue;
            if (f->isMinimized()) f->setMinimized(false);
            LLRect r;
            S32 w = (*it)["width"].asInteger(), h = (*it)["height"].asInteger();
            if (!f->isResizable())
            {
                w = f->getRect().getWidth();
                h = f->getRect().getHeight();
            }
            r.setOriginAndSize((*it)["left"].asInteger(), (*it)["bottom"].asInteger(), w, h);
            f->setShape(r);
            gFloaterView->adjustToFitScreen(f, false);
        }
        if (close_others)
        {
            std::vector<LLFloater*> shut;
            for (LLView* v : *gFloaterView->getChildList())
            {
                LLFloater* f = dynamic_cast<LLFloater*>(v);
                if (f && f->getVisible() && !f->getInstanceName().empty() && f->getKey().isUndefined() && !wanted.count(f->getInstanceName()) &&
                    f->isCloseable())
                    shut.push_back(f);
            }
            for (LLFloater* f : shut) f->closeFloater();
        }
        return true;
    }

    void layoutsPanel()
    {
        layoutsLoad();
        static char names[LAYOUT_SLOTS][40];
        static bool names_read = false;
        static bool close_others = false;
        if (!names_read)
        {
            names_read = true;
            for (int i = 0; i < LAYOUT_SLOTS; ++i)
            {
                const std::string key = llformat("%d", i + 1);
                const std::string label = sLayouts.has(key) ? sLayouts[key]["label"].asString() : std::string();
                strncpy(names[i], label.c_str(), sizeof(names[i]) - 1);
                names[i][sizeof(names[i]) - 1] = 0;
            }
        }
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("A layout remembers which windows are open, where, and how big. Ctrl+Alt+1 to 4 switches to one; "
                            "Ctrl+Alt+Shift+1 to 4 saves the windows as they are now.");
        ImGui::PopTextWrapPos();
        ImGui::Checkbox("Close windows that are not part of the layout", &close_others);
        gSavedSettings.setBOOL("NyxLayoutCloseOthers", close_others);
        ImGui::Separator();
        for (int i = 0; i < LAYOUT_SLOTS; ++i)
        {
            const std::string key = llformat("%d", i + 1);
            const bool have = sLayouts.has(key);
            ImGui::PushID(i);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%d", i + 1);
            ImGui::SameLine(28.f);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 190.f);
            ImGui::InputTextWithHint("##name", have ? "Layout" : "Empty - name it, then Save", names[i], sizeof(names[i]));
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                const int n = layoutSave(i + 1, names[i]);
                nyxToast(llformat("Layout %d saved: %d window%s.", i + 1, n, n == 1 ? "" : "s"), false);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!have);
            if (ImGui::Button("Use")) layoutApply(i + 1, close_others);
            ImGui::SameLine();
            if (ImGui::Button("Forget"))
            {
                sLayouts.erase(key);
                layoutsWrite();
                names[i][0] = 0;
            }
            ImGui::EndDisabled();
            if (have) ImGui::TextDisabled("      %d window%s", (int)sLayouts[key]["windows"].size(), sLayouts[key]["windows"].size() == 1 ? "" : "s");
            ImGui::PopID();
        }
    }

    // Ctrl+Alt+digit: use a layout. With Shift: save it.
    bool layoutHotkey(KEY key, MASK mask)
    {
        if (key < '1' || key >= '1' + LAYOUT_SLOTS) return false;
        if ((mask & (MASK_CONTROL | MASK_ALT)) != (MASK_CONTROL | MASK_ALT)) return false;
        const int slot = (int)(key - '1') + 1;
        if (mask & MASK_SHIFT)
        {
            layoutsLoad();
            const std::string k = llformat("%d", slot);
            const int n = layoutSave(slot, sLayouts.has(k) ? sLayouts[k]["label"].asString() : std::string());
            nyxToast(llformat("Layout %d saved: %d window%s.", slot, n, n == 1 ? "" : "s"), false);
            return true;
        }
        if (!layoutApply(slot, gSavedSettings.getBOOL("NyxLayoutCloseOthers"))) nyxToast(llformat("Layout %d is empty. Ctrl+Alt+Shift+%d saves one.", slot, slot), false);
        return true;
    }

    // -- Photo viewfinder --

    const char* const VF_ASPECTS[] = { "Whole view", "1 : 1", "4 : 5", "3 : 2", "16 : 9", "2.39 : 1", "9 : 16" };
    const float VF_RATIO[] = { 0.f, 1.f, 0.8f, 1.5f, 16.f / 9.f, 2.39f, 9.f / 16.f };
    const char* const VF_GUIDES[] = { "None", "Thirds", "Golden ratio", "Diagonals", "Centre cross", "Thirds and diagonals" };

    void viewfinderOverlay(float W, float H, float margin = 24.f)
    {
        const int aspect = llclamp(gSavedSettings.getS32("NyxViewfinderAspect"), 0, (S32)LL_ARRAY_SIZE(VF_RATIO) - 1);
        const int guide = llclamp(gSavedSettings.getS32("NyxViewfinderGuide"), 0, (S32)LL_ARRAY_SIZE(VF_GUIDES) - 1);
        const float dim = llclamp(gSavedSettings.getF32("NyxViewfinderDim"), 0.f, 1.f);
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        float x0 = 0.f, y0 = 0.f, x1 = W, y1 = H;
        if (VF_RATIO[aspect] > 0.f)
        {
            float fw = W - margin * 2.f, fh = fw / VF_RATIO[aspect];
            if (fh > H - margin * 2.f)
            {
                fh = H - margin * 2.f;
                fw = fh * VF_RATIO[aspect];
            }
            x0 = floorf((W - fw) * 0.5f);
            y0 = floorf((H - fh) * 0.5f);
            x1 = x0 + floorf(fw);
            y1 = y0 + floorf(fh);
            const ImU32 shade = IM_COL32(0, 0, 0, (int)(dim * 255.f));
            dl->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(W, y0), shade);
            dl->AddRectFilled(ImVec2(0.f, y1), ImVec2(W, H), shade);
            dl->AddRectFilled(ImVec2(0.f, y0), ImVec2(x0, y1), shade);
            dl->AddRectFilled(ImVec2(x1, y0), ImVec2(W, y1), shade);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(255, 255, 255, 150));
        }
        // Every guide is drawn twice, dark under light, so it reads on a bright sky and in a dark room.
        auto rule = [&](float ax, float ay, float bx, float by)
        {
            dl->AddLine(ImVec2(ax + 1.f, ay + 1.f), ImVec2(bx + 1.f, by + 1.f), IM_COL32(0, 0, 0, 90));
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), IM_COL32(255, 255, 255, 120));
        };
        const float fw = x1 - x0, fh = y1 - y0;
        auto split = [&](float a, float b)
        {
            rule(x0 + fw * a, y0, x0 + fw * a, y1);
            rule(x0 + fw * b, y0, x0 + fw * b, y1);
            rule(x0, y0 + fh * a, x1, y0 + fh * a);
            rule(x0, y0 + fh * b, x1, y0 + fh * b);
        };
        if (guide == 1 || guide == 5) split(1.f / 3.f, 2.f / 3.f);
        if (guide == 2) split(0.382f, 0.618f);
        if (guide == 3 || guide == 5)
        {
            rule(x0, y0, x1, y1);
            rule(x0, y1, x1, y0);
        }
        if (guide == 4)
        {
            const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, arm = llmin(fw, fh) * 0.06f;
            rule(cx - arm, cy, cx + arm, cy);
            rule(cx, cy - arm, cx, cy + arm);
        }
    }

    void histogramDraw();

    void viewfinderPanel()
    {
        int aspect = llclamp(gSavedSettings.getS32("NyxViewfinderAspect"), 0, (S32)LL_ARRAY_SIZE(VF_RATIO) - 1);
        int guide = llclamp(gSavedSettings.getS32("NyxViewfinderGuide"), 0, (S32)LL_ARRAY_SIZE(VF_GUIDES) - 1);
        float dim = gSavedSettings.getF32("NyxViewfinderDim");
        ImGui::SetNextItemWidth(-90.f);
        if (ImGui::Combo("Frame", &aspect, VF_ASPECTS, (int)LL_ARRAY_SIZE(VF_ASPECTS))) gSavedSettings.setS32("NyxViewfinderAspect", aspect);
        ImGui::SetNextItemWidth(-90.f);
        if (ImGui::Combo("Guides", &guide, VF_GUIDES, (int)LL_ARRAY_SIZE(VF_GUIDES))) gSavedSettings.setS32("NyxViewfinderGuide", guide);
        ImGui::SetNextItemWidth(-90.f);
        if (ImGui::SliderFloat("Outside", &dim, 0.f, 1.f, dim <= 0.01f ? "clear" : (dim >= 0.99f ? "black" : "%.2f"))) gSavedSettings.setF32("NyxViewfinderDim", dim);
        bool meter = gSavedSettings.getBOOL("NyxViewfinderHistogram");
        if (ImGui::Checkbox("Exposure meter", &meter)) gSavedSettings.setBOOL("NyxViewfinderHistogram", meter);
        ImGui::SameLine();
        bool frozen = gSavedSettings.getBOOL("FreezeTime");
        if (ImGui::Checkbox("Freeze the world", &frozen)) gSavedSettings.setBOOL("FreezeTime", frozen);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stops avatars, particles and wind where they are, so you can move the camera around a moment.");
        if (meter) histogramDraw();
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("The frame and guides are drawn over the world only: they are not in the snapshot. "
                            "Set the snapshot to the same shape to keep what you framed.");
        ImGui::PopTextWrapPos();
    }

    // -- Streaming mode --
    // One switch for showing your screen to an audience: other people's name tags, your balance,
    // where you are and who comes online stay off screen. What it changed is written down, so
    // switching it off (even after a restart) puts every setting back as it was.

    struct StreamSetting
    {
        const char* name;
        LLSD value;
    };
    const StreamSetting STREAM_SET[] = {
        { "AvatarNameTagMode", LLSD((S32)0) },
        { "FSShowCurrencyBalanceInStatusbar", LLSD(false) },
        { "ShowMenuBarLocation", LLSD(false) },
        { "ShowNavbarNavigationPanel", LLSD(false) },
        { "ChatOnlineNotification", LLSD(false) },
        { "ShowHoverTips", LLSD(false) },
    };

    std::string streamFile()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "nyx_streaming_saved.xml");
    }

    void streamingTick()
    {
        static int applied = -1;   // -1 unknown, 0 off, 1 on
        if (!gSavedSettings.controlExists("NyxStreamingMode")) return;
        const int want = gSavedSettings.getBOOL("NyxStreamingMode") ? 1 : 0;
        if (want == applied) return;
        const bool first = applied < 0;
        applied = want;
        const std::string file = streamFile();
        if (want)
        {
            // Keep the values from before, unless a record is already there (on at the last exit).
            if (!LLFile::isfile(file))
            {
                LLSD saved = LLSD::emptyMap();
                for (const StreamSetting& s : STREAM_SET)
                    if (LLControlVariable* c = findControl(s.name, false)) saved[s.name] = c->getValue();
                llofstream out(file.c_str());
                if (out.is_open()) LLSDSerialize::toPrettyXML(saved, out);
            }
            for (const StreamSetting& s : STREAM_SET)
                if (LLControlVariable* c = findControl(s.name, false)) c->set(s.value);
            if (!first) nyxToast("Streaming mode is on: name tags, your balance, your location and online notices are hidden.", false);
        }
        else if (LLFile::isfile(file))
        {
            LLSD saved;
            {
                llifstream in(file.c_str());
                if (in.is_open()) LLSDSerialize::fromXML(saved, in);
            }
            if (saved.isMap())
            {
                for (const StreamSetting& s : STREAM_SET)
                    if (saved.has(s.name))
                        if (LLControlVariable* c = findControl(s.name, false)) c->set(saved[s.name]);
            }
            LLFile::remove(file);
            if (!first) nyxToast("Streaming mode is off: your settings are back as they were.", false);
        }
    }

    // -- Inventory lens --
    // A flat, searchable look at the whole inventory: everything in one sortable list, the names
    // you own more than once, and the folders that hold the most. It only reads; a click shows the
    // item in the normal inventory window, a double click opens it.

    struct InvRow
    {
        LLUUID id;
        std::string name, lname, folder, added;
        LLAssetType::EType type = LLAssetType::AT_NONE;
        time_t date = 0;
        bool link = false;
        bool twin = false;   // another item of the same kind has this name
    };
    struct InvFolder
    {
        LLUUID id;
        std::string name;
        int items = 0;
    };
    std::vector<InvRow> sInv;
    std::vector<InvFolder> sInvFolders;
    std::vector<int> sInvShown;
    bool sInvDirty = true;
    bool sInvRead = false;
    int sInvFolderCount = 0;

    struct InvKind
    {
        const char* label;
        LLAssetType::EType type;
    };
    const InvKind INV_KINDS[] = {
        { "Everything", LLAssetType::AT_NONE },   { "Objects", LLAssetType::AT_OBJECT },       { "Clothing", LLAssetType::AT_CLOTHING },
        { "Body parts", LLAssetType::AT_BODYPART }, { "Textures", LLAssetType::AT_TEXTURE },     { "Sounds", LLAssetType::AT_SOUND },
        { "Animations", LLAssetType::AT_ANIMATION }, { "Gestures", LLAssetType::AT_GESTURE },   { "Notecards", LLAssetType::AT_NOTECARD },
        { "Scripts", LLAssetType::AT_LSL_TEXT },    { "Landmarks", LLAssetType::AT_LANDMARK },   { "Materials", LLAssetType::AT_MATERIAL },
        { "Environments", LLAssetType::AT_SETTINGS },
    };

    const char* invKindName(LLAssetType::EType t)
    {
        for (const InvKind& k : INV_KINDS)
            if (k.type == t && t != LLAssetType::AT_NONE) return k.label;
        return "Other";
    }

    void invRead()
    {
        sInv.clear();
        sInvFolders.clear();
        sInvRead = true;
        sInvDirty = true;
        if (!gInventory.isInventoryUsable()) return;
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        gInventory.collectDescendents(gInventory.getRootFolderID(), cats, items, LLInventoryModel::EXCLUDE_TRASH);
        sInvFolderCount = (int)cats.size();
        std::map<LLUUID, int> per_folder;
        std::map<std::string, int> names;
        sInv.reserve(items.size());
        for (const LLPointer<LLViewerInventoryItem>& it : items)
        {
            if (it.isNull()) continue;
            InvRow r;
            r.id = it->getUUID();
            r.name = it->getName();
            r.lname = r.name;
            LLStringUtil::toLower(r.lname);
            r.link = it->getIsLinkType();
            r.type = it->getType();
            r.date = it->getCreationDate();
            char buf[16] = "";
            if (struct tm* lt = localtime(&r.date)) strftime(buf, sizeof(buf), "%Y-%m-%d", lt);
            r.added = buf;
            if (LLViewerInventoryCategory* parent = gInventory.getCategory(it->getParentUUID())) r.folder = parent->getName();
            ++per_folder[it->getParentUUID()];
            if (!r.link) ++names[llformat("%d|", (int)r.type) + r.lname];
            sInv.push_back(r);
        }
        for (InvRow& r : sInv)
            if (!r.link && names[llformat("%d|", (int)r.type) + r.lname] > 1) r.twin = true;
        for (const auto& pf : per_folder)
        {
            InvFolder f;
            f.id = pf.first;
            f.items = pf.second;
            if (LLViewerInventoryCategory* c = gInventory.getCategory(pf.first)) f.name = c->getName();
            sInvFolders.push_back(f);
        }
        std::sort(sInvFolders.begin(), sInvFolders.end(), [](const InvFolder& a, const InvFolder& b) { return a.items > b.items; });
        if (sInvFolders.size() > 60) sInvFolders.resize(60);
    }

    // The outfits as a plain list you can read, print or keep: names only, nothing is copied.
    void outfitListSave()
    {
        const LLUUID outfits = gInventory.findCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);
        if (outfits.isNull()) return;
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        gInventory.collectDescendents(outfits, cats, items, LLInventoryModel::EXCLUDE_TRASH);
        const std::string file = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "nyx_outfit_list.md");
        llofstream out(file.c_str());
        if (!out.is_open())
        {
            nyxToast("Could not write the outfit list.", true);
            return;
        }
        out << "# My outfits\n";
        int count = 0;
        for (const LLPointer<LLViewerInventoryCategory>& c : cats)
        {
            if (c.isNull()) continue;
            LLInventoryModel::cat_array_t sub;
            LLInventoryModel::item_array_t parts;
            gInventory.collectDescendents(c->getUUID(), sub, parts, LLInventoryModel::EXCLUDE_TRASH);
            if (parts.empty()) continue;
            ++count;
            out << "\n## " << c->getName() << "\n\n";
            std::vector<std::string> lines;
            for (const LLPointer<LLViewerInventoryItem>& p : parts)
                if (p.notNull()) lines.push_back(std::string("- ") + p->getName() + "  (" + invKindName(p->getType()) + ")");
            std::sort(lines.begin(), lines.end());
            for (const std::string& l : lines) out << l << "\n";
        }
        out.close();
        nyxToast(llformat("%d outfit%s listed in %s", count, count == 1 ? "" : "s", file.c_str()), false);
    }

    void inventoryPanel()
    {
        static char filter[96] = "";
        static int kind = 0, sort = 0;
        static bool links = false, twins = false;
        static LLUUID selected;
        if (!sInvRead) invRead();

        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputTextWithHint("##inv_filter", "Find by name or folder", filter, sizeof(filter))) sInvDirty = true;
        ImGui::SetNextItemWidth(130.f);
        if (ImGui::BeginCombo("##inv_kind", INV_KINDS[kind].label))
        {
            for (int i = 0; i < (int)LL_ARRAY_SIZE(INV_KINDS); ++i)
                if (ImGui::Selectable(INV_KINDS[i].label, i == kind))
                {
                    kind = i;
                    sInvDirty = true;
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(130.f);
        if (ImGui::Combo("##inv_sort", &sort, "Newest first\0Oldest first\0By name\0By folder\0")) sInvDirty = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Links", &links)) sInvDirty = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Doubles", &twins)) sInvDirty = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Only items whose name you own more than once.\nA quick way to find copies worth clearing out.");
        if (ImGui::GetContentRegionAvail().x > 600.f) ImGui::SameLine();
        if (ImGui::SmallButton("Read again")) invRead();

        if (sInvDirty)
        {
            sInvDirty = false;
            sInvShown.clear();
            std::string f = filter;
            LLStringUtil::toLower(f);
            for (int i = 0; i < (int)sInv.size(); ++i)
            {
                const InvRow& r = sInv[i];
                if (r.link && !links) continue;
                if (twins && !r.twin) continue;
                if (kind > 0 && r.type != INV_KINDS[kind].type) continue;
                if (!f.empty() && r.lname.find(f) == std::string::npos && !containsNoCase(r.folder, f)) continue;
                sInvShown.push_back(i);
            }
            std::sort(sInvShown.begin(), sInvShown.end(), [](int a, int b)
            {
                const InvRow& x = sInv[a];
                const InvRow& y = sInv[b];
                switch (sort)
                {
                    case 0: return x.date != y.date ? x.date > y.date : x.lname < y.lname;
                    case 1: return x.date != y.date ? x.date < y.date : x.lname < y.lname;
                    case 3: return x.folder != y.folder ? x.folder < y.folder : x.lname < y.lname;
                    default: return x.lname < y.lname;
                }
            });
        }

        ImGui::TextDisabled("%d of %d items, %d folders", (int)sInvShown.size(), (int)sInv.size(), sInvFolderCount);
        if (LLViewerInventoryItem* item = gInventory.getItem(selected))
        {
            ImGui::Separator();
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted(item->getName().c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Open selected")) LLInvFVBridgeAction::doAction(selected, &gInventory);
            ImGui::SameLine();
            if (ImGui::Button("Show in folder")) LLInventoryPanel::openInventoryPanelAndSetSelection(true, selected);
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear selection")) selected.setNull();
            ImGui::Separator();
        }
        if (!ImGui::BeginTabBar("##inv_tabs")) return;
        if (ImGui::BeginTabItem("Items"))
        {
            if (ImGui::BeginTable("##inv_rows", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                                  ImGui::GetContentRegionAvail()))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.4f);
                ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthStretch, 0.8f);
                ImGui::TableSetupColumn("Folder", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("Added", ImGuiTableColumnFlags_WidthFixed, 84.f);
                ImGui::TableHeadersRow();
                ImGuiListClipper clip;
                clip.Begin((int)sInvShown.size());
                LLUUID show, open;
                while (clip.Step())
                {
                    for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
                    {
                        const InvRow& r = sInv[sInvShown[i]];
                        ImGui::TableNextRow();
                        ImGui::PushID(i);
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(r.name.empty() ? "(no name)" : r.name.c_str(), selected == r.id,
                                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            if (ImGui::IsMouseDoubleClicked(0)) open = r.id;
                            else show = r.id;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s%s", invKindName(r.type), r.link ? " link" : "");
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", r.folder.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", r.added.c_str());
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
                if (open.notNull()) LLInvFVBridgeAction::doAction(open, &gInventory);
                else if (show.notNull()) selected = show;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fullest folders"))
        {
            ImGui::TextDisabled("Folders holding the most items directly. Click one to show it in your inventory.");
            ImGui::BeginChild("##inv_folders", ImGui::GetContentRegionAvail());
            const float most = sInvFolders.empty() ? 1.f : (float)llmax(sInvFolders[0].items, 1);
            LLUUID show;
            for (int i = 0; i < (int)sInvFolders.size(); ++i)
            {
                const InvFolder& f = sInvFolders[i];
                ImGui::PushID(i);
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const float w = ImGui::GetContentRegionAvail().x;
                // The bar behind the row is the share of the fullest folder.
                ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + w * (float)f.items / most, at.y + ImGui::GetTextLineHeightWithSpacing()),
                                                          IM_COL32(139, 124, 255, 46), 3.f);
                if (ImGui::Selectable(llformat("%5d   %s", f.items, f.name.empty() ? "(no name)" : f.name.c_str()).c_str())) show = f.id;
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (show.notNull()) LLInventoryPanel::openInventoryPanelAndSetSelection(true, show);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Outfit list"))
        {
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextDisabled("Writes every saved outfit and the names of its parts to a text file in your account's settings folder: "
                                "a record to read or print, and a checklist if you ever have to put an outfit back together. "
                                "It lists names only; it copies no content.");
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Write the outfit list")) outfitListSave();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // -- Conversations --
    // Every open conversation as a card (who, the last line, how many unread), and above them the
    // offers still waiting for an answer - teleports, friendship, inventory, group invitations -
    // with their own buttons and the time they have left. One accent colour, used only for the
    // answer that says yes.

    struct OfferCard
    {
        LLNotificationPtr note;
        std::string text;
        F64 left = 0.0;   // seconds until it expires; <= 0 when it does not
    };

    std::string oneLine(std::string t, size_t max)
    {
        for (char& c : t)
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (t.size() > max)
        {
            size_t cut = max;
            while (cut > 0 && (((unsigned char)t[cut]) & 0xC0) == 0x80) --cut;   // not inside a UTF-8 character
            t = t.substr(0, cut) + "...";
        }
        return t;
    }

    void conversationsPanel()
    {
        // Offers first: they are the ones that run out.
        std::vector<OfferCard> offers;
        if (LLNotificationChannelPtr visible = LLNotifications::instance().getChannel("Visible"))
        {
            const F64 now = LLDate::now().secondsSinceEpoch();
            visible->forEachNotification([&](LLNotificationPtr n)
            {
                if (!n || !n->isActive()) return;
                const std::string type = n->getType();
                if (type != "offer" && type != "notify") return;
                LLNotificationFormPtr form = n->getForm();
                if (!form) return;
                bool has_button = false;
                for (S32 i = 0; i < form->getNumElements(); ++i)
                    has_button = has_button || form->getElement(i)["type"].asString() == "button";
                if (!has_button) return;
                OfferCard c;
                c.note = n;
                c.text = plainNotice(n->getMessage());
                const F64 left = n->getExpiration().secondsSinceEpoch() - now;
                c.left = left < 86400.0 ? left : 0.0;
                offers.push_back(c);
            });
        }
        LLNotificationPtr answer;
        std::string answer_name;
        if (!offers.empty())
        {
            ImGui::TextDisabled("Waiting for your answer");
            int idx = 0;
            for (const OfferCard& c : offers)
            {
                ImGui::PushID(idx++);
                ImGui::BeginChild("##offer", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                ImGui::PushTextWrapPos(0.f);
                ImGui::TextUnformatted(oneLine(c.text, 400).c_str());
                ImGui::PopTextWrapPos();
                if (c.left > 0.0)
                {
                    const int secs = (int)c.left;
                    if (secs >= 60) ImGui::TextDisabled("%d min %02d s left", secs / 60, secs % 60);
                    else ImGui::TextDisabled("%d s left", secs);
                }
                LLNotificationFormPtr form = c.note->getForm();
                bool first = true;
                // Script menus (Block / Ignore / FT Light...) have no "yes" answer, so no accent,
                // and many buttons: they wrap into rows instead of running off the panel.
                const std::string note_name = c.note->getName();
                const bool script_menu = note_name == "ScriptDialog" || note_name == "ScriptDialogGroup";
                const ImGuiStyle& style = ImGui::GetStyle();
                const float row_right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
                for (S32 i = 0; i < form->getNumElements(); ++i)
                {
                    const LLSD el = form->getElement(i);
                    if (el["type"].asString() != "button") continue;
                    const std::string text = el.has("text") ? el["text"].asString() : el["name"].asString();
                    const float bw = ImGui::CalcTextSize(text.c_str()).x + style.FramePadding.x * 2.f;
                    if (!first)
                    {
                        const float next_x = ImGui::GetItemRectMax().x + style.ItemSpacing.x + bw;
                        if (next_x < row_right) ImGui::SameLine();   // otherwise it starts a new row
                    }
                    const bool accent = first && !script_menu;
                    if (accent) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.36f, 0.95f, 1.f));
                    if (ImGui::Button((text + "##b" + el["name"].asString()).c_str()))
                    {
                        answer = c.note;
                        answer_name = el["name"].asString();
                    }
                    if (accent) ImGui::PopStyleColor();
                    first = false;
                }
                ImGui::EndChild();
                ImGui::PopID();
            }
            ImGui::Spacing();
        }
        if (answer)
        {
            LLSD response = answer->getResponseTemplate();
            response[answer_name] = true;
            answer->respond(response);
        }

        static char filter[64] = "";
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##conv_filter", "Find a conversation", filter, sizeof(filter));
        struct Row
        {
            LLUUID id;
            std::string name, kind, last, when;
            S32 unread = 0, stamp = 0;
        };
        std::vector<Row> rows;
        if (LLIMModel::instanceExists())
        {
            for (const auto& pair : LLIMModel::instance().mId2SessionMap)
            {
                LLIMModel::LLIMSession* ses = pair.second;
                if (!ses) continue;
                Row r;
                r.id = pair.first;
                r.name = ses->mName;
                r.kind = ses->isGroupSessionType() ? "group" : (ses->isAdHocSessionType() ? "conference" : "person");
                r.unread = ses->mNumUnread;
                if (!ses->mMsgs.empty())
                {
                    const LLSD& m = ses->mMsgs.front();
                    r.last = oneLine(m["from"].asString() + ": " + m["message"].asString(), 120);
                    r.when = m["time"].asString();
                    r.stamp = m["timestamp"].asInteger();
                }
                if (!containsNoCase(r.name, filter) && !containsNoCase(r.last, filter)) continue;
                rows.push_back(r);
            }
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b)
        {
            if ((a.unread > 0) != (b.unread > 0)) return a.unread > 0;
            return a.stamp > b.stamp;
        });
        if (rows.empty())
        {
            ImGui::TextDisabled(filter[0] ? "No conversation matches." : "No conversations open. They appear here as soon as someone writes.");
            return;
        }
        ImGui::BeginChild("##conv_list", ImGui::GetContentRegionAvail());
        LLUUID open;
        const float line = ImGui::GetTextLineHeight();
        const float card_h = line * 2.f + 14.f;
        for (int i = 0; i < (int)rows.size(); ++i)
        {
            const Row& r = rows[i];
            ImGui::PushID(i);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##card", false, 0, ImVec2(w, card_h))) open = r.id;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (r.unread > 0) dl->AddRectFilled(at, ImVec2(at.x + 3.f, at.y + card_h), IM_COL32(139, 124, 255, 255), 2.f);
            const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
            const ImU32 dim_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const std::string right = r.unread > 0 ? llformat("%d new   %s", r.unread, r.when.c_str()) : (r.when.empty() ? r.kind : r.when);
            const float rw = ImGui::CalcTextSize(right.c_str()).x;
            dl->PushClipRect(ImVec2(at.x, at.y), ImVec2(at.x + w - rw - 16.f, at.y + card_h), true);
            dl->AddText(ImVec2(at.x + 10.f, at.y + 5.f), text_col, r.name.empty() ? "(no name)" : r.name.c_str());
            dl->PopClipRect();
            dl->AddText(ImVec2(at.x + w - rw - 8.f, at.y + 5.f), r.unread > 0 ? IM_COL32(170, 158, 255, 255) : dim_col, right.c_str());
            dl->PushClipRect(ImVec2(at.x, at.y), ImVec2(at.x + w - 8.f, at.y + card_h), true);
            dl->AddText(ImVec2(at.x + 10.f, at.y + 7.f + line), dim_col, r.last.empty() ? r.kind.c_str() : r.last.c_str());
            dl->PopClipRect();
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (open.notNull()) studioOpenConversation(open);
    }

    // -- Quick library --
    // Ctrl+Space: type a few letters, Enter. Opens or wears the thing without walking the folders.
    void libraryPanel()
    {
        static char query[96] = "";
        static int pick = 0;
        static std::vector<int> hits;
        static std::string last_query = "\x01";
        const bool appearing = ImGui::IsWindowAppearing();
        if (appearing)
        {
            invRead();
            query[0] = 0;
            last_query = "\x01";
            pick = 0;
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-1.f);
        const bool enter = ImGui::InputTextWithHint("##lib_q", "Name of anything you own", query, sizeof(query), ImGuiInputTextFlags_EnterReturnsTrue);
        if (last_query != query)
        {
            last_query = query;
            pick = 0;
            hits.clear();
            std::string q = query;
            LLStringUtil::toLower(q);
            if (!q.empty())
            {
                std::vector<int> inside;
                for (int i = 0; i < (int)sInv.size() && hits.size() < 60; ++i)
                {
                    const InvRow& r = sInv[i];
                    if (r.link) continue;
                    const size_t at = r.lname.find(q);
                    if (at == 0) hits.push_back(i);
                    else if (at != std::string::npos && inside.size() < 60) inside.push_back(i);
                }
                for (int i : inside)
                    if (hits.size() < 60) hits.push_back(i);
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) pick = llmin(pick + 1, llmax(0, (int)hits.size() - 1));
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) pick = llmax(pick - 1, 0);
        const bool shift = ImGui::GetIO().KeyShift;
        LLUUID act, show;
        if (!query[0])
            ImGui::TextDisabled("Enter opens or wears it. Shift+Enter shows it in the inventory. Esc closes.");
        else if (hits.empty())
            ImGui::TextDisabled("Nothing you own has that in its name.");
        ImGui::BeginChild("##lib_hits", ImGui::GetContentRegionAvail());
        for (int i = 0; i < (int)hits.size(); ++i)
        {
            const InvRow& r = sInv[hits[i]];
            ImGui::PushID(i);
            if (ImGui::Selectable(r.name.c_str(), i == pick, ImGuiSelectableFlags_AllowDoubleClick))
            {
                pick = i;
                if (ImGui::IsMouseDoubleClicked(0)) act = r.id;
            }
            if (i == pick && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) ImGui::SetScrollHereY(0.5f);
            ImGui::SameLine();
            ImGui::TextDisabled("  %s  in %s", invKindName(r.type), r.folder.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (enter && pick < (int)hits.size())
        {
            if (shift) show = sInv[hits[pick]].id;
            else act = sInv[hits[pick]].id;
        }
        bool close = ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (act.notNull())
        {
            LLInvFVBridgeAction::doAction(act, &gInventory);
            close = true;
        }
        else if (show.notNull())
        {
            LLInventoryPanel::openInventoryPanelAndSetSelection(true, show);
            close = true;
        }
        else if (enter)
        {
            ImGui::SetKeyboardFocusHere(-1);
        }
        if (close) gSavedSettings.setBOOL("NyxWindowLibrary", false);
    }

    bool libraryHotkey(KEY key, MASK mask)
    {
        if (key != ' ' || mask != MASK_CONTROL) return false;
        if (!gSavedSettings.controlExists("NyxWindowLibrary")) return false;
        const bool open = !gSavedSettings.getBOOL("NyxWindowLibrary");
        gSavedSettings.setBOOL("NyxWindowLibrary", open);
        if (open)
        {
            gFocusMgr.setKeyboardFocus(NULL);
            for (PanelWindow& w : sWindows)
                if (std::string("library") == w.id) w.focus = true;
        }
        return true;
    }

    // -- Portals --
    // Rings only this viewer draws: walk through one and you are taken to its place.
    void portalsPanel()
    {
        if(stormRoutePortals()) return;
        static char label[64] = "";
        static char dest[256] = "";
        bool on = gSavedSettings.getBOOL("NyxPortals");
        if (ImGui::Checkbox("Portals on", &on)) gSavedSettings.setBOOL("NyxPortals", on);
        ImGui::SameLine();
        ImGui::TextDisabled("Only you see them. Nothing is rezzed.");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##portal_label", "Name on the ring (Home, Shop, Sandbox...)", label, sizeof(label));
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##portal_dest", "Where it leads: paste a SLurl (World map > Copy SLurl)", dest, sizeof(dest));
        const bool ready = dest[0] != 0;
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Place a portal in front of me"))
        {
            if (nyxPortalAddHere(label[0] ? label : "Portal", dest))
            {
                label[0] = 0;
                dest[0] = 0;
            }
            else
            {
                nyxToast("That does not look like a SLurl. Copy one from the world map or a landmark.", true);
            }
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        const LLSD list = nyxPortalsList();
        if (!list.isArray() || list.size() == 0)
        {
            ImGui::TextDisabled("No portals yet.");
            return;
        }
        std::string remove;
        if (ImGui::BeginTable("##portal_rows", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.f);
            ImGui::TableSetupColumn("Stands in", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Leads to", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn("##rm", ImGuiTableColumnFlags_WidthFixed, 62.f);
            ImGui::TableHeadersRow();
            int i = 0;
            for (LLSD::array_const_iterator it = list.beginArray(); it != list.endArray(); ++it, ++i)
            {
                const LLSD& p = *it;
                ImGui::TableNextRow();
                ImGui::PushID(i);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(p["label"].asString().c_str());
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s (%d, %d, %d)", p["region"].asString().c_str(), (int)p["x"].asReal(), (int)p["y"].asReal(), (int)p["z"].asReal());
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", p["dest"].asString().c_str());
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Remove")) remove = p["id"].asString();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (!remove.empty()) nyxPortalRemove(remove);
    }

    // -- Viewfinder exposure meter --
    // A brightness histogram of the view, read from a few dozen lines of the picture four times a
    // second, with how much of it is crushed to black or blown to white.
    float sHisto[64] = { 0.f };
    float sHistoBlack = 0.f, sHistoWhite = 0.f;
    LLTimer sHistoTimer;

    void histogramSample(S32 w, S32 h)
    {
        if (w < 8 || h < 8 || w > 16384) return;
        if (sHistoTimer.getStarted() && sHistoTimer.getElapsedTimeF32() < 0.25f) return;
        sHistoTimer.start();
        static std::vector<U8> row;
        row.resize((size_t)w * 4 + 8);
        U32 bins[64] = { 0 };
        U32 total = 0, black = 0, white = 0;
        const int lines = 40;
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        for (int l = 0; l < lines; ++l)
        {
            const S32 y = (S32)(((F32)l + 0.5f) / (F32)lines * (F32)h);
            glReadPixels(0, y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, row.data());
            for (S32 x = 0; x < w; x += 2)
            {
                const U8* p = &row[(size_t)x * 4];
                const U32 lum = (p[0] * 54u + p[1] * 183u + p[2] * 19u) >> 8;
                ++bins[lum >> 2];
                if (lum <= 2) ++black;
                if (lum >= 253) ++white;
                ++total;
            }
        }
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        if (!total) return;
        U32 top = 1;
        for (int i = 1; i < 63; ++i) top = llmax(top, bins[i]);   // the end bins are shown as numbers
        for (int i = 0; i < 64; ++i) sHisto[i] = llmin(1.f, (float)bins[i] / (float)top);
        sHistoBlack = 100.f * (float)black / (float)total;
        sHistoWhite = 100.f * (float)white / (float)total;
    }

    void histogramDraw()
    {
        ImGui::PlotHistogram("##vf_histo", sHisto, 64, 0, nullptr, 0.f, 1.f, ImVec2(ImGui::GetContentRegionAvail().x, 56.f));
        const bool bad_black = sHistoBlack > 4.f, bad_white = sHistoWhite > 2.f;
        ImGui::TextColored(bad_black ? ImVec4(1.f, 0.72f, 0.35f, 1.f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "%.1f%% black", sHistoBlack);
        ImGui::SameLine();
        ImGui::TextColored(bad_white ? ImVec4(1.f, 0.72f, 0.35f, 1.f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "   %.1f%% white", sHistoWhite);
        if (bad_white) ImGui::TextDisabled("Highlights are burning out: lower the exposure or the sun.");
        else if (bad_black) ImGui::TextDisabled("Shadows are solid black: raise the ambient or add a fill light.");
    }

    #include "fsstormphoto.inl"
    #include "fsstormvisitdiff.inl"
    #include "fsnyxstudio.inl"

    void assistantWorkspacePanel()
    {
        if (ImGui::Button("AI Backend Options", ImVec2(-1.f, 28.f)))
            sStormAISettings = true;
        ImGui::Spacing();
        nyxAssistantPanel();
    }
    bool stormRoutePortals()
    {
        if(!studioEnabled()) return false;
        sStormLeftMode=4; stormRevealPanel(0); sStormRailSelection="Portals";
        gSavedSettings.setBOOL("NyxWindowPortals",false);
        return true;
    }

    void drawWindows(float disp_w, float disp_h)
    {
        if (gSavedSettings.getBOOL("NyxViewfinder")) viewfinderOverlay(disp_w, disp_h);

        int slot = 0;
        for (PanelWindow& w : sWindows)
        {
            ++slot;
            const bool want = windowWanted(w);
            if (!want)
            {
                w.was_open = false;
                continue;
            }
            // First time: on the right, stepped down a little each so they do not hide each other.
            const ImVec2 size(llmin(w.size.x, disp_w - 40.f), llmin(w.size.y, disp_h - 120.f));
            ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(llmax(20.f, disp_w - size.x - 24.f - 28.f * (float)(slot - 1)), 72.f + 28.f * (float)(slot - 1)),
                                    ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(w.min_size, ImVec2(disp_w, disp_h));
            if (w.focus || !w.was_open) ImGui::SetNextWindowFocus();
            w.focus = false;
            w.was_open = true;
            bool open = true;
            const std::string label = std::string(w.title) + "###nyxwin_" + w.id;
            const bool own_scroll = slot <= 3;   // Ani, assistant and browser lay out their own scrolling
            if (ImGui::Begin(label.c_str(), &open, ImGuiWindowFlags_NoCollapse | (own_scroll ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0)))
            {
                // Dragged off the edge of a smaller viewer window: bring the title bar back in reach.
                ImGuiWindow* win = ImGui::GetCurrentWindow();
                if (!ImGui::IsMouseDown(0))
                {
                    ImVec2 p = win->Pos;
                    p.x = llclamp(p.x, 40.f - win->Size.x, llmax(0.f, disp_w - 40.f));
                    p.y = llclamp(p.y, 0.f, llmax(0.f, disp_h - 40.f));
                    if (p.x != win->Pos.x || p.y != win->Pos.y) ImGui::SetWindowPos(win, p);
                }
                w.body();
            }
            ImGui::End();
            if (!open)
            {
                gSavedSettings.setBOOL(w.setting, false);
                w.was_open = false;
            }
        }
    }
}

void fsRegisterStormDocks()
{
    for (StormSide side : STORM_SIDES)
    {
        const char* xml = side == StormSide::Chat ? "floater_storm_dock_chat.xml"
                                                  : "floater_storm_dock.xml";
        LLFloaterReg::add(stormDockName(side), xml, &LLFloaterReg::build<StormDock>);
    }
}

// Open one of the panel windows ("ani", "assistant", "browser") and bring it to the front.
void nyxPanelWindowOpen(const std::string& id)
{
    for (PanelWindow& w : sWindows)
    {
        if (id != w.id) continue;
        if (gSavedSettings.controlExists(w.setting)) gSavedSettings.setBOOL(w.setting, true);
        w.focus = true;
    }
}

// A short notice from a Nyx panel, shown as an ordinary viewer tip.
void nyxToast(const std::string& text, bool error)
{
    LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", text));
}

namespace FSNyxShell
{
    // A panel window is open, signed in, the UI showing and not in mouselook.
    bool active()
    {
        return !sFailed && gViewerWindow && LLStartUp::getStartupState() >= STATE_STARTED && (studioEnabled() || anyWanted()) && !gAgentCamera.cameraMouselook() &&
               gViewerWindow->getUIVisibility() && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
    }

    void draw()
    {
        if (LLStartUp::getStartupState() >= STATE_STARTED)
        {
            noticeHook();
            streamingTick();
            nyxPortalsTick();
        }
        if (!active())
        {
            stormPlaceDesktopWindows(false);
            stormPlaceWorkspaceBrowser(false);
            StormPanels::hideInventory();
            StormPanels::setInlinePose(false);
            if (sPhotoMode) photoClose();
            studioChrome(false);
            stormPlaceDocks(0, 0, false);
            for (PanelWindow& w : sWindows) w.was_open = false;
            return;
        }
        if (!start()) { studioChrome(false); stormPlaceDocks(0, 0, false); return; }
        stormSeedWorkspace();
        studioChrome(studioEnabled());
        const auto screen = gViewerWindow->getWindowRectScaled();
        stormPlaceDocks(screen.getWidth(), screen.getHeight(), studioEnabled());
        stormPlaceDesktopWindows(studioEnabled() && !sPhotoMode &&
            stormSplitAvailable(screen.getWidth(), screen.getHeight()),
            screen.getWidth(), screen.getHeight());
        stormPlaceWorkspaceBrowser(studioEnabled() && !sPhotoMode &&
            stormSplitAvailable(screen.getWidth(), screen.getHeight()),
            screen.getWidth(), screen.getHeight());
        if (!studioEnabled() || sPhotoMode) StormPanels::hideInventory();
        if(sPhotoMode || !sStormPanelRectsValid[2]) StormPanels::setInlinePose(false);
        const S32 w = gViewerWindow->getWindowWidthRaw();
        const S32 h = gViewerWindow->getWindowHeightRaw();
        if (w <= 0 || h <= 0) return;

        const F32 dt = llclamp(sFrameTimer.getElapsedTimeAndResetF32(), 0.0001f, 0.25f);
        {
            // Keys held during Alt-Tab would otherwise stay down in the panel windows.
            static bool sFocused = true;
            const HWND hwnd = (HWND)gViewerWindow->getWindow()->getPlatformWindow();
            const HWND foreground = GetForegroundWindow();
            const bool focused = hwnd && (foreground == hwnd || IsChild(hwnd, foreground));
            if (focused != sFocused)
            {
                sFocused = focused;
                if (!focused) sPhotoKeys.clear();
                onFocus(focused);
            }
        }
        gGL.flush();
        if (sPhotoMode || (gSavedSettings.getBOOL("NyxViewfinder") && gSavedSettings.getBOOL("NyxViewfinderHistogram"))) histogramSample(w, h);
        const F32 ps = scale();
        setPixelScale(ps);
        beginFrame((float)w / ps, (float)h / ps, dt);
        if (sPhotoMode)
        {
            photoDraw(static_cast<float>(w)/ps,static_cast<float>(h)/ps,dt);
        }
        else if (studioEnabled())
        {
            studioDraw((float)w / ps, (float)h / ps);
            stormClipNativeDialogs((float)h / ps);
        }
        if (!sPhotoMode) drawWindows((float)w / ps, (float)h / ps);
        // LLViewerWindow::draw calls this inside the UI scale, and gGL applies the UI matrix to
        // every vertex; these windows work in raw pixels, so they draw with an identity one.
        gGL.pushUIMatrix();
        gGL.loadUIIdentity();
        glRender(endFrame(), w, h);
        gGL.popUIMatrix();
    }

    bool overFirestormUI(S32 x, S32 y)
    {
        if (gFocusMgr.getTopCtrl()) return true;
        if (gMenuHolder && gMenuHolder->hasVisibleMenu()) return true;
        // An open Nyx popup sits above the docks and takes the click even where it
        // overlaps a docked Firestorm window.
        if (ImGui::GetCurrentContext() &&
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) return false;
        for (LLView* view : { (LLView*)gFloaterView, (LLView*)gSnapshotFloaterView })
        {
            if (!view || !view->getVisible()) continue;
            for (LLView* v : *view->getChildList())
            {
                if (v && v->getVisible() && v->calcScreenRect().pointInRect(x, y)) return true;
            }
        }
        return false;
    }

    bool mouseButton(S32 raw_x, S32 raw_y, EMouseClickType type, bool down, bool over_ui)
    {
        int button = -1;
        if (type == CLICK_LEFT || type == CLICK_DOUBLELEFT) button = 0;
        else if (type == CLICK_RIGHT) button = 1;
        else if (type == CLICK_MIDDLE) button = 2;
        if (button < 0 || !sStarted) return false;
        const float ps = scale();
        onMouseMove((float)raw_x / ps, (float)raw_y / ps);
        if (!down)
        {
            // Every release these windows took the press for comes back to them, whoever else
            // wants the mouse: a press never seen released leaves the button held for good.
            if (!sHeld[button]) return false;
            sHeld[button] = false;
            onMouseButton(button, false);
            // An inventory item dragged out of a panel is owned by Firestorm's drag and
            // drop tool now; the release has to reach it so the drop happens.
            if (LLToolDragAndDrop::getInstance()->hasMouseCapture()) return false;
            return true;
        }
        if (!active()) return false;
        // An inventory drag over a panel window is not for it: Firestorm keeps the drag.
        if (LLToolDragAndDrop::getInstance()->hasMouseCapture()) return false;
        const bool popup = ImGui::GetCurrentContext() &&
                           ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
        if (!popup && (over_ui || !windowsOwn((float)raw_x / ps, (float)raw_y / ps)))
        {
            if (!over_ui && button == 0 && photoPickFocus((float)raw_x / ps, (float)raw_y / ps))
                return true;
            if(button==0) StormPanels::nearbyExpanded=false;
            if(!over_ui && button==0 && studioEnabled() && (StormNotebook::picking || StormPanels::snapPortalToWall))
            {
                const auto screen=gViewerWindow->getWindowRectScaled();
                gViewerWindow->pickAsync(ll_round(static_cast<F32>(raw_x)/ps),
                    screen.getHeight()-ll_round(static_cast<F32>(raw_y)/ps),0,
                    StormNotebook::picking ? StormNotebook::picked : StormPanels::portalWallPicked);
                sHeld[button]=true;
                return true;
            }
            if (!over_ui && button == 0 && studioEnabled() && !StormNotebook::notes.empty() &&
                !LLToolMgr::getInstance()->inEdit())
            {
                const auto screen = gViewerWindow->getWindowRectScaled();
                const auto pick = gViewerWindow->pickImmediate(ll_round(raw_x / ps),
                    screen.getHeight() - ll_round(raw_y / ps), false);
                if (StormNotebook::clicked(pick)) return true;
            }
            // A Firestorm window or the world takes the click: the panel windows let go of their
            // text fields.
            letGo();
            return false;
        }
        onMouseButton(button, true);
        if (type == CLICK_DOUBLELEFT) onMouseButton(button, false);
        else sHeld[button] = true;
        // Typing goes to the panel's field from here, not to a Firestorm field clicked earlier.
        if (gFocusMgr.getKeyboardFocus()) gFocusMgr.setKeyboardFocus(nullptr);
        return true;
    }

    void mouseMove(S32 raw_x, S32 raw_y)
    {
        if (!sStarted || !active()) return;
        const float ps = scale();
        onMouseMove((float)raw_x / ps, (float)raw_y / ps);
    }

    bool wheel(F32 dx, F32 dy, bool over_ui)
    {
        if (!sStarted || !active() || over_ui || !ImGui::GetCurrentContext()) return false;
        const ImVec2 at = ImGui::GetIO().MousePos;
        const auto* hovered=ImGui::GetCurrentContext()->HoveredWindow;
        const bool portalHandle=hovered && std::string_view(hovered->Name).starts_with("##firestorm_portal_handle_");
        if (studioEnabled() && !sPhotoMode && (!windowsOwn(at.x,at.y) || portalHandle) &&
            StormPanels::portalScroll(dy)) return true;
        if (!windowsOwn(at.x, at.y)) return false;
        onWheel(dx, dy);
        return true;
    }

    bool key(KEY key, MASK mask, bool down)
    {
        if (down && key == KEY_ESCAPE && StormNotebook::picking)
        {
            StormNotebook::picking = false;
            StormNotebook::status.clear();
            return true;
        }
        if (sPhotoMode && photoKey(key, mask, down)) return true;
        if (LLStartUp::getStartupState() >= STATE_STARTED && studioKey(key, mask, down)) return true;
        if (down && LLStartUp::getStartupState() >= STATE_STARTED && (layoutHotkey(key, mask) || libraryHotkey(key, mask))) return true;
        if (!sStarted || !active()) return false;
        // A Firestorm field has the keyboard: the panel windows stay out of it.
        if (gFocusMgr.getKeyboardFocus())
        {
            if (down) letGo();
            return false;
        }
        if (nyxBrowserKey(key, mask, down)) return true;
        onKey(key, down, mask);
        return wantsKeyboard();
    }

    bool unicodeChar(llwchar uni_char)
    {
        if (!sStarted || !active() || gFocusMgr.getKeyboardFocus()) return false;
        if (nyxBrowserChar((U32)uni_char)) return true;
        if (studioEnabled() && StormPanels::nearbyExpanded && sStormChatFocus && uni_char >= 32 && uni_char != 127)
        {
            auto& draft = StormPanels::conversationDrafts[LLUUID::null];
            draft += wstring_to_utf8str(LLWString(1, uni_char));
            return true;
        }
        onChar((unsigned)uni_char);
        return wantsKeyboard();
    }

    bool startChat(const char* text)
    {
        if (!active() || !studioEnabled() || !sStarted) return false;
        const auto screen = gViewerWindow->getWindowRectScaled();
        if (stormSplitAvailable(screen.getWidth(), screen.getHeight()) &&
            stormSplitPane(StormSplitContent::World) < 0)
            return true;
        stormOpen("fs_im_container", StormSide::Chat);
        StormPanels::nearbySelected=true;
        StormPanels::dockInbox=0;
        StormPanels::nearbyExpanded=true;
        if(text && !sStormChatInput[0]) snprintf(sStormChatInput,sizeof(sStormChatInput),"%s",text);
        sStormChatFocus=true;
        if(gFocusMgr.getKeyboardFocus()) gFocusMgr.setKeyboardFocus(nullptr);
        return true;
    }

    bool worldRect(LLRect& raw)
    {
        if (sPhotoMode && gViewerWindow)
        {
            raw.set(0,gViewerWindow->getWindowHeightRaw(),gViewerWindow->getWindowWidthRaw(),0);
            return true;
        }
        if (!active() || !studioEnabled() || !sStarted) return false;
        const F32 ps = scale();
        const auto screen = gViewerWindow->getWindowRectScaled();
        if (stormSplitAvailable(screen.getWidth(), screen.getHeight()))
        {
            const int pane = stormSplitPane(StormSplitContent::World);
            if (pane < 0)
            {
                // Keep the live renderer at full size behind the opaque empty panes.
                raw.set(0, gViewerWindow->getWindowHeightRaw(), gViewerWindow->getWindowWidthRaw(), 0);
                return true;
            }
            const LLRect cell = stormSplitBody(screen.getWidth(), screen.getHeight(), pane);
            raw.set(ll_round(cell.mLeft * ps), ll_round(cell.mTop * ps),
                    ll_round(cell.mRight * ps), ll_round(cell.mBottom * ps));
            return true;
        }
        const auto layout = stormLayout(screen.getWidth(), screen.getHeight());
        raw.set(ll_round((STORM_RAIL + layout.left) * ps),
                gViewerWindow->getWindowHeightRaw() - ll_round((STORM_HEADER + layout.top) * ps),
                gViewerWindow->getWindowWidthRaw() - ll_round(layout.right * ps),
                ll_round((STORM_FOOTER + layout.chat) * ps));
        return raw.getWidth() > 0 && raw.getHeight() > 0;
    }
}

void nyxSpatialNotebookRender()
{
    if (LLStartUp::getStartupState() >= STATE_STARTED && FSStormWorkspace::enabled())
        StormNotebook::render();
}

void nyxCaptureVisitCache(const LLViewerRegion* region, LLVOCacheEntry::vocache_entry_map_t& entries)
{
    StormVisitDiff::capture(region, entries);
}

void nyxForgetVisitCache(const LLViewerRegion* region)
{
    StormVisitDiff::forget(region);
}

void nyxRenderVisitDiff()
{
    if (LLStartUp::getStartupState() >= STATE_STARTED && FSStormWorkspace::enabled())
        StormVisitDiff::render();
}

void nyxDetachWorkspaceWindows()
{
    stormPlaceDesktopWindows(false);
}

namespace FSStormWorkspace
{
    bool enabled() { return FSNyxShell::active() && studioEnabled(); }

    bool showLand()
    {
        if (!enabled() || !LLFloaterReg::canShowInstance("about_land")) return false;
        stormOpen("about_land", StormSide::RightBottom);
        return true;
    }

    bool showChat()
    {
        return enabled() && LLFloaterReg::canShowInstance("fs_im_container") && FSNyxShell::startChat();
    }

    bool chatVisible()
    {
        return enabled() && (StormPanels::nearbyExpanded || StormPanels::dockInbox != 0);
    }

    bool showSearch(bool groups)
    {
        if (!enabled() || !LLFloaterReg::canShowInstance("search") ||
            !LLFloaterReg::getTypedInstance<FSFloaterSearch>("search")) return false;
        if (groups) StormPanels::searchTab = 1;
        stormToggleSearch();
        return true;
    }

    bool searchVisible()
    {
        return enabled() && sStormLeftMode == 1 && sStormPanelRectsValid[0] && sStormPanelVisible[0];
    }

    bool pinInventoryFolder(const LLUUID& folder)
    {
        if (!enabled() || !gInventory.getCategory(folder)) return false;
        StormPanels::pinnedFolders.try_emplace(folder);
        return true;
    }

    bool dropInventoryFolder(const LLUUID& folder, int x, int y, bool drop)
    {
        if (!enabled() || !gViewerWindow || gFocusMgr.getTopCtrl() || !gInventory.getCategory(folder)) return false;
        const auto& bar = sStormToolbarRect;
        const float leftX = static_cast<float>(x);
        const float topY = static_cast<float>(gViewerWindow->getWindowRectScaled().getHeight() - y);
        if (leftX < bar.x || leftX >= bar.x + bar.z || topY < bar.y || topY >= bar.y + bar.w) return false;
        return !drop || pinInventoryFolder(folder);
    }

    bool showAppearance(bool shape)
    {
        if(!enabled()) return false;
        if(auto* dock=stormEditorDock()) dock->setVisible(false);
        StormPanels::appearanceTab=shape?1:0;
        stormRevealPanel(2);
        return true;
    }

    bool showBuildTools()
    {
        if(!enabled()) return false;
        auto* tools = LLFloaterReg::getTypedInstance<LLFloaterTools>("build");
        if (!tools) return false;
        tools->onOpen(LLSD());
        if(auto* dock=stormEditorDock()) dock->setVisible(false);
        stormRevealPanel(3);
        sStormSplitBuildInspector = sStormSplitWorkspace;
        return true;
    }

    bool showTerrainBrush()
    {
        return showBuildTools();
    }

    bool showPhotoMode()
    {
        if (!enabled() || !gFloaterView) return false;
        if (!sPhotoMode)
        {
            sPhotoNativeVisible=gFloaterView->getVisible();
            sPhotoFreeze=gSavedSettings.getBOOL("FreezeTime");
            sPhotoOriginal = photoCurrentShot();
            sPhotoOriginalFov = LLViewerCamera::getInstance()->getDefaultFOV();
            gFloaterView->setVisible(false);
            gFocusMgr.setKeyboardFocus(nullptr);
            sPhotoMode=true;
        }
        return true;
    }

    bool showInventoryItem(const LLUUID& id, bool open)
    {
        if(!enabled()) return false;
        if(open || sStormPanelVisible[0]) StormPanels::revealItem(id);
        return true;
    }

    bool showActionNotification(const LLUUID& id)
    {
        if(!enabled()) return false;
        auto note=LLNotifications::instance().find(id);
        if(!note || !note->isActive()) return false;
        auto form=note->getForm();
        const bool group=note->getName()=="GroupNotice";
        if(!form && !group) return false;
        const bool script=note->getName()=="ScriptDialog" || note->getName()=="ScriptDialogGroup";
        bool actionable=group;
        for(S32 i=0;form && i<form->getNumElements();++i)
        {
            const LLSD element=form->getElement(i);
            const auto type=element["type"].asString();
            const bool textbox=script && element["name"].asString()=="!!llTextBox!!";
            if(type!="button" && type!="text" && !textbox) return false;
            actionable |= type=="button" || textbox;
        }
        if(!actionable) return false;
        if(group)
        {
            const auto& payload=note->getPayload();
            const auto transaction=payload["inventory_offer"]["transaction_id"].asUUID();
            for(const auto& queued:sStormRoutedNotifications)
            {
                if(queued==id) continue;
                const auto existing=LLNotifications::instance().find(queued);
                if(!existing || !existing->isActive() || existing->getName()!="GroupNotice") continue;
                const auto& previous=existing->getPayload();
                if(previous["group_id"].asUUID()==payload["group_id"].asUUID() &&
                   previous["inventory_offer"]["transaction_id"].asUUID()==transaction &&
                   existing->getDate()==note->getDate() && existing->getMessage()==note->getMessage())
                {
                    doOnIdleOneTime([id]{
                        auto& notifications=LLNotifications::instance();
                        if(auto duplicate=notifications.find(id)) notifications.cancel(duplicate);
                    });
                    return true;
                }
            }
        }
        if(!sStormRoutedNotifications.insert(id).second) return true;
        if(group) return true;
        stormRevealPanel(5);
        StormPanels::nearbySelected=false;
        StormPanels::conversationFilter=3;
        StormPanels::dockInbox=3;
        StormPanels::nearbyExpanded=false;
        sStormNotificationHeight=std::max(sStormNotificationHeight,300.f);
        return true;
    }

    bool showScriptNotification(const LLUUID& id)
    {
        auto note=LLNotifications::instance().find(id);
        return note && (note->getName()=="ScriptDialog" || note->getName()=="ScriptDialogGroup")
            && showActionNotification(id);
    }

    bool showConversation(const LLUUID& id)
    {
        if(!enabled()) return false;
        auto* session=LLIMModel::instance().findIMSession(id);
        if(!session || session->mSessionType==LLIMModel::LLIMSession::NONE_SESSION) return false;
        StormPanels::selectedSession=id; StormPanels::nearbySelected=false;
        StormPanels::conversationFilter=session->isGroupSessionType()?2:1;
        StormPanels::dockInbox=StormPanels::conversationFilter;
        StormPanels::nearbyExpanded=false;
        stormRevealPanel(5); sStormChatFocus=true;
        sStormRailSelection="Chat";
        return true;
    }

    void hideLegacyWindows()
    {
        if(!enabled()) return;
        if(!Nyx::editorMode())
            for(const char* name:STORM_EDITOR_TOOLS)
                for(auto* tool:LLFloaterReg::getFloaterList(name))
                    if(tool && tool->getVisible() && !tool->getHost()) stormDockEditorTool(tool);
        struct Route { const char* name; int panel; };
        static const Route routes[]={
            {"inventory",0},{"secondary_inventory",0},{"people",1},{"appearance",2},
            {"build",3},{"fs_im_container",5},{"fs_nearby_chat",5},{"fs_impanel",5},
            {"about_land",4},{"fs_my_land_ground",4},{"search",0},{"places",0},{"world_map",0},
            {"script_debug",5},{"script_debug_output",5}
        };
        for(const auto& route:routes) for(auto* floater:LLFloaterReg::getFloaterList(route.name))
        {
            if(!floater) continue;
            if(floater==StormPanels::dockedInventory && StormPanels::inventoryEmbedded) continue;
            if(!floater->LLView::getVisible())
            {
                // Async search replies select native rows. They must not steal
                // typing from the visible replacement panel while hidden.
                if(floater->hasFocus()) gFocusMgr.setKeyboardFocus(nullptr);
                continue;
            }
            if(floater->hasFocus())
            {
                stormRevealPanel(route.panel);
                const std::string name(route.name);
                if(name=="script_debug" || name=="script_debug_output") StormPanels::nearbySelected=true;
                if(name=="inventory" || name=="secondary_inventory") sStormLeftMode=0;
                if(name=="search") sStormLeftMode=1;
                if(name=="places") sStormLeftMode=2;
                if(name=="world_map")
                {
                    sStormLeftMode=3;
                    const auto target=LLTracker::getTrackedPositionGlobal();
                    StormPanels::mapCenter=target.isExactlyZero()?gAgent.getPositionGlobal():target;
                    StormPanels::mapTarget=target;
                }
                if(name=="about_land" || name=="fs_my_land_ground") {sLandPanelHidden=false;sLandTab=name=="about_land"?0:8;}
                if(std::string(route.name)=="fs_impanel") showConversation(floater->getKey().asUUID());
                if(gFocusMgr.getKeyboardFocus()) gFocusMgr.setKeyboardFocus(nullptr);
            }
            // Do not close the session or invoke close-floater callbacks. Its backing
            // controls still provide permissions, message queuing, and validation.
            floater->LLFloater::setVisible(false);
        }
    }

    void remapConversation(const LLUUID& old_id,const LLUUID& new_id)
    {
        if(StormPanels::selectedSession==old_id) StormPanels::selectedSession=new_id;
        auto found=StormPanels::conversationDrafts.find(old_id);
        if(found!=StormPanels::conversationDrafts.end())
        {
            StormPanels::conversationDrafts[new_id]=std::move(found->second);
            StormPanels::conversationDrafts.erase(found);
        }
    }
}

// The replacement Build panel owns the toolbox while this workspace is enabled.
bool fsNyxBuildFloaterParked()
{
    return FSStormWorkspace::enabled();
}

// Her right-click menu's "Settings...": the Ani window.
bool fsNyxShellOpenAniSettings(bool open)
{
    if (!gViewerWindow || LLStartUp::getStartupState() < STATE_STARTED || sFailed) return false;
    if (open) nyxPanelWindowOpen("ani");
    return true;
}
