// Firestorm panel placement, independent of the viewer's legacy floaters.
enum StormEdge { StormLeft,StormRight,StormTop,StormBottom,StormFloat,StormChat };
struct StormPlacement
{
    int edge=StormFloat;
    float x=180,y=140,w=360,h=320,weight=1;
};
StormPlacement sStormPlacement[6];
std::vector<int> sStormPanelOrder={0,1,2,3,4,5};
std::vector<int> sStormRailOrder;
float sStormLeftWidth=0,sStormRightWidth=0,sStormChatHeight=0,sStormChatWidth=0;
int sStormExpanded=-1,sStormDragging=-1,sStormCurrentPanel=-1;
ImVec2 sStormDragOffset;
ImVec4 sStormToolbarRect(0,0,0,0);
bool sStormDockLoaded=false;

bool stormPanelShown(int i) {return i!=5 && (i==4?!sLandPanelHidden:sStormPanelVisible[i]);}
void stormLoadPlacement();

void stormRevealPanel(int i)
{
    stormLoadPlacement();
    sStormPanelVisible[i]=true;if(i==4)sLandPanelHidden=false;
    if(sStormPlacement[i].edge==StormTop || sStormPlacement[i].edge==StormBottom) sStormExpanded=i;
}

void stormSavePlacement()
{
    for(int i=0;i<6;++i)
    {
        const auto& p=sStormPlacement[i]; const std::string key="firestorm.panel."+std::to_string(i)+".";
        setSettingFloat(key+"edge",(float)p.edge);setSettingFloat(key+"x",p.x);setSettingFloat(key+"y",p.y);
        setSettingFloat(key+"width",p.w);setSettingFloat(key+"height",p.h);setSettingFloat(key+"weight",p.weight);
    }
    setSettingFloat("firestorm.left_width",sStormLeftWidth);setSettingFloat("firestorm.right_width",sStormRightWidth);setSettingFloat("firestorm.chat_height",sStormChatHeight);
    setSettingFloat("firestorm.chat_width",sStormChatWidth);
    std::string order;for(int i:sStormPanelOrder) order+=std::to_string(i)+",";setSettingString("firestorm.panel_order",order);
}

std::vector<int> stormReadOrder(const std::string& key,int count)
{
    std::vector<int> result;std::istringstream stream(settingString(key,""));std::string word;
    while(std::getline(stream,word,','))
    {
        if(word.empty() || word.find_first_not_of("0123456789")!=std::string::npos || word.size()>2) continue;
        int i=atoi(word.c_str());if(i<count && std::find(result.begin(),result.end(),i)==result.end()) result.push_back(i);
    }
    for(int i=0;i<count;++i) if(std::find(result.begin(),result.end(),i)==result.end()) result.push_back(i);
    return result;
}

void stormLoadPlacement()
{
    if(sStormDockLoaded) return;sStormDockLoaded=true;
    const float weights[]={56,44,38,32,30,100};
    for(int i=0;i<6;++i)
    {
        auto& p=sStormPlacement[i];const std::string key="firestorm.panel."+std::to_string(i)+".";
        p.edge=llclamp(static_cast<int>(settingFloat(key+"edge",static_cast<float>(StormBottom))),static_cast<int>(StormLeft),static_cast<int>(StormChat));
        p.x=settingFloat(key+"x",180.f+i*25);p.y=settingFloat(key+"y",140.f+i*20);
        p.w=llclamp(settingFloat(key+"width",360),220.f,1600.f);p.h=llclamp(settingFloat(key+"height",320),140.f,1400.f);
        p.weight=llclamp(settingFloat(key+"weight",weights[i]),1.f,2000.f);
    }
    sStormLeftWidth=settingFloat("firestorm.left_width",0);sStormRightWidth=settingFloat("firestorm.right_width",0);sStormChatHeight=settingFloat("firestorm.chat_height",0);
    sStormChatWidth=settingFloat("firestorm.chat_width",0);
    sStormPanelOrder=stormReadOrder("firestorm.panel_order",6);sStormRailOrder=stormReadOrder("firestorm.rail_order",13);
}

void stormSetEdge(int index,int edge)
{
    if(edge==StormChat)
        for(int other=0;other<6;++other)
            if(other!=index && sStormPlacement[other].edge==StormChat)
                sStormPlacement[other].edge=StormBottom;
    auto& p=sStormPlacement[index];
    if(sStormPanelRectsValid[index])
    {
        const auto& r=sStormPanelRects[index];const float height=(float)gViewerWindow->getWindowRectScaled().getHeight();
        p.x=(float)r.mLeft;p.y=height-r.mTop;p.w=std::max(240.f,(float)r.getWidth());p.h=std::max(180.f,(float)r.getHeight());
    }
    p.edge=edge;sStormExpanded=-1;stormSavePlacement();
}

void stormPanelDragHandle(int index)
{
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Drag to move or dock. Right-click for placement options.");
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0,5) && sStormDragging<0)
    {
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        stormSetEdge(index,StormFloat);sStormDragging=index;
        auto& p=sStormPlacement[index];sStormDragOffset=ImVec2(std::min(mouse.x-p.x,p.w-20),std::min(std::max(mouse.y-p.y,10.f),24.f));
    }
    if(ImGui::BeginPopupContextItem("##placement"))
    {
        const char* names[]={"Dock left","Dock right","Dock above bottom bar","Float"};
        const int edges[]={StormLeft,StormRight,StormChat,StormFloat};
        for(int e=0;e<4;++e) if(ImGui::MenuItem(names[e],nullptr,sStormPlacement[index].edge==edges[e])) stormSetEdge(index,edges[e]);
        ImGui::EndPopup();
    }
}

void stormFinishPanel()
{
    const int index=sStormCurrentPanel;
    if(index<0) return;
    auto& placement=sStormPlacement[index];
    const ImVec2 size=ImGui::GetWindowSize();
    const ImVec2 origin=ImGui::GetWindowPos();
    const auto screen=gViewerWindow->getWindowRectScaled();
    constexpr float edgeWidth=14.f;
    const ImVec2 positions[]={{0,edgeWidth},{size.x-edgeWidth,edgeWidth},{edgeWidth,0},{edgeWidth,size.y-edgeWidth}};
    const ImVec2 sizes[]={{edgeWidth,size.y-2*edgeWidth},{edgeWidth,size.y-2*edgeWidth},
                         {size.x-2*edgeWidth,edgeWidth},{size.x-2*edgeWidth,edgeWidth}};
    for(int edge=0;edge<4;++edge)
    {
        if((placement.edge==StormLeft && edge==0) ||
           (placement.edge==StormRight && edge==1) ||
           ((placement.edge==StormBottom || placement.edge==StormChat) && edge==3) ||
           (placement.edge==StormTop && edge==2))
            continue;
        if(edge==2 && placement.edge==StormFloat) continue;
        if((placement.edge==StormLeft || placement.edge==StormRight) && edge>=2 &&
           std::count_if(sStormPanelOrder.begin(),sStormPanelOrder.end(),[&](int panel){
               return stormPanelShown(panel) && sStormPlacement[panel].edge==placement.edge;
           })<2) continue;
        ImGui::PushID(edge);
        ImGui::PushClipRect(origin,origin+size,false);
        ImGui::SetCursorPos(positions[edge]);
        ImGui::InvisibleButton("##resize_edge",ImVec2(std::max(4.f,sizes[edge].x),std::max(4.f,sizes[edge].y)));
        const bool horizontal=edge<2;
        const bool engaged=ImGui::IsItemHovered() || ImGui::IsItemActive();
        auto* draw=ImGui::GetWindowDrawList();
        const auto center=ImGui::GetItemRectMin()+ImGui::GetItemRectSize()*.5f;
        // A grip you can actually see and hit, instead of a two-pixel hairline.
        const ImVec2 grip=horizontal?ImVec2(3.f,28.f):ImVec2(28.f,3.f);
        draw->AddRectFilled(center-grip,center+grip,
            engaged?IM_COL32(248,154,160,255):IM_COL32(150,133,141,110),2.f);
        if(engaged)
            ImGui::SetMouseCursor(horizontal?ImGuiMouseCursor_ResizeEW:ImGuiMouseCursor_ResizeNS);

        static float grab=0.f;
        if(ImGui::IsItemActivated())
        {
            const ImVec2 mouse=ImGui::GetIO().MousePos;
            grab=horizontal?(edge==0?mouse.x-origin.x:(origin.x+size.x)-mouse.x)
                           :(edge==2?mouse.y-origin.y:(origin.y+size.y)-mouse.y);
        }
        if(ImGui::IsItemActive())
        {
            // Take the size from the mouse position rather than adding per-frame deltas, so
            // the edge tracks the cursor instead of drifting and jumping. The grab offset
            // keeps the edge from snapping when the press lands inside the handle.
            const ImVec2 mouse=ImGui::GetIO().MousePos;
            const float right=origin.x+size.x;
            const float bottom=origin.y+size.y;
            const float edgeX=edge==0?mouse.x-grab:mouse.x+grab;
            const float edgeY=edge==2?mouse.y-grab:mouse.y+grab;
            if(horizontal)
            {
                const float width=edge==0?right-edgeX:edgeX-origin.x;
                if(placement.edge==StormChat)
                    sStormChatWidth=llclamp(width,300.f,(float)screen.getWidth());
                else if(placement.edge==StormLeft)
                {
                    if(edge==1) sStormLeftWidth=llclamp(width+8,220.f,screen.getWidth()*.42f);
                }
                else if(placement.edge==StormRight)
                {
                    if(edge==0) sStormRightWidth=llclamp((float)screen.getWidth()-edgeX+4,240.f,screen.getWidth()*.42f);
                }
                else
                {
                    placement.w=llclamp(width,220.f,(float)screen.getWidth());
                    if(edge==0) placement.x=right-placement.w;
                }
            }
            else
            {
                const float height=edge==2?bottom-edgeY:edgeY-origin.y;
                if(placement.edge==StormChat)
                {
                    if(edge==2) sStormChatHeight=llclamp(height+8,140.f,screen.getHeight()*.5f);
                }
                else if(placement.edge==StormLeft || placement.edge==StormRight)
                {
                    float otherWeight=0.f;
                    for(int panel:sStormPanelOrder)
                        if(panel!=index && stormPanelShown(panel) && sStormPlacement[panel].edge==placement.edge)
                            otherWeight+=sStormPlacement[panel].weight;
                    const float available=std::max(120.f,(size.y+8.f)*(placement.weight+otherWeight)/placement.weight);
                    const float target=llclamp(height+8.f,60.f,available-60.f);
                    placement.weight=llclamp(otherWeight*target/(available-target),1.f,2000.f);
                }
                else
                {
                    placement.h=llclamp(height,140.f,(float)screen.getHeight());
                    if(edge==2) placement.y=bottom-placement.h;
                }
            }
        }
        if(ImGui::IsItemDeactivated()) { stormSavePlacement(); grab=0.f; }
        ImGui::PopClipRect();
        ImGui::PopID();
    }
    sStormCurrentPanel=-1;
}

void stormDockBars(float width,float height)
{
    if(sStormDragging<0)return;
    auto& p=sStormPlacement[sStormDragging];const auto mouse=ImGui::GetIO().MousePos;
    p.x=llclamp(mouse.x-sStormDragOffset.x,0.f,std::max(0.f,width-p.w));p.y=llclamp(mouse.y-sStormDragOffset.y,STORM_HEADER*1.f,std::max(STORM_HEADER*1.f,height-STORM_FOOTER-p.h));
    const auto& bar=sStormToolbarRect;
    const bool overBar=bar.z>0 && mouse.x>=bar.x-16&&mouse.x<=bar.x+bar.z+16&&mouse.y>=bar.y-20&&mouse.y<=bar.y+bar.w+20;
    const int edge=overBar?StormChat:mouse.x<STORM_RAIL+65?StormLeft:mouse.x>width-65?StormRight:StormFloat;
    auto* dl=ImGui::GetForegroundDrawList();const ImU32 color=IM_COL32(160,48,58,120);
    if(edge==StormChat)dl->AddRectFilled(ImVec2(bar.x,bar.y),ImVec2(bar.x+bar.z,bar.y+bar.w),color);
    if(edge==StormLeft)dl->AddRectFilled(ImVec2(STORM_RAIL,STORM_HEADER),ImVec2(STORM_RAIL+75,height-STORM_FOOTER),color);
    if(edge==StormRight)dl->AddRectFilled(ImVec2(width-75,STORM_HEADER),ImVec2(width,height-STORM_FOOTER),color);
    if(!ImGui::IsMouseDown(0))
    {
        const int index=sStormDragging;
        if(edge==StormChat) stormSetEdge(index,edge);
        else p.edge=edge;
        sStormPanelOrder.erase(std::remove(sStormPanelOrder.begin(),sStormPanelOrder.end(),index),sStormPanelOrder.end());
        auto at=sStormPanelOrder.end();
        if(edge==StormLeft || edge==StormRight)for(auto it=sStormPanelOrder.begin();it!=sStormPanelOrder.end();++it)
            if(sStormPlacement[*it].edge==edge && sStormPanelRectsValid[*it] && mouse.y<height-sStormPanelRects[*it].getCenterY()){at=it;break;}
        sStormPanelOrder.insert(at,index);sStormDragging=-1;stormSavePlacement();
    }
}
