// Firestorm workspace adapters. Controllers remain alive, but are never shown.
// Their commits retain the viewer's validation, permissions and async replies.
namespace StormPanels
{
    LLVector3d mapCenter, mapTarget;
    float mapScale=96.f;
    int searchTab=0, placesTab=0;
    char mapQuery[128]={}, portalLabel[128]={};
    std::string portalDestination, portalDestinationName;
    std::string selectedPortal;
    int portalTab=0;
    bool snapPortalToWall=false;
    bool focusPortalDetails=false;
    bool movePortals=false;

    void horizontalTabs(const char* const* labels, int count, int& selected,
                        const unsigned* icons = nullptr)
    {
        if (!icons)
        {
            tabs(labels, count, selected);
            return;
        }
        const float room = ImGui::GetContentRegionAvail().x;
        constexpr float spacing = 5.f;
        float used = 0.f;
        for (int i = 0; i < count; ++i)
        {
            const float width = std::min(room, ImGui::CalcTextSize(labels[i]).x + 34.f);
            if (used > 0.f && used + spacing + width <= room)
            {
                ImGui::SameLine(0.f, spacing);
                used += spacing;
            }
            else used = 0.f;
            ImGui::PushID(i);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImVec2 size(width, 29.f);
            if (ImGui::InvisibleButton("##tab", size)) selected = i;
            auto* draw = ImGui::GetWindowDrawList();
            const bool active = selected == i;
            const auto background = active ? Theme::AccentSoft : ImGui::IsItemHovered() ? Theme::Raised : Theme::SurfaceAlt;
            draw->AddRectFilled(p, p + size, Theme::col(background), 6.f);
            draw->AddRect(p, p + size, Theme::col(active ? Theme::Accent : Theme::Border), 6.f);
            Icon::draw(draw, icons[i], p + ImVec2(14.f, 14.5f), 15.f,
                Theme::col(active ? Theme::AccentHover : Theme::Text));
            draw->AddText(p + ImVec2(26.f, (29.f - ImGui::GetTextLineHeight()) * 0.5f),
                Theme::col(Theme::Text), labels[i]);
            ImGui::PopID();
            used += width;
        }
        ImGui::Spacing();
    }

    void nativeList(LLScrollListCtrl* list,float height=145.f)
    {
        if(!list) return;
        ImGui::PushID(list); ImGui::BeginDisabled(!list->getEnabled());
        ImGui::BeginChild("##rows",ImVec2(0,height),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
        const auto rows=list->getAllData();
        if(rows.empty())
        {
            auto* comment=list->findChild<LLTextBase>("comment_text");
            ImGui::TextWrapped("%s",comment && !comment->getText().empty()?comment->getText().c_str():"No entries.");
        }
        ImGuiListClipper clip; clip.Begin((int)rows.size());
        while(clip.Step()) for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i)
        {
            auto* row=rows[i]; std::string label;
            for(int col=0;col<row->getNumColumns();++col)
            {
                auto* cell=row->getColumn(col);
                if(!cell || dynamic_cast<LLScrollListIcon*>(cell)) continue;
                const auto value=cell->getValue().asString();
                if(value.empty()) continue;
                if(!label.empty()) label+="   |   "; label+=value;
            }
            if(label.empty()) label=row->getValue().asString();
            ImGui::PushID(i);
            if(ImGui::Selectable(label.c_str(),row->getSelected()))
            {
                if(!ImGui::GetIO().KeyCtrl) list->deselectAllItems();
                list->setSelectedByValue(row->getValue(),!row->getSelected());
                list->onCommit();
            }
            ImGui::PopID();
        }
        ImGui::EndChild(); ImGui::EndDisabled(); ImGui::PopID();
    }

    // Reflow real controls vertically instead of squeezing their old fixed rectangles.
    void nativeControls(LLView* parent,const std::set<std::string>& excluded={})
    {
        if(!parent) return;
        std::vector<LLView*> children(parent->getChildList()->begin(),parent->getChildList()->end());
        std::stable_sort(children.begin(),children.end(),[](LLView* a,LLView* b){
            return a->getRect().mTop==b->getRect().mTop ? a->getRect().mLeft<b->getRect().mLeft : a->getRect().mTop>b->getRect().mTop;
        });
        for(auto* v:children)
        {
            if(!v->getVisible() || excluded.count(v->getName())) continue;
            ImGui::PushID(v); ImGui::BeginDisabled(!v->getEnabled());
            const auto& name=v->getName();
            if(auto* c=dynamic_cast<LLCheckBoxCtrl*>(v))
            {
                auto label=c->getLabel();
                if(label.empty()) label=name=="pg_all"?"General":name=="mature_all"?"Moderate":name=="adult_all"?"Adult":c->getToolTip();
                check(parent,name.c_str(),label.empty()?"##option":label.c_str());
            }
            else if(auto* c=dynamic_cast<LLTextureCtrl*>(v)) texture(parent,name.c_str(),c->getLabel().c_str());
            else if(auto* c=dynamic_cast<LLScrollListCtrl*>(v)) nativeList(c);
            else if(auto* c=dynamic_cast<LLComboBox*>(v)) combo(parent,name.c_str(),"");
            else if(auto* c=dynamic_cast<LLSpinCtrl*>(v)) number(parent,name.c_str(),"##number");
            else if(auto* c=dynamic_cast<LLSliderCtrl*>(v))
            {
                float value=c->getValueF32();
                for(auto* child:*c->getChildList()) if(auto* label=dynamic_cast<LLTextBase*>(child)) if(!label->getText().empty()) ImGui::TextUnformatted(label->getText().c_str());
                ImGui::SetNextItemWidth(-1);
                if(ImGui::SliderFloat("##slider",&value,c->getMinValue(),c->getMaxValue())) {c->setValue(value); c->onCommit();}
            }
            else if(auto* c=dynamic_cast<LLLineEditor*>(v)) field(parent,name.c_str(),"");
            else if(auto* c=dynamic_cast<LLTextEditor*>(v))
            {
                if(c->getReadOnly()) ImGui::TextWrapped("%s",c->getText().c_str());
                else
                {
                    static std::map<LLView*,std::string> edits;
                    auto inserted=edits.emplace(c,c->getText()); auto& edit=inserted.first->second;
                    std::vector<char> buffer(std::max<size_t>(8192,edit.size()+1024),0);
                    std::copy(edit.begin(),edit.end(),buffer.begin());
                    ImGui::InputTextMultiline("##text",buffer.data(),buffer.size(),ImVec2(-1,80));
                    if(ImGui::IsItemActive()) edit=buffer.data();
                    if(ImGui::IsItemDeactivatedAfterEdit()) {c->setValue(edit); c->onCommit();}
                    if(!ImGui::IsItemActive()) edit=c->getText();
                }
            }
            else if(auto* c=dynamic_cast<LLButton*>(v))
            {
                auto label=c->getLabelUnselected();
                if(label.empty()) label=c->getToolTip();
                if(name=="map_btn")
                {
                    if(button(label.c_str())) if(auto* search=LLFloaterReg::findTypedInstance<FSFloaterSearch>("search"))
                    {mapTarget=search->getWorkspacePosition();mapCenter=mapTarget;sStormLeftMode=3;sStormRailSelection="Map";}
                }
                else if(!label.empty() && label!="About Land..." && name!="help_btn") action(parent,name.c_str(),label.c_str());
            }
            else if(auto* c=dynamic_cast<LLTextBase*>(v))
            {
                if(!c->getText().empty()) ImGui::TextWrapped("%s",c->getText().c_str());
            }
            else nativeControls(v,excluded);
            ImGui::EndDisabled(); ImGui::PopID();
        }
    }

    void landObjects(LLPanel* panel)
    {
        ImGui::SeparatorText("Parcel usage");
        for(const char* name:{"objects_available","object_contrib_text","total_objects_text","selected_objects_text"})
            if(auto* value=panel->findChild<LLTextBase>(name))
                ImGui::TextWrapped("%s",value->getText().c_str());

        ImGui::SeparatorText("Object owners");
        action(panel,"Refresh List","Refresh owners");
        nativeList(panel->findChild<LLScrollListCtrl>("owner list"),110.f);
        action(panel,"Return objects...","Return selected owner's objects");

        ImGui::SeparatorText("Highlight and return");
        const char* counts[]={"owner_objects_text","group_objects_text","other_objects_text"};
        const char* show[]={"ShowOwner","ShowGroup","ShowOther"};
        const char* returns[]={"ReturnOwner...","ReturnGroup...","ReturnOther..."};
        for(int i=0;i<3;++i)
        {
            ImGui::PushID(i);
            if(auto* value=panel->findChild<LLTextBase>(counts[i]))
                ImGui::TextWrapped("%s",value->getText().c_str());
            action(panel,show[i],"Show in-world");
            ImGui::SameLine();
            action(panel,returns[i],"Return...");
            ImGui::PopID();
        }
        ImGui::SeparatorText("Auto-return");
        field(panel,"clean other time","Minutes (0 disables auto-return)");
    }

    void landExperiences(LLPanel* panel)
    {
        const char* names[]={"panel_allowed","panel_blocked"};
        const char* titles[]={"Allowed experiences","Blocked experiences"};
        for(int i=0;i<2;++i)
        {
            auto* editor=panel->findChild<LLPanel>(names[i]);
            if(!editor) continue;
            ImGui::PushID(i);
            ImGui::SeparatorText(titles[i]);
            if(auto* count=editor->findChild<LLTextBase>("text_count"))
                ImGui::TextDisabled("%s",count->getText().c_str());
            nativeList(editor->findChild<LLScrollListCtrl>("experience_list"),90.f);
            action(editor,"btn_add","Add...");
            ImGui::SameLine();
            action(editor,"btn_remove","Remove");
            ImGui::SameLine();
            action(editor,"btn_profile","Profile");
            ImGui::PopID();
        }
    }

    void landOptions(LLPanel* panel)
    {
        if(!panel) return;
        auto labelOf=[&](LLView* v,const char* fallback){
            if(auto* c=dynamic_cast<LLCheckBoxCtrl*>(v))
            {
                const std::string text=c->getLabel();
                if(!text.empty()) return text;
            }
            return std::string(fallback);
        };
        auto option=[&](const char* name){
            auto* c=panel->findChild<LLCheckBoxCtrl>(name);
            if(!c) return;
            const std::string label=labelOf(c,name);
            check(panel,name,label.c_str());
        };
        // Label + option rows from the About Land options page, laid out so every option is
        // visible: the old one-control-per-line flow separated labels from their checkboxes.
        const float labelColumn=std::min(132.f,ImGui::GetContentRegionAvail().x*.4f);
        auto row=[&](const char* title,std::initializer_list<const char*> names){
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(title);
            ImGui::SameLine(labelColumn);
            bool first=true;
            for(const char* name:names)
            {
                if(!first) ImGui::SameLine(0,10);
                first=false;
                option(name);
            }
        };

        ImGui::TextUnformatted("Allow other Residents to:");
        if(auto* caution=panel->findChild<LLTextBase>("caution_text"))
            if(!caution->getText().empty()) ImGui::TextDisabled("%s",caution->getText().c_str());
        row("Edit Terrain:",{"edit land check"});
        row("Fly:",{"check fly"});
        row("Build:",{"edit objects check","edit group objects check"});
        row("Object Entry:",{"all object entry check","group object entry check"});
        row("Run Scripts:",{"check other scripts","check group scripts"});

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        option("check safe");
        ImGui::SameLine(0,12);
        option("PushRestrictCheck");
        option("ShowDirectoryCheck");
        combo(panel,"land category","Category:");
        option("MatureCheck");
        std::string seeLabel;
        if(auto* hint=panel->findChild<LLTextBase>("allow_see_label")) seeLabel=hint->getText();
        if(seeLabel.empty()) seeLabel="Avatars on other parcels can see and chat with avatars on this parcel";
        check(panel,"SeeAvatarsCheck",seeLabel.c_str());

        ImGui::SeparatorText("Snapshot");
        texture(panel,"snapshot_ctrl","Change picture");
        if(auto* landing=panel->findChild<LLTextBase>("landing_point"))
            if(!landing->getText().empty()) ImGui::TextUnformatted(landing->getText().c_str());
        action(panel,"Set","Set");
        ImGui::SameLine();
        action(panel,"Clear","Clear");
        ImGui::SameLine();
        action(panel,"teleport_to_landing_point","Teleport");
        combo(panel,"landing type","Teleport Routing:");
    }

    void land(float height)
    {
        static bool wasVisible=false;
        if(sLandPanelHidden) { wasVisible=false; return; }
        sStormPanelVisible[4]=true;
        // Opening the panel starts on the parcel under the avatar; after that the panel
        // follows whatever parcel is selected so another parcel can be inspected.
        if(!wasVisible)
        {
            wasVisible=true;
            if(auto* mgr=LLViewerParcelMgr::getInstance()) mgr->selectParcelAt(gAgent.getPositionGlobal());
        }
        const auto info=landInfo();
        const std::string title="Land Management - "+info.region;
        if(!begin(4,title.c_str(),Icon::Layers,height)) return;
        if(!sStormPanelVisible[4]) sLandPanelHidden=true;
        const char* labels[]={"General","Options","Access","Media","Environment","Objects","Covenant","Experiences","Local Land"};
        sLandTab=std::min(sLandTab,8);
        const unsigned icons[] = {Icon::Home, Icon::Settings, Icon::People, Icon::Video,
            Icon::Globe, Icon::Package, Icon::Edit, Icon::Star, Icon::MapPin};
        horizontalTabs(labels, 9, sLandTab, icons);
        ImGui::BeginChild("##land_content",ImVec2(0,0));
        auto* controller=LLFloaterReg::getTypedInstance<LLFloaterLand>("about_land");
        static LLUUID shownRegion; static S32 shownParcel=-1;
        auto* parcelManager=LLViewerParcelMgr::getInstance();
        auto* agentRegion=gAgent.getRegion();
        const bool refreshParcel=button("Use current parcel");
        ImGui::SameLine();
        const auto position=gAgent.getPositionAgent()-(agentRegion?agentRegion->getOriginAgent():LLVector3::zero);
        ImGui::TextDisabled("%.0f, %.0f, %.0f",position.mV[0],position.mV[1],position.mV[2]);
        if(controller && parcelManager)
        {
            // "Use current parcel" is the only thing that snaps the selection to the ground
            // under the avatar. Otherwise the panel follows whatever parcel is selected, so
            // inspecting another parcel no longer keeps showing the avatar's own parcel.
            if(refreshParcel || parcelManager->selectionEmpty())
            {
                parcelManager->selectParcelAt(gAgent.getPositionGlobal());
                shownRegion.setNull(); shownParcel=-1;
            }
            LLParcelSelectionHandle selection=parcelManager->getParcelSelection();
            LLParcel* selected=selection ? selection->getParcel() : nullptr;
            auto* selectionRegion=parcelManager->getSelectionRegion();
            const LLUUID regionId=selectionRegion ? selectionRegion->getRegionID() : LLUUID::null;
            const S32 parcelId=selected ? selected->getLocalID() : -1;
            if(regionId!=shownRegion || parcelId!=shownParcel)
            {
                shownRegion=regionId; shownParcel=parcelId;
                controller->onOpen(LLSD());
            }
        }
        if(!info.valid) ImGui::TextWrapped("Land information is available once you are in-world.");
        else if(sLandTab>=8)
        {
            auto* ground=LLFloaterReg::getInstance("fs_my_land_ground");
            static LLUUID groundRegion; static bool initialized=false;
            if(ground && (!initialized || groundRegion!=shownRegion)) {ground->onOpen(LLSD());groundRegion=shownRegion;initialized=true;}
            bool hideOtherLand = gSavedSettings.getBOOL("NyxJustMyLand");
            if (ImGui::Checkbox("Hide land", &hideOtherLand))
            {
                gSavedSettings.setBOOL("NyxJustMyLand", hideOtherLand);
                if (!hideOtherLand) fsJustMyLandPickParcels(false);
            }
            ImGui::SeparatorText("Ground textures");
            {
                check(ground,"ground_on","Use custom ground textures");
                action(ground,"ground_this_parcel","Use standing parcel for textures");
                ImGui::TextWrapped("Ground textures for your local parcel surface");
                for(int i=0;i<4;++i) {const std::string key="ground_tex_"+std::to_string(i); texture(ground,key.c_str(),("Layer "+std::to_string(i+1)).c_str());}
                action(ground,"ground_copy_region","Use region textures");
                action(ground,"ground_reset","Reset ground textures");
            }
            {
                ImGui::SeparatorText("Hide scope");
                int scope=gSavedSettings.getS32("NyxHideScope");
                const bool allLand=ImGui::RadioButton("Everything but my land",&scope,0);
                const bool pickedLand=ImGui::RadioButton("Only parcels I pick",&scope,1);
                if(allLand || pickedLand)
                {
                    gSavedSettings.setS32("NyxHideScope",scope);
                    if(scope==0) fsJustMyLandPickParcels(false);
                }
                ImGui::SeparatorText("Hidden ground");
                int mode=gSavedSettings.getS32("NyxHideGround");
                if(ImGui::RadioButton("Keep the ground",&mode,0))
                    gSavedSettings.setS32("NyxHideGround",mode);
                if(ImGui::RadioButton("Sink under the sea",&mode,1))
                    gSavedSettings.setS32("NyxHideGround",mode);
                bool cut=mode==2;
                if(ImGui::Checkbox("Cut the ground away",&cut))
                    gSavedSettings.setS32("NyxHideGround",cut?2:0);
                bool water=gSavedSettings.getBOOL("NyxHideWater");
                if(ImGui::Checkbox("Hide all water",&water))
                    gSavedSettings.setBOOL("NyxHideWater",water);
                const bool picking=fsJustMyLandPicking();
                if(button(picking?"Finish picking":"Pick parcels..."))
                    fsJustMyLandPickParcels(!picking);
                action(ground,"hide_clear","Show picked again");
            }
        }
        else if(controller)
        {
            if(sLandTab==0)
            {
                const float room = ImGui::GetContentRegionAvail().x;
                const bool sideBySide = room >= 570.f;
                const float heroWidth = sideBySide ? room * 0.64f : room;
                ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::col(Theme::SurfaceAlt));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 9));
                ImGui::BeginChild("##parcel_summary", ImVec2(heroWidth, 142.f), ImGuiChildFlags_Borders);
                const float imageWidth = std::min(160.f, ImGui::GetContentRegionAvail().x * 0.39f);
                const ImVec2 p = ImGui::GetCursorScreenPos(), size(imageWidth, 105.f);
                const Tex image = previewTexture(info.snapshot, (int)imageWidth, 105);
                ImGui::Dummy(size);
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(p, p + size, Theme::col(Theme::Surface), 6.f);
                if (image)
                    draw->AddImageRounded(NyxUI::texRef(image), p, p + size, ImVec2(0,1), ImVec2(1,0), IM_COL32_WHITE, 6.f);
                else
                    draw->AddText(p + ImVec2(6, 38), MUTED, info.snapshot.isNull() ? "No snapshot" : "Loading...");
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextWrapped("%s", info.parcel.c_str());
                ImGui::TextWrapped("Owner  %s", info.owner.c_str());
                ImGui::TextWrapped("%s", info.area.c_str());
                ImGui::TextWrapped("%s", info.rating.c_str());
                if (button("Local Land", ImVec2(0, 23), true)) sLandTab = 8;
                ImGui::EndGroup();
                ImGui::EndChild();
                if (sideBySide) ImGui::SameLine();
                ImGui::BeginChild("##parcel_status", ImVec2(0, 142.f), ImGuiChildFlags_Borders);
                ImGui::SeparatorText("Parcel Status");
                const auto landSelection = parcelManager ? parcelManager->getParcelSelection() : LLParcelSelectionHandle();
                if (const auto* parcel = landSelection ? landSelection->getParcel() : nullptr)
                {
                    ImGui::TextUnformatted(parcel->getForSale() ? "For sale" : "Not for sale");
                    const int used = parcel->getSimWidePrimCount();
                    const int capacity = parcel->getSimWideMaxPrimCapacity();
                    if (capacity > 0)
                    {
                        const float percent = std::clamp((float)used / (float)capacity, 0.f, 1.f);
                        const std::string label = llformat("%d / %d", used, capacity);
                        ImGui::ProgressBar(percent, ImVec2(-1.f, 17.f), label.c_str());
                        ImGui::TextDisabled("%d available", std::max(capacity - used, 0));
                    }
                }
                ImGui::TextWrapped("Region  %s", info.region.c_str());
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor();
                ImGui::SeparatorText("Quick Actions");
                LLVector3d lower, upper;
                const bool selectedBounds = parcelManager && parcelManager->getSelection(lower, upper);
                ImGui::BeginDisabled(!selectedBounds);
                if (button("Show on Map"))
                {
                    mapTarget = (lower + upper) * 0.5;
                    mapCenter = mapTarget;
                    stormOpen("world_map", StormSide::Left);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (button("Region Tools")) stormOpen("region_info", StormSide::Right);
                ImGui::SeparatorText("Parcel Details");
            }
            const char* panels[]={"land_general_panel","land_options_panel","land_access_panel","land_media_panel","land_environment_panel","land_objects_panel","land_covenant_panel","land_experiences_panel"};
            auto* tabs=controller->findChild<LLTabContainer>("landtab");
            if(sLandTab<8)
            {
                if(tabs) tabs->selectTabByName(panels[sLandTab]);
                if(auto* panel=controller->findChild<LLPanel>(panels[sLandTab]))
                {
                    if(sLandTab==5) landObjects(panel);
                    else if(sLandTab==7) landExperiences(panel);
                    else if(sLandTab==1) landOptions(panel);
                    else nativeControls(panel);
                }
                if(sLandTab==3) if(auto* audio=controller->findChild<LLPanel>("land_audio_panel")) {ImGui::SeparatorText("Audio"); nativeControls(audio);}
            }
        }
        ImGui::EndChild(); end();
    }

    FSFloaterSearch* searchController(int category)
    {
        auto* controller=LLFloaterReg::getTypedInstance<FSFloaterSearch>("search");
        const char* names[]={"panel_ls_people","panel_ls_groups","panel_ls_places","panel_ls_events","panel_ls_land","panel_ls_classifieds"};
        if(controller) if(auto* tabs=controller->findChild<LLTabContainer>("ls_tabs"))
            if(!tabs->getCurrentPanel() || tabs->getCurrentPanel()->getName()!=names[category]) tabs->selectTabByName(names[category]);
        return controller;
    }

    void searchResults(int category,bool portal=false)
    {
        auto* controller=searchController(category);
        if(!controller) return;
        const char* keys[]={"people","groups","places","events","land","classifieds"};
        const std::string key=keys[category];
        auto* panel=controller->findChild<LLPanel>("panel_ls_"+key);
        static char queries[6][256]={};
        auto* input=controller->findChild<LLComboBox>(key+"_edit");
        if(input)
        {
            ImGui::SetNextItemWidth(std::max(80.f,ImGui::GetContentRegionAvail().x-78.f));
            bool run=ImGui::InputTextWithHint("##search",("Search "+key+"...").c_str(),queries[category],sizeof(queries[category]),ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            run=button("Search",ImVec2(70.f,25.f),true) || run;
            if(run) {input->setTextEntry(std::string(queries[category]));input->onCommit();}
        }
        if(ImGui::CollapsingHeader("Search filters"))
        {
            // The real input panel provides maturity, category and range filters.
            if(auto* filters=panel ? panel->findChild<LLPanel>("panel_ls_input") : nullptr)
            {
                if(category==2)
                {
                    check(filters,"pg_all","General");
                    check(filters,"mature_all","Moderate");
                    check(filters,"adult_all","Adult");
                }
                nativeControls(filters,category==2?
                    std::set<std::string>{key+"_edit","search_text","pg_all","mature_all","adult_all"}:
                    std::set<std::string>{key+"_edit","search_text"});
            }
            else if(category==2) combo(panel,"places_category","Category");
        }
        if(!input && panel) action(panel,(key+"_find").c_str(),"Search");
        ImGui::SeparatorText("Results");
        auto* results=controller->findChild<LLScrollListCtrl>("search_results_"+key);
        nativeList(results,std::max(100.f,ImGui::GetContentRegionAvail().y*.44f));
        action(panel,(key+"_back").c_str(),"Previous"); ImGui::SameLine(); action(panel,(key+"_next").c_str(),"Next");
        if(portal)
        {
            LLVector3d destination; std::string region;
            const bool ready=controller->getWorkspaceDestination(destination,region);
            if(auto* title=controller->findChild<LLUICtrl>("title")) ImGui::TextWrapped("%s",title->getValue().asString().c_str());
            ImGui::BeginDisabled(!ready);
            if(button("Use this destination",ImVec2(-1,25),true))
            {
                LLVector3 local((F32)fmod(destination.mdV[0],256.0),(F32)fmod(destination.mdV[1],256.0),(F32)destination.mdV[2]);
                if(auto* sim=LLWorldMap::getInstance()->simInfoFromPosGlobal(destination)) local=sim->getLocalPos(destination);
                portalDestination=LLSLURL(region,local).getSLURLString();
                portalDestinationName=region;
                if(auto* title=controller->findChild<LLUICtrl>("title")) portalDestinationName=title->getValue().asString();
            }
            ImGui::EndDisabled();
        }
        else if(auto* details=controller->findChild<LLPanel>("panel_ls_details")) nativeControls(details);
    }

    void map()
    {
        if(mapCenter.isExactlyZero()) mapCenter=gAgent.getPositionGlobal();
        ImGui::SetNextItemWidth(-1);
        bool find=ImGui::InputTextWithHint("##region","Find region...",mapQuery,sizeof(mapQuery),ImGuiInputTextFlags_EnterReturnsTrue);
        find=button("Find region")||find; ImGui::SameLine();
        if(button("My location")) mapCenter=gAgent.getPositionGlobal();
        if(find && mapQuery[0]) LLWorldMapMessage::getInstance()->sendNamedRegionRequest(mapQuery);
        if(mapQuery[0])
        {
            ImGui::BeginChild("##regions",ImVec2(0,85),ImGuiChildFlags_Borders);
            for(const auto& entry:LLWorldMap::getInstance()->getRegionMap())
            {
                auto* sim=entry.second;
                if(sim && matches(sim->getName(),mapQuery) && ImGui::Selectable(sim->getName().c_str()))
                {mapCenter=sim->getGlobalPos(LLVector3(sim->getSizeX()*.5f,sim->getSizeY()*.5f,30.f));mapTarget=mapCenter;}
            }
            ImGui::EndChild();
        }
        ImGui::SetNextItemWidth(-1); ImGui::SliderFloat("##zoom",&mapScale,24,256,"Zoom %.0f");
        ImGui::TextDisabled("Drag to pan. Click to choose a destination.");
        const ImVec2 p=ImGui::GetCursorScreenPos(), size(std::max(80.f,ImGui::GetContentRegionAvail().x),std::max(100.f,ImGui::GetContentRegionAvail().y-65));
        ImGui::InvisibleButton("##map",size);
        if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {mapCenter.mdV[0]-=ImGui::GetIO().MouseDelta.x*256/mapScale;mapCenter.mdV[1]+=ImGui::GetIO().MouseDelta.y*256/mapScale;}
        const auto drag=ImGui::GetMouseDragDelta(0,0);
        if(ImGui::IsItemHovered() && ImGui::IsMouseReleased(0) && drag.x*drag.x+drag.y*drag.y<16)
        {const auto m=ImGui::GetIO().MousePos;mapTarget=LLVector3d(mapCenter.mdV[0]+(m.x-p.x-size.x*.5)*256/mapScale,mapCenter.mdV[1]-(m.y-p.y-size.y*.5)*256/mapScale,30);}
        auto* dl=ImGui::GetWindowDrawList(); dl->PushClipRect(p,p+size,true); dl->AddRectFilled(p,p+size,IM_COL32(24,39,47,255));
        const int level=LLWorldMipmap::scaleToLevel(mapScale), span=256*(1<<(level-1));
        const double west=mapCenter.mdV[0]-size.x*.5*256/mapScale, south=mapCenter.mdV[1]-size.y*.5*256/mapScale;
        auto screen=[&](double x,double y){return p+ImVec2(size.x*.5f+(float)(x-mapCenter.mdV[0])*mapScale/256,size.y*.5f-(float)(y-mapCenter.mdV[1])*mapScale/256);};
        for(double y=floor(south/span)*span;y<south+size.y*256/mapScale+span;y+=span)
            for(double x=floor(west/span)*span;x<west+size.x*256/mapScale+span;x+=span)
            {
                if(x<0 || y<0) continue;
                auto tex=LLWorldMap::getInstance()->getObjectsTile((U32)(x/256),(U32)(y/256),level,true);
                if(tex) {tex->addTextureStats(mapScale*mapScale); if(auto handle=NyxUI::textureHandle(tex)) dl->AddImage(NyxUI::texRef(handle),screen(x,y+span),screen(x+span,y),ImVec2(0,1),ImVec2(1,0));}
            }
        static double requestedAt=-10; if(ImGui::GetTime()-requestedAt>2)
        {requestedAt=ImGui::GetTime(); LLWorldMap::getInstance()->updateRegions(std::max(0,(int)(west/256)),std::max(0,(int)(south/256)),std::max(0,(int)((west+size.x*256/mapScale)/256)),std::max(0,(int)((south+size.y*256/mapScale)/256)));}
        for(const auto& entry:LLWorldMap::getInstance()->getRegionMap())
        {auto* sim=entry.second;if(!sim)continue;const auto origin=sim->getGlobalOrigin();const auto a=screen(origin.mdV[0],origin.mdV[1]+sim->getSizeY()),b=screen(origin.mdV[0]+sim->getSizeX(),origin.mdV[1]);if(b.x<p.x||a.x>p.x+size.x||b.y<p.y||a.y>p.y+size.y)continue;dl->AddRect(a,b,IM_COL32(150,150,150,90));dl->PushClipRect(a,b,true);dl->AddText(a+ImVec2(3,3),TEXT,sim->getName().c_str());dl->PopClipRect();}
        const auto agent=gAgent.getPositionGlobal(); dl->AddCircleFilled(screen(agent.mdV[0],agent.mdV[1]),5,IM_COL32(100,230,130,255));
        if(!mapTarget.isExactlyZero()) dl->AddCircle(screen(mapTarget.mdV[0],mapTarget.mdV[1]),7,selectedColor(),0,3);
        dl->PopClipRect();
        auto* selected=LLWorldMap::getInstance()->simInfoFromPosGlobal(mapTarget);
        ImGui::TextWrapped("%s",selected?selected->getName().c_str():"Choose a region on the map");
        ImGui::BeginDisabled(!selected || selected->isDown());
        if(button("Teleport",ImVec2(-1,25),true)) gAgent.teleportViaLocation(mapTarget);
        ImGui::EndDisabled();
    }

    void portalDetails(const LLSD& item)
    {
        const auto id=item["id"].asString();
        ImGui::PushID(id.c_str());
        ImGui::SeparatorText("Placement");
        ImGui::TextWrapped("%s",item["region"].asString().c_str());
        float xyz[]={(float)item["x"].asReal(),(float)item["y"].asReal(),(float)item["z"].asReal()};
        ImGui::TextDisabled("Region coordinates (X / Y / Z)");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::DragFloat3("##position",xyz,.1f,0,0,"%.2f"))
        {
            LLSD changes;
            changes["x"]=xyz[0];
            changes["y"]=xyz[1];
            changes["z"]=xyz[2];
            stormPortalEdit(id,changes,false);
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) stormPortalEdit(id,LLSD(),true);
        float angle=(float)item["heading"].asReal()*RAD_TO_DEG;
        ImGui::SetNextItemWidth(-1);
        if(ImGui::SliderFloat("##rotation",&angle,-180,180,"Rotation %.0f�"))
        {
            LLSD changes;
            changes["heading"]=angle*DEG_TO_RAD;
            stormPortalEdit(id,changes,false);
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) stormPortalEdit(id,LLSD(),true);

        if(button("Edit in-world")) movePortals=true;
        ImGui::SameLine();
        if(button(snapPortalToWall?"Cancel wall snap":"Snap to wall"))
        {
            snapPortalToWall=!snapPortalToWall;
            movePortals=true;
        }
        if(snapPortalToWall) ImGui::TextWrapped("Click a wall in the world.");
        ImGui::SeparatorText("Shape");
        const char* shapes[]={"Ellipse","Doorway","Archway"};
        int shape=llclamp(item["shape"].asInteger(),0,2);
        ImGui::SetNextItemWidth(-1);
        if(ImGui::Combo("##shape",&shape,shapes,3))
        {
            LLSD changes;
            changes["shape"]=shape;
            stormPortalEdit(id,changes);
        }
        float size[]={(float)item["width"].asReal(),(float)item["height"].asReal()};
        ImGui::TextDisabled("Width / height (meters)");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::DragFloat2("##portal_size",size,.05f,.5f,10.f,"%.2f",ImGuiSliderFlags_AlwaysClamp))
        {
            LLSD changes;
            changes["width"]=size[0];
            changes["height"]=size[1];
            stormPortalEdit(id,changes,false);
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) stormPortalEdit(id,LLSD(),true);

        ImGui::SeparatorText("Appearance");
        const char* surfaces[]={"Classic swirl","Violet nebula","Tidal glass","Molten gold","Rose storm"};
        int surface=llclamp(item["texture_preset"].asInteger(),0,4);
        ImGui::SetNextItemWidth(-1.f);
        if(ImGui::Combo("##portal_surface",&surface,surfaces,5))
        {
            LLSD change;
            change["texture"]=LLUUID::null;
            change["texture_preset"]=surface;
            stormPortalEdit(id,change);
        }
        if(button("Inventory texture...")) ImGui::OpenPopup("##portal_texture");
        if(ImGui::BeginPopup("##portal_texture"))
        {
            static char filter[128]={};ImGui::InputTextWithHint("##filter","Filter inventory textures...",filter,sizeof(filter));
            if(item["texture"].asUUID().notNull() && button("Use selected preset"))
            {LLSD changes;changes["texture"]=LLUUID::null;stormPortalEdit(id,changes);ImGui::CloseCurrentPopup();}
            ImGui::BeginChild("##textures",ImVec2(320,240),ImGuiChildFlags_Borders);
            LLInventoryModel::cat_array_t cats;LLInventoryModel::item_array_t items;LLIsType type(LLAssetType::AT_TEXTURE);
            gInventory.collectDescendentsIf(gInventory.getRootFolderID(),cats,items,false,type);
            for(const auto& texture:items)if(matches(texture->getName(),filter))
            {
                ImGui::PushID(texture->getUUID().asString().c_str());
                if(ImGui::Selectable(texture->getName().c_str(),texture->getAssetUUID()==item["texture"].asUUID())){LLSD changes;changes["texture"]=texture->getAssetUUID();stormPortalEdit(id,changes);ImGui::CloseCurrentPopup();}
                ImGui::PopID();
            }
            ImGui::EndChild();ImGui::EndPopup();
        }

        ImGui::Spacing();
        if(button("Copy portal code"))
        {
            LLSD share;
            for(const char* key:{"label","dest","shape","width","height","texture_preset"}) share[key]=item[key];
            std::ostringstream code;
            code<<"NYXPORTAL1 ";
            LLSDSerialize::toNotation(share,code);
            ImGui::SetClipboardText(code.str().c_str());
            nyxToast("Portal code copied.",false);
        }
        ImGui::SameLine();
        if(button("Remove portal"))
        {
            nyxPortalRemove(id);
            selectedPortal.clear();
            snapPortalToWall=false;
        }
        ImGui::PopID();
    }

    void portals()
    {
        bool enabled=gSavedSettings.getBOOL("NyxPortals");
        if(ImGui::Checkbox("Enable portals",&enabled)) gSavedSettings.setBOOL("NyxPortals",enabled);

        if(button("Import portal code...")) ImGui::OpenPopup("##portal_import");
        if(ImGui::BeginPopup("##portal_import"))
        {
            static char code[2048]={};
            ImGui::SetNextItemWidth(340.f);
            ImGui::InputTextWithHint("##code","Paste the portal code",code,sizeof(code));
            if(button("Place imported portal here"))
            {
                const std::string value=code;
                constexpr std::string_view prefix="NYXPORTAL1 ";
                LLSD share;
                std::istringstream input(value.substr(std::min(value.size(),prefix.size())));
                if(value.compare(0,prefix.size(),prefix)==0 &&
                   LLSDSerialize::fromNotation(share,input,static_cast<llssize>(value.size()))>0 &&
                   share.isMap() && share["dest"].isString() && share["label"].isString() &&
                   nyxPortalAddHere(share["label"].asString(),share["dest"].asString()))
                {
                    const auto saved=nyxPortalsList();
                    selectedPortal=saved[static_cast<S32>(saved.size()-1)]["id"].asString();
                    LLSD shape;
                    for(const char* key:{"shape","width","height","texture_preset"})
                        if(share.has(key) && llfinite(share[key].asReal())) shape[key]=share[key];
                    stormPortalEdit(selectedPortal,shape);
                    portalTab=0;
                    code[0]=0;
                    ImGui::CloseCurrentPopup();
                }
                else nyxToast("Invalid portal code or this location cannot host a portal.",true);
            }
            ImGui::EndPopup();
        }
        const char* labels[]={"Saved portals","New portal"};
        horizontalTabs(labels,2,portalTab);
        if(portalTab==1)
        {
            ImGui::SeparatorText("Choose a destination");
            searchResults(2,true);
            if(portalDestination.empty()) return;
            ImGui::SeparatorText("Place portal");
            ImGui::TextWrapped("%s",portalDestinationName.c_str());
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##label","Portal name (optional)",portalLabel,sizeof(portalLabel));
            if(button("Place portal here",ImVec2(-1,26),true))
            {
                if(nyxPortalAddHere(portalLabel[0]?portalLabel:portalDestinationName,portalDestination))
                {
                    portalLabel[0]=0;
                    portalDestination.clear();
                    portalDestinationName.clear();
                    const auto saved=nyxPortalsList();
                    if(saved.size()>0) selectedPortal=saved[static_cast<S32>(saved.size()-1)]["id"].asString();
                    portalTab=0;
                }
                else nyxToast("The portal could not be created at this location.",true);
            }
            return;
        }

        const auto list=nyxPortalsList();
        if(list.size()==0)
        {
            ImGui::TextDisabled("No saved portals.");
            if(button("Create a portal")) portalTab=1;
            return;
        }
        ImGui::BeginDisabled(!enabled);
        ImGui::Checkbox("Move portals in-world",&movePortals);
        ImGui::EndDisabled();
        if(enabled && movePortals)
            ImGui::TextWrapped("Select a portal, then scroll over the world to adjust its depth.");

        static char filter[128]={};
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##saved_filter","Find a saved portal...",filter,sizeof(filter));
        ImGui::BeginChild("##saved_portals",ImVec2(0,100),ImGuiChildFlags_Borders);
        LLSD selected;
        for(auto it=list.beginArray();it!=list.endArray();++it)
        {
            const auto& item=*it;
            const auto id=item["id"].asString();
            const auto label=item["label"].asString()+" � "+item["region"].asString();
            if(matches(label,filter))
            {
                ImGui::PushID(id.c_str());
                if(ImGui::Selectable(label.c_str(),selectedPortal==id)) selectedPortal=id;
                ImGui::PopID();
            }
            if(selectedPortal==id) selected=item;
        }
        ImGui::EndChild();
        if(selected.isDefined())
        {
            if(focusPortalDetails)
            {
                ImGui::SetScrollHereY(0.f);
                focusPortalDetails=false;
            }
            portalDetails(selected);
        }
    }

    bool portalScroll(F32 delta)
    {
        if(delta==0.f || !movePortals || sStormLeftMode!=4 || !sStormPanelVisible[0] ||
           !gSavedSettings.getBOOL("NyxPortals") || selectedPortal.empty()) return false;
        auto* region=gAgent.getRegion();
        if(!region) return false;
        const LLSD list=nyxPortalsList();
        for(auto it=list.beginArray();it!=list.endArray();++it)
        {
            const auto& item=*it;
            if(item["id"].asString()!=selectedPortal || item["region"].asString()!=region->getName()) continue;
            const LLVector3 local(static_cast<F32>(item["x"].asReal()),static_cast<F32>(item["y"].asReal()),static_cast<F32>(item["z"].asReal()));
            const auto moved=local-LLViewerCamera::getInstance()->getAtAxis()*(delta*.35f);
            LLSD change;
            change["x"]=moved.mV[VX];
            change["y"]=moved.mV[VY];
            change["z"]=moved.mV[VZ];
            return stormPortalEdit(selectedPortal,change);
        }
        return false;
    }

    void portalWallPicked(const LLPickInfo& pick)
    {
        if(!snapPortalToWall || selectedPortal.empty() || sStormLeftMode!=4 || !sStormPanelVisible[0]) return;
        auto* region=gAgent.getRegion();
        LLVector3 normal=pick.mNormal;
        const auto object=pick.getObject();
        if(!region || !object || object->getRegion()!=region || fabsf(normal.mV[VZ])>.45f)
        {
            nyxToast("Choose a vertical object surface in this region.",true);
            return;
        }
        normal.mV[VZ]=0.f;
        if(normal.normalize()<.01f) return;
        const auto local=pick.mPosGlobal-region->getOriginGlobal()+LLVector3d(normal)*.08;
        LLSD change;
        change["x"]=local.mdV[VX];
        change["y"]=local.mdV[VY];
        change["z"]=local.mdV[VZ];
        change["heading"]=atan2f(normal.mV[VY],normal.mV[VX]);
        if(stormPortalEdit(selectedPortal,change)) snapPortalToWall=false;
    }

    void portalHandles(float height)
    {
        const bool editing=movePortals && sStormLeftMode==4 && sStormPanelVisible[0] && gSavedSettings.getBOOL("NyxPortals");
        stormPortalEditing(editing?"workspace":"");
        auto* region=gAgent.getRegion();if(!editing || !region)return;
        const LLSD list=nyxPortalsList();
        for(auto it=list.beginArray();it!=list.endArray();++it)
        {
            const auto& entry=*it;if(entry["region"].asString()!=region->getName())continue;
            const std::string id=entry["id"].asString();
            const LLVector3 local((F32)entry["x"].asReal(),(F32)entry["y"].asReal(),(F32)entry["z"].asReal());
            LLCoordGL screen;if(!LLViewerCamera::getInstance()->projectPosAgentToScreen(region->getOriginAgent()+local,screen,true))continue;
            const ImVec2 center((float)screen.mX,height-screen.mY);
            bool covered=false;
            for(int i=0;i<6;++i)if(sStormPanelRectsValid[i] && sStormPanelRects[i].pointInRect(screen.mX,screen.mY))covered=true;
            if(covered)continue;
            ImGui::SetNextWindowPos(center-ImVec2(22,22));ImGui::SetNextWindowSize(ImVec2(44,44));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
            ImGui::Begin(("##firestorm_portal_handle_"+id).c_str(),nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
            ImGui::InvisibleButton("##grab",ImVec2(44,44));
            const bool hovered=ImGui::IsItemHovered();
            const bool activated=ImGui::IsItemActivated();
            const bool active=ImGui::IsItemActive();
            const bool deactivated=ImGui::IsItemDeactivated();
            if(ImGui::IsItemClicked())
            {
                selectedPortal=id;
                portalTab=0;
            }
            if(ImGui::BeginPopupContextItem("##portal_actions"))
            {
                if(ImGui::MenuItem("Edit portal"))
                {
                    selectedPortal=id;
                    portalTab=0;
                    focusPortalDetails=true;
                }
                if(ImGui::MenuItem("Remove portal"))
                {
                    nyxPortalRemove(id);
                    if(selectedPortal==id) selectedPortal.clear();
                    snapPortalToWall=false;
                }
                ImGui::EndPopup();
            }
            auto* dl=ImGui::GetWindowDrawList();dl->AddCircle(center,18,selectedPortal==id?selectedColor():TEXT,24,3);
            dl->AddLine(center-ImVec2(10,0),center+ImVec2(10,0),TEXT,2);dl->AddLine(center-ImVec2(0,10),center+ImVec2(0,10),TEXT,2);
            if(hovered)ImGui::SetTooltip("%s: drag to move, scroll to adjust distance, right-click for actions",entry["label"].asString().c_str());
            static LLVector3d plane,offset;static LLVector3 normal;
            if(activated)
            {
                plane=region->getOriginGlobal()+LLVector3d(local);normal=LLViewerCamera::getInstance()->getAtAxis();
                LLVector3d hit;const auto mouse=ImGui::GetIO().MousePos;
                offset.setZero();if(gViewerWindow->mousePointOnPlaneGlobal(hit,ll_round(mouse.x),ll_round(height-mouse.y),plane,normal))offset=plane-hit;
            }
            if(active && ImGui::IsMouseDragging(0))
            {
                LLVector3d hit;const auto mouse=ImGui::GetIO().MousePos;
                if(gViewerWindow->mousePointOnPlaneGlobal(hit,ll_round(mouse.x),ll_round(height-mouse.y),plane,normal))
                {
                    hit+=offset;hit-=region->getOriginGlobal();LLSD changes;changes["x"]=hit.mdV[0];changes["y"]=hit.mdV[1];changes["z"]=hit.mdV[2];stormPortalEdit(id,changes,false);
                }
            }
            if(deactivated)stormPortalEdit(id,LLSD(),true);
            ImGui::End();ImGui::PopStyleVar();
        }
    }

    void worldPanels(float height)
    {
        land(height);
        portalHandles(height);
        if(!sStormLeftMode) return;
        const char* titles[]={"Inventory","Search","Places","World Map","Portals"};
        if(!begin(0,titles[sStormLeftMode],sStormLeftMode==3?Icon::MapPin:Icon::Search,height)) return;
        ImGui::BeginChild("##world_content",ImVec2(0,0));
        if(sStormLeftMode==1)
        {const char* tabs[]={"People","Groups","Places","Events","Land","Classifieds"};horizontalTabs(tabs,6,searchTab);searchResults(searchTab);}
        else if(sStormLeftMode==2)
        {
            const char* labels[]={"Search Places","Landmarks"};horizontalTabs(labels,2,placesTab);
            if(!placesTab) searchResults(2);
            else
            {
                LLInventoryModel::cat_array_t cats;LLInventoryModel::item_array_t items;LLIsType type(LLAssetType::AT_LANDMARK);
                gInventory.collectDescendentsIf(gInventory.getRootFolderID(),cats,items,false,type);
                static LLUUID selected;
                static char filter[128]={};
                ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##landmarks","Filter landmarks...",filter,sizeof(filter));
                ImGui::BeginChild("##landmark_rows",ImVec2(0,std::max(80.f,ImGui::GetContentRegionAvail().y-65)));
                for(const auto& item:items) if(matches(item->getName(),filter))
                {ImGui::PushID(item->getUUID().asString().c_str());if(ImGui::Selectable(item->getName().c_str(),selected==item->getUUID())) selected=item->getUUID();ImGui::PopID();}
                ImGui::EndChild();
                auto* item=gInventory.getItem(selected);LLVector3d destination;
                auto* landmark=item?gLandmarkList.getAsset(item->getAssetUUID()):nullptr;
                bool ready=landmark && landmark->getGlobalPos(destination);
                ImGui::BeginDisabled(!ready);if(button("Teleport",ImVec2(0,25),true)) gAgent.teleportViaLocation(destination);ImGui::EndDisabled();
                ImGui::SameLine();ImGui::BeginDisabled(!item);if(button("Show in Inventory")) revealItem(selected);ImGui::EndDisabled();
            }
        }
        else if(sStormLeftMode==3) map();
        else portals();
        ImGui::EndChild(); end();
    }
}
