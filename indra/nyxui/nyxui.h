#pragma once

#include "imgui.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

class LLGLTexture;
class LLImageGL;

namespace NyxUI
{
    // ---- Textures ------------------------------------------------------------------------------
    // Every texture the interface draws is an LLImageGL, and its handle (ImTextureID, Image::tex,
    // the icon atlases, the hooks) is that LLImageGL*. The renderer binds it through LLTexUnit, so
    // one handle works under every backend LL's renderer supports. 0 means "none".
    using Tex = ImTextureID;
    // Pixels are tightly packed RGBA8, first row = the top of the image (v 0). Owned by NyxUI.
    Tex createTexture(int width, int height, const void* rgba, bool mipmaps = false);
    // Replaces the whole image and returns its new handle; the old one is retired (kept alive a
    // few frames for the GPU) and must not be used again.
    Tex updateTexture(Tex tex, int width, int height, const void* rgba);
    void destroyTexture(Tex tex);
    // An LL viewer texture (fetched, media, map tile ...): 0 until it has image data. Valid for the frame.
    Tex textureHandle(const LLGLTexture* texture);
    Tex textureHandle(LLImageGL* image);
    // Pixel size of a handle's image, 0 x 0 for none.
    ImVec2 textureSize(Tex tex);
    inline ImTextureRef texRef(Tex tex) { return ImTextureRef(tex); }

    struct Fonts
    {
        ImFont* body = nullptr;
        ImFont* strong = nullptr;
        ImFont* light = nullptr;
        ImFont* icons = nullptr;
    };

    const Fonts& fonts();

    bool init(const std::string& font_dir, const std::string& settings_dir);
    void shutdown();

    // The interface renderer. Despite the names (kept for callers) these pick the backend at
    // runtime: Vulkan when LLVulkan is initialised, OpenGL otherwise. glShutdown is idempotent.
    bool glInit();
    void glShutdown();
    void glRender(ImDrawData* data, int fb_width, int fb_height);

    void onMouseMove(float x, float y);
    void onMouseButton(int button, bool down);
    void onWheel(float dx, float dy);
    void onKey(unsigned key, bool down, unsigned mask);
    void onChar(unsigned codepoint);
    void onFocus(bool focused);
    bool wantsMouse();
    bool wantsKeyboard();

    namespace Theme
    {
        enum Role
        {
            Base,
            Surface,
            SurfaceAlt,
            Raised,
            Border,
            Text,
            TextDim,
            TextFaint,
            Accent,
            AccentHover,
            AccentSoft,
            Accent2,
            Good,
            Warn,
            Bad,
            RoleCount
        };
        ImU32 col(Role r, float alpha = 1.f);
        ImVec4 vec(Role r, float alpha = 1.f);
        void apply(bool dark);
        bool dark();
        constexpr float BodySize = 15.f;
        constexpr float SmallSize = 13.f;
        constexpr float TitleSize = 19.f;
        constexpr float HeroSize = 34.f;
        constexpr float TopBarHeight = 70.f;
        constexpr float BottomBarHeight = 70.f;
        constexpr float TabHeight = 38.f;
        constexpr float Gap = 10.f;
        constexpr float Radius = 16.f;
    }

    namespace FX
    {
        void shadow(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float strength = 1.f);
        void gradient(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 c0, ImU32 c1, float rounding, bool vertical = false, float alpha = 1.f);
        void accent(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float alpha = 1.f);
        void glass(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, float alpha = 0.9f, bool with_shadow = true);
        void glow(ImDrawList* dl, const ImVec2& c, float radius, ImU32 col, float strength = 1.f);
        void ring(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float rounding, ImU32 col);
        float anim(ImGuiID id, float target, float speed = 14.f);
    }

    struct Image
    {
        Tex tex = 0;
        float w = 0.f;
        float h = 0.f;
    };
    void setImage(const std::string& name, Tex tex, float w, float h);
    const Image* image(const std::string& name);
    void setIconAtlas(Tex tex, int cell, int cols, float atlas_w, float atlas_h, const std::map<unsigned, int>& cells);
    void setColorIconAtlas(Tex tex, int cell, int cols, float atlas_w, float atlas_h, const std::map<unsigned, int>& cells);

    // Full-colour icons cut from the NYX icon sheet (textures/nyx_glow_icons.*). They keep their own
    // colours; the colour handed to Icon::draw only sets their opacity. Without the atlas each one
    // falls back to the matching line icon (Icon::plain).
    namespace Glow
    {
        constexpr unsigned Home = 0xF100, Inventory = 0xF101, Build = 0xF102, Edit = 0xF103, Move = 0xF104, Rotate = 0xF105, Scale = 0xF106, Texture = 0xF107;
        constexpr unsigned Appearance = 0xF108, Avatar = 0xF109, People = 0xF10A, Chat = 0xF10B, Voice = 0xF10C, Gestures = 0xF10D, Animations = 0xF10E, Profile = 0xF10F;
        constexpr unsigned Camera = 0xF110, Snapshot = 0xF111, Phototools = 0xF112, Mouselook = 0xF113, Fly = 0xF114, Walk = 0xF115, Run = 0xF116, Sit = 0xF117;
        constexpr unsigned Map = 0xF118, Minimap = 0xF119, Radar = 0xF11A, Teleport = 0xF11B, Land = 0xF11C, Landmark = 0xF11D, Search = 0xF11E, Destinations = 0xF11F;
        constexpr unsigned Settings = 0xF120, Debug = 0xF121, Statistics = 0xF122, Lighting = 0xF123, Materials = 0xF124, Weather = 0xF125, Browser = 0xF126, Help = 0xF127;
        constexpr bool is(unsigned cp) { return cp >= 0xF100 && cp < 0xF200; }
    }

    namespace Icon
    {
        const char* glyph(unsigned codepoint);
        unsigned plain(unsigned cp);
        bool hasImage(unsigned cp);
        void draw(ImDrawList* dl, unsigned cp, const ImVec2& center, float size, ImU32 col);
        void item(unsigned cp, float size, ImU32 col);
        constexpr unsigned Model = 0xF000;
        constexpr unsigned CameraCtl = 0xF001;
        constexpr unsigned Export = 0xF002;
        constexpr unsigned Link = 0xF003;
        constexpr unsigned Share = 0xF004;
        constexpr unsigned Crop = 0xF005;
        constexpr unsigned Fire = 0xF010;
        constexpr unsigned Menu = 0xE700;
        constexpr unsigned Close = 0xE711;
        constexpr unsigned More = 0xE712;
        constexpr unsigned Settings = 0xE713;
        constexpr unsigned Video = 0xE714;
        constexpr unsigned People = 0xE716;
        constexpr unsigned Pin = 0xE718;
        constexpr unsigned Search = 0xE721;
        constexpr unsigned Camera = 0xE722;
        constexpr unsigned Send = 0xE724;
        constexpr unsigned Back = 0xE72B;
        constexpr unsigned Star = 0xE734;
        constexpr unsigned Check = 0xE73E;
        constexpr unsigned Add = 0xE710;
        constexpr unsigned Delete = 0xE74D;
        constexpr unsigned Save = 0xE74E;
        constexpr unsigned Brush = 0xE771;
        constexpr unsigned Globe = 0xE774;
        constexpr unsigned Contact = 0xE77B;
        constexpr unsigned Color = 0xE790;
        constexpr unsigned Package = 0xE7B8;
        constexpr unsigned Edit = 0xE70F;
        constexpr unsigned MapPin = 0xE707;
        constexpr unsigned Airplane = 0xE709;
        constexpr unsigned Home = 0xE80F;
        constexpr unsigned Layers = 0xE81E;
        constexpr unsigned OpenWindow = 0xE8A7;
        constexpr unsigned Folder = 0xE8B7;
        constexpr unsigned Message = 0xE8BD;
        constexpr unsigned Chat = 0xE8F2;
        constexpr unsigned Library = 0xE8F1;
        constexpr unsigned World = 0xE909;
        constexpr unsigned Repair = 0xE90F;
        constexpr unsigned Info = 0xE946;
        constexpr unsigned Robot = 0xE99A;
        constexpr unsigned Lightbulb = 0xEA80;
        constexpr unsigned Bell = 0xEA8F;
        constexpr unsigned Mic = 0xE720;
        constexpr unsigned Volume = 0xE767;
        constexpr unsigned View = 0xE890;
        constexpr unsigned ChevronDown = 0xE70D;
        constexpr unsigned ChevronRight = 0xE76C;
        constexpr unsigned Lock = 0xE72E;
        constexpr unsigned Walk = 0xE805;
        constexpr unsigned Power = 0xE7E8;
        constexpr unsigned Photo = 0xE91B;
    }

    bool iconButton(const char* id, unsigned icon, const char* tip, bool active = false, float size = 34.f);
    bool pillButton(const char* id, unsigned icon, const char* label, bool active);
    // A row of equal-width text tabs with an accent underline under the chosen one; true when it changed.
    bool textTabs(const char* id, const char* const* labels, int n, int& sel);
    // An icon above a short label, as on the top bar, the bottom toolbar and the quick actions.
    bool tileButton(const char* id, unsigned icon, const char* label, bool active, const ImVec2& size, float icon_size = 26.f, const char* tip = nullptr);
    bool primaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool ghostButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool textField(const char* id, const char* hint, char* buf, size_t len, unsigned icon = 0, bool password = false, float width = -1.f);
    void sectionHeader(const char* text);
    void caption(const char* text);
    void emptyState(unsigned icon, const char* title, const char* body);
    bool toggleRow(const char* label, const char* detail, bool* v);
    bool beginCard(const char* id, const ImVec2& size = ImVec2(0, 0));
    void endCard();

    struct Panel
    {
        std::string id;
        std::string title;
        unsigned icon = 0;
        std::function<void()> draw;
        bool viewport = false;
    };
    void registerPanel(const Panel& p);

    struct ExternalPanels
    {
        std::function<bool(const std::string&)> exists;
        std::function<std::string(const std::string&)> title;
        std::function<void(const std::string&, float, float, float, float)> place;
        std::function<void(const std::string&)> undock;
        std::function<void()> frameBegin;
        std::function<void()> frameEnd;
    };
    void setExternalPanels(const ExternalPanels& e);
    const ExternalPanels& externalPanels();
    void setPanelChromeless(const std::string& id);
    bool panelChromeless(const std::string& id);
    const Panel* findPanel(const std::string& id);
    void focusPanel(const std::string& id);
    bool panelOpen(const std::string& id);
    void closePanel(const std::string& id);

    struct Workspace
    {
        std::string id;
        std::string title;
        unsigned icon = 0;
        std::string layout;
    };
    void registerWorkspace(const Workspace& w);

    // One tile on the top bar or the bottom toolbar. With no command the id names a workspace;
    // "panel.<id>" and "workspace.<id>" commands light up while that panel or workspace is showing,
    // anything else while hooks().commandActive says so. An id of "|" draws a separator.
    struct BarItem
    {
        std::string id;
        std::string title;
        unsigned icon = 0;
        std::string command;
        std::string tip;
    };
    void setTopBar(const std::vector<BarItem>& items);
    void setBottomBar(const std::vector<BarItem>& items);
    void setWorkspace(const std::string& id);
    const std::string& workspace();
    const std::vector<Workspace>& workspaces();
    void resetWorkspaceLayout(const std::string& id);

    struct Command
    {
        std::string id;
        std::string label;
        std::string group;
        std::string shortcut;
        std::function<void()> run;
    };
    void registerCommand(const Command& c);
    void runCommand(const std::string& id);
    void focusChat();
    const std::vector<Command>& commands();
    void openPalette();
    void drawPalette();

    struct StatusInfo
    {
        std::string name;
        std::string region;
        std::string parcel;
        std::string balance;
        bool online = false;
        int presence = 0;
    };
    StatusInfo& status();

    void toast(const std::string& text, bool error = false);

    void setPixelScale(float scale);
    float pixelScale();
    void beginFrame(float width, float height, float dt);
    void drawShell(bool signed_in);
    ImDrawData* endFrame();

    bool viewportRect(float& x, float& y, float& w, float& h);

    struct LoginRequest
    {
        std::string user;
        std::string password;
        std::string grid;
        std::string start;
        std::string region;
        std::string mfa;
        bool remember = true;
    };

    struct Hooks
    {
        std::function<void(const std::string&)> chat;
        std::function<void(const LoginRequest&)> signIn;
        std::function<void()> exploreOffline;
        std::function<void()> signOut;
        std::function<void()> quit;
        std::function<float()> fps;
        std::function<void(const std::string&)> openUrl;
        // (username, grid label as shown on the sign-in card): bit 1 saved password, bit 2 trusted for MFA.
        std::function<int(const std::string&, const std::string&)> savedLogin;
        std::function<bool(const std::string&)> commandActive;
        std::function<void(int)> setPresence;
        std::function<void(const std::string&)> search;
        // GL texture of the signed-in avatar's profile picture, 0 until it has loaded.
        std::function<Tex()> selfPortrait;
        std::function<int()> unreadMessages;
        // Connection quality 0 (none) to 4 (good) and a line describing it.
        std::function<int(std::string&)> signal;
        // The application's own menus, drawn inside the popup of the menu button at the left of
        // the top bar (ImGui::BeginMenu / MenuItem calls). No button without it.
        std::function<void()> appMenu;
    };
    Hooks& hooks();

    struct NewsItem
    {
        std::string title;
        std::string link;
        std::string date;
        std::string summary;
    };

    struct LoginState
    {
        bool busy = false;
        std::string message;
        bool error = false;
        std::vector<std::string> grids;
        int online = -1;
        double signups = -1.0;
        double exchange = -1.0;
        std::string gridStatus;
        int gridLevel = -1;
        std::vector<std::string> incidents;
        std::vector<std::string> maintenances;
        std::vector<NewsItem> news;
    };
    LoginState& login();

    bool settingBool(const std::string& key, bool def);
    void setSettingBool(const std::string& key, bool v);
    float settingFloat(const std::string& key, float def);
    void setSettingFloat(const std::string& key, float v);
    std::string settingString(const std::string& key, const std::string& def);
    void setSettingString(const std::string& key, const std::string& v);
    void saveState();

    // Places the shell keeps for world overlays drawn by the app, valid for the frame they were laid out in.
    bool chatLogSlot(float& x0, float& y0, float& x1, float& y1);
    bool locationSlot(float& x0, float& y0, float& x1, float& y1);
}
