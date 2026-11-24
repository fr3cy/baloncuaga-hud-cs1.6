#pragma once
// Aurora: BALONCUAGA's animated theme, ported from the HTML mockup.
// It reuses the shared page registry, Peach section search and visit history,
// so every ASI setting stays reachable exactly as in the other themes.
static float auroraPillY=-1,auroraTabX=-1,auroraTabW;
static std::vector<float> auroraNavHover;
static float auroraQuickHover[32];
static int auroraBarTab=-1;
static ULONGLONG auroraToastTick,auroraRippleTick;
static ImVec2 auroraRipplePos,auroraRippleMin,auroraRippleMax;
static std::string auroraToastText;
static ImU32 auroraC(ImU32 c){return ImGui::GetColorU32(c);}
static ImU32 auroraAcc(float a=1){return auroraC(MenuTheme::accentColor(0,a));}
static ImU32 auroraAcc2(float a=1){return auroraC(MenuTheme::accentColor(1,a));}
static float auroraEase(float t){t=std::clamp(t,0.f,1.f);return 1-(1-t)*(1-t)*(1-t);}
static float auroraStep(float v,float target,float speed){return ImLerp(v,target,1.f-std::exp(-speed*ImGui::GetIO().DeltaTime));}
static void auroraToast(const char *text){auroraToastText=text;auroraToastTick=GetTickCount64();}
// Horizontal gradient inside a rounded rectangle; vertex alpha comes from `alpha`.
static void auroraGradient(ImDrawList *d,ImVec2 a,ImVec2 b,ImU32 left,ImU32 right,float rounding,float alpha=1,ImDrawFlags flags=0){
    int start=d->VtxBuffer.Size;d->AddRectFilled(a,b,auroraC(IM_COL32(255,255,255,static_cast<int>(255*std::clamp(alpha,0.f,1.f)))),rounding,flags);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,a,{b.x,a.y},left,right);
}
static void auroraGlow(ImDrawList *d,ImVec2 a,ImVec2 b,ImU32 color,float strength,float rounding,int layers=4){
    if(strength<=.01f)return;
    for(int i=1;i<=layers;++i){float e=static_cast<float>(i)*1.6f;ImColor c(color);c.Value.w*=strength*(layers+1-i)/(layers*2.f);d->AddRect(a-ImVec2(e,e),b+ImVec2(e,e),auroraC(c),rounding+e,0,2);}
}
// Sections sit on one shared backing (drawn in auroraLayout), so the gaps between
// them never show the game through as dark stripes; a panel is just its outline.
static void auroraPanel(ImVec2 p,ImVec2 s){
    auto *d=ImGui::GetWindowDrawList();
    if(MenuTheme::menuTemplate==MenuTheme::TemplateOrbit){
        d->AddRectFilled(p+ImVec2(1,1),p+s-ImVec2(1,1),auroraC(IM_COL32(16,20,31,225)),14);
        if(MenuTheme::frames)d->AddRect(p,p+s,auroraC(IM_COL32(124,140,183,30)),14);
    }else if(MenuTheme::frames)d->AddRect(p,p+s,auroraC(IM_COL32(255,255,255,17)),14);
    float w=std::min(s.x*.5f,190.f);
    d->AddRectFilledMultiColor(p+ImVec2(14,0),p+ImVec2(14+w,1.2f),auroraAcc(.75f),auroraAcc(0),auroraAcc(0),auroraAcc(.75f));
    d->AddRectFilledMultiColor(p+ImVec2(s.x-14-w,s.y-1.2f),p+ImVec2(s.x-14,s.y),auroraAcc2(0),auroraAcc2(.55f),auroraAcc2(.55f),auroraAcc2(0));
}
// Invisible handle that moves the menu while dragged and saves the position.
static void auroraDrag(const char *id,ImVec2 pos,ImVec2 size){
    ImGui::SetCursorPos(pos);ImGui::InvisibleButton(id,size);
    if(ImGui::IsItemActive()&&!ImGui::IsItemActivated()&&ImGui::IsMouseDragging(0,0)){auto delta=ImGui::GetIO().MouseDelta;menuX+=delta.x*scale;menuY+=delta.y*scale;moving=true;}
    if(ImGui::IsItemDeactivated()&&moving){moving=false;clampPosition();savePosition();}
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}
static void auroraRipple(ImDrawList *d){
    ULONGLONG e=GetTickCount64()-auroraRippleTick;if(e>600)return;float t=e/600.f;
    d->PushClipRect(auroraRippleMin,auroraRippleMax,true);
    d->AddCircleFilled(auroraRipplePos,8+t*(auroraRippleMax.x-auroraRippleMin.x),auroraC(IM_COL32(255,255,255,static_cast<int>(70*(1-t)))),48);d->PopClipRect();
}
static void auroraNavigate(int tab,int sub,bool remember=true){
    if(tab<0||tab>=static_cast<int>(ui::tabs.size()))return;
    sub=std::clamp(sub,0,std::max(0,static_cast<int>(ui::tabs[tab].pages.size())-1));
    if(remember&&(peachVisit<0||peachVisits[peachVisit].tab!=tab||peachVisits[peachVisit].sub!=sub)){
        peachVisits.resize(peachVisit+1);peachVisits.push_back({tab,sub});
        if(peachVisits.size()>32)peachVisits.erase(peachVisits.begin());peachVisit=static_cast<int>(peachVisits.size())-1;
    }
    // Fade the old page out; ui::handle_anims swaps it in once transparent.
    if(tab!=ui::next_tab){ui::next_tab=tab;ui::anim_dst=0;ui::tabs[tab].cur_subtab=ui::tabs[tab].next_subtab=sub;}
    else if(sub!=ui::tabs[tab].next_subtab){ui::tabs[tab].next_subtab=sub;ui::content_anim_dst=0;}
}
static bool auroraNavItem(int index,const char *name,const char *icon,ImVec2 p,ImVec2 size){
    if(index<0||index>=static_cast<int>(ui::tabs.size()))return false;
    auroraNavHover.resize(ui::tabs.size(),0.f);
    ImGui::SetCursorScreenPos(p);ImGui::PushID(index);bool clicked=ImGui::InvisibleButton("nav",size);bool hot=ImGui::IsItemHovered();ImGui::PopID();
    bool active=ui::next_tab==index;
    if(!std::isfinite(auroraNavHover[index]))auroraNavHover[index]=0;
    auroraNavHover[index]=std::clamp(auroraStep(auroraNavHover[index],hot&&!active?1.f:0.f,14),0.f,1.f);
    float h=auroraNavHover[index];auto *d=ImGui::GetWindowDrawList();
    ImU32 col=active?auroraC(IM_COL32(255,255,255,255)):(ImU32)col_anim(ImGui::GetColorU32(ImGuiCol_TextDisabled),ImGui::GetColorU32(ImGuiCol_Text),h);
    float y=p.y+(size.y-ImGui::GetFontSize())/2;
    d->AddText({p.x+13+h*3,y},active?auroraAcc2():col,icon,ImGui::FindRenderedTextEnd(icon));
    d->AddText({p.x+38+h*3,y},col,name);
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);return clicked;
}
static void auroraKey(ImDrawList *d,ImVec2 &p,const char *text){
    auto s=ImGui::CalcTextSize(text);ImVec2 b=p+ImVec2(s.x+10,18);
    d->AddRectFilled(p,b,auroraC(IM_COL32(32,34,47,255)),5);d->AddRect(p,b,auroraC(IM_COL32(255,255,255,22)),5);
    d->AddLine({p.x+3,b.y},{b.x-3,b.y},auroraC(IM_COL32(255,255,255,30)),1.5f);
    d->AddText(p+ImVec2(5,(18-s.y)/2),ImGui::GetColorU32(ImGuiCol_Text),text);p.x=b.x+5;
}
static void auroraHint(ImDrawList *d,ImVec2 &p,const char *text){d->AddText(p+ImVec2(0,1),ImGui::GetColorU32(ImGuiCol_TextDisabled),text);p.x+=ImGui::CalcTextSize(text).x+8;}
static bool auroraIconButton(const char *id,ImVec2 p,bool left,bool enabled){
    ImGui::SetCursorScreenPos(p);bool clicked=ImGui::InvisibleButton(id,{26,26})&&enabled;bool hot=enabled&&ImGui::IsItemHovered();auto *d=ImGui::GetWindowDrawList();
    struct state{float hover;};auto &obj=anim_obj(id,91,state{0.f});obj.hover=animate(obj.hover,0.f,1.f,hot);
    d->AddRectFilled(p,p+ImVec2(26,26),auroraC(IM_COL32(24,26,36,255)),8);
    d->AddRect(p,p+ImVec2(26,26),col_anim(auroraC(IM_COL32(255,255,255,18)),auroraAcc(.7f),obj.hover),8);
    ImU32 col=enabled?(ImU32)col_anim(ImGui::GetColorU32(ImGuiCol_TextDisabled),auroraC(IM_COL32(255,255,255,255)),obj.hover):auroraC(IM_COL32(90,92,110,255));
    float x=p.x+13+(left?-1.5f:1.5f)*(1+obj.hover),dx=left?3.f:-3.f;
    d->AddLine({x+dx,p.y+9},{x-dx,p.y+13},col,2);d->AddLine({x-dx,p.y+13},{x+dx,p.y+17},col,2);
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);return clicked;
}
static void auroraBrand(ImDrawList *d,ImVec2 p){
    const char *text="baLONCUaga";auto *f=font(3);float x=p.x,t=frameTime;
    for(const char *c=text;*c;++c){
        // Logo style (Settings -> Appearance): animated shine, static gradient or plain white.
        ImU32 col;
        if(MenuTheme::logoStyle==MenuTheme::LogoPlain)col=auroraC(IM_COL32(255,255,255,255));
        else if(MenuTheme::logoStyle==MenuTheme::LogoGradient)col=col_anim(ImColor(auroraAcc()),ImColor(auroraAcc2()),(c-text)/9.f);
        else{float wave=.5f+.5f*std::sin(t*1.8f-(c-text)*.45f);
            col=col_anim(col_anim(ImColor(auroraC(IM_COL32(255,255,255,255))),ImColor(auroraAcc2()),std::min(1.f,wave*1.4f)),ImColor(auroraAcc()),std::max(0.f,wave*1.6f-.8f));}
        d->AddText(f,f->FontSize,{x,p.y},col,c,c+1);x+=f->CalcTextSizeA(f->FontSize,FLT_MAX,0,c,c+1).x+.6f;
    }
}
static void auroraAvatarRing(ImDrawList *d,ImVec2 center,float radius){
    constexpr int segments=36;float t=frameTime*2.2f;
    for(int i=0;i<segments;++i){
        float a0=i*IM_PI*2/segments,a1=(i+1)*IM_PI*2/segments+.02f,k=.5f+.5f*std::sin(a0+t);
        d->PathArcTo(center,radius,a0+t*.6f,a1+t*.6f,3);d->PathStroke(col_anim(ImColor(auroraAcc()),ImColor(auroraAcc2()),k),0,2.f);
    }
}
static bool auroraQuickRow(int slot,const char *name,float width){
    auto p=ImGui::GetCursorScreenPos();ImGui::PushID(slot+400);bool clicked=ImGui::InvisibleButton("quick",{width,26});bool hot=ImGui::IsItemHovered();ImGui::PopID();
    float &h=auroraQuickHover[slot%32];h=auroraStep(h,hot?1.f:0.f,14);auto *d=ImGui::GetWindowDrawList();
    if(h>.01f)d->AddRectFilled(p,p+ImVec2(width,26),auroraC(IM_COL32(32,34,47,static_cast<int>(255*h))),7);
    if(h>.01f)d->AddRectFilled(p+ImVec2(0,7),p+ImVec2(2.5f,19),auroraAcc2(h),2);
    float y=p.y+(26-ImGui::GetFontSize())/2;
    d->PushClipRect(p,p+ImVec2(width-16,26),true);
    d->AddText({p.x+8+h*4,y},col_anim(ImGui::GetColorU32(ImGuiCol_TextDisabled),ImGui::GetColorU32(ImGuiCol_Text),h),name);d->PopClipRect();
    d->AddText({p.x+width-12+h*2,y},col_anim(auroraC(IM_COL32(102,106,130,255)),auroraAcc2(),h),u8"›");
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);return clicked;
}
static void auroraTile(ImDrawList *d,ImVec2 p,ImVec2 s,const char *label,const char *value){
    d->AddRectFilled(p,p+s,auroraC(IM_COL32(11,12,18,255)),8);d->AddRect(p,p+s,auroraC(IM_COL32(255,255,255,17)),8);
    d->AddText(p+ImVec2(8,4),ImGui::GetColorU32(ImGuiCol_TextDisabled),label);
    d->PushClipRect(p,p+s-ImVec2(4,0),true);d->AddText(font(1),font(1)->FontSize,p+ImVec2(8,20),ImGui::GetColorU32(ImGuiCol_Text),value);d->PopClipRect();
}
static void auroraToastDraw(ImVec2 origin){
    if(auroraToastText.empty())return;ULONGLONG e=GetTickCount64()-auroraToastTick;if(e>2200)return;
    float in=auroraEase(e/300.f),out=e>1900?auroraEase((e-1900)/300.f):0,k=in*(1-out);
    auto *d=ImGui::GetForegroundDrawList();auto ts=ImGui::CalcTextSize(auroraToastText.c_str());ImVec2 s(ts.x+44,38);
    ImVec2 p(origin.x+(ui::size.x-s.x)/2,origin.y+ui::size.y-s.y-18+(1-k)*26);
    ImU32 a=static_cast<ImU32>(255*k*ImGui::GetStyle().Alpha);auto alpha=[&](ImU32 c){return (c&~IM_COL32_A_MASK)|(((c>>IM_COL32_A_SHIFT&0xFF)*a/255)<<IM_COL32_A_SHIFT);};
    for(int i=0;i<4;++i)d->AddRectFilled(p+ImVec2(-i*2.f,6-i*1.f),p+s+ImVec2(i*2.f,8+i*2.f),alpha(IM_COL32(0,0,0,30)),12+i*2);
    d->AddRectFilled(p,p+s,alpha(IM_COL32(24,26,36,255)),12);d->AddRect(p,p+s,alpha(MenuTheme::accentColor(0,.6f)),12);
    ImVec2 dot=p+ImVec2(18,s.y/2);d->AddCircleFilled(dot,7,alpha(IM_COL32(61,220,132,50)));d->AddCircleFilled(dot,4,alpha(IM_COL32(61,220,132,255)));
    d->AddText(p+ImVec2(32,(s.y-ts.y)/2),alpha(IM_COL32(233,234,242,255)),auroraToastText.c_str());
}
static void auroraLayout(){
    auto &io=ImGui::GetIO();const float ga=1.f; // MenuFadeScope applies final window opacity to all child renderers.
    const bool orbit=MenuTheme::menuTemplate==MenuTheme::TemplateOrbit;
    const float rise=MenuTheme::reduceMotion?0.f:menuMotion.offsetY();
    if(peachVisit<0||peachVisits[peachVisit].tab!=ui::next_tab||peachVisits[peachVisit].sub!=ui::tabs[ui::next_tab].next_subtab)
        auroraNavigate(ui::next_tab,ui::tabs[ui::next_tab].next_subtab);
    constexpr float side=176,gap=8,right=176,bar=44;
    const float mainX=side+gap,mainW=ui::size.x-mainX-right-gap,bodyY=bar+gap,bodyH=ui::size.y-bodyY;
    clampPosition();
    float animatedY=std::clamp(menuY/scale+rise,8.f/scale,std::max(8.f/scale,io.DisplaySize.y-ui::size.y-8.f/scale));
    ImGui::SetNextWindowPos({menuX/scale,animatedY},ImGuiCond_Always);ImGui::SetNextWindowSize(ui::size);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ga);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("baLONCUaga Aurora",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar|(menuRequested?0:ImGuiWindowFlags_NoInputs));
    auto origin=ImGui::GetWindowPos();auto *d=ImGui::GetWindowDrawList();auto *bg=ImGui::GetBackgroundDrawList();
    // Light dimming in game only; the game's own main menu stays untouched.
    if(!outsideGame)bg->AddRectFilled({0,0},io.DisplaySize,auroraC(IM_COL32(4,5,9,orbit?65:55)));
    if(MenuTheme::outerEffects)MenuMotion::drawDecor(bg,origin,ui::size,MenuTheme::accentColor(0),MenuTheme::accentColor(1),ga,menuEffectTime,menuMotion.progress,orbit,MenuTheme::particles,MenuTheme::reduceMotion);
    d->AddRectFilled(origin,origin+ui::size,auroraC(IM_COL32(orbit?10:19,orbit?13:20,orbit?22:30,static_cast<int>(std::clamp(MenuTheme::windowOpacity,60,100)*2.55f))),14);
    if(orbit){
        d->AddRect(origin,origin+ui::size,auroraC(MenuTheme::accentColor(0,.34f)),14,0,1.2f);
        // Soft inset highlight stays inside the rounded frame.
        d->AddLine(origin+ImVec2(22,1),origin+ImVec2(ui::size.x-22,1),auroraC(MenuTheme::accentColor(1,.40f)),1.2f);
    }
    auroraPanel(origin,{side,ui::size.y});
    auroraPanel(origin+ImVec2(mainX,0),{mainW,bar});
    auroraPanel(origin+ImVec2(mainX,bodyY),{mainW,bodyH});
    auroraPanel(origin+ImVec2(ui::size.x-right,0),{right,ui::size.y});

    // Sidebar: brand with the authors line (drag handle), sliding navigation pill, hints, profile.
    auroraBrand(d,origin+ImVec2(16,5));
    d->AddText(origin+ImVec2(16,7+font(3)->FontSize),ImGui::GetColorU32(ImGuiCol_TextDisabled),"detoxy & weta");
    d->AddLine(origin+ImVec2(12,bar),origin+ImVec2(side-12,bar),auroraC(IM_COL32(255,255,255,15)));
    auroraDrag("Aurora drag",{0,0},{side,bar});
    // Sidebar order: Showkeys sits after HUD; Misc/Config stays in its own group.
    static const int navOrder[]={0,1,2,3,9,4,5,8,10,11,6};constexpr int navCount=11;
    auto itemY=[&](int tab){if(tab==7)return 56.f+navCount*28+26;for(int i=0;i<navCount;++i)if(navOrder[i]==tab)return 56.f+i*28;return 56.f;};
    float target=itemY(ui::next_tab);auroraPillY=auroraPillY<0?target:auroraStep(auroraPillY,target,13);
    {
        ImVec2 a=origin+ImVec2(8,auroraPillY),b=a+ImVec2(side-16,30);
        auroraGlow(d,a,b,MenuTheme::accentColor(0,.6f),1,10);
        if(orbit)auroraGradient(d,a,b,MenuTheme::accentColor(0),MenuTheme::accentColor(1),10,.22f);
        else d->AddRectFilled(a,b,auroraAcc(.13f),10);
        d->AddRectFilledMultiColor(a+ImVec2(10,0),b-ImVec2(10,0),auroraAcc(.24f),auroraAcc(0),auroraAcc(0),auroraAcc(.24f));
        d->AddRect(a,b,auroraAcc(.45f),10);
        d->AddRectFilled(a+ImVec2(-1,8),a+ImVec2(2,22),auroraAcc2(),2);d->AddRectFilled(a+ImVec2(-3,6),a+ImVec2(4,24),auroraAcc2(.18f),4);
    }
    for(int i=0;i<navCount;++i){
        int tab=navOrder[i];float entry=MenuTheme::animations?MenuMotion::smooth((menuMotion.progress-i*.025f)/.70f):1.f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ga*entry);
        if(tab<static_cast<int>(ui::tabs.size())&&auroraNavItem(tab,ui::tabs[tab].m_name,ui::tabs[tab].m_icon,origin+ImVec2(8,itemY(tab)),{side-16,27}))auroraNavigate(tab,0);
        ImGui::PopStyleVar();
    }
    d->AddText(origin+ImVec2(20,56+navCount*28+6),ImGui::GetColorU32(ImGuiCol_TextDisabled),L(u8"КОНФИГУРАЦИЯ","CONFIGURATION"));
    if(auroraNavItem(7,"Misc / Config",ui::tabs[7].m_icon,origin+ImVec2(8,itemY(7)),{side-16,30}))auroraNavigate(7,0);
    {ImVec2 k=origin+ImVec2(12,ui::size.y-82);auroraKey(d,k,MenuTheme::currentMenuKey().name);auroraHint(d,k,L(u8"меню","menu"));auroraKey(d,k,"Y");auroraKey(d,k,"U");auroraHint(d,k,L(u8"чат","chat"));}
    d->AddLine(origin+ImVec2(12,ui::size.y-54),origin+ImVec2(side-12,ui::size.y-54),auroraC(IM_COL32(255,255,255,15)));
    {
        ImVec2 av=origin+ImVec2(18,ui::size.y-39);auroraAvatarRing(d,av+ImVec2(12,12),16);Profile::draw(av,Hud::trim(playerName(),10));
        d->AddText(font(1),font(1)->FontSize,origin+ImVec2(52,ui::size.y-42),ImGui::GetColorU32(ImGuiCol_Text),Hud::trim(playerName(),12).c_str());
        const char *profileLabel=L(u8"Локальный профиль","Local profile");
        ImVec2 profilePos=origin+ImVec2(52,ui::size.y-25);float available=side-86;
        float textWidth=font(0)->CalcTextSizeA(12,FLT_MAX,0,profileLabel).x;
        float profileSize=std::min(12.f,12.f*available/std::max(1.f,textWidth));
        d->AddText(font(0),profileSize,profilePos,ImGui::GetColorU32(ImGuiCol_TextDisabled),profileLabel);
        float ph=std::fmod(frameTime,2.f)/2.f;ImVec2 dot=origin+ImVec2(side-20,ui::size.y-27);
        d->AddCircle(dot,4+ph*8,auroraC(IM_COL32(61,220,132,static_cast<int>(140*(1-ph)))),24,1.5f);d->AddCircleFilled(dot,4,auroraC(IM_COL32(61,220,132,255)));
    }

    // Top bar: section title, segmented subtabs with a sliding pill, history.
    auto &tab=ui::tabs[ui::next_tab];
    std::vector<const char *> subs=tab.m_subtabs;if(subs.empty())subs.push_back(tab.m_name);
    float total=8;for(auto name:subs)total+=ImGui::CalcTextSize(name).x+22;
    float titleW=font(3)->CalcTextSizeA(font(3)->FontSize,FLT_MAX,0,tab.m_name).x;
    bool showTitle=total+titleW+106<mainW;
    if(showTitle)d->AddText(font(3),font(3)->FontSize,origin+ImVec2(mainX+14,(bar-font(3)->FontSize)/2),ImGui::GetColorU32(ImGuiCol_Text),tab.m_name);
    float left=showTitle?mainX+26+titleW:mainX+10,boxR=mainX+mainW-70;
    float boxX=std::max(left,std::min(mainX+(mainW-total)/2,boxR-total));
    ImVec2 box=origin+ImVec2(boxX,7);d->AddRectFilled(box,box+ImVec2(total,30),auroraC(IM_COL32(11,12,18,255)),11);d->AddRect(box,box+ImVec2(total,30),auroraC(IM_COL32(255,255,255,15)),11);
    {
        float x=4;std::vector<ImVec2> spans;for(auto name:subs){float w=ImGui::CalcTextSize(name).x+22;spans.push_back({x,w});x+=w;}
        int active=tab.m_subtabs.empty()?0:std::clamp(tab.next_subtab,0,static_cast<int>(spans.size())-1);
        // Spans have a different coordinate system for each category. Never
        // interpolate a new category from the previous category's pill width.
        if(auroraTabX<0||auroraBarTab!=ui::next_tab||!std::isfinite(auroraTabX)||!std::isfinite(auroraTabW)){
            auroraBarTab=ui::next_tab;auroraTabX=spans[active].x;auroraTabW=spans[active].y;
        }
        auroraTabX=auroraStep(auroraTabX,spans[active].x,14);auroraTabW=auroraStep(auroraTabW,spans[active].y,14);
        ImVec2 a=box+ImVec2(auroraTabX,4),b=a+ImVec2(auroraTabW,22);
        for(int i=0;i<3;++i)d->AddRectFilled(a+ImVec2(3-i,4),b+ImVec2(-3+i,3+i),auroraAcc(.10f),10);
        auroraGradient(d,a,b,MenuTheme::accentColor(0),col_anim(ImColor(MenuTheme::accentColor(0)),ImColor(MenuTheme::accentColor(1)),.35f),9);
        d->AddRectFilledMultiColor(a+ImVec2(9,1),ImVec2(b.x-9,a.y+9),auroraC(IM_COL32(255,255,255,40)),auroraC(IM_COL32(255,255,255,40)),0,0);
        for(int i=0;i<static_cast<int>(subs.size());++i){
            ImVec2 p=box+ImVec2(spans[i].x,4);ImGui::SetCursorScreenPos(p);ImGui::PushID(i+300);
            bool hit=ImGui::InvisibleButton("subtab",{spans[i].y,22});bool hot=ImGui::IsItemHovered();ImGui::PopID();
            ImU32 col=i==active?auroraC(IM_COL32(255,255,255,255)):ImGui::GetColorU32(hot?ImGuiCol_Text:ImGuiCol_TextDisabled);
            auto ts=ImGui::CalcTextSize(subs[i]);d->AddText(p+ImVec2((spans[i].y-ts.x)/2,(22-ts.y)/2),col,subs[i]);
            if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);if(hit&&!tab.m_subtabs.empty())auroraNavigate(ui::next_tab,i);
        }
    }
    if(auroraIconButton("aurora back",origin+ImVec2(mainX+mainW-62,9),true,peachVisit>0)){--peachVisit;auto v=peachVisits[peachVisit];auroraNavigate(v.tab,v.sub,false);}
    if(auroraIconButton("aurora forward",origin+ImVec2(mainX+mainW-33,9),false,peachVisit+1<static_cast<int>(peachVisits.size()))){++peachVisit;auto v=peachVisits[peachVisit];auroraNavigate(v.tab,v.sub,false);}
    // The whole top bar moves the window; tabs and buttons above were submitted
    // first, so they keep their clicks and only empty space starts a drag.
    auroraDrag("Aurora drag top",{mainX,0},{mainW,bar});

    // Page content fades and rises in on every tab/subtab change.
    float pageAlpha=ui::anim*ui::content_anim;
    if(MenuTheme::animations)pageAlpha*=MenuMotion::smooth((menuMotion.progress-.06f)/.86f);
    ImGui::SetCursorPos({mainX+1,bodyY+1+(MenuTheme::reduceMotion?0.f:(1-auroraEase(pageAlpha))*14)});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ga*pageAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,10});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,8});
    ImGui::BeginChild("Aurora content",{mainW-2,bodyH-2},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_AlwaysUseWindowPadding);
    ui::render_page();ImGui::EndChild();ImGui::PopStyleVar(3);

    // Right column: search, quick links, live stats.
    ImGui::SetCursorPos({ui::size.x-right+1,1});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,12});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{6,5});
    ImGui::BeginChild("Aurora side",{right-2,ui::size.y-2},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoScrollbar);
    {
        auto *sd=ImGui::GetWindowDrawList();float width=ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(font(1));ImGui::TextUnformatted(L(u8"Поиск раздела","Find section"));ImGui::PopFont();
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##aurora search",L(u8"FOG, radar, тема...","FOG, radar, theme..."),peachSearch,sizeof(peachSearch));
        ImGui::Dummy({0,4});ImGui::PushFont(font(1));ImGui::TextUnformatted(*peachSearch?L(u8"Результаты","Results"):L(u8"Быстрый переход","Quick links"));ImGui::PopFont();
        int slot=0;
        if(*peachSearch){
            int found=0;for(int i=0;i<static_cast<int>(sizeof(peachPages)/sizeof(*peachPages));++i)if(peachMatches(peachPages[i],peachSearch)){
                ++found;if(auroraQuickRow(slot++,peachName(peachPages[i]),width))auroraNavigate(peachPages[i].tab,peachPages[i].sub);}
            if(!found)ImGui::TextDisabled("%s",L(u8"Ничего не найдено.","Nothing found."));
        }else for(const PeachVisit &link:{PeachVisit{6,4},PeachVisit{6,0},PeachVisit{9,0},PeachVisit{3,1},PeachVisit{3,0},PeachVisit{8,0}}){
            for(const auto &page:peachPages)if(page.tab==link.tab&&page.sub==link.sub){if(auroraQuickRow(slot++,peachName(page),width))auroraNavigate(page.tab,page.sub);break;}
        }
        auto base=ImGui::GetWindowPos()+ImVec2(12,ImGui::GetWindowHeight()-150);
        if(ImGui::GetCursorScreenPos().y<base.y-6){
            hud_player_info_t info={};int own=Hud::localIndex();if(own&&eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(own,&info);
            char fps[16],ping[16];snprintf(fps,sizeof(fps),"%.0f",Hud::fps);snprintf(ping,sizeof(ping),"%d",std::max(0,static_cast<int>(info.ping)));
            std::string map=Hud::trim(Hud::mapName(),10);if(map.empty())map="-";
            float tw=(width-5)/2;
            auroraTile(sd,base,{tw,40},"FPS",fps);auroraTile(sd,base+ImVec2(tw+5,0),{tw,40},L(u8"Пинг","Ping"),ping);
            auroraTile(sd,base+ImVec2(0,45),{tw,40},L(u8"Карта","Map"),map.c_str());auroraTile(sd,base+ImVec2(tw+5,45),{tw,40},L(u8"Сборка","Build"),"" BALONCUAGA_VERSION "");
        }
    }
    ImGui::EndChild();ImGui::PopStyleVar(2);
    auroraToastDraw(origin);
    ImGui::End();ImGui::PopStyleVar(2);
    ui::handle_anims();
}
