// Firestorm reference workspace, included in the private UI namespace.
// All mutations go through viewer controls/actions, retaining their permission checks.
namespace StormPanels
{
    ImU32 selectedColor() { return Theme::col(Theme::AccentSoft); }
    constexpr ImU32 TEXT = IM_COL32(238, 237, 235, 255);
    constexpr ImU32 MUTED = IM_COL32(173, 172, 174, 255);
    int contactsTab = 0, appearanceTab = 0, buildTab = 0;
    char contactsFilter[192] = {};
    LLUUID selectedItem, selectedFriend;
    LLUUID selectedSession, closeConversation;
    int conversationFilter=0; // conversation, IM inbox, group inbox
    bool nearbySelected=true;
    int dockInbox=0;
    bool nearbyExpanded=false;
    float inboxAnchors[3]={};
    std::map<LLUUID,std::string> conversationDrafts;
    std::set<LLUUID> revealFolders;
    bool scrollToSelection=false;

    LLFloater* dockedInventory=nullptr;
    bool inventoryEmbedded=false;
    LLUUID pendingInventorySelection;

    void revealItem(const LLUUID& id)
    {
        auto* item=gInventory.getItem(id);
        selectedItem=item && item->getIsLinkType() ? item->getLinkedUUID() : id;
        pendingInventorySelection=selectedItem; sStormLeftMode=0;
        stormRevealPanel(0); sStormRailSelection="Inventory";
        revealFolders.clear();
        auto* object=gInventory.getObject(selectedItem);
        if(gInventory.getCategory(selectedItem)) revealFolders.insert(selectedItem);
        LLUUID parent=object ? object->getParentUUID() : LLUUID::null;
        while(parent.notNull() && revealFolders.insert(parent).second)
        {
            auto* category=gInventory.getCategory(parent);
            parent=category ? category->getParentUUID() : LLUUID::null;
        }
        scrollToSelection=true;
    }

    bool matches(std::string text, const char* filter)
    {
        std::string query(filter);
        LLStringUtil::toLower(text); LLStringUtil::toLower(query);
        return query.empty() || text.find(query) != std::string::npos;
    }

    void detail(const char* name)
    {
        if(std::string(name)=="build") sStormBuildDetails=true;
        if (auto* floater = LLFloaterReg::getInstance(name))
        {
            if (dynamic_cast<StormDock*>(floater->getHost())) LLFloater::onClickTearOff(floater);
            floater->openFloater();
            // Keep auxiliary dialogs in the world area instead of behind a panel.
            const auto screen=gViewerWindow->getWindowRectScaled();
            const auto layout=stormLayout(screen.getWidth(),screen.getHeight());
            const S32 left=STORM_RAIL+layout.left+8;
            const S32 top=screen.getHeight()-STORM_HEADER-8;
            S32 ox=0,oy=0; gFloaterView->localPointToScreen(0,0,&ox,&oy);
            LLRect r=floater->getRect();
            r.translate(left-ox-r.mLeft,top-oy-r.mTop);
            floater->setShape(r);
            floater->setFocus(true);
        }
    }

    bool begin(int index, const char* title, unsigned icon, float height)
    {
        if (!sStormPanelRectsValid[index] ||
            (!sStormPanelVisible[index] && !stormSplitAvailable(
                gViewerWindow->getWindowRectScaled().getWidth(), (S32)height))) return false;
        sStormCurrentPanel=index;
        const auto& r = sStormPanelRects[index];
        const ImVec2 p((float)r.mLeft, height-r.mTop), size((float)r.getWidth(), (float)r.getHeight());
        ImGui::SetNextWindowPos(p); ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::col(Theme::Surface));
        ImGui::PushStyleColor(ImGuiCol_Border, Theme::col(Theme::Border));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, MUTED);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedColor());
        ImGui::PushStyleColor(ImGuiCol_Header, selectedColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, selectedColor());
        ImGui::PushStyleColor(ImGuiCol_CheckMark, Theme::col(Theme::Accent));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, Theme::col(Theme::Accent));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Theme::col(Theme::AccentHover));
        ImGui::PushFont(fonts().body, 16.f);
        const std::string id = "##firestorm_panel_" + std::to_string(index);
        ImGui::Begin(id.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);
        // Fixed docks stay behind floating tools opened from the consolidated menu.
        if(sStormPlacement[index].edge==StormLeft || sStormPlacement[index].edge==StormRight || sStormPlacement[index].edge==StormChat)
            ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilledMultiColor(p+ImVec2(2,2), p+ImVec2(size.x-2,29),
            IM_COL32(37,35,37,255), IM_COL32(32,31,33,255), IM_COL32(23,23,24,255), IM_COL32(23,23,24,255));
        Icon::draw(dl, icon, p+ImVec2(14,14), 14, Theme::col(Theme::Accent));
        dl->PushClipRect(p+ImVec2(27,0), p+ImVec2(size.x-48,29), true);
        dl->AddText(fonts().strong,16.f,p+ImVec2(27,7), TEXT, title); dl->PopClipRect();
        const bool split = stormSplitAvailable(
            gViewerWindow->getWindowRectScaled().getWidth(), (S32)height);
        if (split && index != 3)
        {
            sStormCurrentPanel = -1;
            ImGui::SetCursorPos(ImVec2(9,31));
            ImGui::Dummy(ImVec2(0.f, 0.f));
            return true;
        }
        if (!split)
        {
            ImGui::SetCursorPos(ImVec2(8,3));
            ImGui::InvisibleButton("##move_panel",ImVec2(std::max(20.f,size.x-66),25));
            stormPanelDragHandle(index);
            ImGui::SetCursorPos(ImVec2(size.x-53,4));
            if(ImGui::SmallButton("-"))
            {
                if(sStormPlacement[index].edge==StormTop || sStormPlacement[index].edge==StormBottom) sStormExpanded=-1;
                else stormSetEdge(index,StormBottom);
            }
        }
        ImGui::SetCursorPos(ImVec2(size.x-29,4));
        if (ImGui::SmallButton("x"))
        {
            if(index==5)
            {
                if(!nearbySelected){closeConversation=selectedSession;nearbySelected=true;}
                else stormSetEdge(index,StormBottom);
            }
            else
            {
                sStormPanelVisible[index] = false;
                if (index == 4) sLandPanelHidden = true;
                if (sStormExpanded == index) sStormExpanded = -1;
                if (index == 3)
                {
                    sStormSplitBuildInspector = false;
                    const int pane = split ? stormSplitPane(StormSplitContent::Build) : -1;
                    if (pane >= 0) sStormSplitPanes[pane] = StormSplitContent::Empty;
                    if (auto* tools = LLFloaterReg::findTypedInstance<LLFloaterTools>("build"))
                        tools->onClose(false);
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close %s", title);
        if (split) sStormCurrentPanel = -1;
        ImGui::SetCursorPos(ImVec2(9,31));
        ImGui::Dummy(ImVec2(0.f, 0.f));
        return true;
    }

    void end()
    {
        stormFinishPanel();
        ImGui::End(); ImGui::PopFont(); ImGui::PopStyleColor(15); ImGui::PopStyleVar(7);
    }

    void tabs(const char* const* labels, int count, int& selected)
    {
        const float room = ImGui::GetContentRegionAvail().x;
        const float spacing = 4.f;
        float used = 0.f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        for (int i = 0; i < count; ++i)
        {
            const float width = std::min(room, ImGui::CalcTextSize(labels[i]).x + 20.f);
            if (used > 0.f && used + spacing + width <= room)
            {
                ImGui::SameLine(0.f, spacing);
                used += spacing;
            }
            else used = 0.f;
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::col(i == selected ? Theme::AccentSoft : Theme::SurfaceAlt));
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::col(i == selected ? Theme::Accent : Theme::Border));
            if (ImGui::Button(labels[i], ImVec2(width, 27.f))) selected = i;
            ImGui::PopStyleColor(2);
            ImGui::PopID();
            used += width;
        }
        ImGui::PopStyleVar(2);
        ImGui::Spacing();
    }

    bool button(const char* text, ImVec2 size = ImVec2(0,25), bool accent = false)
    {
        if (accent) ImGui::PushStyleColor(ImGuiCol_Button, selectedColor());
        bool clicked = ImGui::Button(text, size);
        if (accent) ImGui::PopStyleColor();
        return clicked;
    }

    void itemRow(LLViewerInventoryItem* item)
    {
        if (!item) return;
        ImGui::PushID(item->getUUID().asString().c_str());
        // A drag out of this row has to hand the mouse over to Firestorm's drag and drop
        // tool, otherwise the row only ever gets selected and nothing is rezzed.
        static LLUUID sItemDrag;
        if (!ImGui::IsMouseDown(0)) sItemDrag.setNull();
        const bool coalesced=item->getType()==LLAssetType::AT_OBJECT &&
            (item->getFlags() & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS)!=0;
        const std::string label=item->getName()+(coalesced?"  [Coalesced]":
            item->getType()==LLAssetType::AT_OBJECT?"  [Object / Box]":"");
        const bool row_clicked = ImGui::Selectable((label+"###inventory_item").c_str(), selectedItem==item->getUUID(), ImGuiSelectableFlags_AllowDoubleClick);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 5.f) && sItemDrag.isNull())
        {
            const EDragAndDropType type = LLViewerAssetType::lookupDragAndDropType(item->getActualType());
            if (type != DAD_NONE && !LLToolDragAndDrop::getInstance()->hasMouseCapture())
            {
                sItemDrag = item->getUUID();
                selectedItem = item->getUUID();
                ImGui::ClearActiveID();
                gFocusMgr.setMouseCapture(nullptr);
                gFocusMgr.setKeyboardFocus(nullptr);
                LLToolDragAndDrop::getInstance()->beginDrag(type, item->getUUID(),
                    LLToolDragAndDrop::SOURCE_AGENT, gAgentID);
            }
        }
        if (row_clicked)
        {
            selectedItem=item->getUUID();
            if (ImGui::IsMouseDoubleClicked(0))
            {
                if(item->getWearableType()==LLWearableType::WT_SHAPE && gAgentWearables.isWearingItem(item->getLinkedUUID()))
                {appearanceTab=1; stormRevealPanel(2);}
                else LLInvFVBridgeAction::doAction(selectedItem, &gInventory);
            }
        }
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Open")) LLInvFVBridgeAction::doAction(item->getUUID(), &gInventory);
            if (ImGui::MenuItem("Show in inventory"))
            {
                revealItem(item->getUUID());
            }
            ImGui::EndPopup();
        }
        if(scrollToSelection && selectedItem==item->getUUID())
        { ImGui::SetScrollHereY(.5f); scrollToSelection=false; }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s",item->getName().c_str(),
            coalesced?"\nContains multiple coalesced objects.":
            item->getType()==LLAssetType::AT_OBJECT?"\nInventory object. Contents are only known after rezzing.":"");
        ImGui::PopID();
    }

    struct PinnedFolder { bool dockLeft=false; bool dockRight=false; };
    std::map<LLUUID,PinnedFolder> pinnedFolders;

    void folder(const LLUUID& id, int depth=0)
    {
        static std::set<LLUUID> ancestors;
        if(!ancestors.insert(id).second) return;
        auto* category=gInventory.getCategory(id);
        if (!category) { ancestors.erase(id); return; }
        ImGui::PushID(id.asString().c_str());
        if(revealFolders.erase(id)) ImGui::SetNextItemOpen(true);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(228,191,106,255));
        ImGuiTreeNodeFlags flags=depth==0 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        if(selectedItem==id) flags|=ImGuiTreeNodeFlags_Selected;
        const S32 children=category->getDescendentCount();
        const std::string label=category->getName()+(children>=0?"  ("+std::to_string(children)+")":"");
        const bool open=ImGui::TreeNodeEx("folder",flags,"%s",label.c_str());
        if(scrollToSelection && selectedItem==id) {ImGui::SetScrollHereY(.5f); scrollToSelection=false;}
        ImGui::PopStyleColor();
        if(ImGui::BeginPopupContextItem("##folder_pin"))
        {
            if(ImGui::MenuItem("Pin folder copy")) pinnedFolders.try_emplace(id);
            ImGui::EndPopup();
        }
        if(ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("STORM_FOLDER",&id,sizeof(id));
            ImGui::TextUnformatted(category->getName().c_str());
            ImGui::EndDragDropSource();
        }
        if (open)
        {
            if(ImGui::IsItemToggledOpen() || depth==0)
            {
                static std::set<LLUUID> requested;
                if(requested.insert(id).second) LLInventoryModelBackgroundFetch::instance().start(id,false);
            }
            LLInventoryModel::cat_array_t* categories=nullptr;
            LLInventoryModel::item_array_t* items=nullptr;
            gInventory.getDirectDescendentsOf(id, categories, items);
            if (categories) for (const auto& cat : *categories) if (cat.notNull()) folder(cat->getUUID(),depth+1);
            if (items) for (const auto& item : *items) itemRow(item);
            ImGui::TreePop();
        }
        ImGui::PopID();
        ancestors.erase(id);
    }

    void hideInventory()
    {
        if(auto* floater=LLFloaterReg::findInstance("inventory"); inventoryEmbedded && floater)
        {
            floater->setVisible(false);
            floater->setCanResize(true);
            floater->setCanDrag(true);
            floater->setCanClose(true);
            floater->setCanMinimize(true);
        }
        inventoryEmbedded=false;
        dockedInventory=nullptr;
    }

    void inventory(float height)
    {
        const std::string title=llformat("Inventory (%d Items)",gInventory.getItemCount());
        if (!begin(0,title.c_str(),Icon::Package,height))
        {
            hideInventory();
            return;
        }
        auto* floater=LLFloaterReg::getInstance("inventory");
        if (floater)
        {
            const bool entering=!inventoryEmbedded;
            dockedInventory=floater;
            inventoryEmbedded=true;
            auto rect=sStormPanelRects[0];
            // Leave the workspace header and resize handles available to the shell.
            rect.stretch(-14);
            rect.mTop=sStormPanelRects[0].mTop-31;
            if(rect.getWidth()>0 && rect.getHeight()>0)
            {
                if(floater->getRect()!=rect) floater->setShape(rect);
                floater->setVisible(true);
                if(entering)
                {
                    floater->setMinimized(false);
                    floater->setCanResize(false);
                    floater->setCanDrag(false);
                    floater->setCanClose(false);
                    floater->setCanMinimize(false);
                    floater->setFrontmost(false);
                }
                if(pendingInventorySelection.notNull() && gInventory.isInventoryUsable())
                {
                    if(auto* side=floater->findChild<LLSidepanelInventory>("main_panel"))
                    {
                        side->showInventoryPanel();
                        side->selectAllItemsPanel();
                        if(auto* main=side->getMainInventoryPanel())
                        {
                            main->onFilterEdit("");
                            if(auto* tree=main->getActivePanel())
                            {
                                tree->setSelection(pendingInventorySelection,false);
                                pendingInventorySelection.setNull();
                            }
                        }
                    }
                }
            }
            else hideInventory();
        }
        end();
    }

    void radar()
    {
        auto& radar=FSRadar::instance();
        std::vector<LLSD> entries;
        LLSD stats;
        radar.getCurrentData(entries,stats);
        if(ImGui::BeginTable("##nearby_radar",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Resident");
            ImGui::TableSetupColumn("Distance",ImGuiTableColumnFlags_WidthFixed,82.f);
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableHeadersRow();
            for(const auto& row:entries)
            {
                const auto& entry=row["entry"];
                const LLUUID id=entry["id"].asUUID();
                const auto name=entry["name"].asString();
                if(id.isNull() || !matches(name,contactsFilter)) continue;
                ImGui::PushID(id.asString().c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if(ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_AllowDoubleClick) &&
                   ImGui::IsMouseDoubleClicked(0)) radar.zoomAvatar(id,name);
                if(ImGui::BeginPopupContextItem())
                {
                    if(ImGui::MenuItem("Message")) LLAvatarActions::startIM(id);
                    if(ImGui::MenuItem("Profile")) LLAvatarActions::showProfile(id);
                    if(ImGui::MenuItem("Focus camera")) radar.zoomAvatar(id,name);
                    ImGui::EndPopup();
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted((entry["range"].asString()+" m").c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void contacts(float height)
    {
        if (!begin(1,"Contacts",Icon::People,height)) return;
        ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##filter","Filter people...",contactsFilter,sizeof(contactsFilter));
        LLAvatarTracker::buddy_map_t buddies; LLAvatarTracker::instance().copyBuddyList(buddies);
        std::string friends=llformat("Friends (%d)",(int)buddies.size());
        const char* labels[]={friends.c_str(),"Groups","Blocked","Radar"}; tabs(labels,4,contactsTab);
        if(contactsTab==3)
        {
            ImGui::BeginChild("##radar_body",ImVec2(0,-31));
            radar();
            ImGui::EndChild();
            if(button("Radar options")) LLFloaterReg::showInstance("fs_radar");
            end();
            return;
        }
        ImGui::BeginChild("##contacts",ImVec2(0,-32));
        int count=0;
        if (contactsTab==0)
        {
            // Online contacts first. Names come from the viewer's name cache.
            for (bool online : {true,false}) for (const auto& entry:buddies)
            {
                if (!entry.second || entry.second->isOnline()!=online) continue;
                LLAvatarName avatar;
                const std::string name=LLAvatarNameCache::get(entry.first,&avatar) ? avatar.getDisplayName() : "Loading name...";
                if (!matches(name,contactsFilter)) continue;
                ++count; ImGui::PushID(entry.first.asString().c_str());
                const ImVec2 p=ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##person",selectedFriend==entry.first,ImGuiSelectableFlags_AllowDoubleClick,ImVec2(0,37)))
                {
                    selectedFriend=entry.first;
                    if (ImGui::IsMouseDoubleClicked(0)) LLAvatarActions::startIM(entry.first);
                }
                auto* dl=ImGui::GetWindowDrawList();
                dl->AddCircleFilled(p+ImVec2(6,16),4,online ? IM_COL32(122,219,126,255) : IM_COL32(105,105,108,255));
                dl->AddCircleFilled(p+ImVec2(27,17),12,IM_COL32(56,49,51,255));
                LLUUID* photo=LLAvatarIconIDCache::instance().get(entry.first);
                const Tex portrait=photo && photo->notNull() ? NyxUI::textureHandle(LLViewerTextureManager::getFetchedTexture(*photo)) : (Tex)0;
                if(portrait) dl->AddImageRounded(NyxUI::texRef(portrait),p+ImVec2(15,5),p+ImVec2(39,29),ImVec2(0,1),ImVec2(1,0),IM_COL32_WHITE,12);
                else Icon::draw(dl,Icon::Contact,p+ImVec2(27,17),17,MUTED);
                dl->AddText(p+ImVec2(45,2),TEXT,name.c_str());
                dl->AddText(p+ImVec2(45,19),MUTED,online ? "Online" : "Offline");
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Message")) LLAvatarActions::startIM(entry.first);
                    if (ImGui::MenuItem("Profile")) LLAvatarActions::showProfile(entry.first);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        else if (contactsTab==1)
        {
            for (const auto& group:gAgent.mGroups) if (matches(group.mName,contactsFilter))
            {
                ++count; ImGui::PushID(group.mID.asString().c_str());
                if (ImGui::Selectable(group.mName.c_str())) LLGroupActions::startIM(group.mID);
                ImGui::PopID();
            }
        }
        else for (const auto& mute:LLMuteList::getInstance()->getMutes()) if (matches(mute.mName,contactsFilter))
        {
            ++count; ImGui::TextUnformatted(mute.mName.c_str());
        }
        if (!count) ImGui::TextDisabled("No matching contacts.");
        ImGui::EndChild();
        if(button("+ Add Contact")) LLFloaterAvatarPicker::show([](const uuid_vec_t& ids,const std::vector<LLAvatarName>& names)
        {if(!ids.empty()) LLAvatarActions::requestFriendshipDialog(ids.front());},false,true);
        ImGui::SameLine(); ImGui::BeginDisabled(selectedFriend.isNull() || contactsTab!=0);
        if (button("Message",ImVec2(-1,25))) LLAvatarActions::startIM(selectedFriend);
        ImGui::EndDisabled(); end();
    }

    Tex previewTexture(const LLUUID& id,int width,int height);

    void outfitGallery(LLAppearanceMgr& appearance)
    {
        static char filter[160]={};
        static bool favorites=false;
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##outfit_filter","Find outfits...",filter,sizeof(filter));
        ImGui::Checkbox("Favorites only",&favorites);
        const auto root=gInventory.findCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);
        LLInventoryModel::cat_array_t folders;
        LLInventoryModel::item_array_t items;
        gInventory.collectDescendents(root,folders,items,LLInventoryModel::EXCLUDE_TRASH);
        folders.erase(std::remove_if(folders.begin(),folders.end(),[&](const auto& folder){
            return folder->getPreferredType()!=LLFolderType::FT_OUTFIT ||
                (favorites && !folder->getIsFavorite()) || !matches(folder->getName(),filter);
        }),folders.end());
        std::sort(folders.begin(),folders.end(),[](const auto& a,const auto& b){
            if(a->getIsFavorite()!=b->getIsFavorite()) return a->getIsFavorite();
            return LLStringUtil::compareDict(a->getName(),b->getName())<0;
        });
        const LLUUID wearing=appearance.getBaseOutfitUUID();
        ImGui::BeginChild("##outfit_gallery",ImVec2(0,0));
        const int columns=std::max(1,static_cast<int>(ImGui::GetContentRegionAvail().x/155.f));
        if(folders.empty()) ImGui::TextDisabled("No matching outfits.");
        else if(ImGui::BeginTable("##outfit_cards",columns,ImGuiTableFlags_SizingStretchSame))
        {
            for(const auto& folder:folders)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(folder->getUUID().asString().c_str());
                const float width=ImGui::GetContentRegionAvail().x;
                const ImVec2 pos=ImGui::GetCursorScreenPos();
                const ImVec2 size(width,112.f);
                ImGui::Dummy(size);
                if(ImGui::IsItemVisible())
                {
                    auto* draw=ImGui::GetWindowDrawList();
                    draw->AddRectFilled(pos,pos+size,IM_COL32(41,37,42,255),5.f);
                    if(auto image=previewTexture(folder->getThumbnailUUID(),static_cast<int>(width),112))
                        draw->AddImageRounded(NyxUI::texRef(image),pos,pos+size,ImVec2(0,1),ImVec2(1,0),IM_COL32_WHITE,5.f);
                    else Icon::draw(draw,Icon::Contact,pos+size*.5f,28.f,TEXT);
                    if(folder->getIsFavorite()) Icon::draw(draw,Icon::Star,pos+ImVec2(13.f,13.f),15.f,TEXT);
                }
                ImGui::TextWrapped("%s",folder->getName().c_str());
                const bool worn=folder->getUUID()==wearing;
                ImGui::BeginDisabled(worn || appearance.isOutfitLocked());
                if(button(worn?"Wearing":"Wear",ImVec2(-1.f,24.f)))
                    appearance.wearInventoryCategory(folder.get(),false,false);
                ImGui::EndDisabled();
                ImGui::Spacing();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    LLUUID inlinePoseAvatar;

    void setInlinePose(bool enabled)
    {
        if(!gAgentAvatarp) {inlinePoseAvatar.setNull();return;}
        if(enabled)
        {
            if(!gAgentAvatarp->isMotionActive(ANIM_AGENT_CUSTOMIZE) &&
                gAgentAvatarp->startMotion(ANIM_AGENT_CUSTOMIZE))
                inlinePoseAvatar=gAgentAvatarp->getID();
        }
        else if(inlinePoseAvatar.notNull())
        {
            if(inlinePoseAvatar==gAgentAvatarp->getID())
                gAgentAvatarp->stopMotion(ANIM_AGENT_CUSTOMIZE);
            inlinePoseAvatar.setNull();
        }
    }

    void appearance(float height)
    {
        if(!begin(2,"Appearance",Icon::Contact,height)) return;
        if(inlinePoseAvatar.notNull() && (!gAgentAvatarp || !gAgentAvatarp->isMotionActive(ANIM_AGENT_CUSTOMIZE)))
            inlinePoseAvatar.setNull();
        struct Part { const char* label; const char* group; };
        static const Part parts[]={
            {"Wearing",""},{"Body","shape_body"},{"Head","shape_head"},
            {"Eyes","shape_eyes"},{"Ears","shape_ears"},{"Nose","shape_nose"},
            {"Mouth","shape_mouth"},{"Chin","shape_chin"},{"Torso","shape_torso"},{"Legs","shape_legs"},
            {"Outfits",nullptr}
        };
        if(button("Appearance options")) ImGui::OpenPopup("##appearance_options");
        if(ImGui::BeginPopup("##appearance_options"))
        {
            const bool posed=inlinePoseAvatar.notNull();
            if(ImGui::MenuItem("Legacy Pose Mode",nullptr,posed,gAgentAvatarp.notNull()))
                setInlinePose(!posed);
            ImGui::EndPopup();
        }
        ImGui::BeginChild("##appearance_tabs",ImVec2(0,36),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        const int galleryTab=static_cast<int>(LL_ARRAY_SIZE(parts))-1;
        if(button("Outfits",ImVec2(70,23),appearanceTab==galleryTab)) appearanceTab=galleryTab;
        for(int i=0;i<galleryTab;++i)
        {
            ImGui::SameLine(0,3);
            if(button(parts[i].label,ImVec2(70,23),appearanceTab==i)) appearanceTab=i;
        }
        ImGui::EndChild();
        auto& mgr=LLAppearanceMgr::instance();
        if(appearanceTab==galleryTab) outfitGallery(mgr);
        else if(appearanceTab==0)
        {
            std::string outfit; mgr.getBaseOutfitName(outfit);
            ImGui::TextWrapped("Current Outfit: %s",outfit.empty() ? "Custom outfit" : outfit.c_str());
            ImGui::BeginChild("##wearing",ImVec2(0,-34),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
            LLInventoryModel::cat_array_t cats; LLInventoryModel::item_array_t items;
            gInventory.collectDescendents(mgr.getCOF(),cats,items,LLInventoryModel::EXCLUDE_TRASH);
            for(const auto& item:items)
            {
                ImGui::PushID(item->getUUID().asString().c_str());
                if(ImGui::Selectable(item->getName().c_str(),selectedItem==item->getLinkedUUID())) revealItem(item->getUUID());
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show %s in Inventory",item->getName().c_str());
                ImGui::PopID();
            }
            if(items.empty()) ImGui::TextDisabled("Loading worn items...");
            ImGui::EndChild();
            const float w=ImGui::GetContentRegionAvail().x;
            ImGui::BeginDisabled(mgr.isOutfitLocked() || !mgr.isOutfitDirty());
            if(button("Save Outfit",ImVec2(w*.58f,26),true)) mgr.updateBaseOutfit();
            ImGui::EndDisabled(); ImGui::SameLine();
            if(button("Save As...",ImVec2(-1,26))) ImGui::OpenPopup("##save_outfit");
            if(ImGui::BeginPopup("##save_outfit"))
            {
                static char name[192]={};
                ImGui::InputTextWithHint("##name","New outfit name",name,sizeof(name));
                ImGui::BeginDisabled(!name[0] || mgr.isOutfitLocked());
                if(button("Save new outfit")) {mgr.makeNewOutfitLinks(name,false); name[0]=0; ImGui::CloseCurrentPopup();}
                ImGui::EndDisabled(); ImGui::EndPopup();
            }
        }
        else
        {
            auto* shape=gAgentWearables.getViewerWearable(LLWearableType::WT_SHAPE,0);
            if(!shape || !gAgentAvatarp)
            { ImGui::TextWrapped("Waiting for your worn shape to load."); end(); return; }
            ImGui::TextWrapped("Shape: %s",shape->getName().c_str());
            const bool editable=gAgentWearables.isWearableModifiable(LLWearableType::WT_SHAPE,0) &&
                LLFloaterSidePanelContainer::canShowPanel("appearance",LLSD());
            if(!editable) ImGui::TextWrapped("This shape is read-only.");
            ImGui::BeginDisabled(!editable);
            if(button("Save this part...")) fsnyx_export_shape_part(parts[appearanceTab].group);
            ImGui::SameLine();
            if(button("Apply saved part...")) fsnyx_import_shape_part();
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Apply a saved body section to the worn shape and save the result as a new inventory shape.");
            static char filter[128]={};
            ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##shape_filter","Filter shape controls...",filter,sizeof(filter));
            ImGui::BeginChild("##shape_controls",ImVec2(0,-35));
            std::vector<LLViewerVisualParam*> params;
            for(const auto& entry:shape->mVisualParamIndexMap)
            {
                auto* param=dynamic_cast<LLViewerVisualParam*>(entry.second);
                if(param && param->isTweakable() && param->getEditGroup()==parts[appearanceTab].group &&
                   (param->getSex() & gAgentAvatarp->getSex()) && matches(param->getDisplayName(),filter)) params.push_back(param);
            }
            std::sort(params.begin(),params.end(),[](auto* a,auto* b){return a->getDisplayOrder()<b->getDisplayOrder();});
            ImGui::BeginDisabled(!editable);
            for(auto* param:params)
            {
                const float range=param->getMaxWeight()-param->getMinWeight();
                if(range<=0.f) continue;
                ImGui::PushID(param->getID());
                ImGui::TextUnformatted(param->getDisplayName().c_str());
                float value=(shape->getVisualParamWeight(param->getID())-param->getMinWeight())/range*100.f;
                ImGui::SetNextItemWidth(-1);
                if(ImGui::SliderFloat("##value",&value,0.f,100.f,"%.0f",ImGuiSliderFlags_AlwaysClamp))
                {
                    shape->setVisualParamWeight(param->getID(),param->getMinWeight()+value*.01f*range,false);
                    shape->writeToAvatar(gAgentAvatarp); gAgentAvatarp->updateVisualParams();
                }
                ImGui::TextDisabled("%s  /  %s",param->getMinDisplayName().c_str(),param->getMaxDisplayName().c_str());
                ImGui::Spacing(); ImGui::PopID();
            }
            ImGui::EndDisabled();
            if(params.empty()) ImGui::TextWrapped("No matching controls in this shape section.");
            ImGui::EndChild();
            const float w=(ImGui::GetContentRegionAvail().x-10.f)/3.f;
            ImGui::BeginDisabled(!editable || !shape->isDirty());
            if(button("Save Shape",ImVec2(w,27),true)) gAgentWearables.saveWearable(LLWearableType::WT_SHAPE,0);
            ImGui::SameLine();
            if(button("Revert",ImVec2(w,27))) gAgentWearables.revertWearable(LLWearableType::WT_SHAPE,0);
            ImGui::EndDisabled(); ImGui::SameLine();
            ImGui::BeginDisabled(!editable || !gAgentWearables.isWearableCopyable(LLWearableType::WT_SHAPE,0));
            if(button("Save As...",ImVec2(-1,27))) ImGui::OpenPopup("##save_shape");
            ImGui::EndDisabled();
            if(ImGui::BeginPopup("##save_shape"))
            {
                static char name[192]={}; ImGui::InputTextWithHint("##name","New shape name",name,sizeof(name));
                ImGui::BeginDisabled(!name[0] || !editable);
                if(button("Save shape copy"))
                {gAgentWearables.saveWearableAs(LLWearableType::WT_SHAPE,0,name,shape->getDescription(),false); name[0]=0; ImGui::CloseCurrentPopup();}
                ImGui::EndDisabled(); ImGui::EndPopup();
            }
        }
        end();
    }

    LLUICtrl* control(LLView* parent,const char* name)
    {
        return parent ? parent->findChild<LLUICtrl>(name) : nullptr;
    }

    void check(LLView* parent,const char* name,const char* label)
    {
        auto* c=control(parent,name); bool value=c && c->getValue().asBoolean();
        ImGui::BeginDisabled(!c || !c->getEnabled());
        if (ImGui::Checkbox(label,&value)) { c->setValue(value); c->onCommit(); }
        ImGui::EndDisabled();
    }

    void number(LLView* parent,const char* name,const char* label)
    {
        auto* c=parent ? parent->findChild<LLSpinCtrl>(name) : nullptr;
        float value=c ? static_cast<float>(c->getValue().asReal()) : 0.f;
        ImGui::PushID(name); ImGui::BeginDisabled(!c || !c->getEnabled());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat(label,&value,0,0,"%.3f",ImGuiInputTextFlags_EnterReturnsTrue))
        { c->setValue(llclamp(value,c->getMinValue(),c->getMaxValue())); c->onCommit(); }
        ImGui::EndDisabled(); ImGui::PopID();
    }

    void field(LLView* parent,const char* name,const char* label)
    {
        auto* c=control(parent,name);
        static std::map<LLUICtrl*,std::string> edits;
        auto& edit=edits[c];
        if (edit.empty() && c) edit=c->getValue().asString();
        char buffer[512]; snprintf(buffer,sizeof(buffer),"%s",edit.c_str());
        ImGui::PushID(name);
        if(label[0]) {ImGui::TextUnformatted(label); ImGui::SameLine(85);}
        ImGui::SetNextItemWidth(-1); ImGui::BeginDisabled(!c || !c->getEnabled());
        bool commit=ImGui::InputText("##value",buffer,sizeof(buffer),ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemActive()) edit=buffer;
        else if (!commit && c) edit=c->getValue().asString();
        if (commit) { c->setValue(std::string(buffer)); c->onCommit(); edit=buffer; }
        ImGui::EndDisabled(); ImGui::PopID();
    }

    void combo(LLView* parent,const char* name,const char* label)
    {
        auto* c=parent ? parent->findChild<LLComboBox>(name) : nullptr;
        ImGui::PushID(name); ImGui::BeginDisabled(!c || !c->getEnabled());
        ImGui::TextUnformatted(label); ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##combo",c ? c->getSelectedItemLabel().c_str() : ""))
        {
            if(c) if(auto* list=c->findChild<LLScrollListCtrl>("ComboBox"))
                for(auto* row:list->getAllData())
                {
                    auto* cell=row->getColumn(0);
                    if(cell && ImGui::Selectable(cell->getValue().asString().c_str(),row->getValue()==c->getValue()))
                    { c->setValue(row->getValue()); c->onCommit(); }
                }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled(); ImGui::PopID();
    }

    void action(LLView* parent,const char* name,const char* label)
    {
        auto* c=control(parent,name);
        ImGui::BeginDisabled(!c || !c->getEnabled());
        if(button(label)) c->onCommit();
        ImGui::EndDisabled();
    }

    // UI previews must own a reference and submit demand every frame. Otherwise
    // the fetcher can evict a displayed image and alternate it with the fallback.
    Tex previewTexture(const LLUUID& id,int width,int height)
    {
        if(id.isNull()) return (Tex)0;
        struct Preview {LLPointer<LLViewerFetchedTexture> image; double used=0;};
        static std::map<LLUUID,Preview> previews;
        const double now=ImGui::GetTime();
        for(auto it=previews.begin();it!=previews.end();)
            if(now-it->second.used>30)
            {
                if(it->second.image) it->second.image->forceActive();
                it=previews.erase(it);
            }
            else ++it;
        auto& preview=previews[id]; preview.used=now;
        if(!preview.image) preview.image=LLViewerTextureManager::getFetchedTexture(id);
        if(!preview.image) return (Tex)0;
        preview.image->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
        preview.image->setKnownDrawSize(width,height);
        preview.image->addTextureStats((F32)(width*height));
        return NyxUI::textureHandle(preview.image);
    }

    void texture(LLView* parent,const char* name,const char* label)
    {
        auto* c=parent ? parent->findChild<LLTextureCtrl>(name) : nullptr;
        ImGui::PushID(name); ImGui::BeginDisabled(!c || !c->getEnabled());
        const ImVec2 p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##texture",ImVec2(54,54)) && c) c->showPicker(true);
        auto* dl=ImGui::GetWindowDrawList();
        dl->AddRectFilled(p,p+ImVec2(54,54),IM_COL32(42,42,44,255),4);
        const Tex image=c && c->getImageAssetID().notNull() ?
            previewTexture(c->getImageAssetID(),54,54) : (Tex)0;
        if(image) dl->AddImageRounded(NyxUI::texRef(image),p,p+ImVec2(54,54),ImVec2(0,1),ImVec2(1,0),IM_COL32_WHITE,4);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Choose %s",label);
        ImGui::SameLine(); ImGui::TextUnformatted(label);
        ImGui::EndDisabled(); ImGui::PopID();
    }

    void build(float height)
    {
        if (!begin(3,"Build Tools",Icon::Model,height)) return;
        ImGui::PushFont(fonts().body,14.f);
        auto* tools=LLFloaterReg::getTypedInstance<LLFloaterTools>("build");
        const auto* currentTool=LLToolMgr::getInstance()->getCurrentTool();
        if(tools && (currentTool==LLToolBrushLand::getInstance() || currentTool==LLToolSelectLand::getInstance()))
        {
            ImGui::TextUnformatted("Terrain Brush");
            if(auto* mode=control(tools,"land_radio_group"))
            {
                const char* names[]={"Select","Flatten","Raise","Lower","Smooth","Noise","Revert"};
                const char* values[]={"radio select land","radio flatten","radio raise","radio lower","radio smooth","radio noise","radio revert"};
                const auto selected=mode->getValue().asString();
                int index=0;
                for(int i=1;i<7;++i)
                    if(selected==values[i]) index=i;
                ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("##terrain_action",&index,names,7))
                {
                    mode->setValue(values[index]);
                    mode->onCommit();
                }
            }
            for(const auto& slider : {std::pair{"slider brush size","Brush size"},std::pair{"slider force","Brush strength"}})
            {
                if(auto* c=tools->findChild<LLSlider>(slider.first))
                {
                    float value=static_cast<float>(c->getValue().asReal());
                    ImGui::TextUnformatted(slider.second);
                    ImGui::PushID(slider.first);
                    ImGui::SetNextItemWidth(-1);
                    if(ImGui::SliderFloat("##terrain_slider",&value,c->getMinValue(),c->getMaxValue()))
                    {
                        c->setValue(value);
                        c->onCommit();
                    }
                    ImGui::PopID();
                }
            }
            action(tools,"button apply to selection","Apply to Selection");
            ImGui::PopFont();
            end();
            return;
        }
        // The ordinary panels own validation, permissions and server messages.
        if (tools) for (const char* name:{"General","Object","Features"})
            if (auto* panel=tools->findChild<LLPanel>(name)) panel->refresh();
        static const char* labels[] = {"Inspector", "Object", "Content", "Material", "Features"};
        tabs(labels, 5, buildTab);
        ImGui::BeginChild("##build_inspector", ImVec2(0, 0));
        auto* manager = LLToolMgr::getInstance();
        struct BuildTool
        {
            const char* label;
            unsigned icon;
            LLTool* native;
        };
        const BuildTool modes[] = {
            {"Move", Icon::plain(Glow::Move), LLToolCompTranslate::getInstance()},
            {"Rotate", Icon::plain(Glow::Rotate), LLToolCompRotate::getInstance()},
            {"Scale", Icon::plain(Glow::Scale), LLToolCompScale::getInstance()},
            {"Select Face", Icon::Edit, LLToolFace::getInstance()},
            {"Create", Icon::Model, LLToolCompCreate::getInstance()}
        };
        auto selectTool = [&](LLTool* tool)
        {
            manager->setCurrentToolset(gBasicToolset);
            if (tools) tools->onOpen(LLSD());
            LLFloaterTools::setEditTool(tool);
        };

        if (ImGui::BeginTable("##build_controls", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Tools", ImGuiTableColumnFlags_WidthFixed, 34.f);
            ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();
            for (const auto& mode : modes)
            {
                ImGui::PushID(mode.label);
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const bool clicked = ImGui::InvisibleButton("##mode", ImVec2(30, 30));
                const bool active = manager->getBaseTool() == mode.native;
                ImGui::GetWindowDrawList()->AddRectFilled(p, p + ImVec2(30, 30),
                    active || ImGui::IsItemHovered() ? selectedColor() : IM_COL32(38,38,40,255), 4);
                Icon::draw(ImGui::GetWindowDrawList(), mode.icon, p + ImVec2(15,15), 18, TEXT);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", mode.label);
                if (clicked) selectTool(mode.native);
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            if (ImGui::BeginTable("##build_modes", 2, ImGuiTableFlags_SizingStretchSame,
                                   ImVec2(0, 0)))
            {
                ImGui::TableNextColumn();
                for (const auto& mode : modes)
                    if (ImGui::RadioButton(mode.label, manager->getBaseTool() == mode.native))
                        selectTool(mode.native);
                check(tools, "checkbox edit linked parts", "Edit Linked");
                ImGui::TableNextColumn();
                check(tools, "checkbox uniform", "Stretch Both Sides");
                check(tools, "checkbox stretch textures", "Stretch Textures");
                check(tools, "checkbox snap to grid", "Snap to Grid");
                check(tools, "checkbox show highlight", "Show Highlight");
                check(tools, "checkbox surface snap", "Surface Snap");
                combo(tools, "combobox grid mode", "Edit Axis");
                ImGui::EndTable();
            }
            ImGui::EndTable();
        }

        const auto drawTransform = [&]()
        {
            auto selection = LLSelectMgr::getInstance()->getSelection();
            const auto* object = selection->getFirstObject();
            const LLUUID objectID = object ? object->getID() : LLUUID::null;
            if (ImGui::BeginTable("##transform", 4, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 14.f);
                for (const char* title : {"Position (m)", "Rotation (deg)", "Size (m)"})
                    ImGui::TableSetupColumn(title, ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (const char* axis : {"X", "Y", "Z"})
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(axis);
                    for (const char* prefix : {"Pos ", "Rot ", "Scale "})
                    {
                        ImGui::TableNextColumn();
                        const std::string name = std::string(prefix) + axis;
                        number(tools, name.c_str(), "##axis");
                        if (auto* spin = tools ? tools->findChild<LLSpinCtrl>(name) : nullptr)
                        {
                            static ImGuiID editing = 0;
                            static float draggedValue = 0.f;
                            static LLUUID editedObject;
                            static S32 editedCount = 0;
                            static double nextCommit = 0.0;
                            static bool pendingCommit = false;
                            ImGui::PushID(name.c_str());
                            ImGui::BeginDisabled(objectID.isNull() || !spin->getEnabled());
                            const ImGuiID id = ImGui::GetID("##axis_slider");
                            float value = editing == id ? draggedValue : (float)spin->getValue().asReal();
                            ImGui::SetNextItemWidth(-1);
                            const float speed = prefix[0] == 'R' ? 0.25f :
                                prefix[0] == 'S' ? llclamp(value * 0.01f, 0.002f, 0.025f) : 0.01f;
                            const bool changed = ImGui::DragFloat("##axis_slider", &value, ImGui::GetIO().KeyShift ? speed * 0.1f : speed,
                                spin->getMinValue(), spin->getMaxValue(), "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoSpeedTweaks);
                            if (ImGui::IsItemActivated())
                            {
                                editing = id;
                                draggedValue = value;
                                editedObject = objectID;
                                editedCount = selection->getObjectCount();
                                nextCommit = 0.0;
                                pendingCommit = false;
                            }
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Drag left or right to adjust. Hold Shift for finer control.");
                            if (editing == id)
                            {
                                if (changed)
                                {
                                    draggedValue = value;
                                    pendingCommit = true;
                                }
                                const bool ended = ImGui::IsItemDeactivated();
                                const bool validTarget = !objectID.isNull() && spin->getEnabled() &&
                                    editedObject == objectID && editedCount == selection->getObjectCount();
                                const double now = ImGui::GetTime();
                                // Use native transform commits at a bounded rate, then flush the final value.
                                if (validTarget && pendingCommit && (ended || now >= nextCommit))
                                {
                                    spin->setValue(draggedValue);
                                    spin->onCommit();
                                    pendingCommit = false;
                                    nextCommit = now + 1.0 / 30.0;
                                }
                                if (ended || !validTarget || !ImGui::IsItemActive()) editing = 0;
                            }
                            ImGui::EndDisabled();
                            ImGui::PopID();
                        }
                    }
                }
                ImGui::EndTable();
            }
        };
        if (!LLSelectMgr::getInstance()->getSelection()->getObjectCount())
            ImGui::TextDisabled("Select an object in the world to edit.");
        if (buildTab == 0)
        {
            static int inspectorTab = 0;
            const char* sections[] = {"Transform", "Properties", "Permissions", "Physics"};
            ImGui::PushID("inspector");
            tabs(sections, 4, inspectorTab);
            ImGui::PopID();
            if (inspectorTab == 0)
            {
                drawTransform();
                ImGui::Separator();
                ImGui::TextUnformatted("Material");
                if (tools) tools->refreshPanelFace();
                combo(tools, "combobox matmedia", "Material mode");
                const auto* materialMode = control(tools, "combobox matmedia");
                if (materialMode && materialMode->getValue().asString() == "PBR")
                {
                    texture(tools, "pbr_control", "PBR Material");
                    action(tools, "edit_selected_pbr", "Edit material factors...");
                }
                else
                {
                    texture(tools, "texture control", "Diffuse texture");
                    check(tools, "checkbox fullbright", "Full Bright");
                    ImGui::TextUnformatted("Glow");
                    number(tools, "glow", "##glow");
                }
                if (button("Material settings")) buildTab = 3;
            }
            else if (inspectorTab == 1)
            {
                field(tools, "Object Name", "Name");
                field(tools, "Object Description", "Description");
                for (const char* name : {"Creator Name", "Owner Name", "Group Name"})
                {
                    auto* c = control(tools, name);
                    ImGui::TextDisabled("%s", name);
                    ImGui::SameLine(100);
                    ImGui::TextUnformatted(c ? c->getValue().asString().c_str() : "");
                }
            }
            else if (inspectorTab == 2)
            {
                check(tools, "checkbox locked", "Locked");
                check(tools, "checkbox share with group", "Share with Group");
                check(tools, "checkbox allow everyone move", "Everyone Can Move");
                check(tools, "checkbox allow everyone copy", "Everyone Can Copy");
                check(tools, "checkbox next owner can modify", "Next Owner Can Modify");
                check(tools, "checkbox next owner can copy", "Next Owner Can Copy");
                check(tools, "checkbox next owner can transfer", "Next Owner Can Transfer");
            }
            else
            {
                check(tools, "Physical Checkbox Ctrl", "Physical");
                check(tools, "Temporary Checkbox Ctrl", "Temporary");
                check(tools, "Phantom Checkbox Ctrl", "Phantom");
                combo(tools, "Physics Shape Type Combo Ctrl", "Physics Shape");
                for (const char* name : {"Physics Gravity", "Physics Friction", "Physics Density", "Physics Restitution"})
                {
                    ImGui::TextUnformatted(name);
                    number(tools, name, "##physics");
                }
            }
        }
        else if (buildTab == 1)
        {
            drawTransform();
            combo(tools, "comboBaseType", "Primitive Shape");
            for (const char* name : {"Path Limit Begin", "Path Limit End", "Twist Begin", "Twist End", "Taper X", "Taper Y", "Skew"})
            {
                if (auto* c = control(tools, name); c && c->getEnabled())
                {
                    ImGui::TextUnformatted(name);
                    number(tools, name, "##shape");
                }
            }
            if (button("Align")) selectTool(QToolAlign::getInstance());
            ImGui::SameLine();
            if (button("Terrain")) selectTool(LLToolSelectLand::getInstance());
        }
        else if(buildTab==4)
        {
            check(tools,"Flexible1D Checkbox Ctrl","Flexible path");
            check(tools,"Light Checkbox Ctrl","Light");
            number(tools,"Physics Gravity","##gravity"); ImGui::TextDisabled("Gravity multiplier");
            number(tools,"Physics Friction","##friction"); ImGui::TextDisabled("Friction");
            number(tools,"Physics Density","##density"); ImGui::TextDisabled("Density");
        }
        else if(buildTab==3)
        {
            if(tools) tools->refreshPanelFace();
            combo(tools,"combobox matmedia","Material mode");
            const auto* materialMode = control(tools, "combobox matmedia");
            if (materialMode && materialMode->getValue().asString() == "PBR")
            {
                texture(tools, "pbr_control", "PBR Material");
                action(tools, "edit_selected_pbr", "Edit material factors...");
                action(tools, "save_selected_pbr", "Save Material");
                for (const char* name : {"gltfTextureScaleU", "gltfTextureScaleV", "gltfTextureRotation",
                                         "gltfTextureOffsetU", "gltfTextureOffsetV"})
                {
                    ImGui::TextUnformatted(name);
                    number(tools, name, "##material_uv");
                }
            }
            else
            {
            texture(tools,"texture control","Diffuse texture");
            texture(tools,"bumpytexture control","Normal map");
            texture(tools,"shinytexture control","Specular map");
            check(tools,"checkbox fullbright","Full bright");
            ImGui::TextDisabled("Transparency"); number(tools,"ColorTrans","##transparency");
            ImGui::TextDisabled("Glow"); number(tools,"glow","##glow");
            combo(tools,"combobox alphamode","Alpha mode");
            if(ImGui::BeginTable("##uv",2,ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn(); ImGui::TextDisabled("Repeat U"); number(tools,"TexScaleU","##u");
                ImGui::TableNextColumn(); ImGui::TextDisabled("Repeat V"); number(tools,"TexScaleV","##v");
                ImGui::EndTable();
            }
            ImGui::TextDisabled("Rotation"); number(tools,"TexRot","##rotation");
            }
            action(tools,"copy_face_btn","Copy"); ImGui::SameLine(); action(tools,"paste_face_btn","Paste");
        }
        else
        {
            action(tools,"button new script","New Script"); ImGui::SameLine();
            action(tools,"button new notecard","New Notecard");
            action(tools,"button permissions","Permissions..."); ImGui::SameLine();
            action(tools,"button refresh","Refresh");
            ImGui::Separator();
            if(auto* object=LLSelectMgr::getInstance()->getSelection()->getFirstObject())
            {
                LLInventoryObject::object_list_t objects; object->getInventoryContents(objects);
                for(const auto& item:objects) if(item->getType()!=LLAssetType::AT_CATEGORY)
                    ImGui::TextUnformatted(item->getName().c_str());
                if(objects.empty()) ImGui::TextDisabled("No contents loaded.");
            }
        }
        ImGui::Separator();
        if (manager->inBuildMode() && button("Exit Build Mode")) manager->leaveBuildMode();
        if (ImGui::BeginMenu("More Build Tools"))
        {
            if (ImGui::MenuItem("Landscape Designer")) stormOpen("fs_landscaper",StormSide::RightMid);
            if (ImGui::MenuItem("Landscape Painter")) stormOpen("fs_ai_planner",StormSide::RightMid);
            if (ImGui::MenuItem("Lighting and Weather")) stormOpen("nyx_lighting",StormSide::RightMid);
            ImGui::EndMenu();
        }
        ImGui::EndChild(); ImGui::PopFont(); end();
    }

    void pinnedFolderWindows(float width,float height)
    {
        int index=0;
        for(auto it=pinnedFolders.begin();it!=pinnedFolders.end();)
        {
            auto& [id,pin]=*it;
            auto* category=gInventory.getCategory(id);
            if(!category) {it=pinnedFolders.erase(it);continue;}
            const float panelWidth=std::min(330.f,width*.45f);
            if(pin.dockLeft || pin.dockRight)
                ImGui::SetNextWindowPos(ImVec2(pin.dockLeft?5.f:width-panelWidth-5.f,42.f));
            else ImGui::SetNextWindowPos(ImVec2(width*.25f+static_cast<float>(index)*18.f,80.f),ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(panelWidth,std::min(410.f,height*.55f)),ImGuiCond_FirstUseEver);
            bool keep=true;
            const std::string title=category->getName()+"###storm_pinned_"+id.asString();
            if(ImGui::Begin(title.c_str(),&keep,ImGuiWindowFlags_NoSavedSettings))
            {
                if(ImGui::SmallButton("Dock left")) {pin.dockLeft=true;pin.dockRight=false;}
                ImGui::SameLine();
                if(ImGui::SmallButton("Dock right")) {pin.dockRight=true;pin.dockLeft=false;}
                ImGui::SameLine();
                if(ImGui::SmallButton("Move")) {pin.dockLeft=false;pin.dockRight=false;}
                folder(id);
            }
            ImGui::End();
            if(keep) {++it;++index;}
            else it=pinnedFolders.erase(it);
        }
    }

    void copyChatText(const char* id,const std::string& text)
    {
        if(ImGui::BeginPopupContextItem(id))
        {
            if(ImGui::MenuItem("Copy text")) ImGui::SetClipboardText(text.c_str());
            ImGui::EndPopup();
        }
    }

    void conversationComposer(const LLUUID& id,bool nearby)
    {
        bool direct=false;
        if(nearby && FSClaudeIntegration::instanceExists() &&
           (!gSavedSettings.getBOOL("NyxAIDataConsent") || !gSavedSettings.getBOOL("FSCompanionEnabled")))
            FSClaudeIntegration::instance().setDirectAIMode(false);
        if(nearby && gSavedSettings.getBOOL("NyxAIDataConsent") &&
           gSavedSettings.getBOOL("FSCompanionEnabled") && FSClaudeIntegration::instanceExists())
        {
            auto& ai=FSClaudeIntegration::instance();
            direct=ai.isDirectAIMode();
        }
        auto& draft=conversationDrafts[id];char input[4096];snprintf(input,sizeof(input),"%s",draft.c_str());
        if(nearby && sStormChatInput[0]){snprintf(input,sizeof(input),"%s",sStormChatInput);sStormChatInput[0]=0;}
        LLChatEntry* entry=nullptr;
        if(nearby){if(auto* chat=FSFloaterNearbyChat::getInstance())entry=chat->getChatBox();}
        else if(auto* floater=FSFloaterIM::getInstance(id))entry=floater->findChild<LLChatEntry>("chat_editor");
        const bool enabled=direct || (entry && entry->getEnabled());ImGui::BeginDisabled(!enabled);
        if(sStormChatFocus && (nearby || selectedSession==id)){ImGui::SetKeyboardFocusHere();sStormChatFocus=false;}
        bool send=ImGui::InputTextMultiline("##message",input,sizeof(input),
            ImVec2(std::max(40.f,ImGui::GetContentRegionAvail().x-60.f),44.f),
            ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CtrlEnterForNewLine);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s. Ctrl+Enter adds a line.",nearby?(direct?"Message Ani":"Nearby chat"):"Write a message");
        draft=input;if(ImGui::IsItemActive()&&!nearby)selectedSession=id;
        if(ImGui::BeginPopupContextItem("##composer_clipboard"))
        {
            if(ImGui::MenuItem("Copy draft",nullptr,false,!draft.empty()))
                ImGui::SetClipboardText(draft.c_str());
            if(ImGui::MenuItem("Paste"))
            {
                if(const char* clipboard=ImGui::GetClipboardText())
                    draft.append(clipboard,std::min(std::strlen(clipboard),sizeof(input)-1-draft.size()));
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();send=button("Send",ImVec2(-1,25),true)||send;
        if(send && input[0] && enabled)
        {
            if(direct)
            {
                if(FSClaudeIntegration::instance().processChatCommand(input,CHAT_TYPE_NORMAL))
                    draft.clear();
            }
            else if(entry)
            {
                entry->setText(LLStringExplicit(input));
                entry->onCommit();
                draft=entry->getText();
            }
            selectedSession=id;sStormChatFocus=true;
        }
        ImGui::EndDisabled();
    }

    // ImGui renders text unclipped unless told otherwise, so long rows in a narrow
    // column bleed into the pane beside them. Cut to the column and mark the cut.
    std::string clipText(const std::string& text,float maxWidth)
    {
        if(maxWidth<=0.f || ImGui::CalcTextSize(text.c_str()).x<=maxWidth) return text;
        constexpr const char* dots="...";
        const float dotsWidth=ImGui::CalcTextSize(dots).x;
        size_t low=0,high=text.size();
        while(low<high)
        {
            const size_t mid=(low+high+1)/2;
            if(ImGui::CalcTextSize(text.substr(0,mid).c_str()).x+dotsWidth<=maxWidth) low=mid;
            else high=mid-1;
        }
        size_t cut=low;
        while(cut>0 && (static_cast<unsigned char>(text[cut])&0xC0)==0x80) --cut; // stay on a UTF-8 boundary
        return text.substr(0,cut)+dots;
    }

    std::string noticeTitle(const LLNotification& note)
    {
        // Script dialogs and toasts carry viewer markup (<nolink>, <b>, <icon>...).
        // ImGui prints tags literally, which made notice rows hugely wide.
        std::string message=plainNotice(note.getName()=="GroupNotice"?
            note.getSubstitutions()["SUBJECT"].asString():note.getMessage());
        LLStringUtil::trim(message);
        return message.substr(0,std::min<size_t>(message.find_first_of("\r\n"),96));
    }

    void chatNotifications()
    {
        std::vector<LLNotificationPtr> pending;
        auto& notifications=LLNotifications::instance();
        for(auto it=sStormRoutedNotifications.begin();it!=sStormRoutedNotifications.end();)
        {
            auto note=notifications.find(*it);
            if(!note || !note->isActive()) it=sStormRoutedNotifications.erase(it);
            else
            {
                pending.push_back(note);
                ++it;
            }
        }
        static LLUUID selectedNotice;
        static std::map<LLUUID,std::map<std::string,std::array<char,1024>>> replies;
        for(auto it=replies.begin();it!=replies.end();)
        {
            if(std::none_of(pending.begin(),pending.end(),[&](const auto& note){return note->id()==it->first;}))
                it=replies.erase(it);
            else ++it;
        }
        if(pending.empty())
        {
            selectedNotice.setNull();
            sStormNotificationHeight=0.f;
            ImGui::TextDisabled("You're all caught up.");
            return;
        }
        std::sort(pending.begin(),pending.end(),[](const auto& a,const auto& b){
            return b->getDate()<a->getDate();
        });
        auto selected=std::find_if(pending.begin(),pending.end(),[&](const auto& note){
            return note->id()==selectedNotice;
        });
        if(selected==pending.end()) selected=pending.begin();
        selectedNotice=(*selected)->id();
        const float width=ImGui::GetContentRegionAvail().x;
        const bool columns=width>=510.f;
        if(columns)
        {
            ImGui::BeginChild("##notice_inbox",ImVec2(std::min(280.f,width*.34f),0.f),ImGuiChildFlags_Borders);
            ImGui::TextDisabled("Inbox (%d)",static_cast<int>(pending.size()));
            const float rowWidth=ImGui::GetContentRegionAvail().x-ImGui::GetStyle().FramePadding.x*2.f;
            for(auto it=pending.begin();it!=pending.end();++it)
            {
                const auto& note=*it;
                ImGui::PushID(note->id().asString().c_str());
                if(ImGui::Selectable(clipText(noticeTitle(*note),rowWidth).c_str(),note->id()==selectedNotice))
                    selected=it;
                if(note->getName()=="GroupNotice")
                    ImGui::TextDisabled("%s",clipText(note->getSubstitutions()["GROUP"].asString(),rowWidth).c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::SameLine();
        }
        else if(ImGui::BeginCombo("##notice_inbox",noticeTitle(**selected).c_str()))
        {
            for(auto it=pending.begin();it!=pending.end();++it)
            {
                ImGui::PushID((*it)->id().asString().c_str());
                if(ImGui::Selectable(noticeTitle(**it).c_str(),it==selected)) selected=it;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        const auto& note=*selected;
        selectedNotice=note->id();
        ImGui::BeginChild("##notice_detail",ImVec2(0.f,0.f),ImGuiChildFlags_Borders);
        {
            ImGui::PushID(note->id().asString().c_str());
            const bool groupNotice=note->getName()=="GroupNotice";
            const auto& substitutions=note->getSubstitutions();
            if(groupNotice)
            {
                ImGui::TextWrapped("%s",noticeTitle(*note).c_str());
                ImGui::TextDisabled("%s  /  %s",substitutions["GROUP"].asString().c_str(),
                    substitutions["SENDER"].asString().c_str());
                ImGui::Separator();
            }
            auto form=note->getForm();
            const std::string textbox="!!llTextBox!!";
            const bool script=note->getName()=="ScriptDialog" || note->getName()=="ScriptDialogGroup";
            const bool scriptReply=script && form && form->getElement(textbox).isDefined();
            bool needsInput=false;
            if(form)
                for(S32 i=0;i<form->getNumElements();++i)
                {
                    const auto type=form->getElement(i)["type"].asString();
                    needsInput |= type!="button" && type!="text";
                }
            const ImGuiStyle& style=ImGui::GetStyle();
            // Reserve the room the controls below the message need. A fixed message height
            // clipped long notices and pushed the action buttons out of the pane.
            float actionsHeight=0.f;
            if(scriptReply) actionsHeight=25.f*2.f+style.ItemSpacing.y;
            else if(needsInput) actionsHeight=ImGui::GetTextLineHeight()+style.ItemSpacing.y;
            else if(form && !groupNotice)
            {
                for(S32 i=0;i<form->getNumElements();++i)
                {
                    const auto item=form->getElement(i);
                    if(item["type"].asString()!="text") continue;
                    if(!item["text"].asString().empty()) actionsHeight+=ImGui::GetTextLineHeight()+style.ItemSpacing.y;
                    actionsHeight+=25.f+style.ItemSpacing.y;
                }
                float rowWidth=0.f;
                int rows=0;
                const float available=ImGui::GetContentRegionAvail().x;
                for(S32 i=0;i<form->getNumElements();++i)
                {
                    const auto item=form->getElement(i);
                    if(item["type"].asString()!="button") continue;
                    const auto label=item["text"].asString();
                    const auto name=item["name"].asString();
                    const float buttonWidth=ImGui::CalcTextSize((label.empty()?name:label).c_str()).x+style.FramePadding.x*2.f;
                    if(rowWidth<=0.f || rowWidth+style.ItemSpacing.x+buttonWidth>available) { rows++; rowWidth=buttonWidth; }
                    else rowWidth+=style.ItemSpacing.x+buttonWidth;
                }
                if(rows) actionsHeight+=rows*25.f+(rows-1)*style.ItemSpacing.y+style.ItemSpacing.y;
            }
            const float messageHeight=groupNotice?
                std::max(65.f,ImGui::GetContentRegionAvail().y-86.f):
                std::max(48.f,ImGui::GetContentRegionAvail().y-actionsHeight);
            ImGui::BeginChild("##notice_message",ImVec2(0.f,messageHeight));
            // Markup like <nolink> is literal text to ImGui; strip it before wrapping.
            const auto body=plainNotice(groupNotice?substitutions["MESSAGE"].asString():note->getMessage());
            ImGui::TextWrapped("%s",body.c_str());
            copyChatText("##copy_notice",body);
            ImGui::EndChild();
            if(groupNotice)
            {
                const auto& payload=note->getPayload();
                const auto group=payload["group_id"].asUUID();
                const bool attached=payload["inventory_offer"].isDefined();
                bool dismiss=button("X##dismiss_notice");
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Dismiss notice");
                ImGui::SameLine();
                if(button("Group notices")) LLGroupActions::show(group,true);
                ImGui::SameLine();
                if(button("Group chat")) LLGroupActions::startIM(group);
                bool accept=false;
                if(attached)
                {
                    ImGui::TextWrapped("%s",payload["inventory_name"].asString().c_str());
                    accept=button("Accept attachment");
                }
                if(dismiss || accept)
                {
                    if(attached) (new LLOfferInfo(payload["inventory_offer"]))->forceResponse(accept?IOR_ACCEPT:IOR_DECLINE);
                    note->respond(note->getResponseTemplate());
                }
            }
            else if(form)
            {
                if(scriptReply)
                {
                    auto& reply=replies[note->id()][textbox];
                    ImGui::SetNextItemWidth(-1.f);
                    ImGui::InputText("##script_reply",reply.data(),reply.size());
                    if(button("Submit"))
                    {
                        LLSD response=note->getResponseTemplate();
                        response[textbox]=reply[0]?LLSD(std::string(reply.data())):LLSD(true);
                        note->respond(response);
                    }
                    ImGui::SameLine();
                    if(note->isActive() && button("Ignore")) note->respond(note->getResponseTemplate());
                }
                else if(needsInput) ImGui::TextDisabled("Complete this form in its original dialog.");
                else
                {
                    LLSD fields=note->getResponseTemplate();
                    for(S32 i=0;i<form->getNumElements();++i)
                    {
                        const auto item=form->getElement(i);
                        if(item["type"].asString()!="text") continue;
                        const auto name=item["name"].asString();
                        const auto label=item["text"].asString();
                        auto [field, inserted]=replies[note->id()].try_emplace(name);
                        auto& value=field->second;
                        if(inserted) snprintf(value.data(),value.size(),"%s",item["value"].asString().c_str());
                        if(!label.empty()) ImGui::TextWrapped("%s",label.c_str());
                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(-1.f);
                        const auto limit=item["max_length_chars"].asInteger();
                        const auto length=limit>0?std::min(value.size(),static_cast<size_t>(limit)+1):value.size();
                        ImGui::InputText("##response",value.data(),length);
                        ImGui::PopID();
                        fields[name]=std::string(value.data());
                    }
                    // Script menus (Block / Ignore / FT Light...) have many buttons; wrap them
                    // into rows instead of letting each one take a line of its own.
                    bool first=true;
                    const float rowRight=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
                    for(S32 i=0;i<form->getNumElements();++i)
                    {
                        const auto item=form->getElement(i);
                        if(item["type"].asString()!="button") continue;
                        const auto name=item["name"].asString();
                        const auto label=item["text"].asString();
                        const std::string text=label.empty()?name:label;
                        const float buttonWidth=ImGui::CalcTextSize(text.c_str()).x+style.FramePadding.x*2.f;
                        if(!first)
                        {
                            const float nextX=ImGui::GetItemRectMax().x+style.ItemSpacing.x+buttonWidth;
                            if(nextX<rowRight) ImGui::SameLine();   // otherwise it starts a new row
                        }
                        ImGui::PushID(i);
                        ImGui::BeginDisabled(item.has("enabled") && !item["enabled"].asBoolean());
                        const bool selected=button(text.c_str());
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        if(selected)
                        {
                            fields[name]=true;
                            note->respond(fields);
                            break;
                        }
                        first=false;
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        const bool outstanding=note->isActive() || pending.size()>1;
        sStormNotificationHeight=outstanding?325.f:0.f;
        if(!outstanding) nearbySelected=true;
    }

    void conversationInbox(LLIMModel& model)
    {
        const bool groups=conversationFilter==2;
        static bool choosingGroup=false;
        static char groupQuery[128]={};
        if(groups && ImGui::SmallButton(choosingGroup?"Back to group messages":"Join group chat"))
            choosingGroup=!choosingGroup;
        if(groups && choosingGroup)
        {
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputTextWithHint("##group_filter","Find one of your groups...",groupQuery,sizeof(groupQuery));
            ImGui::BeginChild("##join_group_list",ImVec2(0.f,0.f),ImGuiChildFlags_Borders,ImGuiWindowFlags_AlwaysVerticalScrollbar);
            for(const auto& group:gAgent.mGroups)
            {
                if(!matches(group.mName,groupQuery)) continue;
                ImGui::PushID(group.mID.asString().c_str());
                const auto name=clipText(group.mName,ImGui::GetContentRegionAvail().x-8.f);
                if(ImGui::Selectable(name.c_str()))
                {
                    const auto id=LLGroupActions::startIMInline(group.mID);
                    if(id.notNull())
                    {
                        selectedSession=id;
                        sStormChatFocus=true;
                        choosingGroup=false;
                        groupQuery[0]=0;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            return;
        }
        const bool all=conversationFilter==0;
        std::vector<std::pair<LLUUID,LLIMModel::LLIMSession*>> sessions;
        for(const auto& [id,session]:model.mId2SessionMap)
        {
            if(session && session->mSessionType!=LLIMModel::LLIMSession::NONE_SESSION &&
               (all || session->isGroupSessionType()==groups))
                sessions.emplace_back(id,session);
        }
        std::sort(sessions.begin(),sessions.end(),[](const auto& a,const auto& b){
            const bool unreadA=a.second->mNumUnread>0,unreadB=b.second->mNumUnread>0;
            if(unreadA!=unreadB) return unreadA;
            return LLStringUtil::compareDictInsensitive(a.second->mName,b.second->mName)<0;
        });
        auto findSelected=[&] {
            return std::find_if(sessions.begin(),sessions.end(),[](const auto& item){
                return item.first==selectedSession;
            });
        };
        const auto choose=[](const LLUUID& id){
            selectedSession=id;
            sStormChatFocus=true;
        };
        const bool wide=ImGui::GetContentRegionAvail().x>=580.f;
        if(wide)
        {
            const float listWidth=std::min(300.f,ImGui::GetContentRegionAvail().x*.36f);
            ImGui::BeginChild("##inbox_threads",ImVec2(listWidth,0.f),ImGuiChildFlags_Borders);
            ImGui::TextDisabled("%s  (%d)",all?"Conversations":groups?"Group inbox":"IM inbox",static_cast<int>(sessions.size()));
            const float rowWidth=ImGui::GetContentRegionAvail().x-ImGui::GetStyle().FramePadding.x*2.f;
            for(const auto& [id,session]:sessions)
            {
                ImGui::PushID(id.asString().c_str());
                const std::string suffix=session->mNumUnread>0?
                    "  ("+std::to_string(session->mNumUnread)+")":"";
                // Keep the unread count visible when a long name has to be shortened.
                const std::string title=clipText(session->mName,rowWidth-ImGui::CalcTextSize(suffix.c_str()).x)+suffix;
                if(ImGui::Selectable(title.c_str(),id==selectedSession)) choose(id);
                ImGui::Separator();
                ImGui::PopID();
            }
        }
        else
        {
            const auto active=findSelected();
            ImGui::SetNextItemWidth(-1.f);
            if(ImGui::BeginCombo("##inbox_threads",active==sessions.end()?"Select a conversation":active->second->mName.c_str()))
            {
                for(const auto& [id,session]:sessions)
                {
                    ImGui::PushID(id.asString().c_str());
                    const std::string title=session->mName+(session->mNumUnread>0?
                        "  ("+std::to_string(session->mNumUnread)+")":"");
                    if(ImGui::Selectable(title.c_str(),id==selectedSession)) choose(id);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        if(wide)
        {
            ImGui::EndChild();
            ImGui::SameLine();
        }

        ImGui::BeginChild("##inbox_detail",ImVec2(0.f,0.f),ImGuiChildFlags_Borders);
        const auto active=findSelected();
        if(active==sessions.end())
        {
            ImGui::TextDisabled(sessions.empty()?"Your inbox is empty.":"Select a conversation to read and reply.");
            ImGui::EndChild();
            return;
        }
        const auto& [id,session]=*active;
        ImGui::PushID(id.asString().c_str());
        if(ImGui::SmallButton("Back")) selectedSession.setNull();
        ImGui::SameLine();
        const ImGuiStyle& style=ImGui::GetStyle();
        const float closeWidth=ImGui::CalcTextSize("x").x+style.FramePadding.y*2.f;
        ImGui::TextUnformatted(clipText(session->mName,
            std::max(40.f,ImGui::GetContentRegionAvail().x-closeWidth-style.ItemSpacing.x)).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-closeWidth);
        if(ImGui::SmallButton("x"))
        {
            // Closing the thread leaves it and drops back to Nearby Chat.
            closeConversation=id;
            nearbySelected=true;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Close conversation");
        ImGui::Separator();
        ImGui::BeginChild("##history",ImVec2(0.f,-53.f));
        const bool bottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-4.f;
        static LLUUID lastThread;
        static size_t lastCount=0;
        int messageIndex=0;
        for(auto it=session->mMsgs.rbegin();it!=session->mMsgs.rend();++it)
        {
            ImGui::PushID(messageIndex++);
            ImGui::TextColored(ImVec4(.86f,.57f,.60f,1.f),"%s",(*it)["from"].asString().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s",(*it)["time"].asString().c_str());
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted((*it)["message"].asString().c_str());
            copyChatText("##copy_message",(*it)["message"].asString());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::PopID();
        }
        if(lastThread!=id || (bottom && lastCount!=session->mMsgs.size())) ImGui::SetScrollHereY(1.f);
        lastThread=id;
        lastCount=session->mMsgs.size();
        ImGui::EndChild();
        if(selectedSession==id)
        {
            conversationComposer(id,false);
            model.setActiveSessionID(id);
            if(session->mNumUnread) model.sendNoUnreadMessages(id);
        }
        ImGui::PopID();
        ImGui::EndChild();
    }

    void chatFooter();

    void chat(float width,float height)
    {
        auto& model=LLIMModel::instance();
        model.resetActiveSessionID();
        const auto bar=sStormToolbarRect;
        const bool split = stormSplitAvailable((S32)width, (S32)height);
        const int worldPane = split ? stormSplitPane(StormSplitContent::World) : -1;
        const bool showNearby = !split || worldPane >= 0;
        if (!showNearby && !dockInbox)
        {
            nearbyExpanded = false;
            sStormChatFocus = false;
        }
        const LLRect world = worldPane >= 0
            ? stormSplitBody((S32)width, (S32)height, worldPane) : LLRect();
        const float top = worldPane >= 0 ? height - world.mBottom - 32.f : bar.y - 31.f;
        const float chatWidth = std::min(worldPane >= 0 ? (float)world.getWidth() - 8.f : bar.z, 540.f);
        const float chatX = worldPane >= 0 ? world.mLeft + 4.f : bar.x + (bar.z - chatWidth) * .5f;
        bool hovered=false;
        if (showNearby)
        {
        ImGui::SetNextWindowPos(ImVec2(chatX,top));
        ImGui::SetNextWindowSize(ImVec2(chatWidth,30.f));
        constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(5.f,2.f));
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0,0,0));
        ImGui::Begin("##nearby_strip",nullptr,flags|ImGuiWindowFlags_NoBackground);
        if(ImGui::InvisibleButton("##activate_nearby",ImVec2(ImGui::GetContentRegionAvail().x,25.f)))
        {
            nearbyExpanded=!nearbyExpanded;
            if(nearbyExpanded) {dockInbox=0;sStormChatFocus=true;}
        }
        hovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        ImGui::End();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        }
        if(sStormChatFocus && !dockInbox) nearbyExpanded=true;
        if((showNearby && nearbyExpanded) || dockInbox)
        {
            const float panelWidth=dockInbox?std::min(650.f,width-16.f):chatWidth;
            const float panelHeight = std::min(dockInbox == 3 ? 325.f : 290.f,
                !dockInbox && worldPane >= 0 ? (float)world.getHeight() - 40.f : height * .48f);
            const float x=dockInbox?llclamp(inboxAnchors[dockInbox-1]-panelWidth*.5f,8.f,std::max(8.f,width-panelWidth-8.f)):chatX;
            ImGui::SetNextWindowPos(ImVec2(x,(dockInbox?bar.y:top)-panelHeight-4.f));
            ImGui::SetNextWindowSize(ImVec2(panelWidth,panelHeight));
            const char* titles[]={"Nearby Chat","IMs","Group Messages","Notifications"};
            bool opened=true;
            const std::string title=std::string(titles[dockInbox])+"###dock_chat_panel";
            if(!dockInbox)
            {
                ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive,ImVec4(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(0,0,0,0));
            }
            const bool nearbyOverlay=dockInbox==0;
            const ImGuiWindowFlags panelFlags=ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
                ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings;
            if(ImGui::Begin(title.c_str(),nearbyOverlay?nullptr:&opened,panelFlags|
                (nearbyOverlay?ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground:0)))
            {
                if(dockInbox==3) chatNotifications();
                else if(dockInbox)
                {
                    conversationFilter=dockInbox;
                    conversationInbox(model);
                }
                else
                {
                    ImGui::BeginChild("##nearby_history",ImVec2(0.f,-53.f));
                    const bool bottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-4.f;
                    static S32 generation=-1;
                    if(auto* nearby=FSFloaterNearbyChat::getInstance())
                        if(auto* history=nearby->findChild<LLTextBase>("chat_history"))
                        {
                            const auto message=history->getText();
                            ImGui::PushTextWrapPos(0.f);
                            ImGui::TextUnformatted(message.c_str());
                            copyChatText("##copy_nearby",message);
                            ImGui::PopTextWrapPos();
                            if(bottom && generation!=history->getTextGeneration()) ImGui::SetScrollHereY(1.f);
                            generation=history->getTextGeneration();
                        }
                    ImGui::EndChild();
                    conversationComposer(LLUUID::null,true);
                }
            }
            hovered|=ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            ImGui::End();
            if(nearbyOverlay) ImGui::PopStyleColor(4);
            if(!opened) {nearbyExpanded=false;dockInbox=0;sStormChatFocus=false;}
        }
        if(nearbyExpanded && !hovered && ImGui::IsMouseClicked(0) &&
           !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup))
            nearbyExpanded=false;
        if(closeConversation.notNull())
        {
            const LLUUID id=closeConversation;
            closeConversation.setNull();
            gIMMgr->leaveSession(id);
            conversationDrafts.erase(id);
            selectedSession.setNull();
        }
        sStormNotificationHeight=0.f;
    }

    #include "fsstormanimations.inl"

    void inboxTabs(float width)
    {
        std::erase_if(sStormRoutedNotifications,[](const LLUUID& id){
            const auto note=LLNotifications::instance().find(id);
            return !note || !note->isActive();
        });
        int counts[]={0,0,static_cast<int>(sStormRoutedNotifications.size())};
        for(const auto& [id,session]:LLIMModel::instance().mId2SessionMap)
            if(session && session->mSessionType!=LLIMModel::LLIMSession::NONE_SESSION)
                counts[session->isGroupSessionType()?1:0]+=session->mNumUnread;
        const char* labels[]={"IMs","Groups","Notifications"};
        const float fractions[]={.22f,.32f,.46f};
        for(int i=0;i<3;++i)
        {
            if(i) ImGui::SameLine(0.f,4.f);
            const auto pos=ImGui::GetCursorScreenPos();
            const float tabWidth=(width-8.f)*fractions[i];
            inboxAnchors[i]=pos.x+tabWidth*.5f;
            const bool alert=counts[i]>0 && (static_cast<int>(ImGui::GetTime()*2)%2)==0;
            const auto label=std::string(labels[i])+(counts[i] ? "  " + (counts[i]>99?std::string("99+"):std::to_string(counts[i])) : "");
            ImGui::PushID(i);
            constexpr float tabHeight=36.f;
            const bool pressed=ImGui::InvisibleButton("##inbox_tab",ImVec2(tabWidth,tabHeight));
            const bool selected=dockInbox==i+1;
            const bool hovered=ImGui::IsItemHovered();
            auto* draw=ImGui::GetWindowDrawList();
            const ImVec2 end=pos+ImVec2(tabWidth,tabHeight);
            draw->AddRectFilled(pos,end,Theme::col(selected ? Theme::AccentSoft : hovered ? Theme::Raised : Theme::SurfaceAlt),
                8.f);
            draw->AddRect(pos,end,Theme::col(selected||alert ? Theme::Accent : Theme::Border),
                8.f);
            const auto size=ImGui::CalcTextSize(label.c_str());
            draw->PushClipRect(pos+ImVec2(3.f,0.f),end-ImVec2(3.f,0.f),true);
            draw->AddText(pos+ImVec2(std::max(3.f,(tabWidth-size.x)*.5f),(tabHeight-size.y)*.5f),
                Theme::col(selected||alert ? Theme::AccentHover : Theme::Text),label.c_str());
            draw->PopClipRect();
            ImGui::PopID();
            if(pressed)
            {
                dockInbox=dockInbox==i+1?0:i+1;
                nearbyExpanded=false;
                sStormChatFocus=false;
            }
        }
    }

    void chatFooter()
    {
        ImGui::BeginChild("##chat_dock",ImVec2(0,0),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        StormVisitDiff::button();
        ImGui::SameLine(0.f, 4.f);
        if (stormDockIconButton("split_workspace", -1, Icon::OpenWindow,
            sStormSplitWorkspace ? "Exit split view" : "Split view", sStormSplitWorkspace))
            sStormSplitWorkspace = !sStormSplitWorkspace;
        ImGui::SameLine(0.f, 4.f);
        if(auto* dock=stormDock(StormSide::Chat); dock && dock->getFloaterCount()>0)
        {
            if(stormDockIconButton("docked_tools",33,Icon::OpenWindow,"Docked windows",dock->getVisible()))
                dock->setVisible(!dock->getVisible());
            ImGui::SameLine(0.f,4.f);
        }
        stormChatDockShortcuts();
        auto* bar=ImGui::GetCurrentWindow();
        if(ImGui::BeginDragDropTargetCustom(bar->InnerRect,bar->GetID("folder_drop")))
        {
            if(const auto* payload=ImGui::AcceptDragDropPayload("STORM_FOLDER"); payload && payload->DataSize==static_cast<int>(sizeof(LLUUID)))
            {
                LLUUID folderId;
                std::memcpy(folderId.mData,payload->Data,UUID_BYTES);
                pinnedFolders.try_emplace(folderId);
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine(0.f,4.f);
        if(stormDockIconButton("mesh_editor",12,Icon::Crop,"Mesh Editor"))
            stormOpen("fs_mesh_edit",StormSide::RightMid);
        ImGui::SameLine(0.f,4.f);
        if(stormDockIconButton("nyx_painter",13,Icon::Color,"Nyx Painter"))
            stormOpen("fs_paint",StormSide::RightMid);
        ImGui::SameLine(0.f,4.f);
        if(stormDockIconButton("landscape_painter",14,Icon::Brush,"Landscape Painter"))
            stormOpen("fs_ai_planner",StormSide::RightMid);
        ImGui::SameLine(0,8);
        struct Tool { const char* name; const char* tip; unsigned icon; };
        static const Tool actions[]={
            {"Movement Controls","Movement",Icon::Walk},
            {"Camera Controls","Camera",Icon::CameraCtl},
            {"Take Snapshot","Snapshot",Icon::Camera},
            {"Gestures","Gestures",Icon::People},
            {"Stand up","Stand up",Icon::Contact}
        };
        for(int i=0;i<5;++i)
        {
            if(i) ImGui::SameLine(0,4);
            if(stormDockIconButton(actions[i].name,15+i,actions[i].icon,actions[i].tip)) stormAction(actions[i].name);
        }
        ImGui::SameLine(0,4);

        const bool muted=gSavedSettings.getBOOL("MuteAudio");
        if(stormDockIconButton("sound",20,Icon::Volume,"Sound and volume",!muted))
            ImGui::OpenPopup("##sound_mixer");
        if(ImGui::BeginPopup("##sound_mixer"))
        {
            ImGui::TextUnformatted("Sound");
            ImGui::Separator();
            bool muteAll=muted;
            if(ImGui::Checkbox("Mute all",&muteAll))
                gSavedSettings.setBOOL("MuteAudio",muteAll);
            struct Channel { const char* label; const char* setting; };
            static const Channel channels[]={
                {"Master","AudioLevelMaster"},{"Sounds","AudioLevelSFX"},
                {"Voice","AudioLevelVoice"},{"Music","AudioLevelMusic"},
                {"Media","AudioLevelMedia"},{"Ambient","AudioLevelAmbient"}
            };
            for(const auto& channel:channels)
            {
                float volume=gSavedSettings.getF32(channel.setting);
                ImGui::SetNextItemWidth(155.f);
                if(ImGui::SliderFloat(channel.label,&volume,0.f,1.f,"%.2f"))
                    gSavedSettings.setF32(channel.setting,volume);
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine(0,4);
        auto* media=LLViewerMedia::getInstance();
        if(stormDockIconButton("music",21,Icon::Volume,media->isParcelAudioPlaying()?"Pause music":"Play music",media->isParcelAudioPlaying())&&gStatusBar)gStatusBar->toggleStream(!media->isParcelAudioPlaying());
        ImGui::SameLine(0,4);
        const bool playing=media->isAnyMediaPlaying()||media->isParcelMediaPlaying();
        if(stormDockIconButton("media",22,Icon::Video,playing?"Pause media":"Play media",playing)&&gStatusBar)gStatusBar->toggleMedia(!playing);
        ImGui::SameLine(0,4);if(stormDockIconButton("preferences",23,Icon::Settings,"Preferences"))detail("preferences");
        if(gSavedSettings.getBOOL("NyxAIDataConsent") && gSavedSettings.getBOOL("FSCompanionEnabled") &&
           FSClaudeIntegration::instanceExists())
        {
            auto& ai=FSClaudeIntegration::instance();
            ImGui::SameLine(0.f,4.f);
            if(stormDockIconButton("direct_ani",25,Icon::Robot,"Talk directly to Ani",ai.isDirectAIMode()))
                ai.setDirectAIMode(!ai.isDirectAIMode());
        }
        ImGui::SameLine(0.f,4.f);
        if (stormDockIconButton("notebook",24,Icon::MapPin,"Spatial Notebook",StormNotebook::picking))
            ImGui::OpenPopup("Spatial Notebook");
        if (ImGui::BeginPopup("Spatial Notebook"))
        {
            ImGui::TextUnformatted("Spatial Notebook");
            StormNotebook::draw();
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.f,4.f);
        if (stormDockIconButton("ao",29,Icon::Walk,"Animation override",gSavedPerAccountSettings.getBOOL("UseAO")))
        {
            StormAnimations::prepareFolders();
            ImGui::OpenPopup("Animation Override");
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(336.f,0.f),ImVec2(360.f,380.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(13.f,12.f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize,1.f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg,Theme::col(Theme::Surface));
        ImGui::PushStyleColor(ImGuiCol_Text,Theme::col(Theme::Text));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled,Theme::col(Theme::TextDim));
        ImGui::PushStyleColor(ImGuiCol_Border,Theme::col(Theme::Border));
        ImGui::PushStyleColor(ImGuiCol_Button,Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,Theme::col(Theme::Raised));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,Theme::col(Theme::AccentSoft));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,Theme::col(Theme::SurfaceAlt));
        ImGui::PushStyleColor(ImGuiCol_CheckMark,Theme::col(Theme::Accent));
        if (ImGui::BeginPopup("Animation Override"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+310.f);
            StormAnimations::drawAO();
            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(9);
        ImGui::PopStyleVar(2);
        ImGui::SameLine(0.f,4.f);
        if (stormDockIconButton("dances",30,Icon::Walk,"Dances",StormAnimations::activeDance.notNull()))
        {
            StormAnimations::prepareFolders();
            ImGui::OpenPopup("Dances");
        }
        if (ImGui::BeginPopup("Dances"))
        {
            StormAnimations::drawDances();
            ImGui::EndPopup();
        }
        ImGui::EndChild();
    }
}

void stormPanelsDraw(float height)
{
    if(!sStormLeftMode) StormPanels::inventory(height);
    else StormPanels::hideInventory();
    StormPanels::contacts(height);
    StormPanels::appearance(height);
    StormPanels::build(height);

}

// Native dialogs are drawn before the shell. Cut their rectangles out of the
// Firestorm overlay so confirmations and detail editors remain visible and usable.
// Nyx's independent windows and their draw lists are deliberately excluded.
void stormClipNativeDialogs(float height)
{
    std::vector<ImVec4> exclusions;
    auto add=[&](LLView* view)
    {
        if(!view || !view->getVisible()) return;
        const LLRect r=view->calcScreenRect();
        exclusions.emplace_back((float)r.mLeft,height-r.mTop,(float)r.mRight,height-r.mBottom);
    };
    if(gFloaterView) for(auto* view:*gFloaterView->getChildList())
        if(dynamic_cast<LLFloater*>(view) && (!dynamic_cast<StormDock*>(view) || view==stormEditorDock())) add(view);
    add(gFocusMgr.getTopCtrl());
    if(exclusions.empty()) return;
    for(auto* window:ImGui::GetCurrentContext()->Windows)
    {
        if(!window->Active || window->Hidden) continue;
        const std::string name=window->Name;
        if(name.find("##firestorm_")!=0 && name.find("##storm_")!=0 &&
           name.find("##studio_header")!=0 && name.find("##studio_rail")!=0) continue;
        auto* list=window->DrawList;
        ImVector<ImDrawCmd> commands;
        for(const auto& command:list->CmdBuffer)
        {
            if(!command.ElemCount || command.UserCallback) { commands.push_back(command); continue; }
            std::vector<ImVec4> clips{command.ClipRect};
            for(const auto& cut:exclusions)
            {
                std::vector<ImVec4> remainder;
                for(const auto& r:clips)
                {
                    const float l=llmax(r.x,cut.x),t=llmax(r.y,cut.y),right=llmin(r.z,cut.z),b=llmin(r.w,cut.w);
                    if(l>=right || t>=b) { remainder.push_back(r); continue; }
                    if(r.y<t) remainder.emplace_back(r.x,r.y,r.z,t);
                    if(b<r.w) remainder.emplace_back(r.x,b,r.z,r.w);
                    if(r.x<l) remainder.emplace_back(r.x,t,l,b);
                    if(right<r.z) remainder.emplace_back(right,t,r.z,b);
                }
                clips.swap(remainder);
                if(clips.empty()) break;
            }
            for(const auto& clip:clips) { ImDrawCmd copy=command; copy.ClipRect=clip; commands.push_back(copy); }
        }
        list->CmdBuffer.swap(commands);
    }
}

void stormAISettingsDraw()
{
    if(!sStormAISettings)return;
    ImGui::SetNextWindowSize(ImVec2(570,620),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(300,80),ImGuiCond_FirstUseEver);
    if(ImGui::Begin("AI Backend Options##firestorm_ai",&sStormAISettings))nyxBrainSettingsPanel();
    ImGui::End();
}
