#pragma once
// Offsets are in framebuffer pixels regardless of the menu/HUD scale. Draw all
// border passes first, then one intact number (no per-digit spacing changes).
static void speedometerBorder(ImDrawList *d,ImFont *font,float fs,ImVec2 at,const char *text){
    if(!Speedometer::outline)return;
    float radius=std::clamp(Speedometer::outlineWidth,1,3)/std::max(.01f,scale);
    ImU32 color=IM_COL32(Speedometer::outlineRgb[0],Speedometer::outlineRgb[1],Speedometer::outlineRgb[2],255);
    const ImVec2 directions[]={{-1,0},{1,0},{0,-1},{0,1},{-.707107f,-.707107f},{.707107f,-.707107f},{-.707107f,.707107f},{.707107f,.707107f}};
    for(int layer=1;layer<=std::clamp(Speedometer::outlineWidth,1,3);++layer){
        float r=radius*layer/std::clamp(Speedometer::outlineWidth,1,3);
        for(auto dir:directions){
            // ImGui rounds text origins to whole logical pixels. Translate the
            // finished glyph vertices instead to keep fractional HUD scales exact.
            int first=d->VtxBuffer.Size;hudDrawText(d,font,fs,at,color,text);
            for(int i=first;i<d->VtxBuffer.Size;++i)d->VtxBuffer[i].pos+=dir*r;
        }
    }
}
static ImVec4 hudAccent(){const auto &t=Hud::hudTheme();return {t.rgb[0]/255.f,t.rgb[1]/255.f,t.rgb[2]/255.f,1};}
static void overlayHeader(ImVec2 p,ImVec2 size,float height,bool transparent=false){
    auto *d=ImGui::GetWindowDrawList();if(!transparent)d->AddRectFilled(p,p+size,IM_COL32(16,16,16,255),4);
    if(!transparent)d->AddRectFilled(p,{p.x+size.x,p.y+height-2},IM_COL32(20,20,20,255),4,ImDrawFlags_RoundCornersTop);
    d->AddRectFilled({p.x,p.y+height-2},{p.x+size.x,p.y+height},ImGui::ColorConvertFloat4ToU32(hudAccent()));
}
// Top info bar (watermark): style, place, palette and fields from Settings -> Watermark.
static std::vector<Watermark::Field> watermarkFields(){
    hud_player_info_t info={};int index=Hud::localIndex();if(index&&eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(index,&info);
    auto time=Hud::now();char number[32];std::vector<Watermark::Field> list;int f=Watermark::fields;
    if(f&Watermark::Nick)list.push_back({Hud::trim(playerName(),16),""});
    if(f&Watermark::Map)list.push_back({Hud::trim(Hud::mapName(),22),""});
    if(f&Watermark::Fps){snprintf(number,sizeof(number),"%.0f",Hud::fps);list.push_back({number,"fps"});}
    if(f&Watermark::Ping){snprintf(number,sizeof(number),"%d",std::max(0,static_cast<int>(info.ping)));list.push_back({number,"ms"});}
    if(f&Watermark::Speed){snprintf(number,sizeof(number),"%.0f",Speedometer::fresh(GetTickCount64())?Speedometer::speed:0.f);list.push_back({number,"u/s"});}
    if(f&Watermark::Time){snprintf(number,sizeof(number),"%02u:%02u",time.wHour,time.wMinute);list.push_back({number,""});}
    return list;
}
static void gameInfoOverlay(){
    if(!Hud::watermarkOn)return;auto &io=ImGui::GetIO();
    Watermark::modernChrome=combatHudEnabled;Watermark::modernBrand=combatLabel;Watermark::modernValue=combatSmall;Watermark::modernUnit=combatSmall;
    Watermark::draw(ImGui::GetBackgroundDrawList(),{0,0},io.DisplaySize,watermarkFields(),Watermark::fields&Watermark::Brand,Watermark::opacity/100.f,HudResolution::factor(screenW,screenH)/scale);
}
// Showkeys on screen; while the menu is open the widget can be dragged.
static bool showkeysDragging;
static void showkeysOverlay(bool menu){
    if(!Showkeys::enabled||outsideGame||Showkeys::lineCount()==0)return;
    auto &io=ImGui::GetIO();float unit=Showkeys::unitFor(HudResolution::factor(screenW,screenH)/scale);
    ImVec2 extent=Showkeys::extent(unit);extent.x+=extent.y*.2f;
    ImVec2 center(io.DisplaySize.x*Showkeys::posX/100.f,io.DisplaySize.y*Showkeys::posY/100.f);
    if(!menu){Showkeys::draw(ImGui::GetBackgroundDrawList(),center,unit,1,false,frameTime);return;}
    ImGui::SetNextWindowPos(center-extent/2,ImGuiCond_Always);ImGui::SetNextWindowSize(extent,ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("##ShowkeysOverlay",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetCursorPos({0,0});ImGui::InvisibleButton("showkeys drag",extent);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,0)){
        Showkeys::posX=std::clamp(Showkeys::posX+io.MouseDelta.x/io.DisplaySize.x*100.f,0.f,100.f);
        Showkeys::posY=std::clamp(Showkeys::posY+io.MouseDelta.y/io.DisplaySize.y*100.f,0.f,100.f);showkeysDragging=true;
    }
    if(ImGui::IsItemDeactivated()&&showkeysDragging){showkeysDragging=false;Showkeys::save();}
    auto *d=ImGui::GetWindowDrawList();bool hot=ImGui::IsItemHovered()||ImGui::IsItemActive();
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    ImVec2 p=ImGui::GetWindowPos();d->AddRect(p,p+extent,IM_COL32(255,255,255,hot?90:35),8,0,1.5f);
    Showkeys::draw(d,center,unit,1,false,frameTime);
    ImGui::End();ImGui::PopStyleVar();
}
#include "reference_chat.h"
static void fogGlowText(ImDrawList *d,ImFont *font,float size,ImVec2 p,ImU32 color,const char *text,float strength,float radius){
    for(int ring=2;ring>=1;--ring){
        float r=radius*(ring==2?1.f:.45f);int alpha=static_cast<int>((ring==2?9:18)*strength);
        for(int i=0;i<8;++i){float angle=i*.7853981634f;
            d->AddText(font,size,p+ImVec2(std::cos(angle)*r,std::sin(angle)*r),(color&~IM_COL32_A_MASK)|IM_COL32(0,0,0,alpha),text);
        }
    }
}
#include "fog_styles.h"
static void movementOverlay(bool preview=false){
    ULONGLONG now=GetTickCount64();bool fresh=Speedometer::fresh(now);const char *level=eng->pfnGetLevelName?eng->pfnGetLevelName():nullptr;
    if(!preview&&(!fresh||!level||!*level))return;
    bool measured=MovementHud::visible(now),demo=preview&&!measured;
    auto result=demo?MovementHud::Result{MovementHud::Bhop,MovementHud::previewFog,284.106f,MovementHud::previewTakeoff(now),true}:MovementHud::displayResult();
    bool statsOwnFog=false;
    FogArt value(result,fresh?Speedometer::speed:286.f,demo?45.f:MovementHud::track.gain,Speedometer::enabled,(measured&&MovementHud::showDetails(result)&&!statsOwnFog)||demo,MovementHud::showGain&&(measured||demo));
    if(!value.top&&!value.bottom)return;
    float resolution=HudResolution::factor(screenW,screenH),fs=std::max(14.f,Speedometer::size*resolution)/scale;
    auto &io=ImGui::GetIO();ImVec2 extent;float pad;
    // Include glow/backdrop/outline in the fit, not just the digit string.
    for(int pass=0;pass<4;++pass){
        pad=std::max(3.f/scale,fs*.3f);
        if(Speedometer::outline)pad=std::max(pad,(Speedometer::outlineWidth+1)/scale);
        extent=fogArtSize(MovementHud::style,fs,value)+ImVec2(pad*2,pad*2);
        float fit=HudBounds::fit(io.DisplaySize.x,io.DisplaySize.y,extent.x,extent.y,2/scale);
        if(fit>=.9999f)break;fs*=fit*.995f;
    }
    pad=std::max(3.f/scale,fs*.3f);
    if(Speedometer::outline)pad=std::max(pad,(Speedometer::outlineWidth+1)/scale);
    extent=fogArtSize(MovementHud::style,fs,value)+ImVec2(pad*2,pad*2);
    ImVec2 p={HudBounds::axis(io.DisplaySize.x,extent.x,2/scale,Speedometer::xPercent),
              HudBounds::axis(io.DisplaySize.y,extent.y,2/scale,Speedometer::yPercent)};
    fogArt(ImGui::GetForegroundDrawList(),p+ImVec2(pad,pad),MovementHud::style,fs,value,now);
}
static void speedometerOverlay(bool preview=false){
    // The FOG settings page already contains the live style gallery. Avoid a
    // second floating HUD preview covering its controls and thumbnails.
    if(preview&&ui::cur_tab==3&&ui::tabs[3].cur_subtab==1)return;
    if(MovementHud::enabled){movementOverlay(preview&&ui::cur_tab==3&&ui::tabs[3].cur_subtab==1);return;}
    if(!Speedometer::enabled)return;
    bool fresh=Speedometer::fresh(GetTickCount64());
    const char *level=eng->pfnGetLevelName?eng->pfnGetLevelName():nullptr;
    if(!preview&&(!fresh||!level||!*level))return;
    char text[32];snprintf(text,sizeof(text),"%.0f",fresh?Speedometer::speed:0.f);
    auto &io=ImGui::GetIO();ImFont *font=speedFont?speedFont:chatBold?chatBold:ImGui::GetFont();
    float fontSize=std::max(14.f,std::clamp(Speedometer::size,12,96)*HudResolution::factor(screenW,screenH))/scale;
    float margin=(Speedometer::outline?Speedometer::outlineWidth+1:2)/scale;
    font=hudFontFor(font,fontSize);auto extent=font->CalcTextSizeA(fontSize,FLT_MAX,0,text);
    float fit=HudBounds::fit(io.DisplaySize.x,io.DisplaySize.y,extent.x,extent.y,margin+1/scale);
    if(fit<1.f){fontSize*=fit;font=hudFontFor(font,fontSize);extent=font->CalcTextSizeA(fontSize,FLT_MAX,0,text);}
    ImVec2 p={HudBounds::axis(io.DisplaySize.x,extent.x,margin+1/scale,Speedometer::xPercent),
              HudBounds::axis(io.DisplaySize.y,extent.y,margin+1/scale,Speedometer::yPercent)};
    p={std::round(p.x*scale)/scale,std::round(p.y*scale)/scale};
    auto *d=ImGui::GetForegroundDrawList();
    speedometerBorder(d,font,fontSize,p,text);
    hudDrawText(d,font,fontSize,p,IM_COL32(Speedometer::rgb[0],Speedometer::rgb[1],Speedometer::rgb[2],255),text);
}
