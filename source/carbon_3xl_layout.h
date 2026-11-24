#pragma once
// Carbon 3XL has its own navigation rail, header and content canvas. It shares
// only the setting-page registry and navigation history with the other layouts.
static constexpr int carbonNavOrder[]={0,1,2,3,9,4,5,8,10,11,6,7};
static ImVec2 carbonNavLocal(int slot){return {10.f,86.f+slot*41.f};}
static ImVec2 carbonHeaderDragLocal(){return {480.f,70.f};}
static ImVec2 carbonSearchLocal(){return {ui::size.x-270.f,25.f};}
static float carbonSelectionY=-1.f,carbonSubX=-1.f,carbonSubW;
static int carbonSubCategory=-1;
static float carbonNavHover[12]={};
static float carbonStep(float v,float target,float rate=14.f){
    if(!std::isfinite(v)||MenuTheme::reduceMotion)return target;
    return ImLerp(v,target,1.f-std::exp(-rate*ImGui::GetIO().DeltaTime));
}
static ImU32 carbonInk(int gray,int alpha=255){return ImGui::GetColorU32(IM_COL32(gray,gray,gray,alpha));}
static void carbonText(ImDrawList *d,ImFont *face,ImVec2 at,ImU32 color,const char *text){d->AddText(face,face->FontSize,at,color,text);}
static void carbonCenterText(ImDrawList *d,ImFont *f,float fs,ImVec2 at,float width,ImU32 color,const char *text){
    ImVec2 sz=f->CalcTextSizeA(fs,FLT_MAX,0,text);d->AddText(f,fs,{at.x+(width-sz.x)*.5f,at.y},color,text);
}
static void carbonWordmark(ImDrawList *d,ImVec2 p){
    const char *name="baLONCUaga";auto *face=menuBold;float x=p.x;
    for(int i=0;name[i];++i){
        int grey=235;
        if(!MenuTheme::reduceMotion&&MenuTheme::logoStyle==MenuTheme::LogoShine)
            grey=static_cast<int>(180+75*(.5f+.5f*std::sin(menuEffectTime*1.7f-i*.5f)));
        else if(MenuTheme::logoStyle==MenuTheme::LogoGradient)grey=245-i*8;
        d->AddText(face,face->FontSize,{x,p.y},carbonInk(grey),name+i,name+i+1);
        x+=face->CalcTextSizeA(face->FontSize,FLT_MAX,0,name+i,name+i+1).x;
    }
}
static void carbonDragHeader(){
    ImGui::SetCursorPos({82,0});ImGui::InvisibleButton("##carbon header drag",{ui::size.x-82,82});
    if(ImGui::IsItemActive()&&!ImGui::IsItemActivated()&&ImGui::IsMouseDragging(0,0)){
        auto delta=ImGui::GetIO().MouseDelta;menuX+=delta.x*scale;menuY+=delta.y*scale;moving=true;
    }
    if(ImGui::IsItemDeactivated()&&moving){moving=false;clampPosition();savePosition();}
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}
// kind: 0 search, 1 close, 2 back, 3 forward. Geometry stays monochrome.
static bool carbonHeaderButton(const char *id,ImVec2 p,int kind,bool enabled,const char *tip){
    auto *d=ImGui::GetWindowDrawList();ImGui::SetCursorScreenPos(p);
    ImGui::PushID(id);bool hit=ImGui::InvisibleButton("action",{32,32})&&enabled;bool hot=enabled&&ImGui::IsItemHovered();ImGui::PopID();
    d->AddRectFilled(p,p+ImVec2(32,32),carbonInk(hot?42:22),8);d->AddRect(p,p+ImVec2(32,32),carbonInk(255,hot?70:24),8);
    ImVec2 c=p+ImVec2(16,16);ImU32 ink=carbonInk(enabled?(hot?255:196):73);
    if(kind==0){d->AddCircle(c-ImVec2(2,2),5.5f,ink,20,1.6f);d->AddLine(c+ImVec2(2,2),c+ImVec2(7,7),ink,1.6f);}
    else if(kind==1){d->AddLine(c-ImVec2(4,4),c+ImVec2(4,4),ink,1.6f);d->AddLine(c+ImVec2(4,-4),c+ImVec2(-4,4),ink,1.6f);}
    else{float dx=kind==2?3.f:-3.f;d->AddLine(c+ImVec2(dx,-5),c-ImVec2(dx,0),ink,1.6f);d->AddLine(c-ImVec2(dx,0),c+ImVec2(dx,5),ink,1.6f);}
    if(hot){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);if(tip&&*tip)ImGui::SetTooltip("%s",tip);}return hit;
}
static void carbonKey(ImDrawList *d,ImVec2 &p,const char *name,const char *hint){
    auto size=menuSmall->CalcTextSizeA(menuSmall->FontSize,FLT_MAX,0,name);float width=size.x+12;
    d->AddRectFilled(p,p+ImVec2(width,19),carbonInk(24),4);d->AddRect(p,p+ImVec2(width,19),carbonInk(255,28),4);
    carbonText(d,menuSmall,p+ImVec2(6,3),carbonInk(225),name);p.x+=width+7;
    carbonText(d,menuSmall,p+ImVec2(0,3),carbonInk(163),hint);p.x+=menuSmall->CalcTextSizeA(menuSmall->FontSize,FLT_MAX,0,hint).x+17;
}
static void carbonSearchPopup(ImVec2 origin){
    ImGui::SetNextWindowPos(origin+ImVec2(ui::size.x-436,72),ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({380,330},ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,16});
    if(ImGui::BeginPopup("Carbon search",ImGuiWindowFlags_NoMove)){
        ImGui::PushFont(menuBold);ImGui::TextUnformatted(L(u8"Найти функцию","Find a setting"));ImGui::PopFont();
        if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##carbon search",L(u8"Модели, радар, прицел...","Models, radar, crosshair..."),peachSearch,sizeof(peachSearch));
        ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
        ImGui::BeginChild("##carbon search results",{0,0},false,ImGuiWindowFlags_NoBackground);
        int matches=0;
        for(const auto &page:peachPages)if(!*peachSearch||peachMatches(page,peachSearch)){
            ++matches;ImGui::PushID(matches);
            if(ImGui::Selectable(peachName(page),false,0,{0,25})){auroraNavigate(page.tab,page.sub);ImGui::CloseCurrentPopup();}
            ImGui::PopID();
        }
        if(!matches)ImGui::TextDisabled("%s",L(u8"Ничего не найдено.","Nothing found."));
        ImGui::EndChild();ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}
static void carbon3XLLayout(){
    if(ui::tabs.empty())return;
    ui::next_tab=std::clamp(ui::next_tab,0,static_cast<int>(ui::tabs.size())-1);
    if(peachVisit<0||peachVisits[peachVisit].tab!=ui::next_tab||peachVisits[peachVisit].sub!=ui::tabs[ui::next_tab].next_subtab)
        auroraNavigate(ui::next_tab,ui::tabs[ui::next_tab].next_subtab);
    constexpr float mainX=104,bodyY=142;
    const float mainW=ui::size.x-128,bodyH=ui::size.y-bodyY-48;
    auto &io=ImGui::GetIO();clampPosition();
    float rise=MenuTheme::reduceMotion?0.f:menuMotion.offsetY();
    float top=std::clamp(menuY/scale+rise,8.f/scale,std::max(8.f/scale,io.DisplaySize.y-ui::size.y-8.f/scale));
    ImGui::SetNextWindowPos({menuX/scale,top},ImGuiCond_Always);ImGui::SetNextWindowSize(ui::size);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,1.f);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("baLONCUaga Carbon 3XL",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar|(menuRequested?0:ImGuiWindowFlags_NoInputs));
    ImVec2 origin=ImGui::GetWindowPos();auto *d=ImGui::GetWindowDrawList();auto *bg=ImGui::GetBackgroundDrawList();
    if(!outsideGame)bg->AddRectFilled({0,0},io.DisplaySize,carbonInk(0,75));
    if(MenuTheme::outerEffects&&MenuTheme::rimGlow)SneakerArt::surround(bg,origin,ui::size,menuEffectTime,menuMotion.progress,MenuTheme::particles,MenuTheme::reduceMotion);
    for(int i=6;i>=1;--i){float e=i*2.f;bg->AddRectFilled(origin+ImVec2(-e,3),origin+ui::size+ImVec2(e,e+3),carbonInk(0,9),16+e);}
    int backing=static_cast<int>(std::clamp(MenuTheme::windowOpacity,60,100)*2.55f);
    d->AddRectFilled(origin,origin+ui::size,carbonInk(9,backing),16);
    // Carbon fabric is confined to quiet surfaces behind the chrome, not text.
    SneakerArt::carbon(d,origin+ImVec2(12,84),origin+ImVec2(72,ui::size.y-76),MenuTheme::carbonStrength/100.f);
    SneakerArt::carbon(d,origin+ImVec2(104,11),origin+ImVec2(ui::size.x-24,78),MenuTheme::carbonStrength/100.f*.6f);
    d->AddRectFilled(origin+ImVec2(82,0),origin+ImVec2(83,ui::size.y),carbonInk(255,19));
    if(MenuTheme::frames)d->AddRect(origin,origin+ui::size,carbonInk(255,48),16,0,1.f);
    d->AddLine(origin+ImVec2(104,82),origin+ImVec2(ui::size.x-24,82),carbonInk(255,19));
    // The same 3XL shoe appears in the rail and the appearance previews.
    SneakerArt::shoe(d,origin+ImVec2(41,40),34,MenuTheme::reduceMotion?0.f:menuEffectTime,1.f);
    carbonWordmark(d,origin+ImVec2(mainX,19));
    ImVec2 authors=origin+ImVec2(mainX+1,43);
    auto author=[&](const char *name,ImU32 color){carbonText(d,menuSmall,authors,ImGui::GetColorU32(color),name);authors.x+=menuSmall->CalcTextSizeA(menuSmall->FontSize,FLT_MAX,0,name).x;};
    author("detoxy",IM_COL32(184,158,248,255));author(" & ",IM_COL32(164,168,177,255));author("weta",IM_COL32(112,214,233,255));
    d->AddLine(origin+ImVec2(300,22),origin+ImVec2(300,58),carbonInk(255,23));
    // A single sliding silver tile makes the rail feel like one control.
    int selectedSlot=0;for(int i=0;i<12;++i)if(carbonNavOrder[i]==ui::next_tab)selectedSlot=i;
    float target=carbonNavLocal(selectedSlot).y;
    carbonSelectionY=carbonSelectionY<0?target:carbonStep(carbonSelectionY,target,15);
    ImVec2 select=origin+ImVec2(10,carbonSelectionY);
    for(int i=3;i>=1;--i)d->AddRect(select-ImVec2(i,i),select+ImVec2(62+i,40+i),carbonInk(255,5),9+i);
    int first=d->VtxBuffer.Size;d->AddRectFilled(select,select+ImVec2(62,40),carbonInk(255),9);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,first,d->VtxBuffer.Size,select,select+ImVec2(0,40),IM_COL32(250,250,250,255),IM_COL32(197,197,197,255));
    static const char *ru[]={u8"Модели",u8"Камера",u8"Прицел","HUD",u8"Клавиши",u8"Колесо",u8"Чат","Stats","CS FIX",u8"Серверы",u8"Настройки",u8"Конфиг"};
    static const char *en[]={"Models","Camera","Crosshair","HUD","Keys","Wheel","Chat","Stats","CS FIX","Servers","Settings","Config"};
    for(int i=0;i<12;++i){int tab=carbonNavOrder[i];if(tab>=static_cast<int>(ui::tabs.size()))continue;
        ImVec2 p=origin+carbonNavLocal(i);ImGui::SetCursorScreenPos(p);ImGui::PushID(i+750);
        bool hit=ImGui::InvisibleButton("##carbon category",{62,40}),hot=ImGui::IsItemHovered();ImGui::PopID();
        bool active=ui::next_tab==tab;carbonNavHover[i]=carbonStep(carbonNavHover[i],hot?1.f:0.f);
        if(!active&&carbonNavHover[i]>.01f)d->AddRectFilled(p,p+ImVec2(62,40),carbonInk(255,static_cast<int>(15*carbonNavHover[i])),9);
        float entry=MenuTheme::animations?MenuMotion::smooth((menuMotion.progress-i*.025f)/.70f):1.f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,entry);
        // Keep text readable while the silver selector travels: light ink on
        // carbon, then a precisely clipped dark copy only over the silver tile.
        auto navText=[&](ImU32 ink){
            carbonCenterText(d,menuBody,menuBody->FontSize,p+ImVec2(0,4),62,ink,ui::tabs[tab].m_icon);
            carbonCenterText(d,menuSmall,menuSmall->FontSize,p+ImVec2(0,24),62,ink,Lang::english()?en[i]:ru[i]);
        };
        navText(carbonInk(active?237:static_cast<int>(165+75*carbonNavHover[i])));
        d->PushClipRect(select,select+ImVec2(62,40),true);navText(carbonInk(17));d->PopClipRect();
        ImGui::PopStyleVar();
        if(hot){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);ImGui::SetTooltip("%s",ui::tabs[tab].m_name);}if(hit)auroraNavigate(tab,0);
    }
    // Header typography is deliberately separate from the subtab strip.
    auto &tab=ui::tabs[ui::next_tab];
    d->PushClipRect(origin+ImVec2(320,13),origin+ImVec2(ui::size.x-288,76),true);
    carbonText(d,menuTitle,origin+ImVec2(320,22),carbonInk(244),tab.m_name);
    const char *subTitle=tab.m_subtabs.empty()?L(u8"Параметры раздела","Section settings"):tab.m_subtabs[std::clamp(tab.next_subtab,0,static_cast<int>(tab.m_subtabs.size())-1)];
    carbonText(d,menuSmall,origin+ImVec2(321,54),carbonInk(163),subTitle);d->PopClipRect();
    if(carbonHeaderButton("carbon search",origin+carbonSearchLocal(),0,true,L(u8"Поиск функций","Find a setting")))ImGui::OpenPopup("Carbon search");
    ImVec2 profile=origin+ImVec2(ui::size.x-218,21);
    d->AddRectFilled(profile,profile+ImVec2(160,42),carbonInk(17),8);d->AddRect(profile,profile+ImVec2(160,42),carbonInk(255,20),8);
    Profile::draw(profile+ImVec2(8,9),Hud::trim(playerName(),14));
    carbonText(d,menuSmall,profile+ImVec2(42,5),carbonInk(161),L(u8"ПРОФИЛЬ","PROFILE"));
    d->PushClipRect(profile+ImVec2(42,20),profile+ImVec2(153,40),true);
    carbonText(d,menuBold,profile+ImVec2(42,22),carbonInk(235),Hud::trim(playerName(),14).c_str());d->PopClipRect();
    if(carbonHeaderButton("carbon close",origin+ImVec2(ui::size.x-46,26),1,true,L(u8"Закрыть меню","Close menu"))){opened=false;Mouse::sync();}
    carbonDragHeader();
    // Flat underlined tabs replace the old segmented capsule.
    std::vector<const char *> subs=tab.m_subtabs;if(subs.empty())subs.push_back(tab.m_name);
    std::vector<ImVec2> spans;float x=0;
    for(const char *name:subs){float width=menuBold->CalcTextSizeA(menuBold->FontSize,FLT_MAX,0,name).x+30;spans.push_back({x,width});x+=width+6;}
    int active=tab.m_subtabs.empty()?0:std::clamp(tab.next_subtab,0,static_cast<int>(spans.size())-1);
    if(carbonSubX<0||carbonSubCategory!=ui::next_tab){carbonSubCategory=ui::next_tab;carbonSubX=spans[active].x;carbonSubW=spans[active].y;}
    carbonSubX=carbonStep(carbonSubX,spans[active].x);carbonSubW=carbonStep(carbonSubW,spans[active].y);
    d->AddLine(origin+ImVec2(mainX,127),origin+ImVec2(ui::size.x-24,127),carbonInk(255,18));
    d->AddRectFilled(origin+ImVec2(mainX+carbonSubX+9,125),origin+ImVec2(mainX+carbonSubX+carbonSubW-9,127),carbonInk(232),1);
    for(int i=0;i<static_cast<int>(subs.size());++i){
        ImVec2 p=origin+ImVec2(mainX+spans[i].x,94);ImGui::SetCursorScreenPos(p);ImGui::PushID(i+850);
        bool hit=ImGui::InvisibleButton("##carbon subtab",{spans[i].y,30}),hot=ImGui::IsItemHovered();ImGui::PopID();
        if(hot)d->AddRectFilled(p,p+ImVec2(spans[i].y,29),carbonInk(255,10),5);
        carbonCenterText(d,menuBold,menuBold->FontSize,p+ImVec2(0,6),spans[i].y,carbonInk(i==active?244:hot?222:166),subs[i]);
        if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);if(hit&&!tab.m_subtabs.empty())auroraNavigate(ui::next_tab,i);
    }
    if(carbonHeaderButton("carbon back",origin+ImVec2(ui::size.x-96,93),2,peachVisit>0,L(u8"Назад","Back"))){--peachVisit;auto v=peachVisits[peachVisit];auroraNavigate(v.tab,v.sub,false);}
    if(carbonHeaderButton("carbon forward",origin+ImVec2(ui::size.x-58,93),3,peachVisit+1<static_cast<int>(peachVisits.size()),L(u8"Вперёд","Forward"))){++peachVisit;auto v=peachVisits[peachVisit];auroraNavigate(v.tab,v.sub,false);}
    // The setting pages use a much wider canvas; no right-hand utility column.
    d->AddRectFilled(origin+ImVec2(mainX,bodyY),origin+ImVec2(mainX+mainW,bodyY+bodyH),carbonInk(12),10);
    if(MenuTheme::frames)d->AddRect(origin+ImVec2(mainX,bodyY),origin+ImVec2(mainX+mainW,bodyY+bodyH),carbonInk(255,16),10);
    float pageAlpha=ui::anim*ui::content_anim;
    if(MenuTheme::animations)pageAlpha*=MenuMotion::smooth((menuMotion.progress-.06f)/.86f);
    float contentRise=MenuTheme::reduceMotion?0.f:(1-MenuMotion::smooth(pageAlpha))*10;
    ImGui::SetCursorPos({mainX+1,bodyY+1+contentRise});ImGui::PushStyleVar(ImGuiStyleVar_Alpha,pageAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,12});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10,10});
    ImGui::BeginChild("Carbon 3XL content",{mainW-2,std::max(1.f,bodyH-2-contentRise)},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_AlwaysUseWindowPadding);
    ui::render_page();ImGui::EndChild();ImGui::PopStyleVar(3);
    // Compact session information leaves the configuration canvas uncluttered.
    float footer=ui::size.y-35;
    d->AddLine(origin+ImVec2(mainX,footer-6),origin+ImVec2(ui::size.x-24,footer-6),carbonInk(255,15));
    hud_player_info_t info={};int own=Hud::localIndex();if(own&&eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(own,&info);
    std::string map=Hud::mapName();if(map=="menu"||map.empty())map=L(u8"Главное меню","Main menu");
    char session[180];if(Hud::fps>0)snprintf(session,sizeof(session),"%s  /  %.0f FPS  /  %d ms",Hud::trim(map,27).c_str(),Hud::fps,std::max(0,static_cast<int>(info.ping)));
    else snprintf(session,sizeof(session),"%s  /  FPS --",Hud::trim(map,27).c_str());
    d->PushClipRect(origin+ImVec2(mainX,footer),origin+ImVec2(ui::size.x-320,footer+22),true);
    carbonText(d,menuSmall,origin+ImVec2(mainX,footer+4),carbonInk(170),session);d->PopClipRect();
    ImVec2 keys=origin+ImVec2(ui::size.x-295,footer);carbonKey(d,keys,MenuTheme::currentMenuKey().name,L(u8"МЕНЮ","MENU"));carbonKey(d,keys,"F11",L(u8"ВЫГРУЗИТЬ","UNLOAD"));
    ImVec2 railProfile=origin+ImVec2(29,ui::size.y-51);d->AddCircle(railProfile+ImVec2(12,12),16,carbonInk(255,62),32,1);Profile::draw(railProfile,Hud::trim(playerName(),10));
    carbonCenterText(d,menuSmall,menuSmall->FontSize,origin+ImVec2(8,ui::size.y-18),66,carbonInk(154),BALONCUAGA_VERSION);
    carbonSearchPopup(origin);
    ImGui::End();ImGui::PopStyleVar(2);ui::handle_anims();
}
