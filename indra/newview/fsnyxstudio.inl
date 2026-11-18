// Nyx Studio in-world workspace. Included in fsnyxshell.cpp's private namespace.
// Commands are the live viewer's callbacks, not a second implementation of permissions,
// selection rules or network actions. Login deliberately never enters this code.

    struct StudioAction
    {
        LLHandle<LLView> handle;
        std::string label, path, name, search;
    };
    std::vector<StudioAction> sStudioActions;
    std::string sStudioSection = "Overview";
    bool sStudioOpen = false;
    bool sStudioSearchFocus = false;
    char sStudioSearch[192] = {};
    std::vector<std::string> sStudioRecent;
    std::map<std::string, std::pair<LLHandle<LLView>, bool>> sStudioChrome;

    constexpr S32 STORM_RAIL = 0;
    constexpr S32 STORM_HEADER_TOP = 0;
    constexpr S32 STORM_HEADER_NAV = 36;
    constexpr S32 STORM_HEADER_MENU = 30;
    constexpr S32 STORM_HEADER = STORM_HEADER_TOP + STORM_HEADER_NAV + STORM_HEADER_MENU;
    constexpr S32 STORM_FOOTER = 56;
    enum class StormSide { Left, LeftLower, Right, RightMid, RightBottom, Chat };

    class StormDock final : public LLMultiFloater
    {
    public:
        explicit StormDock(const LLSD& key) : LLMultiFloater(key) { mAutoResize = false; }
        S32 activeToolWidth()
        {
            auto* active=getActiveFloater();
            if(!active) return 440;
            const auto found=mFloaterDataMap.find(active->getHandle());
            const S32 original=found==mFloaterDataMap.end()?0:found->second.mWidth;
            return std::max(440,std::max(original,active->getMinWidth()));
        }
        bool postBuild() override
        {
            if (!LLMultiFloater::postBuild()) return false;
            if (auto* close = findChild<LLButton>("close_active_tool"))
            {
                close->setCommitCallback([this](LLUICtrl*, const LLSD&)
                {
                    if (auto* active = getActiveFloater(); active && active->canClose())
                        active->closeFloater();
                });
            }
            return true;
        }

        void draw() override
        {
            if (auto* close = findChild<LLButton>("close_active_tool"))
            {
                auto* active = getActiveFloater();
                close->setEnabled(active && active->canClose());
            }
            // One flat surface per dock column. No per-window fill, no rounded card and
            // no drag chrome, so stacked docks read as a single edge of the interface.
            const LLRect r = getLocalRect();
            gl_rect_2d(r, LLColor4(0.075f, 0.071f, 0.075f, 1.f), true);
            const LLColor4 seam(0.155f, 0.140f, 0.150f, 1.f);
            const std::string& name = getName();
            if (name.find("right") != std::string::npos)
                gl_rect_2d(0, r.getHeight(), 1, 0, seam, true);
            else if (name.find("chat") != std::string::npos)
                gl_rect_2d(0, r.getHeight(), r.getWidth(), r.getHeight() - 1, seam, true);
            else
                gl_rect_2d(r.getWidth() - 1, r.getHeight(), r.getWidth(), 0, seam, true);
            LLMultiFloater::draw();
        }
    };

    const char* stormDockName(StormSide side)
    {
        switch (side)
        {
        case StormSide::Left: return "storm_dock_left";
        case StormSide::LeftLower: return "storm_dock_left_lower";
        case StormSide::Right: return "storm_dock_right";
        case StormSide::RightMid: return "storm_dock_right_mid";
        case StormSide::RightBottom: return "storm_dock_right_bottom";
        case StormSide::Chat: return "storm_dock_chat";
        }
        return "";
    }

    StormDock* stormDock(StormSide side)
    {
        return LLFloaterReg::findTypedInstance<StormDock>(stormDockName(side));
    }

    constexpr StormSide STORM_SIDES[] = {
        StormSide::Left, StormSide::LeftLower, StormSide::Right,
        StormSide::RightMid, StormSide::RightBottom, StormSide::Chat
    };

    // The Land Management panel is drawn by the shell rather than hosted as a native
    // floater: it keeps the reference layout while every control calls the real land
    // code. sLandRect is the band the right column reserved for it.
    LLRect sLandRect;
    bool sLandRectValid = false;
    bool sLandPanelHidden = false;
    int sLandTab = 0;
    bool sStormAISettings=false;
    void stormNyxMenuContents();
    int sStormLeftMode = 0; // Inventory, Search, Places, Map, Portals

    // Reference panels own their bands independently of native floater visibility.
    LLRect sStormPanelRects[6];
    bool sStormPanelVisible[6] = { true, true, true, true, true, true };
    bool sStormPanelRectsValid[6] = {};
    bool sStormChatFocus = false;
    float sStormNotificationHeight=0.f;
    std::set<LLUUID> sStormRoutedNotifications;
    char sStormChatInput[2048] = {};
    bool sStormBuildDetails = false;
    std::string sStormRailSelection = "Home";

    bool stormHasTabs(StormSide side)
    {
        const auto* dock = stormDock(side);
        return dock && dock->getFloaterCount() > 0;
    }

    #include "fsstormdocking.inl"

    constexpr const char* STORM_EDITOR_TOOLS[] = {
        "fs_world_project", "fs_world_hierarchy", "fs_world_inspector", "fs_world_lights",
        "nyx_lighting", "fs_world_graph", "nyx_terrain", "nyx_meshterrain",
        "fs_terrain_studio", "fs_terrain_snapshot", "fs_landscaper", "fs_ai_planner", "gestures", "fs_paint"
    };

    bool stormIsEditorTool(const std::string& name)
    {
        return std::any_of(std::begin(STORM_EDITOR_TOOLS), std::end(STORM_EDITOR_TOOLS),
            [&](const char* tool) { return name == tool; });
    }

    StormDock* stormEditorDock()
    {
        auto* dock = stormDock(StormSide::RightMid);
        return !Nyx::editorMode() && dock && dock->getVisible() && dock->getFloaterCount()>0
            ? dock : nullptr;
    }

    struct StormLayout { S32 left=0,right=0,chat=0,top=0,bottom=32; };
    bool sStormSplitWorkspace = false;
    enum class StormSplitContent { Empty, World, Inventory, Browser, Contacts, Appearance, Build, DesktopWindow };
    constexpr StormSplitContent STORM_SPLIT_PANELS[] = {
        StormSplitContent::Inventory, StormSplitContent::Contacts,
        StormSplitContent::Appearance, StormSplitContent::Build
    };
    StormSplitContent sStormSplitPanes[4] = {};
    bool sStormSplitBuildInspector = false;

    int stormSplitPane(StormSplitContent content)
    {
        for (int i = 0; i < 4; ++i)
            if (sStormSplitPanes[i] == content) return i;
        return -1;
    }

    bool stormSplitAvailable(S32 width, S32 height)
    {
        return sStormSplitWorkspace && width >= 700 && height >= 520;
    }

    LLRect stormSplitCell(S32 width, S32 height, bool right, bool lower)
    {
        const S32 top = height - STORM_HEADER;
        const S32 middle = (top + STORM_FOOTER) / 2;
        const S32 center = width / 2;
        return LLRect((right ? center : 0) + 4,
                      (lower ? middle : top) - 4,
                      (right ? width : center) - 4,
                      (lower ? STORM_FOOTER : middle) + 4);
    }

    LLRect stormSplitBody(S32 width, S32 height, int pane)
    {
        LLRect rect = stormSplitCell(width, height, pane % 2 != 0, pane >= 2);
        rect.mTop -= 32;
        return rect;
    }

    struct StormDesktopWindow
    {
        HWND window = nullptr;
        DWORD process = 0;
        LONG_PTR style = 0;
        WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};

        bool exists() const
        {
            DWORD owner = 0;
            return window && GetWindowThreadProcessId(window, &owner) && owner == process;
        }

        void detach()
        {
            if (exists())
            {
                SetParent(window, nullptr);
                SetWindowLongPtrW(window, GWL_STYLE, style);
                SetWindowPos(window, nullptr, 0, 0, 0, 0,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
                SetWindowPlacement(window, &placement);
            }
            window = nullptr;
        }
    };

    StormDesktopWindow sStormDesktopWindows[4];
    HWINEVENTHOOK sStormDesktopDropHook = nullptr;
    HWND sStormDroppedWindow = nullptr;
    POINT sStormDropPoint{};

    void CALLBACK stormDesktopDrop(HWINEVENTHOOK, DWORD, HWND window,
                                   LONG object, LONG child, DWORD, DWORD)
    {
        if (object != OBJID_WINDOW || child != CHILDID_SELF || !window ||
            GetAncestor(window, GA_ROOT) != window || GetWindow(window, GW_OWNER) ||
            !IsWindowVisible(window) || IsIconic(window) || IsZoomed(window) ||
            !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION)) return;
        if (GetCursorPos(&sStormDropPoint)) sStormDroppedWindow = window;
    }

    void stormPlaceDesktopWindows(bool show, S32 width = 0, S32 height = 0)
    {
        if (!show)
        {
            if (sStormDesktopDropHook) UnhookWinEvent(sStormDesktopDropHook);
            sStormDesktopDropHook = nullptr;
            sStormDroppedWindow = nullptr;
        }
        else if (!sStormDesktopDropHook)
        {
            sStormDesktopDropHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZEEND,
                EVENT_SYSTEM_MOVESIZEEND, nullptr, stormDesktopDrop, 0, 0,
                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        }

        const HWND host = show ? (HWND)gViewerWindow->getWindow()->getPlatformWindow() : nullptr;
        const F32 pixelScale = show ? scale() : 1.f;
        for (int pane = 0; pane < 4; ++pane)
        {
            auto& desktop = sStormDesktopWindows[pane];
            auto& content = sStormSplitPanes[pane];
            if (!host || content != StormSplitContent::DesktopWindow)
            {
                desktop.detach();
                if (content == StormSplitContent::DesktopWindow) content = StormSplitContent::Empty;
                continue;
            }
            const LLRect body = stormSplitBody(width, height, pane);
            if (sStormDroppedWindow && !desktop.exists())
            {
                POINT point = sStormDropPoint;
                ScreenToClient(host, &point);
                if (body.pointInRect(ll_round(point.x / pixelScale),
                                     height - ll_round(point.y / pixelScale)))
                {
                    HWND window = sStormDroppedWindow;
                    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
                    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
                    DWORD process = 0;
                    if (GetAncestor(window, GA_ROOT) == window &&
                        GetWindowThreadProcessId(window, &process) &&
                        GetWindowPlacement(window, &placement))
                    {
                        SetLastError(0);
                        if (SetWindowLongPtrW(window, GWL_STYLE,
                            (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME)) | WS_CHILD) ||
                            GetLastError() == 0)
                        {
                            SetLastError(0);
                            if (SetParent(window, host) || GetLastError() == 0)
                            {
                                desktop = {window, process, style, placement};
                                SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                                    SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
                            }
                            else SetWindowLongPtrW(window, GWL_STYLE, style);
                        }
                    }
                }
            }
            if (desktop.exists())
            {
                const RECT target{ll_round(body.mLeft * pixelScale),
                    ll_round((height - body.mTop) * pixelScale),
                    ll_round(body.mRight * pixelScale),
                    ll_round((height - body.mBottom) * pixelScale)};
                RECT current{};
                GetWindowRect(desktop.window, &current);
                MapWindowPoints(nullptr, host, reinterpret_cast<POINT*>(&current), 2);
                if (!EqualRect(&current, &target))
                    SetWindowPos(desktop.window, nullptr, target.left, target.top,
                        target.right - target.left, target.bottom - target.top,
                        SWP_NOACTIVATE | SWP_NOZORDER);
            }
        }
        sStormDroppedWindow = nullptr;
    }

    LLSD stormWorkspaceBrowserKey()
    {
        LLSD key;
        key["target"] = "nyx_split_workspace";
        key["trusted_content"] = false;
        key["url"] = "about:blank";
        return key;
    }

    void stormPlaceWorkspaceBrowser(bool show, S32 width = 0, S32 height = 0)
    {
        const LLSD key = stormWorkspaceBrowserKey();
        auto* browser = LLFloaterReg::findInstance("web_content", key);
        const int pane = stormSplitPane(StormSplitContent::Browser);
        if (!show || pane < 0)
        {
            if (browser) browser->setVisible(false);
            return;
        }
        if (!browser)
        {
            browser = LLFloaterReg::showInstance("web_content", key);
            if (!browser) return;
            browser->setCanDrag(false);
            browser->setCanResize(false);
            browser->setCanMinimize(false);
            browser->setCanClose(false);
        }
        LLRect rect = stormSplitBody(width, height, pane);
        S32 x = 0, y = 0;
        gFloaterView->localPointToScreen(0, 0, &x, &y);
        rect.translate(-x, -y);
        if (browser->getRect() != rect) browser->setShape(rect);
        browser->setVisible(true);
    }

    StormLayout stormLayout(S32 width,S32 height)
    {
        stormLoadPlacement();StormLayout result;
        if(width<700 || height<420) return result;
        for(int i=0;i<6;++i) if(stormPanelShown(i))
        {
            switch(sStormPlacement[i].edge)
            {
            case StormLeft: result.left=llclamp(ll_round(sStormLeftWidth>0?sStormLeftWidth:width*.168f),220,width*42/100);break;
            case StormRight: result.right=llclamp(ll_round(sStormRightWidth>0?sStormRightWidth:width*.276f),240,width*42/100);break;
            case StormChat: result.chat=llclamp(ll_round(sStormChatHeight>0?sStormChatHeight:height*.205f),140,height/2); result.chat=std::max(result.chat,std::min(ll_round(sStormNotificationHeight),height*2/3));break;
            case StormTop: break;
            case StormBottom: break;
            }
        }
        const int room=width-STORM_RAIL-250;
        if (auto* dock=stormEditorDock())
        {
            result.right=std::min(room,dock->activeToolWidth()+8);
            result.left=std::min(result.left,room-result.right);
        }
        if(result.left+result.right>room) {float scale=room/(float)(result.left+result.right);result.left=ll_round(result.left*scale);result.right=ll_round(result.right*scale);}
        return result;
    }

    void stormPlaceDocks(S32 width,S32 height,bool show)
    {
        sLandRectValid=false;for(bool& valid:sStormPanelRectsValid) valid=false;
        static bool restoreEditorDock=false;
        if(auto* dock=stormDock(StormSide::RightMid))
        {
            if(!show || Nyx::editorMode() || width<700 || height<420)
            {
                restoreEditorDock=restoreEditorDock || dock->getVisible();
                dock->setVisible(false);
            }
            else if(restoreEditorDock)
            {
                dock->setVisible(dock->getFloaterCount()>0);
                restoreEditorDock=false;
            }
        }
        auto* editorDock=show?stormEditorDock():nullptr;
        for(StormSide side:STORM_SIDES)
            if(auto* dock=stormDock(side); dock && dock!=editorDock && side!=StormSide::RightMid &&
               (side!=StormSide::Chat || !show))
                dock->setVisible(false);
        if(!show)return;
        const auto layout=stormLayout(width,height);
        const float inboxWidth=std::min(440.f,width*.32f);
        const float toolbarWidth=std::max(1.f,std::min(1180.f,width-inboxWidth-24.f));
        sStormToolbarRect=ImVec4(std::min((static_cast<float>(width)-toolbarWidth)*.5f, width-inboxWidth-toolbarWidth-12.f),
            static_cast<float>(height-STORM_FOOTER),toolbarWidth,static_cast<float>(STORM_FOOTER));
        if (stormSplitAvailable(width, height))
        {
            if (editorDock)
            {
                restoreEditorDock = true;
                editorDock->setVisible(false);
            }
            for (int panel = 0; panel < 4; ++panel)
            {
                const int pane = stormSplitPane(STORM_SPLIT_PANELS[panel]);
                if (pane < 0) continue;
                sStormPanelRects[panel] = stormSplitBody(width, height, pane);
                sStormPanelRectsValid[panel] = true;
            }
            if (!sStormPanelRectsValid[3] && sStormSplitBuildInspector && sStormPanelVisible[3])
            {
                const S32 right = width - 8;
                sStormPanelRects[3].set(right - std::min(460, width / 2), height - STORM_HEADER - 8,
                                        right, STORM_FOOTER + 8);
                sStormPanelRectsValid[3] = true;
            }
            return;
        }
        if(auto* dock=stormDock(StormSide::Chat); dock && dock->getVisible())
        {
            const S32 dockWidth=std::min(width-16,std::max(500,dock->activeToolWidth()));
            auto* active=dock->getActiveFloater();
            const S32 dockHeight=std::min(height-STORM_FOOTER-layout.bottom-STORM_HEADER-12,
                active?std::max(height/2,active->getMinHeight()):height/2);
            const S32 left=(width-dockWidth)/2;
            const S32 bottom=STORM_FOOTER+layout.bottom+4;
            const LLRect rect(left,bottom+dockHeight,left+dockWidth,bottom);
            if(dockWidth>0 && dockHeight>0 && dock->getRect()!=rect) dock->setShape(rect);
        }
        const float top=(float)(STORM_HEADER+layout.top),bottom=(float)(height-STORM_FOOTER-layout.bottom),available=bottom-top;
        if(editorDock)
        {
            LLRect rect(width-layout.right+4,height-STORM_HEADER-4,width-4,STORM_FOOTER+layout.bottom+4);
            if(editorDock->getRect()!=rect) editorDock->setShape(rect);
        }
        for(int edge:{StormLeft,StormRight})
        {
            if(edge==StormRight && editorDock) continue;
            float total=0;for(int i:sStormPanelOrder) if(stormPanelShown(i)&&sStormPlacement[i].edge==edge) total+=sStormPlacement[i].weight;
            float y=top;
            for(int i:sStormPanelOrder) if(stormPanelShown(i)&&sStormPlacement[i].edge==edge)
            {
                float h=available*sStormPlacement[i].weight/std::max(1.f,total);
                const float x=(float)(edge==StormLeft?STORM_RAIL:width-layout.right),w=(float)(edge==StormLeft?layout.left:layout.right);
                sStormPanelRects[i].set(ll_round(x+4),ll_round(height-y-4),ll_round(x+w-4),ll_round(height-y-h+4));sStormPanelRectsValid[i]=w>0;y+=h;
            }
        }
        for(int i=0;i<6;++i) if(stormPanelShown(i))
        {
            auto& p=sStormPlacement[i];
            if(p.edge==StormChat)
            {sStormPanelRects[i].set(STORM_RAIL+layout.left+4,STORM_FOOTER+layout.bottom+layout.chat-4,std::min(width-layout.right-4, sStormChatWidth>0 ? STORM_RAIL+layout.left+4+ll_round(sStormChatWidth) : width),STORM_FOOTER+layout.bottom+4);sStormPanelRectsValid[i]=true;}
            if(p.edge==StormFloat || ((p.edge==StormTop || p.edge==StormBottom)&&sStormExpanded==i))
            {
                float w=std::min(p.w,(float)width-16),h=std::min(p.h,available-8);
                float x=llclamp(p.x,8.f,std::max(8.f,width-w-8));
                float y=(p.edge==StormTop || p.edge==StormBottom)?llclamp(sStormToolbarRect.y-h-4,top,std::max(top,bottom-h)):llclamp(p.y,top,std::max(top,bottom-h));
                sStormPanelRects[i].set(ll_round(x),ll_round(height-y),ll_round(x+w),ll_round(height-y-h));sStormPanelRectsValid[i]=true;
            }
        }
        sLandRect=sStormPanelRects[4];sLandRectValid=sStormPanelRectsValid[4];
    }

    void stormAttach(LLFloater* window, StormSide side)
    {
        if (!window || !gFloaterView || dynamic_cast<LLModalDialog*>(window)) return;
        // The Conversations window is itself a multi-floater, but it is the real home of
        // Nearby Chat and IM tabs, so it is the one nested floater the chat dock accepts.
        const bool conversations = window->getName() == "floater_im_box";
        if (dynamic_cast<LLMultiFloater*>(window) && !conversations) return;
        if (window->getHost() && !dynamic_cast<StormDock*>(window->getHost())) return;
        StormDock* dock = LLFloaterReg::getTypedInstance<StormDock>(stormDockName(side));
        if (!dock) return;
        if (window->getHost() != dock)
        {
            window->setMinimized(false);
            dock->addFloater(window, true);
        }
        else dock->selectFloater(window);
        if(side==StormSide::Chat) dock->openFloater();
        const auto screen = gViewerWindow->getWindowRectScaled();
        stormPlaceDocks(screen.getWidth(), screen.getHeight(), true);
        if (!dock->getVisible()) LLFloater::onClickTearOff(window);
        else window->setFocus(true);
    }

    void stormDockEditorTool(LLFloater* tool)
    {
        if (!tool || Nyx::editorMode() || !gViewerWindow) return;
        if (dynamic_cast<LLMultiFloater*>(tool) || dynamic_cast<LLModalDialog*>(tool)) return;
        auto* dock=LLFloaterReg::getTypedInstance<StormDock>(stormDockName(StormSide::RightMid));
        if(!dock) return;
        if(tool->getHost() && tool->getHost()!=dock)
        {
            tool->openFloater(tool->getKey());
            return;
        }
        dock->setMinimized(false);
        tool->setMinimized(false);
        if(tool->getHost()==dock) dock->selectFloater(tool);
        else dock->addFloater(tool,true);
        Nyx::themeFloater(tool);
        dock->openFloater();
        const auto screen=gViewerWindow->getWindowRectScaled();
        stormPlaceDocks(screen.getWidth(),screen.getHeight(),true);
        tool->setFocus(true);
    }

    void stormOpen(const char* name, StormSide side)
    {
        const std::string id(name);
        if(id=="search" || id=="places" || id=="world_map" || id=="storm_portals")
        {
            sStormLeftMode=id=="places"?2:id=="world_map"?3:id=="storm_portals"?4:1;
            stormRevealPanel(0); return;
        }
        if(id=="fs_my_land_ground" || id=="storm_ground_textures" || id=="about_land")
        {stormRevealPanel(4);sLandTab=id=="about_land"?0:8;return;}
        if(id=="inventory") sStormLeftMode=0;
        if (id == "build" && FSStormWorkspace::showBuildTools()) return;
        if (id == "inventory" || id == "people" || id == "appearance" || id == "fs_im_container")
        {
            stormRevealPanel((int)side);
            if (id == "fs_im_container") sStormChatFocus = true;
            return;
        }
        auto* floater=LLFloaterReg::showInstance(name, LLSD(), true);
        if(stormIsEditorTool(id)) stormDockEditorTool(floater);
        else if (floater && gFloaterView && !floater->getHost())
            gFloaterView->adjustToFitScreen(floater, false);
    }

    const char* stormSideLabel(StormSide side)
    {
        switch (side)
        {
        case StormSide::Left: return "Upper left";
        case StormSide::LeftLower: return "Lower left";
        case StormSide::Right: return "Upper right";
        case StormSide::RightMid: return "Middle right";
        case StormSide::RightBottom: return "Lower right";
        case StormSide::Chat: return "Bottom icon bar";
        }
        return "";
    }

    void stormDockPopup(const char* id)
    {
        if (!ImGui::BeginPopup(id)) return;
        if(ImGui::MenuItem("Reset panel positions"))
        {
            const int edges[]={StormBottom,StormBottom,StormBottom,StormBottom,StormBottom,StormBottom};
            const float weights[]={56,44,38,32,30,100};
            for(int i=0;i<6;++i){sStormPlacement[i].edge=edges[i];sStormPlacement[i].weight=weights[i];}
            sStormLeftWidth=sStormRightWidth=sStormChatHeight=0;sStormPanelOrder={0,1,2,3,4,5};sStormExpanded=-1;stormSavePlacement();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Landscape Designer")) stormOpen("fs_landscaper",StormSide::RightMid);
        if(ImGui::MenuItem("Lighting and Weather")) stormOpen("nyx_lighting",StormSide::RightMid);
        if(ImGui::MenuItem("Landscape Painter")) stormOpen("fs_ai_planner",StormSide::RightMid);
        if(ImGui::MenuItem("Gestures")) stormOpen("gestures",StormSide::RightMid);
        if(ImGui::MenuItem("Mesh Editor")) stormOpen("fs_mesh_edit",StormSide::RightMid);
        if(ImGui::MenuItem("Nyx Painter")) stormOpen("fs_paint",StormSide::RightMid);
        if(ImGui::MenuItem("Viewer Help")) {sStudioSection="Help";sStudioOpen=true;}
        ImGui::Separator();
        const char* panels[]={"Inventory","Contacts","Appearance","Build Tools","Land Management"};
        for(int i=0;i<5;++i)
        {
            const bool open=sStormPlacement[i].edge!=StormBottom || sStormExpanded==i;
            if(ImGui::MenuItem(panels[i],nullptr,open))
            {
                stormRevealPanel(i);
                if(open) stormSetEdge(i,StormBottom);
            }
        }
        ImGui::Separator();
        for (StormSide side : STORM_SIDES)
        {
            StormDock* dock = stormDock(side);
            LLFloater* active = dock ? dock->getActiveFloater() : nullptr;
            if (!active) continue;
            if (ImGui::BeginMenu(stormSideLabel(side)))
            {
                ImGui::TextUnformatted(active->getTitle().c_str());
                ImGui::Separator();
                if (ImGui::BeginMenu("Move tab to"))
                {
                    for (StormSide target : STORM_SIDES)
                    {
                        if (target == side) continue;
                        if (ImGui::MenuItem(stormSideLabel(target))) stormAttach(active, target);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Undock tab")) LLFloater::onClickTearOff(active);
                if (ImGui::MenuItem("Close tab")) active->closeFloater();
                ImGui::EndMenu();
            }
        }
        LLFloater* front = gFloaterView ? gFloaterView->getFrontmost() : nullptr;
        if (front && !front->getHost() && !dynamic_cast<LLMultiFloater*>(front))
        {
            if (ImGui::BeginMenu("Dock front window"))
            {
                ImGui::TextUnformatted(front->getTitle().c_str());
                ImGui::Separator();
                for (StormSide target : STORM_SIDES)
                    if (ImGui::MenuItem(stormSideLabel(target))) stormAttach(front, target);
                ImGui::EndMenu();
            }
        }
        ImGui::Separator();stormNyxMenuContents();
        ImGui::EndPopup();
    }

    void studioOpenConversation(const LLUUID& id)
    {
        FSFloaterIM::show(id);
    }

    bool studioEnabled()
    {
        // NyxUI settings are loaded by start(); before that the new shell is the default.
        return !FSMeshEditorInput::grabsInput() && settingBool("studio.enabled", true);
    }

    void studioChrome(bool hide)
    {
        if (!gViewerWindow || !gViewerWindow->getRootView()) return;
        if (!hide)
        {
            for (const auto& entry : sStudioChrome)
                if (LLView* view = entry.second.first.get()) view->setVisible(entry.second.second);
            sStudioChrome.clear();
            return;
        }
        static const char* names[] = { "status_bar_container", "toolbar_view_holder", "navigation_bar" };
        for (const char* name : names)
        {
            if (LLView* view = gViewerWindow->getRootView()->findChildView(name, true))
            {
                auto saved = sStudioChrome.find(name);
                if (saved == sStudioChrome.end() || saved->second.first.get() != view)
                    sStudioChrome[name] = std::make_pair(view->getHandle(), view->getVisible());
                view->setVisible(false);
            }
        }
    }

    void studioCollect(LLMenuGL* menu, const std::string& path, int depth = 0)
    {
        if (!menu || depth > 16) return;
        for (LLMenuItemGL* item : *menu->getItems())
        {
            if (!item || dynamic_cast<LLMenuItemSeparatorGL*>(item)) continue;
            const std::string label = item->getLabel();
            const std::string full = path.empty() ? label : path + " / " + label;
            if (auto branch = dynamic_cast<LLMenuItemBranchGL*>(item))
                studioCollect(branch->getBranch(), full, depth + 1);
            else if (!label.empty())
            {
                StudioAction a;
                a.handle = item->getHandle();
                a.label = label;
                a.path = full;
                a.name = item->getName();
                a.search = full + " " + a.name;
                LLStringUtil::toLower(a.search);
                sStudioActions.push_back(a);
            }
        }
    }

    void studioIndex()
    {
        if (!sStudioActions.empty() || !gMenuBarView) return;
        studioCollect(gMenuBarView, "");
        std::stable_sort(sStudioActions.begin(), sStudioActions.end(), [](const StudioAction& a, const StudioAction& b) { return a.path < b.path; });
    }

    void studioRun(const StudioAction& action)
    {
        const StudioAction copy = action; // a callback may replace menus or the index
        auto item = dynamic_cast<LLMenuItemGL*>(copy.handle.get());
        if (!item) return;
        item->buildDrawLabel();
        if (!item->getEnabled() || !item->getVisible()) return;
        sStudioRecent.erase(std::remove(sStudioRecent.begin(), sStudioRecent.end(), copy.path), sStudioRecent.end());
        sStudioRecent.insert(sStudioRecent.begin(), copy.path);
        if (sStudioRecent.size() > 12) sStudioRecent.resize(12);
        sStudioOpen = false;
        item->onCommit();
    }

    void studioOpen(const char* section)
    {
        sStudioSection = section;
        sStudioOpen = true;
        sStudioSearch[0] = 0;
    }

    bool studioKey(KEY key, MASK mask, bool down)
    {
        if (!down) return false;
        // Always available, even if an editor owns the keyboard.
        if (key == KEY_F10 && (mask & MASK_CONTROL) && (mask & MASK_ALT))
        {
            setSettingBool("studio.enabled", !studioEnabled());
            saveState();
            studioChrome(false);
            return true;
        }
        if (!studioEnabled()) return false;
        if (key == 'K' && (mask & MASK_CONTROL))
        {
            studioOpen("All tools");
            sStudioSearchFocus = true;
            if (gFocusMgr.getKeyboardFocus()) gFocusMgr.setKeyboardFocus(nullptr);
            return true;
        }
        if (key == KEY_ESCAPE && sStudioOpen && !gFocusMgr.getKeyboardFocus())
        {
            sStudioOpen = false;
            return true;
        }
        return false;
    }

    void studioWindow(const char* id, float x, float y, float w, float h)
    {
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(w, h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
        ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar(2);
    }

    bool studioTile(const char* id, unsigned icon, const char* title, const char* detail, float width)
    {
        ImGui::PushID(id);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, 104.f);
        const bool clicked = ImGui::InvisibleButton("##tile", size);
        const bool hover = ImGui::IsItemHovered();
        auto dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, p + size, Theme::col(hover ? Theme::Raised : Theme::SurfaceAlt), 10.f);
        dl->AddRect(p, p + size, Theme::col(hover ? Theme::Accent : Theme::Border), 10.f);
        Icon::draw(dl, icon, p + ImVec2(26.f, 27.f), 22.f, Theme::col(Theme::Accent));
        dl->PushClipRect(p, p + size, true);
        dl->AddText(p + ImVec2(14.f, 50.f), Theme::col(Theme::Text), title);
        dl->AddText(p + ImVec2(14.f, 76.f), Theme::col(Theme::TextDim), detail);
        dl->PopClipRect();
        if (hover) ImGui::SetTooltip("%s\n%s", title, detail);
        ImGui::PopID();
        return clicked;
    }

    void studioOverview()
    {
        if (ImGui::Button("Open viewer workspace"))
        {
            stormOpen("inventory", StormSide::Left);
            stormOpen("people", StormSide::LeftLower);
            stormOpen("appearance", StormSide::Right);
            stormOpen("build", StormSide::RightMid);
            sLandPanelHidden = false;
            stormOpen("fs_im_container", StormSide::Chat);
            sStudioOpen = false;
        }
        const float w = ImGui::GetContentRegionAvail().x;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float hero_h = w < 600.f ? 150.f : 186.f;
        auto dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, p + ImVec2(w, hero_h), Theme::col(Theme::SurfaceAlt), 12.f);
        if (const Image* art = NyxUI::image("studio_world"))
        {
            dl->AddImageRounded(texRef(art->tex), p, p + ImVec2(w, hero_h), ImVec2(0.f, .20f), ImVec2(1.f, .80f), IM_COL32(255,255,255,210), 12.f);
            dl->AddRectFilledMultiColor(p, p + ImVec2(w, hero_h), IM_COL32(10,16,20,240), IM_COL32(10,16,20,40), IM_COL32(10,16,20,120), IM_COL32(10,16,20,245));
        }
        dl->AddText(p + ImVec2(22.f, 20.f), Theme::col(Theme::Accent), "YOUR WORLD. YOUR WORKSPACE.");
        dl->AddText(fonts().strong, 30.f, p + ImVec2(20.f, 51.f), Theme::col(Theme::Text), "Make something extraordinary.");
        dl->AddText(p + ImVec2(22.f, 101.f), Theme::col(Theme::TextDim), "Explore, create and connect with your tools in one place.");
        ImGui::Dummy(ImVec2(w, hero_h + 8.f));
        sectionHeader("Workspaces");
        struct Tile { const char* name; const char* detail; unsigned icon; };
        static const Tile tiles[] = {
            { "Create", "Model, sculpt and paint", Icon::Model },
            { "World", "Land, terrain and weather", Icon::World },
            { "Photography", "Lighting, camera and capture", Icon::Camera },
            { "Social", "People and conversations", Icon::People },
            { "Inventory", "Find and organize your assets", Icon::Package },
            { "System", "Graphics, audio and diagnostics", Icon::Settings }
        };
        const int cols = w > 780.f ? 3 : (w > 450.f ? 2 : 1);
        const float tw = (w - (cols - 1) * ImGui::GetStyle().ItemSpacing.x) / cols;
        for (int i = 0; i < (int)LL_ARRAY_SIZE(tiles); ++i)
        {
            if (i % cols) ImGui::SameLine();
            if (studioTile(tiles[i].name, tiles[i].icon, tiles[i].name, tiles[i].detail, tw)) studioOpen(tiles[i].name);
        }
        ImGui::Spacing();
        sectionHeader("Recently used");
        if (sStudioRecent.empty()) caption("Tools you use will appear here. Ctrl+K finds any command.");
        for (const std::string& path : sStudioRecent)
        {
            if (ImGui::Selectable(path.c_str()))
            {
                for (const StudioAction& a : sStudioActions) if (a.path == path) { studioRun(a); break; }
                break;
            }
        }
    }

    bool studioMatches(const StudioAction& a, const std::string& section)
    {
        if (section == "All tools") return true;
        const std::map<std::string, std::vector<std::string>> keywords = {
            { "Create", { "build", "mesh", "paint", "sculpt", "material", "texture", "script", "bake", "import", "export", "object", "particle" } },
            { "World", { "world", "land", "terrain", "weather", "water", "sky", "environment", "region", "map", "teleport", "portal", "location" } },
            { "Photography", { "photo", "camera", "snapshot", "light", "render", "visual", "pose", "cinema", "viewfinder" } },
            { "Social", { "communicate", "chat", "people", "friend", "group", "voice", "contact", "profile", "notice", "ani", "assistant" } },
            { "Inventory", { "inventory", "outfit", "appearance", "wear", "avatar", "animation", "gesture", "market", "upload" } },
            { "System", { "preference", "setting", "advanced", "develop", "help", "debug", "performance", "statistic", "sound", "audio", "graphics" } }
        };
        auto group = keywords.find(section);
        if (group == keywords.end()) return true;
        for (const std::string& word : group->second) if (a.search.find(word) != std::string::npos) return true;
        return false;
    }

    void studioActions()
    {
        if (sStudioSearchFocus) { ImGui::SetKeyboardFocusHere(); sStudioSearchFocus = false; }
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##studio_search", "Search tools, actions and menu paths...", sStudioSearch, sizeof(sStudioSearch));
        std::string query = sStudioSearch;
        LLStringUtil::toLower(query);
        std::vector<int> found;
        for (int i = 0; i < (int)sStudioActions.size(); ++i)
        {
            const StudioAction& a = sStudioActions[i];
            if ((sStudioSection!="Help" && !studioMatches(a, sStudioSection)) ||
                a.search.find("firestorm")!=std::string::npos ||
                (!query.empty() && a.search.find(query) == std::string::npos)) continue;
            auto item = dynamic_cast<LLMenuItemGL*>(a.handle.get());
            if (item && item->getVisible()) found.push_back(i);
        }
        ImGui::TextDisabled("%d commands  /  %d indexed", (int)found.size(), (int)sStudioActions.size());
        ImGui::Separator();
        ImGui::BeginChild("##studio_actions", ImVec2(0, 0));
        int run = -1;
        ImGuiListClipper clip;
        clip.Begin((int)found.size(), 57.f);
        while (clip.Step()) for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
        {
            const StudioAction& a = sStudioActions[found[i]];
            auto item = dynamic_cast<LLMenuItemGL*>(a.handle.get());
            if (!item) continue;
            item->buildDrawLabel();
            ImGui::PushID(found[i]);
            const bool enabled = item->getEnabled() && item->getVisible();
            ImGui::BeginDisabled(!enabled);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##action", false, 0, ImVec2(w, 48.f))) run = found[i];
            auto dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(p, p + ImVec2(w, 48.f), true);
            dl->AddText(p + ImVec2(10.f, 3.f), Theme::col(enabled ? Theme::Text : Theme::TextFaint), item->getLabel().c_str());
            dl->AddText(p + ImVec2(10.f, 26.f), Theme::col(Theme::TextDim), a.path.c_str());
            dl->PopClipRect();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s%s", a.path.c_str(), enabled ? "" : "\nUnavailable in the current context or selection.");
            ImGui::PopID();
        }
        if (found.empty()) emptyState(Icon::Search, "No matching tools", "Try a broader term or choose All tools.");
        ImGui::EndChild();
        if (run >= 0) studioRun(sStudioActions[run]);
    }

    void studioQuickTools()
    {
        struct NativeTool { const char* section; const char* label; const char* id; };
        static const NativeTool native[] = {
            { "Create", "Mesh Editor", "fs_mesh_edit" },
            { "Create", "Sculpt", "fs_zbrush" },
            { "Create", "Paint", "fs_paint" },
            { "Create", "Bake Studio", "fs_bake_studio" },
            { "Create", "Shader Graph", "fs_world_graph" },
            { "Create", "Scene Hierarchy", "fs_world_hierarchy" },
            { "Create", "Object Inspector", "fs_world_inspector" },
            { "Create", "Projects", "fs_world_project" },
            { "Create", "Effects", "nyx_vfx" },
            { "Create", "Sequencer", "nyx_sequencer" },
            { "World", "Local World", "fs_world" },
            { "World", "Landscape", "fs_landscaper" },
            { "World", "Terrain Studio", "fs_terrain_studio" },
            { "World", "Mesh Terrain", "nyx_meshterrain" },
            { "World", "My Land Ground", "fs_my_land_ground" },
            { "World", "Procedural Generation", "nyx_pcg" },
            { "Photography", "Lighting and Weather", "nyx_lighting" },
            { "Photography", "Local Lights", "fs_world_lights" },
            { "Photography", "Camera", "nyx_vcam" },
            { "Photography", "Render Queue", "nyx_render_queue" },
            { "Photography", "Poser", "fs_poser" },
            { "Photography", "Snapshot", "snapshot" },
            { "System", "Preferences", "preferences" },
            { "System", "Frame Debugger", "nyx_framedebug" },
            { "System", "Engine Console", "nyx_console" }
        };
        if (ImGui::BeginTable("##native_tools", ImGui::GetContentRegionAvail().x >= 550.f ? 3 : 2))
        {
            for (const auto& tool : native)
            {
                if (sStudioSection != tool.section || !LLFloaterReg::isRegistered(tool.id)) continue;
                ImGui::TableNextColumn();
                if (ImGui::Button(tool.label, ImVec2(-1.f, 0.f)))
                {
                    stormOpen(tool.id, StormSide::Right);
                    sStudioOpen = false;
                }
            }
            ImGui::EndTable();
        }
        struct Tool { const char* section; const char* title; const char* setting; };
        static const Tool tools[] = {
            { "Social", "Conversations", "NyxWindowConversations" },
            { "Social", "Notices", "NyxWindowNotices" },
            { "Social", "Ani", "NyxWindowAni" },
            { "Social", "Assistant", "NyxWindowAssistant" },
            { "Inventory", "Inventory Lens", "NyxWindowInventory" },
            { "Inventory", "Quick Library", "NyxWindowLibrary" },
            { "World", "Portals", "NyxWindowPortals" },
            { "Photography", "Viewfinder", "NyxViewfinder" },
            { "System", "Changed settings", "NyxWindowChanges" },
            { "System", "Window layouts", "NyxWindowLayouts" },
            { "System", "Browser", "NyxWindowBrowser" }
        };
        float row = 0.f;
        for (const Tool& t : tools)
        {
            if (sStudioSection != t.section || !gSavedSettings.controlExists(t.setting)) continue;
            const float width = ImGui::CalcTextSize(t.title).x + 24.f;
            if (row > 0.f && row + width < ImGui::GetContentRegionAvail().x) ImGui::SameLine(); else row = 0.f;
            if (ImGui::Button(t.title)) { gSavedSettings.setBOOL(t.setting, true); sStudioOpen = false; }
            row += width + 9.f;
        }
        ImGui::Spacing();
    }

    void studioSettings()
    {
        // Only documented bounded controls belong here; debug settings remain searchable commands.
        struct Slider { const char* label; const char* key; float low, high; };
        static const Slider sliders[] = {
            { "Draw distance", "RenderFarClip", 32.f, 512.f },
            { "Exposure", "RenderExposure", .5f, 3.f },
            { "Master volume", "AudioLevelMaster", 0.f, 1.f },
            { "Voice", "AudioLevelVoice", 0.f, 1.f },
            { "Music", "AudioLevelMusic", 0.f, 1.f },
            { "World sounds", "AudioLevelSFX", 0.f, 1.f }
        };
        sectionHeader("Live controls");
        for (const Slider& s : sliders)
        {
            LLControlVariable* c = gSavedSettings.getControl(s.key);
            if (!c || c->type() != TYPE_F32) continue;
            float value = (float)c->getValue().asReal();
            ImGui::PushID(s.key);
            ImGui::SetNextItemWidth(llmax(120.f, ImGui::GetContentRegionAvail().x * .55f));
            if (ImGui::SliderFloat(s.label, &value, s.low, s.high, "%.2f")) c->setValue(LLSD(value));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c->getComment().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) c->resetToDefault(true);
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    char sStormFind[192] = {};

    int stormMoney()
    {
        return gStatusBar ? gStatusBar->getBalance() : 0;
    }

    bool stormAction(const char* name, const char* label = nullptr)
    {
        for (const StudioAction& a : sStudioActions)
        {
            if (a.name != name) continue;
            if (label && a.label != label) continue;
            studioRun(a);
            return true;
        }
        return false;
    }

    // Same toggle the Avatar > Search menu item performs, so the button, the rail
    // tab and the menu all end up on the same real search window.
    void stormToggleSearch()
    {
        sStormLeftMode=1; stormRevealPanel(0);
        sStormRailSelection="Search";
    }

    bool stormHeaderButton(const char* id, const char* label, float width,
                           const char* tip = nullptr, bool accent = false)
    {
        ImGui::PushID(id);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 6.f));
        if (accent)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::col(Theme::AccentSoft));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::col(Theme::Raised));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::col(Theme::AccentSoft));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::col(Theme::SurfaceAlt));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::col(Theme::Raised));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::col(Theme::AccentSoft));
        }
        const bool clicked = ImGui::Button(label, ImVec2(width, 30.f));
        if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        return clicked;
    }

    void stormSearchField(float width)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::col(Theme::Raised));
        ImGui::SetNextItemWidth(width);
        const bool submit = ImGui::InputTextWithHint("##storm_find", "Find a tool or command   Ctrl+K",
                                                     sStormFind, sizeof(sStormFind), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        if (!submit) return;
        const std::string query = sStormFind;
        studioOpen("All tools");
        const size_t n = llmin(query.size(), sizeof(sStudioSearch) - 1);
        memcpy(sStudioSearch, query.data(), n);
        sStudioSearch[n] = 0;
        sStudioSearchFocus = true;
        sStormFind[0] = 0;
    }

    std::string stormWhere()
    {
        const std::string parcel = LLViewerParcelMgr::getInstance()->getAgentParcelName();
        const std::string region = gAgent.getRegion() ? gAgent.getRegion()->getName() : std::string();
        if (!parcel.empty() && !region.empty()) return parcel + "  ·  " + region;
        return !region.empty() ? region : parcel;
    }

    bool stormLocationPill(float width)
    {
        static char address[1024]={};static bool editing=false;
        if(!editing && gAgent.getRegion())
        {
            const auto pos=gAgent.getPositionAgent()-gAgent.getRegion()->getOriginAgent();
            snprintf(address,sizeof(address),"%s/%d/%d/%d",gAgent.getRegion()->getName().c_str(),ll_round(pos.mV[0]),ll_round(pos.mV[1]),ll_round(pos.mV[2]));
        }
        auto* parcelManager=LLViewerParcelMgr::getInstance();
        const auto* parcel=parcelManager->getAgentParcel();
        const bool restricted[]={parcel && !parcel->getAllowOtherScripts(),
            parcel && !parcel->getAllowModify(),parcel && parcel->getRestrictPushObject()};
        const int statusCount=static_cast<int>(restricted[0])+static_cast<int>(restricted[1])+static_cast<int>(restricted[2]);
        ImGui::SetNextItemWidth(std::max(48.f,width-static_cast<F32>(statusCount)*22.f));
        bool submit=ImGui::InputTextWithHint("##location","Region/x/y/z or location URL",address,sizeof(address),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll);
        editing=ImGui::IsItemActive();
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Type a destination and press Enter");
        if(submit)
        {
            std::string input(address);LLStringUtil::trim(input);
            LLSLURL destination(input);
            if(destination.getType()!=LLSLURL::LOCATION)
                destination=LLSLURL("secondlife://"+input);
            if(destination.getType()==LLSLURL::LOCATION)
            {
                const auto region=destination.getRegion();const auto pos=destination.getPosition();
                LLWorldMapMessage::getInstance()->sendNamedRegionRequest(region,[pos](U64 handle,const std::string&,const LLUUID&,bool){if(handle && LLStartUp::getStartupState()>=STATE_STARTED)gAgent.teleportViaLocation(from_region_handle(handle)+LLVector3d(pos));},"",false);
            }
            else nyxToast("Enter a region name or a location URL.",true);
            editing=false;
        }
        const unsigned icons[]={Icon::Repair,Icon::Model,Icon::People};
        const char* labels[]={"Scripts disabled","Building disabled","Pushing disabled"};
        for(int i=0;i<3;++i)
        {
            if(!restricted[i]) continue;
            ImGui::SameLine(0.f,6.f);
            const auto pos=ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(16.f,18.f));
            Icon::draw(ImGui::GetWindowDrawList(),icons[i],pos+ImVec2(8.f,9.f),15.f,IM_COL32_WHITE);
            ImGui::GetWindowDrawList()->AddLine(pos+ImVec2(2.f,16.f),pos+ImVec2(14.f,2.f),IM_COL32(240,102,108,255),2.f);
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",labels[i]);
        }
        return false;
    }

    bool stormCommitMenu(LLMenuItemGL* item)
    {
        if (!item) return false;
        item->buildDrawLabel();
        if (!item->getEnabled() || !item->getVisible()) return false;
        auto* previous = gFloaterView ? gFloaterView->getFrontmost() : nullptr;
        item->onCommit();
        auto* opened = gFloaterView ? gFloaterView->getFrontmost() : nullptr;
        if (opened && opened != previous && !opened->getHost() && !dynamic_cast<LLModalDialog*>(opened))
            gFloaterView->adjustToFitScreen(opened, false);
        return true;
    }

    void stormMenuItems(LLMenuGL* menu, int depth = 0)
    {
        if (!menu || depth > 8) return;
        std::set<std::string> labelsSeen;
        for (LLMenuItemGL* item : *menu->getItems())
        {
            if (!item) continue;
            // buildDrawLabel runs the on_visible/on_enable callbacks, so visibility and
            // enablement match what the native menu would show.
            item->buildDrawLabel();
            if (!item->getVisible()) continue;
            if (dynamic_cast<LLMenuItemSeparatorGL*>(item)) { ImGui::Separator(); continue; }
            if (dynamic_cast<LLMenuItemTearOffGL*>(item)) continue;
            const std::string label = item->getLabel();
            if (label.empty()) continue;
            std::string key = label;
            LLStringUtil::toLower(key);
            if (!labelsSeen.insert(key).second) continue;
            if (auto branch = dynamic_cast<LLMenuItemBranchGL*>(item))
            {
                if (!branch->getBranch()) continue;
                if (ImGui::BeginMenu(label.c_str(), item->getEnabled()))
                {
                    stormMenuItems(branch->getBranch(), depth + 1);
                    ImGui::EndMenu();
                }
                continue;
            }
            if (auto check = dynamic_cast<LLMenuItemCheckGL*>(item))
            {
                const bool checked = check->getValue().asBoolean();
                if (ImGui::MenuItem(label.c_str(), nullptr, checked, item->getEnabled())) stormCommitMenu(item);
            }
            else if (ImGui::MenuItem(label.c_str(), nullptr, false, item->getEnabled()))
                stormCommitMenu(item);
        }
    }

    // Firestorm hides Advanced, Admin and the developer menu until developer mode or
    // godmode. NyxStorm always offers them so the bar is complete, and the developer
    // menu is shown under the name players know it by.
    bool stormMenuBarIncludes(LLMenuItemGL* item, std::string& label)
    {
        if (!item) return false;
        item->buildDrawLabel();
        label = item->getLabel();
        const std::string name = item->getName();
        if (name == "Deprecated") return false;
        if (item->getVisible()) return !label.empty();
        if (name == "Advanced" || name == "Admin" || name == "Develop")
        {
            if (name == "Develop") label = "Debug";
            return true;
        }
        return false;
    }

    bool stormBeginDropdown(const char* id)
    {
        if (!ImGui::IsPopupOpen(id)) return false;
        const auto anchor = ImGui::GetItemRectMin();
        const auto bottom = ImGui::GetItemRectMax().y;
        const auto* viewport = ImGui::GetMainViewport();
        const float width = std::min(300.f, viewport->WorkSize.x - 8.f);
        const float x = std::clamp(anchor.x, viewport->WorkPos.x,
            viewport->WorkPos.x + viewport->WorkSize.x - width);
        const float height = std::max(1.f, viewport->WorkPos.y + viewport->WorkSize.y - bottom - 8.f);
        ImGui::SetNextWindowPos(ImVec2(x, bottom));
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, height));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.f);
        const bool open = ImGui::BeginPopup(id);
        ImGui::PopStyleVar(2);
        return open;
    }

    void stormMenuBar(float width)
    {
        if (!gMenuBarView) return;
        std::vector<LLMenuItemGL*> items;
        std::vector<std::string> labels;
        for (LLMenuItemGL* item : *gMenuBarView->getItems())
        {
            auto* branch = dynamic_cast<LLMenuItemBranchGL*>(item);
            if (!branch || !branch->getBranch()) continue;
            std::string label;
            if (!stormMenuBarIncludes(item, label)) continue;
            items.push_back(item);
            labels.push_back(label);
        }

        // Narrow windows fold the whole bar into one Menus popup instead of clipping it.
        if (width <= 900.f)
        {
            if (stormHeaderButton("menus", "Menus", 74.f, "Firestorm menus")) ImGui::OpenPopup("##storm_menus");
            if (stormBeginDropdown("##storm_menus"))
            {
                for (size_t i = 0; i < items.size(); ++i)
                {
                    auto* branch = dynamic_cast<LLMenuItemBranchGL*>(items[i]);
                    if (ImGui::BeginMenu(labels[i].c_str(), items[i]->getEnabled()))
                    {
                        stormMenuItems(branch->getBranch());
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndPopup();
            }
            return;
        }

        for (size_t i = 0; i < items.size(); ++i)
        {
            auto* branch = dynamic_cast<LLMenuItemBranchGL*>(items[i]);
            if (i) ImGui::SameLine(0.f, 1.f);
            ImGui::PushID(items[i]);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 3.f));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::col(Theme::Raised));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::col(Theme::AccentSoft));
            if (ImGui::Button(labels[i].c_str())) ImGui::OpenPopup("##branch");
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            if (stormBeginDropdown("##branch"))
            {
                stormMenuItems(branch->getBranch());
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }

    void stormNyxMenuContents()
    {
        struct Item { const char* label; const char* setting; };
        static const Item items[] = {
            { "Ani", "NyxWindowAni" },
            { "Assistant", "NyxWindowAssistant" },
            { "Browser", "NyxWindowBrowser" },
            { "Conversations", "NyxWindowConversations" },
            { "Notices", "NyxWindowNotices" },
            { "Portals", "NyxWindowPortals" },
            { "Inventory Lens", "NyxWindowInventory" },
            { "Quick Library", "NyxWindowLibrary" },
            { "Viewfinder", "NyxViewfinder" },
            { "Window layouts", "NyxWindowLayouts" },
            { "Changed settings", "NyxWindowChanges" }
        };
        if(ImGui::BeginMenu("Editor tools"))
        {
            const char* titles[]={"Scene project","Scene hierarchy","Inspector","Scene lights",
                "Lighting and weather","Shader graph","Edit terrain","Mesh terrain",
                "Terrain Studio","Terrain snapshots","Landscape Designer","Landscape Painter","Gestures","Nyx Painter"};
            for(size_t i=0;i<LL_ARRAY_SIZE(STORM_EDITOR_TOOLS);++i)
                if(ImGui::MenuItem(titles[i])) stormOpen(STORM_EDITOR_TOOLS[i],StormSide::RightMid);
            ImGui::EndMenu();
        }
        if(ImGui::MenuItem("AI Backend Options...")) sStormAISettings=true;
        ImGui::Separator();
        if(gMenuBarView) for(LLMenuItemGL* item:*gMenuBarView->getItems())
        {
            auto* branch=dynamic_cast<LLMenuItemBranchGL*>(item);
            if(!branch || !branch->getBranch()) continue;
            std::string label;
            if(!stormMenuBarIncludes(item,label)) continue;
            if(ImGui::BeginMenu(label.c_str(),item->getEnabled())) {stormMenuItems(branch->getBranch());ImGui::EndMenu();}
        }
        ImGui::Separator();
        for (const Item& it : items)
        {
            if (!gSavedSettings.controlExists(it.setting)) continue;
            const bool on = gSavedSettings.getBOOL(it.setting);
            if (ImGui::MenuItem(it.label, nullptr, on)) gSavedSettings.setBOOL(it.setting, !on);
        }
        if (gSavedSettings.controlExists("NyxStreamingMode"))
        {
            const bool on = gSavedSettings.getBOOL("NyxStreamingMode");
            if (ImGui::MenuItem("Streaming mode", nullptr, on)) gSavedSettings.setBOOL("NyxStreamingMode", !on);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("All tools", "Ctrl+K")) { studioOpen("All tools"); sStudioSearchFocus = true; }
        if (ImGui::MenuItem("Preferences")) stormOpen("preferences", StormSide::Right);
    }

    void stormSeedWorkspace()
    {
        // Panels are drawn from live models without opening or focusing native windows.
        // In particular, opening the workspace must not activate the build tool.
    }

    // ---- Land Management panel ----------------------------------------------------------------
    // Drawn to match the reference layout. Every value comes from the selected parcel and every
    // control calls the same land code the real About Land floater uses.

    struct LandInfo
    {
        bool valid = false;
        std::string parcel, owner, group, area, rating, region;
        LLUUID snapshot;
    };

    LandInfo landInfo()
    {
        LandInfo info;
        LLViewerParcelMgr* mgr = LLViewerParcelMgr::getInstance();
        // The panel describes the parcel being inspected, which is the current parcel
        // selection, not the one the avatar happens to stand on. Using the agent parcel
        // made every parcel opened in the panel look like the avatar's own.
        LLParcelSelectionHandle selection = mgr->getParcelSelection();
        LLParcel* parcel = selection ? selection->getParcel() : nullptr;
        LLViewerRegion* region = mgr->getSelectionRegion();
        if (!parcel || !region)
        {
            parcel = mgr->getAgentParcel();
            region = gAgent.getRegion();
        }
        if (!parcel || !region) return info;
        info.valid = true;

        info.parcel = parcel->getName();
        if (info.parcel.empty()) info.parcel = region->getName();

        const LLUUID owner = parcel->getOwnerID();
        if (parcel->getIsGroupOwned())
        {
            std::string name;
            info.owner = (gCacheName && gCacheName->getGroupName(owner, name)) ? name : std::string("(group owned)");
        }
        else if (owner.isNull()) info.owner = "(none)";
        else if (owner == gAgent.getID()) info.owner = "You";
        else
        {
            LLAvatarName av;
            info.owner = LLAvatarNameCache::get(owner, &av) ? av.getDisplayName() : std::string("Loading...");
        }

        const LLUUID group = parcel->getGroupID();
        if (group.isNull()) info.group = "(none)";
        else
        {
            std::string name;
            info.group = (gCacheName && gCacheName->getGroupName(group, name)) ? name : std::string("Loading...");
        }

        const S32 area = parcel->getArea();
        info.area = llformat("%d m\u00B2 (%d prims)", area, parcel->getMaxPrimCapacity());
        info.rating = region->getSimAccessString();
        info.region = region->getName();
        info.snapshot = parcel->getSnapshotID();
        return info;
    }

    ImVec4 stormThemeColor(EThemeElement element)
    {
        const auto color = FSDynamicTheme::instance().getColor(element);
        return ImVec4(color.mV[0], color.mV[1], color.mV[2], color.mV[3]);
    }

    void stormDockIcon(int tile,unsigned fallback,const ImVec2& center)
    {
        static auto* atlas=new LLPointer<LLViewerTexture>();
        static bool loaded=false;
        if(!loaded)
        {
            loaded=true;
            LLPointer<LLImagePNG> png=new LLImagePNG;
            LLPointer<LLImageRaw> raw=new LLImageRaw;
            if(png->load(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,"storm_dock_icons.png")) &&
               png->decode(raw.get(),0.f) && raw->scale(1024,1024))
                *atlas=LLViewerTextureManager::getLocalTexture(raw.get(),true);
        }
        auto* draw=ImGui::GetWindowDrawList();
        if(auto texture=NyxUI::textureHandle(atlas->get()); texture && tile >= 0 && tile < 34 && tile != 15 && tile != 18)
        {
            constexpr float step = .125f;
            const float x=static_cast<float>(tile%8)*step;
            const float y=static_cast<float>(tile/8)*step;
            const auto color = stormThemeColor(THEME_BUTTON_TEXT);
            const ImVec4 tint(.68f+.32f*color.x,.76f+.24f*color.y,.84f+.16f*color.z,color.w);
            draw->AddImage(NyxUI::texRef(texture),center-ImVec2(16.f,16.f),center+ImVec2(16.f,16.f),
                ImVec2(x,1.f-y),ImVec2(x+step,1.f-y-step),ImGui::GetColorU32(tint));
        }
        else Icon::draw(draw,fallback,center,23.f,ImGui::GetColorU32(stormThemeColor(THEME_BUTTON_TEXT)));
    }

    bool stormDockIconButton(const char* id,int tile,unsigned fallback,const char* tooltip,bool active=false)
    {
        const auto pos=ImGui::GetCursorScreenPos();
        const bool clicked=ImGui::InvisibleButton(id,ImVec2(36.f,36.f));
        if(ImGui::IsItemHovered())
            ImGui::GetWindowDrawList()->AddRectFilled(pos,pos+ImVec2(36.f,36.f),Theme::col(Theme::Raised),5.f);
        if(active)
            ImGui::GetWindowDrawList()->AddCircleFilled(pos+ImVec2(32.f,32.f),2.f,Theme::col(Theme::Accent));
        stormDockIcon(tile,fallback,pos+ImVec2(18.f,18.f));
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tooltip);
        return clicked;
    }

    void stormChatDockShortcuts()
    {
        struct Rail { const char* label; unsigned icon; const char* id; StormSide side; };
        static const Rail rail[] = {
            { "Home", Icon::Home, nullptr, StormSide::Left },
            { "Inventory", Icon::Folder, "inventory", StormSide::Left },
            { "Appearance", Icon::Contact, "appearance", StormSide::Right },
            { "People", Icon::People, "people", StormSide::LeftLower },
            { "Chat", Icon::Chat, "fs_im_container", StormSide::Chat },
            { "Build", Icon::Model, "build", StormSide::RightMid },
            { "Land", Icon::World, nullptr, StormSide::RightBottom },
            { "Map", Icon::MapPin, "world_map", StormSide::Right },
            { "Places", Icon::Star, "places", StormSide::Left },
            { "Search", Icon::Search, nullptr, StormSide::Right },
            { "Portals", Icon::Link, "storm_portals", StormSide::Left },
            { "Ground", Icon::Brush, "fs_my_land_ground", StormSide::RightBottom },
            { "Textures", Icon::Photo, "storm_ground_textures", StormSide::RightBottom }
        };
        const int slots[]={-1,0,2,1,5,3,4,0,0,0,0,4,4};
        bool first=true;
        for(int index:sStormRailOrder)
        {
            if(index==4 || index==12) continue; // Retain the indices in saved layouts.
            const auto& tab=rail[index];
            const int slot=slots[index];
            if(!first) ImGui::SameLine(0,4);
            first=false;
            ImGui::PushID(index);
            const auto pos=ImGui::GetCursorScreenPos();
            const bool clicked=ImGui::InvisibleButton("##dock_shortcut",ImVec2(36.f,36.f));
            auto* draw=ImGui::GetWindowDrawList();
            if(ImGui::IsItemHovered())
            {
                draw->AddRectFilled(pos,pos+ImVec2(36.f,36.f),Theme::col(Theme::Raised),5.f);
                ImGui::SetTooltip("%s",index==0?"Redock all panels":tab.label);
            }
            constexpr int tiles[]={0,1,2,3,32,4,5,6,7,8,9,11,11};
            stormDockIcon(tiles[index],tab.icon,pos+ImVec2(18.f,18.f));
            if(clicked)
            {
                sStormRailSelection=tab.label;
                if(index==0)
                {
                    for(int i=0;i<6;++i)
                    {
                        stormRevealPanel(i);
                        stormSetEdge(i,StormBottom);
                    }
                }
                else if(slot==5)
                {
                    if(sStormPanelRectsValid[5]) stormSetEdge(5,StormBottom);
                    else
                    {
                        if(sStormPlacement[5].edge!=StormLeft && sStormPlacement[5].edge!=StormRight)
                            stormSetEdge(5,StormLeft);
                        stormRevealPanel(5);
                    }
                    sStormChatFocus=true;
                }
                else
                {
                    if(index==1 && sStormLeftMode==0 && sStormPanelRectsValid[0] &&
                       sStormPlacement[0].edge!=StormChat)
                        stormSetEdge(0,StormBottom);
                    else if(index==9) stormToggleSearch();
                    else if(index==6) { stormRevealPanel(4); sLandTab=0; }
                    else stormOpen(tab.id,tab.side);
                    if(sStormPlacement[slot].edge!=StormChat &&
                       !(index==1 && sStormLeftMode==0 && sStormPanelRectsValid[0]) &&
                       sStormPlacement[slot].edge!=StormLeft && sStormPlacement[slot].edge!=StormRight)
                        stormSetEdge(slot,slot<=1?StormLeft:StormRight);
                }
            }
            ImGui::PopID();
        }
    }

    #include "fsstormnotebook.inl"
    #include "fsstormpanels.inl"
    #include "fsstormworldpanels.inl"

    void studioDraw(float width, float height)
    {
        studioIndex();
        ImGui::PushStyleColor(ImGuiCol_WindowBg,IM_COL32(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Border,IM_COL32(0,0,0,0));
        studioWindow("##studio_header",0,0,width,STORM_HEADER);
        ImGui::SetCursorPos(ImVec2(6,2));
        if(stormHeaderButton("back","<",30,"Previous location"))LLTeleportHistory::getInstance()->goBack();
        ImGui::SameLine(0,4);if(stormHeaderButton("fwd",">",30,"Next location"))LLTeleportHistory::getInstance()->goForward();
        ImGui::SameLine(0,4);if(stormHeaderButton("home","Home",46,"Teleport home"))gAgent.teleportHome();
        ImGui::SameLine(0,4);
        if(iconButton("teleport_history",Icon::Library,"Teleport history",false,24.f))
            LLFloaterReg::toggleInstance("fs_teleporthistory");
        ImGui::SameLine(0,6);
        LLFloater* front = gFloaterView ? gFloaterView->getFrontmost() : nullptr;
        if (auto* dock = dynamic_cast<StormDock*>(front)) front = dock->getActiveFloater();
        const bool canClose = front && !dynamic_cast<LLModalDialog*>(front) && front->canClose();
        const bool showFPS = gSavedSettings.getBOOL("FSStatusBarShowFPS");
        const bool wide = width > 1000.f;
        const bool showMarket = width >= 430.f;
        const float rightWidth = (wide ? 341.f : 0.f) + (showMarket ? 106.f : 0.f)
                               + (showFPS ? 66.f : 0.f) + (canClose ? 30.f : 0.f);
        stormLocationPill(std::max(60.f, width - ImGui::GetItemRectMax().x - rightWidth - 24.f));
        if(wide)
        {
            ImGui::SameLine(0,6);if(stormHeaderButton("search","Search...",180,"Search people, places and events"))stormToggleSearch();
            ImGui::SameLine(0,6);const std::string balance=llformat("L$ %d",stormMoney());
            if(stormHeaderButton("balance",balance.c_str(),85,"Linden dollar balance"))stormAction("Buy and Sell L$");
            ImGui::SameLine(0,6);if(stormHeaderButton("buy","Buy L$",58,"Buy Linden dollars"))stormAction("Buy and Sell L$");
        }
        if (showMarket)
        {
            ImGui::SameLine(0,6);
            if(stormHeaderButton("market","Marketplace",100,"Marketplace"))stormAction("SL Marketplace");
        }
        if (showFPS)
        {
            static LLTimer fpsTimer;
            static std::string fpsText = "FPS";
            if (fpsTimer.getElapsedTimeF32() >= 1.f)
            {
                fpsTimer.reset();
                const auto fps = LLTrace::get_frame_recording().getPeriodMedianPerSec(LLStatViewer::FPS);
                fpsText = llformat("%.0f FPS", fps);
            }
            ImGui::SameLine(0, 6);
            if (stormHeaderButton("fps", fpsText.c_str(), 60, "Frame rate. Open Statistics"))
                LLFloaterReg::toggleInstance("stats");
        }
        if (canClose)
        {
            ImGui::SameLine(0, 4);
            const std::string tip = "Close " + front->getTitle();
            if (iconButton("close_front_window", Icon::Close, tip.c_str(), false, 24.f))
                front->closeFloater();
        }
        // Full Firestorm menu bar on its own row: World, Advanced, Admin and Debug are
        // always present even when the native menu bar hides them.
        ImGui::SetCursorPos(ImVec2(6, STORM_HEADER_NAV - 4));
        stormMenuBar(width);
        ImGui::SameLine(0.f, 8.f);
        int diagnostic = llclamp(gSavedSettings.getS32("RenderNyxLightingDiagnostic"), 0, 3);
        const char* views[] = {"Normal lighting", "Indirect only", "Direct only", "Pre-tonemap HDR"};
        ImGui::SetNextItemWidth(165.f);
        if (ImGui::Combo("##lighting_diagnostic", &diagnostic, views, IM_ARRAYSIZE(views)))
        {
            gSavedSettings.setS32("RenderNextGenDebugMode", 0);
            gSavedSettings.setS32("RenderNyxLightingDiagnostic", diagnostic);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Direct and indirect use fixed exposure. HDR shows the signal going into the tone curve.");
        ImGui::End();ImGui::PopStyleColor(2);

        // The redrawn Land Management panel fills the lower-right band whether or not the
        // studio workspace is open.
        if (stormSplitAvailable((S32)width, (S32)height))
        {
            StormPanels::inventory(height);
            StormPanels::contacts(height);
            StormPanels::appearance(height);
            static constexpr const char* labels[] = {
                "Empty", "World view", "Inventory", "Browser", "Contacts", "Appearance", "Build Tools", "Desktop Window"
            };
            for (int i = 0; i < 4; ++i)
            {
                const auto content = sStormSplitPanes[i];
                const LLRect cell = stormSplitCell((S32)width, (S32)height, i % 2 != 0, i >= 2);
                const std::string id = "##storm_split_" + std::to_string(i);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::col(Theme::Surface));
                studioWindow(id.c_str(), (float)cell.mLeft, height - cell.mTop,
                             (float)cell.getWidth(),
                             content == StormSplitContent::Empty ||
                             (content == StormSplitContent::DesktopWindow && !sStormDesktopWindows[i].exists())
                                 ? (float)cell.getHeight() : 30.f);
                ImGui::SetCursorPos(ImVec2(5.f, 3.f));
                ImGui::SetNextItemWidth(std::min(175.f, (float)cell.getWidth() - 10.f));
                if (ImGui::BeginCombo("##pane_content",
                    content == StormSplitContent::Empty ? "Choose a view" : labels[(int)content]))
                {
                    for (int option = 0; option < IM_ARRAYSIZE(labels); ++option)
                    {
                        if (!ImGui::Selectable(labels[option], (int)content == option)) continue;
                        const auto next = static_cast<StormSplitContent>(option);
                        const int previous = stormSplitPane(next);
                        if (next != StormSplitContent::Empty && next != StormSplitContent::DesktopWindow &&
                            previous >= 0 && previous != i)
                        {
                            if (content == StormSplitContent::DesktopWindow)
                                sStormSplitPanes[previous] = StormSplitContent::Empty;
                            else
                                sStormSplitPanes[previous] = content;
                        }
                        sStormSplitPanes[i] = next;
                        if (content == StormSplitContent::Build || next == StormSplitContent::Build)
                            sStormSplitBuildInspector = false;
                    }
                    ImGui::EndCombo();
                }
                if (content == StormSplitContent::DesktopWindow && !sStormDesktopWindows[i].exists())
                    ImGui::TextWrapped("Drag a desktop app by its title bar and release it here.");
                ImGui::End();
                ImGui::PopStyleColor();
            }
            StormPanels::build(height);
        }
        else
        {
            stormPanelsDraw(height);
            StormPanels::worldPanels(height);
        }
        StormPanels::pinnedFolderWindows(width,height);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.f,0.f,0.f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0.f,0.f,0.f,0.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,stormThemeColor(THEME_BG_PRIMARY));
        ImGui::PushStyleColor(ImGuiCol_Text,stormThemeColor(THEME_TEXT_PRIMARY));
        ImGui::PushStyleColor(ImGuiCol_Button,Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,Theme::col(Theme::AccentSoft));
        studioWindow("##storm_icon_bar",sStormToolbarRect.x,sStormToolbarRect.y,sStormToolbarRect.z,sStormToolbarRect.w);
        ImGui::SetCursorPos(ImVec2(5.f,4.f));
        StormPanels::chatFooter();
        ImGui::End();
        ImGui::PopStyleColor(7);
        const float inboxWidth=std::min(440.f,width*.32f);
        ImGui::SetNextWindowPos(ImVec2(width-inboxWidth-8.f,sStormToolbarRect.y));
        ImGui::SetNextWindowSize(ImVec2(inboxWidth,STORM_FOOTER));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(4.f,8.f));
        ImGui::Begin("##storm_inbox_header",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
            ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
        StormPanels::inboxTabs(inboxWidth-8.f);
        ImGui::End();
        ImGui::PopStyleVar();
        StormPanels::chat(width,height);
        StormNotebook::load();
        for (auto& note : StormNotebook::notes) StormNotebook::sync(note);
        StormNotebook::drawEditor();
        stormDockBars(width,height);
        stormAISettingsDraw();

        if (!sStudioOpen) return;
        const auto layout = stormLayout((S32)width, (S32)height);
        const float left = (float)(STORM_RAIL + layout.left);
        const float center = width - left - layout.right;
        ImGui::SetNextWindowPos(ImVec2(left + 6.f, STORM_HEADER + 6.f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(llmax(220.f, center - 12.f),
            llmax(160.f, height - STORM_HEADER - STORM_FOOTER - layout.top - layout.bottom - layout.chat - 12.f)), ImGuiCond_Always);
        const std::string title = "NyxStorm / " + sStudioSection + "###studio_workspace";
        if (ImGui::Begin(title.c_str(), &sStudioOpen, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            if (sStudioSection == "Overview") studioOverview();
            else if (sStudioSection == "Help")
            {
                ImGui::SeparatorText("Viewer Help");
                ImGui::TextWrapped("Drag panel titles to dock them. Drag the edge markers to resize.");
                ImGui::TextWrapped("Use Local Land for parcel isolation and terrain textures. In Portals, select a portal and enable editing to move it or snap it to a wall.");
                ImGui::TextWrapped("Ctrl+Shift+S opens Photo Mode. Chat holds conversations, group messages, and notifications.");
                studioActions();
            }
            else
            {
                studioQuickTools();
                if (sStudioSection == "System") studioSettings();
                studioActions();
            }
        }
        ImGui::End();
    }
