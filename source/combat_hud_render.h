#pragma once
// Resolution-scaled in-game panels. The draw list is behind ASI windows, has no input
// targets, and publishes only components actually submitted by this renderer.
static bool combatHudEnabled=true,combatSettingsLoaded,combatScoresHeld;
static int combatPanelOpacity=36;
static float combatChatInset=48;
static void loadCombatHud(){
    if(combatSettingsLoaded)return;combatSettingsLoaded=true;
    combatHudEnabled=GetPrivateProfileIntA("interface","combat_hud_enabled",1,iniPath)!=0;
    combatPanelOpacity=std::clamp(static_cast<int>(GetPrivateProfileIntA("interface","combat_hud_opacity",36,iniPath)),0,75);
}
static void saveCombatHud(){
    if(!combatSettingsLoaded)return;char value[16];
    WritePrivateProfileStringA("interface","combat_hud_enabled",combatHudEnabled?"1":"0",iniPath);
    snprintf(value,sizeof(value),"%d",combatPanelOpacity);WritePrivateProfileStringA("interface","combat_hud_opacity",value,iniPath);
}
struct CombatLayout {ImVec2 health,armor,ammo,money,timer;float healthW,armorW,ammoW,moneyW,timerW,height,ammoH,moneyH,timerH,chatInset,viewportW;};
static float combatUiScale(){return HudResolution::factor(screenW,screenH);}
static CombatLayout combatLayout(float w,float h,bool graph,int graphPosition,bool recording){
    const float units=HudResolution::factor(static_cast<int>(w),static_cast<int>(h));w/=units;h/=units;
    float margin=std::round(std::clamp(w*.014f,10.f,22.f));bool tiny=w<720,small=w<1000,large=w>=1600;
    CombatLayout l{};l.viewportW=w;l.healthW=tiny?164:small?184:large?238:224;l.armorW=tiny?152:small?172:large?216:204;
    l.ammoW=tiny?196:small?244:312;l.moneyW=tiny?196:small?208:236;l.timerW=tiny?124:140;
    l.height=tiny?76:small?80:large?92:88;l.ammoH=tiny?64:small?68:72;l.moneyH=tiny?60:68;l.timerH=60;
    float lift=std::max(recording?60.f:0.f,(graph&&graphPosition==3)?88.f:0.f);
    float gap=tiny?8:12,bottom=h-margin-lift;l.health={margin,bottom-l.height};l.armor={margin+l.healthW+gap,bottom-l.height};
    float ammoBottom=h-margin-((graph&&graphPosition==1)?88:0);l.ammo={w-margin-l.ammoW,ammoBottom-l.ammoH};
    float timerY=bottom-l.timerH;if((graph&&graphPosition==2)||(w-l.timerW)/2<l.armor.x+l.armorW+12)timerY=l.health.y-l.timerH-12;
    l.timer={(w-l.timerW)/2,timerY};
    ImVec2 inlineMoney(l.ammo.x-l.moneyW-12,ammoBottom-l.moneyH);
    bool moneyFits=inlineMoney.x>=l.timer.x+l.timerW+12&&inlineMoney.x>=l.armor.x+l.armorW+12;
    l.money=moneyFits?inlineMoney:ImVec2(w-margin-l.moneyW,std::max(54.f,l.ammo.y-l.moneyH-12));l.chatInset=margin+l.height+14+lift;
    return l;
}
static ImVec2 combatPoint(ImVec2 p){float k=std::max(.01f,scale),units=combatUiScale();return {std::round(p.x*units)/k,std::round(p.y*units)/k};}
static ImU32 combatInk(int grey,int alpha=255){return IM_COL32(grey,grey,grey,std::clamp(alpha,0,255));}
static void combatText(ImDrawList *d,ImFont *face,ImVec2 p,ImU32 ink,const char *text,bool shadow=true){
    if(!face||!text)return;float fs=face->FontSize*combatUiScale()/std::max(.01f,scale);p=combatPoint(p);
    auto draw=[&](ImVec2 origin,ImU32 color){
        int first=d->VtxBuffer.Size;d->AddText(face,fs,origin,color,text);
        // ImGui floors logical text origins; realign glyph quads in framebuffer
        // pixels after this so a scaled open menu cannot soften the combat HUD.
        for(int i=first;i+3<d->VtxBuffer.Size;i+=4){ImVec2 delta(std::round(d->VtxBuffer[i].pos.x*scale)/scale-d->VtxBuffer[i].pos.x,std::round(d->VtxBuffer[i].pos.y*scale)/scale-d->VtxBuffer[i].pos.y);for(int n=0;n<4;++n)d->VtxBuffer[i+n].pos+=delta;}
    };
    if(shadow)draw(p+ImVec2(1/scale,1/scale),IM_COL32(0,0,0,190));draw(p,ink);
}
static float combatWidth(ImFont *face,const char *s){return face?face->CalcTextSizeA(face->FontSize,FLT_MAX,0,s).x:0;}
static void combatPanel(ImDrawList *d,ImVec2 p,float w,float h,float fade){
    int alpha=static_cast<int>(combatPanelOpacity*2.55f*fade);float k=std::max(.01f,scale)/combatUiScale(),radius=h*.5f/k;
    for(int i=3;i>0;--i)d->AddRectFilled(combatPoint(p+ImVec2(-i,2-i)),combatPoint(p+ImVec2(w+i,h+2+i)),IM_COL32(0,0,0,int((4-i)*5*fade)),radius+i/k);
    int first=d->VtxBuffer.Size;d->AddRectFilled(combatPoint(p),combatPoint(p+ImVec2(w,h)),IM_COL32(255,255,255,alpha),radius);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,first,d->VtxBuffer.Size,combatPoint(p),combatPoint(p+ImVec2(0,h)),IM_COL32(76,78,80,255),IM_COL32(23,25,27,255));
    d->AddRect(combatPoint(p),combatPoint(p+ImVec2(w,h)),combatInk(240,int(112*fade)),radius,0,1/k);
    d->AddRect(combatPoint(p+ImVec2(1,1)),combatPoint(p+ImVec2(w-1,h-1)),combatInk(255,int(23*fade)),std::max(1.f,radius-1/k),0,1/k);
}
static void combatHealthIcon(ImDrawList *d,ImVec2 p,bool warning,float size=26){
    ImU32 ink=warning?IM_COL32(247,103,113,255):combatInk(245);float k=std::max(.01f,scale)/combatUiScale(),third=std::round(size/3);
    d->AddRectFilled(combatPoint(p+ImVec2(third,0)),combatPoint(p+ImVec2(size-third,size)),ink,1/k);
    d->AddRectFilled(combatPoint(p+ImVec2(0,third)),combatPoint(p+ImVec2(size,size-third)),ink,1/k);
}
static void combatArmorIcon(ImDrawList *d,ImVec2 p,float size=28){
    ImVec2 a[7]={{.05f,.08f},{.30f,.10f},{.50f,0},{.70f,.10f},{.95f,.08f},{.85f,.65f},{.50f,1.f}};
    for(auto &v:a)v=combatPoint(p+v*size);d->PathLineTo(a[0]);d->PathLineTo(a[1]);d->PathLineTo(a[2]);d->PathLineTo(a[3]);d->PathLineTo(a[4]);d->PathLineTo(a[5]);d->PathLineTo(a[6]);d->PathLineTo(combatPoint(p+ImVec2(.15f,.65f)*size));d->PathStroke(combatInk(237),ImDrawFlags_Closed,2.f*combatUiScale()/std::max(.01f,scale));
}
static void combatBar(ImDrawList *d,ImVec2 p,float w,int value,bool health){
    float k=std::max(.01f,scale)/combatUiScale();d->AddRectFilled(combatPoint(p),combatPoint(p+ImVec2(w,10)),combatInk(225,64),5/k);
    float filled=w*std::clamp(value/100.f,0.f,1.f);if(filled<=0)return;
    ImU32 ink=health&&value<=25?IM_COL32(247,103,113,235):health?IM_COL32(179,239,194,235):combatInk(194,220);
    d->AddRectFilled(combatPoint(p),combatPoint(p+ImVec2(filled,10)),ink,5/k);
}
static ImFont *combatLargeFit(const CombatLayout &l,float width,const char *text){
    ImFont *face=CombatFonts::largeFor(l.viewportW);if(!face)face=combatValue;
    if(combatWidth(face,text)<=width)return face;
    for(int i=4;i>=3;--i)if(CombatFonts::faces[i]&&CombatFonts::faces[i]->FontSize<face->FontSize&&combatWidth(CombatFonts::faces[i],text)<=width)return CombatFonts::faces[i];
    return combatWidth(combatValue,text)<=width?combatValue:combatLabel;
}
static float combatValueY(ImFont *face,float height,const char *text){
    float top=FLT_MAX,bottom=-FLT_MAX;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p)if(auto *glyph=face->FindGlyph(*p)){top=std::min(top,glyph->Y0);bottom=std::max(bottom,glyph->Y1);}
    if(top==FLT_MAX)return 0;return 8.f+std::max(0.f,(height-34.f-(bottom-top))*.5f)-top;
}
static std::string combatNumber(int n){
    std::string s=std::to_string(std::max(0,n));for(int pos=static_cast<int>(s.size())-3;pos>0;pos-=3)s.insert(pos," ");return s;
}
static const char *combatWeaponName(const GameHudState::Snapshot &s){
    switch(s.weaponId){case 1:return "P228";case 3:return "SCOUT";case 4:return L(u8"HE-ГРАНАТА","HE GRENADE");case 6:return "C4";case 7:return "MAC-10";case 8:return "AUG";case 9:return L(u8"ДЫМОВАЯ","SMOKE");case 10:return "DUAL ELITES";case 11:return "FIVE-SEVEN";case 12:return "UMP-45";case 14:return "GALIL";case 15:return "FAMAS";case 16:return "USP";case 17:return "GLOCK-18";case 18:return "AWP";case 19:return "MP5";case 20:return "M249";case 21:return "M3";case 22:return "M4A1";case 23:return "TMP";case 24:return "G3SG1";case 25:return L(u8"ФЛЕШКА","FLASHBANG");case 26:return "DESERT EAGLE";case 27:return "SG552";case 28:return "AK-47";case 29:return L(u8"НОЖ","KNIFE");case 30:return "P90";default:return *s.weaponName?s.weaponName:L(u8"ОРУЖИЕ","WEAPON");}
}
static uint32_t drawCombatHud(ImDrawList *d,const GameHudState::Snapshot &s,const CombatLayout &l,bool scores,bool menu){
    if(!d||!combatSmall||!combatLabel||!combatValue||!s.inGame||!s.alive||s.spectating||s.intermission||s.noHud)return 0;
    uint32_t drawn=0;float fade=scores?.42f:menu?.62f:1.f;int dim=scores?185:210;char number[64];
    if(s.eligibleMask&GameHudState::Health){
        combatPanel(d,l.health,l.healthW,l.height,fade);combatHealthIcon(d,l.health+ImVec2(20,22),s.health<=25,l.height<80?23:27);
        snprintf(number,sizeof(number),"%d",s.health);auto *face=combatLargeFit(l,l.healthW-76,number);
        combatText(d,face,l.health+ImVec2(62,combatValueY(face,l.height,number)),combatInk(248),number);
        combatText(d,combatSmall,l.health+ImVec2(23,l.height-25),combatInk(199,dim),"HP");
        combatBar(d,l.health+ImVec2(65,l.height-22),l.healthW-86,s.health,true);drawn|=GameHudState::Health;
    }
    if(s.eligibleMask&GameHudState::Armor){
        combatPanel(d,l.armor,l.armorW,l.height,fade);combatArmorIcon(d,l.armor+ImVec2(20,21),l.height<80?25:29);
        snprintf(number,sizeof(number),"%d",s.armor);auto *face=combatLargeFit(l,l.armorW-76,number);
        combatText(d,face,l.armor+ImVec2(62,combatValueY(face,l.height,number)),combatInk(248),number);
        combatText(d,combatSmall,l.armor+ImVec2(20,l.height-25),combatInk(199,dim),L(u8"БРОНЯ","ARMOR"));
        combatBar(d,l.armor+ImVec2(81,l.height-22),l.armorW-102,s.armor,false);drawn|=GameHudState::Armor;
    }
    if(s.eligibleMask&GameHudState::Ammo){
        combatPanel(d,l.ammo,l.ammoW,l.ammoH,fade);int main=s.hasClip?s.clip:s.reserve;
        if(s.hasClip)snprintf(number,sizeof(number),"%d / %d",std::max(0,main),std::max(0,s.reserve));else snprintf(number,sizeof(number),"%d",std::max(0,main));
        auto *weaponFace=l.viewportW<1000?combatSmall:combatLabel;float weaponW=combatWidth(weaponFace,combatWeaponName(s));
        ImFont *valueFace=combatWidth(combatValue,number)<=l.ammoW-32?combatValue:combatLabel;
        bool inlineAmmo=weaponW+combatWidth(valueFace,number)+56<=l.ammoW;
        if(inlineAmmo){
            combatText(d,weaponFace,l.ammo+ImVec2(18,(l.ammoH-weaponFace->FontSize)*.5f-1),combatInk(197),combatWeaponName(s));
            float x=18+weaponW+12;d->AddLine(combatPoint(l.ammo+ImVec2(x,17)),combatPoint(l.ammo+ImVec2(x-5,l.ammoH-17)),combatInk(220,105),1/std::max(.01f,scale));
            combatText(d,valueFace,l.ammo+ImVec2(x+13,(l.ammoH-valueFace->FontSize)*.5f-2),main==0?IM_COL32(247,103,113,255):combatInk(248),number);
        }else{
            combatText(d,combatSmall,l.ammo+ImVec2(18,6),combatInk(197),combatWeaponName(s));
            combatText(d,valueFace,l.ammo+ImVec2(18,l.ammoH-valueFace->FontSize-9),main==0?IM_COL32(247,103,113,255):combatInk(248),number);
        }
        drawn|=GameHudState::Ammo;
    }else if(s.weaponId==29&&s.eligibleMask&&!(s.hideFlags&(GameHudState::HideAll|GameHudState::HideWeapons))){
        combatPanel(d,l.ammo+ImVec2(0,l.ammoH-42),l.ammoW,42,fade);combatText(d,combatLabel,l.ammo+ImVec2(18,l.ammoH-30),combatInk(224),combatWeaponName(s));
    }
    if(s.eligibleMask&GameHudState::Money){
        float width=l.moneyW;combatPanel(d,l.money,width,l.moneyH,fade);combatText(d,combatSmall,l.money+ImVec2(18,7),combatInk(190),L(u8"БАЛАНС","BALANCE"));
        auto value=std::string("$ ")+combatNumber(s.money);ImFont *face=combatWidth(combatValue,value.c_str())<=width-24?combatValue:combatLabel;
        combatText(d,face,l.money+ImVec2(18,l.moneyH-face->FontSize-9),combatInk(246),value.c_str());drawn|=GameHudState::Money;
    }
    if(s.eligibleMask&GameHudState::Timer){
        combatPanel(d,l.timer,l.timerW,l.timerH,fade);combatText(d,combatSmall,l.timer+ImVec2(18,5),combatInk(190),L(u8"РАУНД","ROUND"));
        int seconds=std::max(0,s.roundSeconds);snprintf(number,sizeof(number),"%d:%02d",seconds/60,seconds%60);
        ImFont *face=combatWidth(combatValue,number)<=l.timerW-20?combatValue:combatLabel;
        combatText(d,face,l.timer+ImVec2(17,l.timerH-face->FontSize-7),seconds<15?IM_COL32(247,103,113,255):combatInk(246),number);drawn|=GameHudState::Timer;
    }
    return drawn;
}
static bool combatRecording(){return eng&&eng->pDemoAPI&&eng->pDemoAPI->IsRecording&&eng->pDemoAPI->IsRecording();}
static void combatHudOverlay(bool menu){
    loadCombatHud();combatChatInset=48;
    auto snapshot=GameHudState::snapshot();uint32_t mask=0;
    if(combatHudEnabled&&NativeHudStyle::installed&&combatSmall&&combatLabel&&combatValue){
        bool graph=eng&&eng->pfnGetCvarFloat&&eng->pfnGetCvarFloat(const_cast<char *>("net_graph"))>0;
        int graphPosition=eng&&eng->pfnGetCvarFloat?int(eng->pfnGetCvarFloat(const_cast<char *>("net_graphpos"))):1;
        auto layout=combatLayout(float(screenW),float(screenH),graph,graphPosition,combatRecording());
        mask=drawCombatHud(ImGui::GetBackgroundDrawList(),snapshot,layout,combatScoresHeld,menu);
        float top=float(screenH)/combatUiScale();
        if(mask&GameHudState::Health)top=std::min(top,layout.health.y);if(mask&GameHudState::Armor)top=std::min(top,layout.armor.y);
        if(mask&GameHudState::Ammo)top=std::min(top,layout.ammo.y);if(mask&GameHudState::Money)top=std::min(top,layout.money.y);if(mask&GameHudState::Timer)top=std::min(top,layout.timer.y);
        if(mask)combatChatInset=std::max(48.f,float(screenH)-(top-14)*combatUiScale());
    }
    NativeHudStyle::replacementRendered(mask,snapshot.frameSerial);
}
static void combatHudPage(){
    loadCombatHud();card(L(u8"Игровой HUD","In-game HUD"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    bool edit=ImGui::Checkbox(L(u8"Оформление baLONCUaga","baLONCUaga HUD"),&combatHudEnabled);
    edit|=ImGui::SliderInt(L(u8"Плотность подложки","Panel backing"),&combatPanelOpacity,0,75,"%d%%");
    ImGui::TextWrapped("%s",L(u8"Здоровье, броня, боеприпасы, деньги и время раунда. При удержании TAB подложки становятся прозрачнее. Данные берутся из сообщений игры; недоступный показатель остаётся штатным.","Health, armour, ammunition, money and round time. Holding TAB makes the panels lighter. Values come from game messages; unavailable components retain their native display."));
    ImGui::TextWrapped("%s",L(u8"Стеклянные капсулы, крупные жирные числа и мятный индикатор здоровья. Красный цвет предупреждает об опасности. Цвета команд и сообщений сохранены.","Glass capsules, large black-weight numbers and a mint health bar. Red warns about critical values. Team and message colours are preserved."));
    if(edit)saveCombatHud();endCard();
}
