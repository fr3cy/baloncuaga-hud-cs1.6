#pragma once
#ifdef clamp
#undef clamp
#endif
#include "weave/ui/ui.hpp"
#include "weave/imgui/imgui_freetype.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <mutex>
#include <set>
#include <map>
#include <memory>
#include "lang.h"
#include "menu_theme.h"
#include "menu_motion.h"
#include "sneaker_art.h"
#include "showkeys.h"
#include "watermark.h"
#include "ui_surface.h"
#include "hud_bounds.h"

namespace Weave {
static ImGuiContext *context;
static ImFont *chatBold,*chatSmall,*chatButtons,*speedFont,*hudRegular,*hudBold;
static int fontDensity=0,fontRequestDensity=0;
static constexpr int hudFontCount=12;
static constexpr float hudFontSizes[hudFontCount]={4,6,8,12,16,20,26,34,44,58,76,100};
static ImFont *hudRegularSizes[hudFontCount]={},*hudBoldSizes[hudFontCount]={},*speedFontSizes[hudFontCount]={};
// Menu faces are rasterized at the actual framebuffer size. The HUD keeps its
// existing font bank and density; small menu labels must not shrink a 14/17 px
// bitmap to an unrelated size and then scale it by another 95 percent.
static ImFont *menuSmall,*menuBody,*menuBold,*menuTitle;
static ImFont *combatSmall,*combatLabel,*combatValue;
#include "combat_fonts.h"
static float nativeMenuRequestScale=0.f;
static float nativeHudRequestScale=0.f;
static float scale=1.f,frameTime;
static bool nativeMenuFont(const ImFont *face){return face&&(face==menuSmall||face==menuBody||face==menuBold||face==menuTitle);}
static float menuScaleFor(int w,int h){
    auto size=MenuTheme::size();float fit=std::min((w-32.f)/size.x,(h-32.f)/size.y);
    return std::max(.1f,std::min(HudResolution::factor(w,h)*(MenuTheme::isCarbon()?1.f:.95f)*MenuTheme::auroraScale/100.f,fit));
}

static void loadCombatHud();static void saveCombatHud();
static void chatPanel(bool);
static void fogStyleGallery();
static HGLRC glContext;
static GLuint atlas,avatar;
static ULONGLONG lastFrame;
static MenuMotion::State menuMotion;
static bool menuRequested;
static float menuEffectTime;
static bool isAnimating(){return menuMotion.visible();}
// Custom previews (chat, FOG, radar) share renderers with the in-game HUD and
// may submit raw colours. Apply the window fade once to the complete menu,
// including child/popup draw lists, without touching earlier or later HUD work.
struct MenuFadeScope {
    struct Range {ImDrawList *list;int first;};std::vector<Range> before;
    void remember(ImDrawList *list){before.push_back({list,list->VtxBuffer.Size});}
    MenuFadeScope(){
        remember(ImGui::GetBackgroundDrawList());remember(ImGui::GetForegroundDrawList());
        auto &g=*ImGui::GetCurrentContext();
        for(auto *window:g.Windows)if(window->LastFrameActive==g.FrameCount)remember(window->DrawList);
    }
    void paint(ImDrawList *list,float alpha){
        int first=0;for(const auto &range:before)if(range.list==list){first=range.first;break;}
        // Snap only textured glyph quads. Carbon curves, textured avatar images,
        // antialiased edges and HUD vertices submitted outside this scope retain
        // their original geometry. Translate a complete glyph uniformly.
        if(MenuTheme::isCarbon()&&scale>0.f)for(const auto &cmd:list->CmdBuffer){
            if(cmd.UserCallback||cmd.GetTexID()!=reinterpret_cast<ImTextureID>(static_cast<intptr_t>(atlas)))continue;
            unsigned end=std::min(cmd.IdxOffset+cmd.ElemCount,static_cast<unsigned>(list->IdxBuffer.Size));
            for(unsigned i=cmd.IdxOffset;i+5<end;){
                unsigned b=cmd.VtxOffset+list->IdxBuffer[i];
                bool quad=b>=static_cast<unsigned>(first)&&b+3<static_cast<unsigned>(list->VtxBuffer.Size)&&
                    list->IdxBuffer[i+1]+cmd.VtxOffset==b+1&&list->IdxBuffer[i+2]+cmd.VtxOffset==b+2&&
                    list->IdxBuffer[i+3]+cmd.VtxOffset==b&&list->IdxBuffer[i+4]+cmd.VtxOffset==b+2&&list->IdxBuffer[i+5]+cmd.VtxOffset==b+3;
                if(quad){auto *v=list->VtxBuffer.Data+b;
                    quad=v[0].pos.y==v[1].pos.y&&v[1].pos.x==v[2].pos.x&&v[2].pos.y==v[3].pos.y&&v[3].pos.x==v[0].pos.x&&
                        v[0].uv.y==v[1].uv.y&&v[1].uv.x==v[2].uv.x&&v[2].uv.y==v[3].uv.y&&v[3].uv.x==v[0].uv.x&&v[0].uv.x!=v[2].uv.x&&v[0].uv.y!=v[2].uv.y;
                    if(quad){ImVec2 delta(std::round(v[0].pos.x*scale)/scale-v[0].pos.x,std::round(v[0].pos.y*scale)/scale-v[0].pos.y);
                        for(int n=0;n<4;++n)v[n].pos+=delta;}
                }i+=quad?6:3;
            }
        }
        for(int i=first;i<list->VtxBuffer.Size;++i){auto &color=list->VtxBuffer[i].col;
            ImU32 a=static_cast<ImU32>(((color>>IM_COL32_A_SHIFT)&255)*alpha+.5f);
            color=(color&~IM_COL32_A_MASK)|(a<<IM_COL32_A_SHIFT);}
    }
    void apply(float alpha){
        alpha=std::clamp(alpha,0.f,1.f);
        paint(ImGui::GetBackgroundDrawList(),alpha);paint(ImGui::GetForegroundDrawList(),alpha);
        auto &g=*ImGui::GetCurrentContext();
        for(auto *window:g.Windows)if(window->LastFrameActive==g.FrameCount)paint(window->DrawList,alpha);
    }
};
#include "steam_avatar.h"
static ImFont *hudFontFor(ImFont *family,float size){
    ImFont **sizes=family==hudRegular?hudRegularSizes:family==hudBold?hudBoldSizes:family==speedFont?speedFontSizes:nullptr;
    if(!sizes)return family;
    for(int i=0;i<hudFontCount;++i)if(sizes[i]&&sizes[i]->FontSize>=size-.01f)return sizes[i];
    return sizes[hudFontCount-1]?sizes[hudFontCount-1]:family;
}
static void hudDrawText(ImDrawList *draw,ImFont *face,float size,ImVec2 position,ImU32 color,const char *text){
    if(!draw||!face||!text)return;
    const int first=draw->VtxBuffer.Size;draw->AddText(face,size,position,color,text);
    // AddText floors in logical units. At fractional UI scales that is not a
    // framebuffer pixel. Shift each complete glyph, preserving its UVs/shape.
    const float pixels=std::max(.01f,scale);
    for(int i=first;i+3<draw->VtxBuffer.Size;i+=4){
        auto *quad=draw->VtxBuffer.Data+i;
        ImVec2 delta(std::round(quad[0].pos.x*pixels)/pixels-quad[0].pos.x,std::round(quad[0].pos.y*pixels)/pixels-quad[0].pos.y);
        for(int n=0;n<4;++n)quad[n].pos+=delta;
    }
}
static int screenW,screenH;
static float menuX=-1,menuY=-1;
static bool moving;
static void savePosition(){char value[32];snprintf(value,sizeof(value),"%d",static_cast<int>(std::round(menuX)));WritePrivateProfileStringA("interface","menu_x",value,iniPath);snprintf(value,sizeof(value),"%d",static_cast<int>(std::round(menuY)));WritePrivateProfileStringA("interface","menu_y",value,iniPath);}
static void clampPosition(){
    float w=ui::size.x*scale,h=ui::size.y*scale;
    if(menuX<0||menuY<0){menuX=(screenW-w)/2;menuY=std::max((screenH-h)/2,Hud::watermarkOn?48.f:8.f);}
    menuX=std::clamp(menuX,8.f,std::max(8.f,screenW-w-8));menuY=std::clamp(menuY,8.f,std::max(8.f,screenH-h-8));
}
static void positionMenu(){clampPosition();ImGui::SetNextWindowPos({menuX/scale,menuY/scale},ImGuiCond_Always);}
static void dragHeader(const char *id){
    ImGui::SetCursorPos({0,0});ImGui::InvisibleButton(id,{ImGui::GetWindowWidth(),40});
    if(ImGui::IsItemActive() && !ImGui::IsItemActivated() && ImGui::IsMouseDragging(0,0)){
        auto d=ImGui::GetIO().MouseDelta;menuX+=d.x*scale;menuY+=d.y*scale;moving=true;
    }
    if(ImGui::IsItemDeactivated() && moving){moving=false;clampPosition();savePosition();}
    if(ImGui::IsItemHovered())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
}
struct Input {UINT message;WPARAM value;bool wide;};
static std::mutex inputLock;
static std::deque<Input> inputs;
static bool useMenu=true,preferencesLoaded,glitchMenu;
// True while the menu is drawn from SwapBuffers in the game's main menu.
static bool outsideGame,menuOnly;
static void (*outsideGameDecor)(ImDrawList *,ImVec2,float);
static void savePreferences(){
    if(!preferencesLoaded)return;
    saveCombatHud();MenuTheme::save();Lang::save();Showkeys::save();Watermark::save();SpectatorResource::save();
    ChatBox::saveTheme();
    WritePrivateProfileStringA("interface","weave_menu","1",iniPath);WritePrivateProfileStringA("interface","glitch_menu",glitchMenu?"1":"0",iniPath);savePosition();
    WritePrivateProfileStringA("interface","chat_time",ChatBox::timeOn?"1":"0",iniPath);WritePrivateProfileStringA("interface","chat_server",ChatBox::serverOn?"1":"0",iniPath);WritePrivateProfileStringA("interface","chat_dead",ChatBox::deadOn?"1":"0",iniPath);
    WritePrivateProfileStringA("interface","preview_spin",Preview::spin?"1":"0",iniPath);WritePrivateProfileStringA("interface","preview_hands",Preview::showHands?"1":"0",iniPath);WritePrivateProfileStringA("interface","trainer_enabled",Trainer::enabled?"1":"0",iniPath);
}
static bool acceptsInput(){return true;}
static void event(UINT message,WPARAM value,bool wide){
    std::lock_guard<std::mutex> guard(inputLock);
    if(inputs.size()<512)inputs.push_back({message,value,wide});
}
static ImGuiKey key(int vk){
    if(vk>='A'&&vk<='Z')return static_cast<ImGuiKey>(ImGuiKey_A+vk-'A');
    if(vk>='0'&&vk<='9')return static_cast<ImGuiKey>(ImGuiKey_0+vk-'0');
    switch(vk){case VK_TAB:return ImGuiKey_Tab;case VK_LEFT:return ImGuiKey_LeftArrow;case VK_RIGHT:return ImGuiKey_RightArrow;
    case VK_UP:return ImGuiKey_UpArrow;case VK_DOWN:return ImGuiKey_DownArrow;case VK_PRIOR:return ImGuiKey_PageUp;case VK_NEXT:return ImGuiKey_PageDown;
    case VK_HOME:return ImGuiKey_Home;case VK_END:return ImGuiKey_End;case VK_DELETE:return ImGuiKey_Delete;case VK_BACK:return ImGuiKey_Backspace;
    case VK_RETURN:return ImGuiKey_Enter;case VK_SPACE:return ImGuiKey_Space;case VK_ESCAPE:return ImGuiKey_Escape;default:return ImGuiKey_None;}
}
static std::string playerName(){
    hud_player_info_t p={};int own=Hud::localIndex();if(own&&eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(own,&p);
    const char *s=p.name?p.name:eng->pfnGetCvarString?eng->pfnGetCvarString(const_cast<char *>("name")):nullptr;
    return s?Hud::clean(s,48):"Player";
}
static ImU32 fadeColor(ImU32 c){return ImGui::GetColorU32(c);}
static void changed(bool value){if(value)save();}
static void card(const char *name,ImVec2 size={0,0}){ui::begin_child(name,size);ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);}
static void endCard(){ImGui::PopItemWidth();ui::end_child();}
#include "stats_page.h"
#include "jump_stats_ui.h"
#include "server_names.h"
#include "servers.h"
#include "servers_ui.h"
#include "bratuha.h"
#include "bratuha_ui.h"

struct PreviewCommand {int x,y,w,h;std::string path;float opacity=1;};
static PreviewCommand preview;
static void modelCallback(const ImDrawList *,const ImDrawCmd *){
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    Preview::draw(preview.x,preview.y,preview.w,preview.h,screenW,screenH,preview.path.c_str(),frameTime,preview.opacity);
    glPopClientAttrib();
}
static const char *categoryTitle(int category){
    static const char *ru[]={u8"Нож",u8"HE-граната",u8"Флешка",u8"Дымовая"};
    return Lang::english()?titles[category]:ru[category];
}
static void models(int category){
    row=category;
    const float width=(ImGui::GetContentRegionAvail().x-10)/2;
    const char *stock=L(u8"Стандартная модель","Default model");
    card(L(u8"Выбор модели","Model"),{width,ImGui::GetContentRegionAvail().y});
    ImGui::TextDisabled("%s",categoryTitle(category));
    if(ImGui::Button(L(u8"Обновить список","Refresh list"),{ImGui::CalcItemWidth(),32})){Models::refresh();}
    ImGui::TextDisabled(L(u8"Свои модели: %d","Custom models: %d"),counts[category]);
    for(int i=0;i<=counts[category];++i){
        ImGui::PushID(i);const char *name=i?files[category][i-1]:stock;
        if(ImGui::Selectable(name,Models::pending[category]==i)){Models::pending[category]=i;Preview::release();}
        ImGui::PopID();
    }
    if(ImGui::Button(L(u8"Применить","Apply")))Models::apply(category,Models::pending[category]);
    if(ImGui::Button((std::string(stock)+"##reset").c_str()))Models::apply(category,0);
    ImGui::TextWrapped(L(u8"Сейчас: %s","Current: %s"),choice[category]?files[category][choice[category]-1]:stock);
    ImGui::TextWrapped("%s",Models::message.c_str());
    endCard();ImGui::SameLine();
    card(L(u8"3D-просмотр","3D preview"),{width,ImGui::GetContentRegionAvail().y});
    changed(ImGui::Checkbox(L(u8"Автовращение","Auto rotate"),&Preview::spin));changed(ImGui::Checkbox(L(u8"Показывать руки","Show hands"),&Preview::showHands));
    ImGui::SliderFloat(L(u8"Поворот","Yaw"),&Preview::yaw,-180.f,180.f,"%.0f deg");
    ImGui::SliderFloat(L(u8"Наклон","Pitch"),&Preview::pitch,-80.f,80.f,"%.0f deg");
    if(ImGui::Button(L(u8"Сбросить ракурс","Reset view"),{ImGui::CalcItemWidth(),28})){Preview::yaw=Preview::pitch=0;Preview::spin=true;}
    auto p=ImGui::GetCursorScreenPos();auto a=ImGui::GetContentRegionAvail();a.y=std::max(35.f,a.y);
    char path[128];if(Models::pending[category]>0&&Models::pending[category]<=counts[category])snprintf(path,sizeof(path),"models/customizer/%s/%s",categories[category],files[category][Models::pending[category]-1]);else snprintf(path,sizeof(path),"models/%s",stockNames[category]);
    preview={static_cast<int>(p.x*scale),static_cast<int>(p.y*scale),static_cast<int>(a.x*scale),static_cast<int>(a.y*scale),path,ImGui::GetStyle().Alpha};
    ImGui::GetWindowDrawList()->AddCallback(modelCallback,nullptr);ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    ImGui::Dummy(a);endCard();
}
#include "outfits_page.h"
static void camera(){
    row=4;card(L(u8"Камера","Camera"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    int value=fov;ImGui::SliderInt(L(u8"Угол обзора","Field of view"),&fov,60,120,"%d deg");changed(value!=fov);
    if(ImGui::Button(L(u8"Стандартный FOV","Default FOV"),{ImGui::CalcItemWidth(),32})){fov=90;save();}endCard();
}
static void crosshair(){
    row=5;float w=(ImGui::GetContentRegionAvail().x-10)/2;
    static const char *behaviorsEn[]={"Static","Dynamic","Hybrid"};
    static const char *colorsEn[]={"Green","White","Cyan","Yellow","Red","Purple","Orange","Blue","Custom RGB"};
    const char *const *behaviors=Lang::english()?behaviorsEn:CrossConfig::behaviors;const char *const *colors=Lang::english()?colorsEn:CrossConfig::colors;
    card(L(u8"Прицел","Crosshair"),{w,ImGui::GetContentRegionAvail().y});
    if(ImGui::Combo(L(u8"Вид","Type"),&crossStyle,crossStyles,13)){crossPending=true;save();}
    if(crossStyle==12){
        bool edit=false;edit|=ImGui::Combo(L(u8"Стиль","Style"),&CrossConfig::behavior,behaviors,3);
        edit|=ImGui::SliderInt(L(u8"Размер","Length"),&CrossConfig::length,0,40,u8"%d px");
        edit|=ImGui::SliderInt(L(u8"Толщина","Thickness"),&CrossConfig::thickness,1,10,u8"%d px");
        edit|=ImGui::SliderInt(L(u8"Зазор","Gap"),&CrossConfig::gap,0,30,u8"%d px");
        edit|=ImGui::Checkbox(L(u8"Точка в центре","Center dot"),&CrossConfig::dot);
        if(ImGui::Combo(L(u8"Цвет","Color"),&CrossConfig::preset,colors,9)){if(CrossConfig::preset<8)for(int i=0;i<3;++i)CrossConfig::rgb[i]=CrossConfig::values[CrossConfig::preset][i];edit=true;}
        float color[3]={CrossConfig::rgb[0]/255.f,CrossConfig::rgb[1]/255.f,CrossConfig::rgb[2]/255.f};
        if(ImGui::ColorEdit3("RGB",color,ImGuiColorEditFlags_DisplayRGB)){for(int i=0;i<3;++i)CrossConfig::rgb[i]=std::clamp(static_cast<int>(std::round(color[i]*255)),0,255);CrossConfig::preset=8;edit=true;}
        edit|=ImGui::Checkbox(L(u8"Контур","Outline"),&CrossConfig::outline);
        if(CrossConfig::outline)edit|=ImGui::SliderInt(L(u8"Толщина контура","Outline width"),&CrossConfig::outlineWidth,1,3,u8"%d px");
        edit|=ImGui::SliderInt(L(u8"Прозрачность","Transparency"),&CrossConfig::transparency,0,100,"%d%%");
        if(ImGui::Button(L(u8"Сбросить прицел","Reset crosshair"),{ImGui::CalcItemWidth(),28})){CrossConfig::reset();edit=true;}if(edit)save();
    }else if(ImGui::Combo(L(u8"Цвет","Color"),&crossColor,crossColors,9)){crossPending=true;save();}
    if(ImGui::Button(L(u8"Как в игре","Game default"),{ImGui::CalcItemWidth(),32})){crossStyle=crossColor=0;crossPending=true;save();}
    endCard();ImGui::SameLine();card(L(u8"Предпросмотр","Preview"),{w,ImGui::GetContentRegionAvail().y});
    auto p=ImGui::GetCursorScreenPos();auto a=ImGui::GetContentRegionAvail();auto *d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,p+a,fadeColor(IM_COL32(16,16,16,255)),4);ImVec2 c=p+a/2;
    if(crossStyle==12){
        float movement=std::max(0.f,std::sin(frameTime*1.4f))*200.f;float fire=std::max(0.f,std::sin(frameTime*5.f))*4.f;
        auto rectangles=CrossConfig::geometry(static_cast<int>(c.x),static_cast<int>(c.y),CrossConfig::expansion(movement,fire));int alpha=CrossConfig::alpha();d->PushClipRect(p,p+a,true);
        for(bool border:{true,false})for(const auto &r:rectangles)if(r.border==border)d->AddRectFilled({static_cast<float>(r.x),static_cast<float>(r.y)},{static_cast<float>(r.x+r.w),static_cast<float>(r.y+r.h)},border?fadeColor(IM_COL32(0,0,0,alpha)):fadeColor(IM_COL32(CrossConfig::rgb[0],CrossConfig::rgb[1],CrossConfig::rgb[2],alpha)));
        d->AddText(p+ImVec2(10,10),fadeColor(IM_COL32(160,160,160,255)),L(u8"Анимация движения и стрельбы","Movement and firing animation"));d->PopClipRect();ImGui::Dummy(a);endCard();return;
    }
    int *rgb=const_cast<int *>(palette[crossColor]);ImU32 col=fadeColor(IM_COL32(rgb[0],rgb[1],rgb[2],255));
    float gap=crossStyle==8?3.f:crossStyle==9?0.f:5.f,length=crossStyle==8?6.f:9.f;
    if(crossStyle==11||crossStyle>=4&&crossStyle<=6)gap+=std::sin(frameTime*3.f)*2.f+2.f;
    auto line=[&](ImVec2 b,ImVec2 e){d->AddLine(b,e,fadeColor(IM_COL32(0,0,0,255)),4);d->AddLine(b,e,col,2);};
    if(crossStyle==9){d->AddCircleFilled(c,3,fadeColor(IM_COL32(0,0,0,255)));d->AddCircleFilled(c,2,col);}
    else{line(c+ImVec2(-gap-length,0),c+ImVec2(-gap,0));line(c+ImVec2(gap,0),c+ImVec2(gap+length,0));line(c+ImVec2(0,gap),c+ImVec2(0,gap+length));if(crossStyle!=10)line(c+ImVec2(0,-gap-length),c+ImVec2(0,-gap));}
    ImGui::Dummy(a);endCard();
}
// ---- Radar preview: a sample map that follows the radar settings live ---------
namespace RadarDemo {
// Sample map in world units (0..2400): floors, crates and a loop the player runs.
static const float floors[][4]={{200,200,800,300},{200,500,300,1200},{900,200,300,900},{1200,800,900,300},{1700,1100,500,1000},{200,1700,1500,300},{1500,300,700,400},{1200,500,300,300}};
static const float crates[][3]={{260,260,90},{360,230,64},{700,380,110},{620,230,64},{960,600,90},{1110,230,70},{1280,860,100},{1600,1000,80},{1760,1180,120},{2050,1500,90},{2060,1950,110},{1800,1980,70},{900,1760,100},{560,1900,80},{240,1100,90},{400,800,70},{1600,360,110},{2050,560,90},{1300,560,80}};
static const ImVec2 path[]={{350,350},{1050,350},{1050,950},{1500,950},{1950,950},{1950,1300},{1950,1850},{1100,1850},{350,1850},{350,700}};
static constexpr int pathCount=sizeof(path)/sizeof(*path);
static float yaw=0;
static float pathLength(){float total=0;for(int i=0;i<pathCount;++i){ImVec2 a=path[i],b=path[(i+1)%pathCount];total+=std::hypot(b.x-a.x,b.y-a.y);}return total;}
static ImVec2 along(float distance,float *angle=nullptr){
    static const float total=pathLength();distance=std::fmod(std::fmod(distance,total)+total,total);
    for(int i=0;i<pathCount;++i){ImVec2 a=path[i],b=path[(i+1)%pathCount];float len=std::hypot(b.x-a.x,b.y-a.y);
        if(distance<=len){if(angle)*angle=std::atan2(b.y-a.y,b.x-a.x);float t=distance/len;return a+(b-a)*t;}distance-=len;}
    return path[0];
}
static void quad(ImDrawList *d,const ImVec2 *q,ImU32 col){d->AddQuadFilled(q[0],q[1],q[2],q[3],col);}
static void draw(ImVec2 origin,ImVec2 area){
    auto *d=ImGui::GetWindowDrawList();
    d->AddRectFilled(origin,origin+area,fadeColor(IM_COL32(12,14,22,255)),10);   // same colour as the corner masks below
    // "Radar size" 0..100% maps to 75..100% of the preview, so the change is visible.
    float px=std::floor((std::min(area.x,area.y)-24)*(.75f+.25f*Hud::radarSizePercent/100.f));if(px<60)return;
    ImVec2 box=origin+ImVec2(std::floor((area.x-px)/2),std::floor((area.y-px)/2)),c=box+ImVec2(px,px)*.5f;
    float t=static_cast<float>(ImGui::GetTime()),angle=0;ImVec2 me=along(t*260,&angle);
    float da=std::atan2(std::sin(angle-yaw),std::cos(angle-yaw));yaw+=da*std::min(1.f,ImGui::GetIO().DeltaTime*6);
    // Rotating view centred on the player, or the fixed whole-map view when "Center map" is on.
    bool fixed=Hud::radarCentered;float s=fixed?px/2400.f*Hud::radarCenteredZoom/100.f:(px/2)/std::max(1,Hud::range);
    float th=fixed?0:-1.5707963f-yaw,cs=std::cos(th),sn=std::sin(th);ImVec2 focus=fixed?ImVec2(1200,1200):me;
    auto map=[&](float x,float y){float rx=x-focus.x,ry=y-focus.y;return c+ImVec2((rx*cs-ry*sn)*s,(rx*sn+ry*cs)*s);};
    auto rect=[&](float x,float y,float w,float h,ImVec2 *q){q[0]=map(x,y);q[1]=map(x+w,y);q[2]=map(x+w,y+h);q[3]=map(x,y+h);};
    const ImU32 bg=fadeColor(IM_COL32(11,13,18,255)),sand=fadeColor(IM_COL32(168,138,85,255)),white=fadeColor(IM_COL32(255,255,255,235));
    // The map is clipped to the inside of the border, never to the outer edge: at fractional
    // menu scales the outer pixel of the border is only partly covered and the map showed through.
    const float r=14,bw=Hud::radarBorderOn?static_cast<float>(Hud::radarBorderWidth):0.f,inset=bw>0?std::ceil(bw):0.f;
    const ImVec2 inMin=box+ImVec2(inset,inset),inMax=box+ImVec2(px-inset,px-inset);
    d->AddRectFilled(box,box+ImVec2(px,px),bg,r);
    d->PushClipRect(inMin,inMax,true);
    ImVec2 q[4];
    for(auto &f:floors){rect(f[0],f[1],f[2],f[3],q);quad(d,q,sand);}
    for(auto &f:floors){rect(f[0],f[1],f[2],f[3],q);d->AddPolyline(q,4,white,ImDrawFlags_Closed,std::max(1.f,7*s));}
    for(auto &f:floors){rect(f[0]+4,f[1]+4,f[2]-8,f[3]-8,q);quad(d,q,sand);}                // only outer edges stay white
    for(auto &f:floors)for(int i=0;i<18;++i){float u=std::fmod((i*73.1f+f[0])*.6180339f,1.f),v=std::fmod((i*37.7f+f[1])*.4142135f,1.f);
        ImVec2 dot=map(f[0]+8+u*(f[2]-16),f[1]+8+v*(f[3]-16));d->AddRectFilled(dot,dot+ImVec2(1.5f,1.5f),i%2?fadeColor(IM_COL32(90,66,30,70)):fadeColor(IM_COL32(255,236,190,45)));}
    for(auto &k:crates){rect(k[0],k[1],k[2],k[2],q);quad(d,q,fadeColor(IM_COL32(110,79,42,255)));
        float lw=std::max(1.f,5*s);d->AddLine(q[0],q[2],fadeColor(IM_COL32(40,24,6,205)),lw);d->AddLine(q[1],q[3],fadeColor(IM_COL32(40,24,6,205)),lw);d->AddPolyline(q,4,white,ImDrawFlags_Closed,std::max(1.f,6*s));}
    for(float back:{320.f,700.f}){ImVec2 a=map(along(t*260-back).x,along(t*260-back).y);d->AddCircleFilled(a,6,fadeColor(IM_COL32(56,201,255,255)),20);d->AddCircle(a,6,fadeColor(IM_COL32(4,19,28,255)),20,2);}
    {ImVec2 e=map(1800+std::sin(t*.9f)*260,1300+std::cos(t*.7f)*80);float k=7;d->AddLine(e-ImVec2(k,k),e+ImVec2(k,k),fadeColor(IM_COL32(255,59,77,255)),3);d->AddLine(e+ImVec2(k,-k),e+ImVec2(-k,k),fadeColor(IM_COL32(255,59,77,255)),3);}
    // Black arrow with a white outline; in the rotating view it always points up.
    ImVec2 at=fixed?map(me.x,me.y):c;float dir=fixed?yaw+1.5707963f:0,ac=std::cos(dir),as=std::sin(dir),a=6+Hud::radarArrowPercent*.1f;
    auto rot=[&](float x,float y){return at+ImVec2(x*ac-y*as,x*as+y*ac);};
    ImVec2 tip=rot(0,-a),right=rot(a*.72f,a*.8f),notch=rot(0,a*.42f),left=rot(-a*.72f,a*.8f);
    d->AddTriangleFilled(tip,right,notch,fadeColor(IM_COL32(0,0,0,255)));d->AddTriangleFilled(tip,notch,left,fadeColor(IM_COL32(0,0,0,255)));
    ImVec2 outline[4]={tip,right,notch,left};d->AddPolyline(outline,4,white,ImDrawFlags_Closed,1.4f);
    d->PopClipRect();
    // Round the inner corners: the map outside the inner arc is painted with the radar backing,
    // so it stays hidden under a see-through border and with the border turned off.
    const float ri=std::max(0.f,r-inset);
    if(ri>0){ImVec2 corners[4]={inMin,ImVec2(inMax.x,inMin.y),inMax,ImVec2(inMin.x,inMax.y)};
        for(int i=0;i<4;++i){ImVec2 centre=corners[i]+ImVec2(i==0||i==3?ri:-ri,i<2?ri:-ri);float start=3.14159265f+i*1.5707963f;
            d->PathLineTo(corners[i]);d->PathArcTo(centre,ri,start,start+1.5707963f,8);d->PathFillConvex(bw>0?bg:fadeColor(IM_COL32(12,14,22,255)));}}
    // Border covers the whole band between the outer edge and the clipped map, with a
    // concentric rounding; it overlaps the map by half a pixel so no seam is left.
    if(bw>0){float half=(inset+.5f)/2;
        d->AddRect(box+ImVec2(half,half),box+ImVec2(px,px)-ImVec2(half,half),fadeColor(IM_COL32(Hud::radarBorderRgb[0],Hud::radarBorderRgb[1],Hud::radarBorderRgb[2],Hud::radarBorderOpacity*255/100)),std::max(0.f,r-half),0,inset+.5f);}
}
}
static void radarPreviewCard(ImVec2 size){
    card(L(u8"Предпросмотр","Preview"),size);
    ImVec2 origin=ImGui::GetCursorScreenPos();
    ImVec2 area(ImGui::GetContentRegionAvail().x,std::max(160.f,ImGui::GetContentRegionAvail().y-6));
    ImGui::Dummy(area);ImGui::GetWindowDrawList()->PushClipRect(origin,origin+area,true);RadarDemo::draw(origin,area);ImGui::GetWindowDrawList()->PopClipRect();
    endCard();
}
static int fogPart=0;
static void hudPage(int section){
    // Radar: live preview on the left, settings on the right. FOG keeps its style gallery on the right.
    row=9;
    if(section==3){
        if(ImGui::Button("FOG"))fogPart=0;ImGui::SameLine();if(ImGui::Button(L(u8"Стрейфы","Strafes")))fogPart=1;
        if(fogPart==1){jumpStatsPage();return;}
    }
    if(section==2){float h=ImGui::GetContentRegionAvail().y,w=std::floor((ImGui::GetContentRegionAvail().x-10)*.56f);radarPreviewCard({w,h});ImGui::SameLine();}
    card(section==2?L(u8"Радар","Radar"):"FOG",{section==3?0.f:ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    if(section==2){
        changed(ImGui::Checkbox(L(u8"Текстурный радар","Textured radar"),&Hud::radarOn));
        changed(ImGui::Checkbox(L(u8"Центрировать карту","Center map"),&Hud::radarCentered));
        changed(ImGui::Checkbox(L(u8"В здании — только здание","Inside a building: only that building"),&Hud::radarInsideOnly));
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",L(u8"Под крышей радар показывает только это здание изнутри, улицу — после выхода.","Under a roof the radar shows only that building from inside; the outside comes back when you leave."));
        if(Hud::radarCentered){
            int oldZoom=Hud::radarCenteredZoom;ImGui::SliderInt(L(u8"Масштаб карты","Map zoom"),&Hud::radarCenteredZoom,100,400,"%d%%");changed(oldZoom!=Hud::radarCenteredZoom);
        }
        if(ImGui::Checkbox(L(u8"3D-радар","3D radar"),&Hud::radar3D))Hud::save();
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",L(u8"Карта в объёме: стены и здания с текстурами карты, диск вокруг тебя.","The map in 3D: walls and buildings with the map's textures, a disc around you."));
        int oldArrow=Hud::radarArrowPercent;ImGui::SliderInt(L(u8"Размер стрелки","Arrow size"),&Hud::radarArrowPercent,30,150,"%d%%");changed(oldArrow!=Hud::radarArrowPercent);
        if(!Hud::radarCentered){int old=Hud::range;ImGui::SliderInt(L(u8"Радиус карты","Map range"),&Hud::range,200,2200,L(u8"%d ед.","%d u"));changed(old!=Hud::range);}
        int oldSize=Hud::radarSizePercent;ImGui::SliderInt(L(u8"Размер радара","Radar size"),&Hud::radarSizePercent,0,100,u8"+%d%%");changed(oldSize!=Hud::radarSizePercent);
        ImGui::Separator();
        bool borderEdit=ImGui::Checkbox(L(u8"Обводка радара","Radar border"),&Hud::radarBorderOn);
        if(Hud::radarBorderOn){
            float borderColor[3]={Hud::radarBorderRgb[0]/255.f,Hud::radarBorderRgb[1]/255.f,Hud::radarBorderRgb[2]/255.f};
            if(ImGui::ColorEdit3(L(u8"Цвет обводки","Border color"),borderColor,ImGuiColorEditFlags_DisplayRGB)){
                for(int i=0;i<3;++i)Hud::radarBorderRgb[i]=std::clamp(static_cast<int>(std::round(borderColor[i]*255)),0,255);borderEdit=true;
            }
            borderEdit|=ImGui::SliderInt(L(u8"Толщина обводки","Border width"),&Hud::radarBorderWidth,1,10,u8"%d px");
            borderEdit|=ImGui::SliderInt(L(u8"Непрозрачность обводки","Border opacity"),&Hud::radarBorderOpacity,0,100,"%d%%");
        }
        if(ImGui::Button(L(u8"Сбросить обводку","Reset border"))){
            Hud::radarBorderOn=true;Hud::radarBorderWidth=2;Hud::radarBorderOpacity=100;
            Hud::radarBorderRgb[0]=30;Hud::radarBorderRgb[1]=45;Hud::radarBorderRgb[2]=48;borderEdit=true;
        }
        changed(borderEdit);
    }
    if(section==3){
        if(ImGui::Combo(L(u8"Стиль FOG","FOG style"),&MovementHud::style,MovementHud::styleNames,MovementHud::StyleCount))save();
        ImGui::Separator();
        ImGui::TextDisabled(L(u8"Спидометр","Speedometer"));
        bool edit=ImGui::Checkbox(L(u8"Показывать спидометр","Show speedometer"),&Speedometer::enabled);
        float color[3]={Speedometer::rgb[0]/255.f,Speedometer::rgb[1]/255.f,Speedometer::rgb[2]/255.f};
        if(ImGui::ColorEdit3(L(u8"Цвет цифр","Digit color"),color,ImGuiColorEditFlags_DisplayRGB)){for(int i=0;i<3;++i)Speedometer::rgb[i]=std::clamp(static_cast<int>(std::round(color[i]*255)),0,255);edit=true;}
        edit|=ImGui::Checkbox(L(u8"Обводка спидометра","Speedometer outline"),&Speedometer::outline);
        if(Speedometer::outline){
            edit|=ImGui::SliderInt(L(u8"Толщина обводки","Outline width"),&Speedometer::outlineWidth,1,3,"%d px");
            float border[3]={Speedometer::outlineRgb[0]/255.f,Speedometer::outlineRgb[1]/255.f,Speedometer::outlineRgb[2]/255.f};
            if(ImGui::ColorEdit3(L(u8"Цвет обводки","Outline color"),border,ImGuiColorEditFlags_DisplayRGB)){for(int i=0;i<3;++i)Speedometer::outlineRgb[i]=std::clamp(static_cast<int>(std::round(border[i]*255)),0,255);edit=true;}
        }
        if(ImGui::Button(L(u8"Сбросить спидометр","Reset speedometer"))){Speedometer::defaults();edit=true;}
        ImGui::Separator();ImGui::TextDisabled("FOG");
        edit|=ImGui::Checkbox(L(u8"HUD как на образце","Reference-style HUD"),&MovementHud::enabled);
        edit|=ImGui::SliderInt(L(u8"Длительность эффекта","Effect duration"),&MovementHud::animationMs,300,1600,L(u8"%d мс","%d ms"));
        edit|=ImGui::Checkbox(L(u8"Прирост после отрыва","Gain after takeoff"),&MovementHud::showGain);
        edit|=ImGui::Checkbox(L(u8"FOG до следующего отрыва","Keep FOG until next takeoff"),&MovementHud::keep);
        if(!MovementHud::keep)edit|=ImGui::SliderInt(L(u8"Время показа FOG","FOG display time"),&MovementHud::holdMs,1000,15000,L(u8"%d мс","%d ms"));
        edit|=ImGui::SliderInt(L(u8"Размер спидометра и FOG","Speedometer / FOG size"),&Speedometer::size,12,96,u8"%d px");
        edit|=ImGui::SliderInt(L(u8"HUD по X","HUD X"),&Speedometer::xPercent,0,100,"%d%%");
        edit|=ImGui::SliderInt(L(u8"HUD по Y","HUD Y"),&Speedometer::yPercent,0,100,"%d%%");
        if(ImGui::Button(L(u8"Сбросить оформление FOG","Reset FOG style"))){MovementHud::defaults();edit=true;}changed(edit);
    }
    endCard();if(section==3){ImGui::SameLine();fogStyleGallery();}
}
// ---- Scroll trainer (same look as the site demo) ------------------------------
// Green: on target (20 +/- 2 ms), amber: close (+/- 6 ms), red: off.
static ImU32 trainerColor(double ms,float alpha=1){
    double off=std::abs(ms-20.);int a=static_cast<int>(alpha*255);
    return off<=2?fadeColor(IM_COL32(52,211,153,a)):off<=6?fadeColor(IM_COL32(245,165,36,a)):fadeColor(IM_COL32(255,77,94,a));
}
static void trainerZone(float height){
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),size(ImGui::GetContentRegionAvail().x,height),c=p+size*.5f;
    ImGui::Dummy(size);
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(17,18,29,255)),10);
    d->PushClipRect(p+ImVec2(1,1),p+size-ImVec2(1,1),true);
    for(int i=10;i>=1;--i)d->AddCircleFilled(c,size.x*.055f*i,fadeColor(MenuTheme::accentColor(0,.018f)),64);   // soft accent glow
    d->PopClipRect();
    d->AddRect(p,p+size,fadeColor(MenuTheme::accentColor(0,Trainer::enabled?.75f:.35f)),10,0,1.2f);
    bool any=!Trainer::recent.empty();double last=any?Trainer::recent.back():0;
    char digits[16];snprintf(digits,sizeof(digits),any?"%.0f":"-",last);
    ImFont *big=hudFontFor(speedFont?speedFont:font(4),44),*unit=font(4);float bs=44,us=26;
    ImVec2 dw=big->CalcTextSizeA(bs,FLT_MAX,0,digits),uw=any?unit->CalcTextSizeA(us,FLT_MAX,0," ms"):ImVec2();
    ImU32 col=any?trainerColor(last):ImGui::GetColorU32(ImGuiCol_Text);
    float x=c.x-(dw.x+uw.x)/2,y=c.y-dw.y/2-10;
    d->AddText(big,bs,{std::round(x),std::round(y)},col,digits);
    if(any)d->AddText(unit,us,{std::round(x+dw.x),std::round(y+dw.y-uw.y-3)},col," ms");
    const char *hint=Trainer::enabled?L(u8"Крути колесо","Scroll the wheel"):L(u8"Нажми «Начать»","Press Start");
    ImVec2 hw=font(0)->CalcTextSizeA(font(0)->FontSize,FLT_MAX,0,hint);
    d->AddText(font(0),font(0)->FontSize,{std::round(c.x-hw.x/2),std::round(y+dw.y+6)},ImGui::GetColorU32(ImGuiCol_TextDisabled),hint);
}
static void trainerBars(float height){
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),size(ImGui::GetContentRegionAvail().x,height);ImGui::Dummy(size);
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(11,13,20,255)),10);
    const float pad=8,top=p.y+pad,bottom=p.y+size.y-pad,span=bottom-top;auto yOf=[&](double ms){return bottom-static_cast<float>(std::clamp(ms/60.,0.,1.))*span;};
    d->AddRectFilled({p.x,yOf(22)},{p.x+size.x,yOf(18)},fadeColor(IM_COL32(52,211,153,20)));                 // target band 18-22 ms
    for(float x=p.x;x<p.x+size.x;x+=8){d->AddLine({x,yOf(22)},{std::min(x+4,p.x+size.x),yOf(22)},fadeColor(IM_COL32(52,211,153,150)));d->AddLine({x,yOf(18)},{std::min(x+4,p.x+size.x),yOf(18)},fadeColor(IM_COL32(52,211,153,150)));}
    size_t n=Trainer::recent.size();if(!n)return;
    float gap=3,bw=(size.x-pad*2-gap*(n-1))/n;
    for(size_t i=0;i<n;++i){double ms=Trainer::recent[i];float x=p.x+pad+i*(bw+gap),y=std::min(yOf(std::max(ms,3.)),bottom-2);
        d->AddRectFilled({x,y},{x+bw,bottom},trainerColor(ms),3,ImDrawFlags_RoundCornersTop);}
}
static void trainerTile(const char *label,const char *value,float width){
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),size(width,58);ImGui::Dummy(size);
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(11,13,20,255)),10);
    d->AddText(font(0),font(0)->FontSize,p+ImVec2(12,9),ImGui::GetColorU32(ImGuiCol_TextDisabled),label);
    d->AddText(font(4),font(4)->FontSize,p+ImVec2(12,29),ImGui::GetColorU32(ImGuiCol_Text),value);
}
static void trainer(){
    row=11;float avail=ImGui::GetContentRegionAvail().x,w=std::floor((avail-10)*.58f),h=ImGui::GetContentRegionAvail().y;
    card(L(u8"Тренажёр колеса","Scroll trainer"),{w,h});
    trainerZone(std::max(150.f,ImGui::GetContentRegionAvail().y*.42f));
    trainerBars(110);
    {float tw=(ImGui::GetContentRegionAvail().x-16)/3;char a[32],b[32],c[32];
     snprintf(a,sizeof(a),"%llu",static_cast<unsigned long long>(Trainer::totalPatterns));snprintf(b,sizeof(b),"%llu",static_cast<unsigned long long>(Trainer::totalScrolls));
     snprintf(c,sizeof(c),"%.1f%%",Trainer::intervals?100.*Trainer::validIntervals/Trainer::intervals:0.);
     trainerTile(L(u8"Серий","Patterns"),a,tw);ImGui::SameLine(0,8);trainerTile(L(u8"Прокруток","Wheel events"),b,tw);ImGui::SameLine(0,8);trainerTile(L(u8"Точность","Accuracy"),c,tw);}
    endCard();ImGui::SameLine();
    card(L(u8"Цель","Target"),{ImGui::GetContentRegionAvail().x,h});
    ImGui::PushFont(MenuTheme::isCarbon()?menuBold:font(4));ImGui::TextUnformatted(L(u8"Цель: 20 мс ± 2 мс","Target: 20 ms +/- 2 ms"));ImGui::PopFont();
    ImGui::TextDisabled("%s",L(u8"Пауза серии: 100 мс","Pattern gap: 100 ms"));
    {float bw=(ImGui::CalcItemWidth()-8)/2;
     if(ImGui::Button(Trainer::enabled?L(u8"Пауза","Pause"):L(u8"Начать","Start"),{bw,32}))Trainer::toggle();ImGui::SameLine(0,8);
     if(ImGui::Button(L(u8"Сбросить","Reset"),{bw,32}))Trainer::reset();}
    ImGui::Separator();ImGui::TextDisabled("%s",L(u8"История серий","Pattern history"));
    auto current=Trainer::describe(Trainer::current);if(!current.events.empty()){
        ImGui::TextColored({215.f/255,96.f/255,20.f/255,1},L(u8"Сейчас: %s","Now: %s"),Trainer::sequence(current).c_str());ImGui::Text("%.2f ms / %.1f%%",current.average,current.accuracy);ImGui::Separator();}
    for(const auto &p:Trainer::history){ImGui::Text("%s",Trainer::sequence(p).c_str());ImGui::TextDisabled("%.2f ms  /  %.1f%%",p.average,p.accuracy);ImGui::Separator();}endCard();
}
static char search[128];static uint64_t selectedMessage;
static void chatSettingsPage(){
    row=10;card(L(u8"Настройки чата","Chat settings"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    changed(ImGui::Checkbox(L(u8"Интерфейс чата","Chat interface"),&Hud::chatOn));
    bool edit=ImGui::Combo(L(u8"Тема","Theme"),&ChatBox::theme,ChatBox::themeNames,ChatBox::ThemeCount);
    edit|=ImGui::SliderInt(L(u8"Размер текста","Text size"),&ChatBox::fontPercent,70,150,"%d%%");
    edit|=ImGui::SliderInt(L(u8"Ширина чата","Chat width"),&ChatBox::widthPercent,50,160,"%d%%");
    edit|=ImGui::SliderInt(L(u8"Строк на экране","Visible rows"),&ChatBox::lineCount,2,12);
    if(ChatBox::theme==ChatBox::Css){
        edit|=ImGui::Checkbox(L(u8"Время сообщений","Message timestamps"),&ChatBox::timeOn);
        edit|=ImGui::Checkbox(L(u8"Сообщения сервера","Server messages"),&ChatBox::serverOn);
        edit|=ImGui::Checkbox(L(u8"Сообщения мёртвых игроков","Dead-player messages"),&ChatBox::deadOn);
        float color[3]={Hud::chatBorderRgb[0]/255.f,Hud::chatBorderRgb[1]/255.f,Hud::chatBorderRgb[2]/255.f};
        if(ImGui::ColorEdit3(L(u8"Обводка чата","Chat border"),color,ImGuiColorEditFlags_DisplayRGB)){for(int i=0;i<3;++i)Hud::chatBorderRgb[i]=std::clamp(static_cast<int>(std::round(color[i]*255)),0,255);Hud::save();}
    }
    if(ImGui::Button(L(u8"Сбросить размеры","Reset dimensions"))){ChatBox::fontPercent=ChatBox::widthPercent=100;ChatBox::lineCount=5;edit=true;}
    if(edit){Hud::chatScroll=0;savePreferences();}
    endCard();
}
static void historyPage(){
    row=10;card(L(u8"Чат","Chat"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    chatPanel(true);endCard();
}
// ---- Small widgets for the new settings pages ----------------------------
// Pill-shaped segmented choice, e.g. Left | Center | Right.
static bool segmented(const char *id,int *value,const char *const *labels,int count){
    ImGui::PushID(id);auto *d=ImGui::GetWindowDrawList();float w=ImGui::CalcItemWidth();ImVec2 p=ImGui::GetCursorScreenPos();const float h=28,seg=w/count;bool changedValue=false;
    d->AddRectFilled(p,p+ImVec2(w,h),ImGui::GetColorU32(ImGuiCol_FrameBg),8);d->AddRect(p,p+ImVec2(w,h),ImGui::GetColorU32(ImGuiCol_Border),8);
    for(int i=0;i<count;++i){
        ImVec2 a=p+ImVec2(seg*i+2,2),b=a+ImVec2(seg-4,h-4);ImGui::SetCursorScreenPos(a);ImGui::PushID(i);bool hit=ImGui::InvisibleButton("segment",b-a);bool hot=ImGui::IsItemHovered();ImGui::PopID();
        if(*value==i){int start=d->VtxBuffer.Size;d->AddRectFilled(a,b,fadeColor(IM_COL32_WHITE),6);ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,a,{b.x,a.y},MenuTheme::accentColor(0),MenuTheme::accentColor(1));}
        else if(hot)d->AddRectFilled(a,b,ImGui::GetColorU32(ImGuiCol_FrameBgHovered),6);
        auto ts=ImGui::CalcTextSize(labels[i]);d->AddText((a+b)/2-ts/2,*value==i?(MenuTheme::isCarbon()?fadeColor(IM_COL32(14,14,14,255)):fadeColor(IM_COL32_WHITE)):ImGui::GetColorU32(hot?ImGuiCol_Text:ImGuiCol_TextDisabled),labels[i]);
        if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);if(hit&&*value!=i){*value=i;changedValue=true;}
    }
    ImGui::SetCursorScreenPos(p);ImGui::Dummy({w,h});ImGui::PopID();return changedValue;
}
// Toggle chip with a check mark (Fields: Brand, Nick, ...).
static bool chip(const char *label,bool on){
    auto *d=ImGui::GetWindowDrawList();ImVec2 ts=ImGui::CalcTextSize(label),size(ts.x+38,28),p=ImGui::GetCursorScreenPos();
    ImGui::PushID(label);bool hit=ImGui::InvisibleButton("chip",size);bool hot=ImGui::IsItemHovered();ImGui::PopID();
    ImU32 acc=MenuTheme::accentColor(0);ImVec4 a=ImGui::ColorConvertU32ToFloat4(acc);
    d->AddRectFilled(p,p+size,ImGui::GetColorU32(on?ImVec4(a.x,a.y,a.z,.16f):ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)),6);
    d->AddRect(p,p+size,ImGui::GetColorU32(on?ImVec4(a.x,a.y,a.z,.85f):ImVec4(1,1,1,hot?.25f:.12f)),6,0,1.4f);
    if(on){ImVec2 c=p+ImVec2(13,size.y/2);d->AddLine(c+ImVec2(-4,0),c+ImVec2(-1,3.5f),fadeColor(acc),2);d->AddLine(c+ImVec2(-1,3.5f),c+ImVec2(5,-4),fadeColor(acc),2);}
    d->AddText(p+ImVec2(26,(size.y-ts.y)/2),ImGui::GetColorU32(on?ImGuiCol_Text:ImGuiCol_TextDisabled),label);
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);return hit;
}
// Colour square that opens the picker; edits an int RGB triple.
static bool swatch(const char *id,int rgb[3]){
    float col[4]={rgb[0]/255.f,rgb[1]/255.f,rgb[2]/255.f,1};
    if(!ImGui::ColorEdit4(id,col,NO_ALPHA))return false;
    for(int i=0;i<3;++i)rgb[i]=std::clamp(static_cast<int>(std::round(col[i]*255)),0,255);return true;
}
// Palette card: mini menu preview tinted with the palette and its name.
static bool paletteCard(int index,bool selected,ImVec2 size){
    ImGui::PushID(index+900);ImVec2 p=ImGui::GetCursorScreenPos();bool hit=ImGui::InvisibleButton("palette",size);bool hot=ImGui::IsItemHovered();ImGui::PopID();
    auto *d=ImGui::GetWindowDrawList();ImU32 c0=MenuTheme::accentColor(0,1,index),c1=MenuTheme::accentColor(1,1,index);
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(16,17,25,255)),10);
    d->AddRectFilledMultiColor(p+ImVec2(1,1),p+ImVec2(size.x-1,size.y*.6f),fadeColor(MenuTheme::accentColor(0,.10f,index)),fadeColor(MenuTheme::accentColor(0,.10f,index)),0,0);
    d->AddRect(p,p+size,selected?fadeColor(c0):fadeColor(IM_COL32(255,255,255,hot?45:18)),10,0,selected?2.f:1.f);
    float bodyH=size.y-22;
    d->AddRectFilled(p+ImVec2(14,14),p+ImVec2(14+size.x*.4f,18),fadeColor(IM_COL32(150,152,170,140)),2);
    d->AddRectFilled(p+ImVec2(14,24),p+ImVec2(14+size.x*.28f,28),fadeColor(IM_COL32(110,112,130,120)),2);
    d->AddRectFilled(p+ImVec2(size.x-44,12),p+ImVec2(size.x-16,20),fadeColor(c0),4);
    d->AddRectFilled(p+ImVec2(size.x-44,24),p+ImVec2(size.x-16,30),fadeColor(IM_COL32(60,62,78,200)),3);
    {int start=d->VtxBuffer.Size;d->AddRectFilled(p+ImVec2(14,bodyH-8),p+ImVec2(size.x-14,bodyH-5),fadeColor(IM_COL32_WHITE),2);
     ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,p+ImVec2(14,0),p+ImVec2(size.x-14,0),c0,c1);}
    const char *name=MenuTheme::accentNames[index];auto ts=ImGui::CalcTextSize(name);
    d->AddText({p.x+(size.x-ts.x)/2,p.y+bodyH+(22-ts.y)/2-2},selected?fadeColor(c0):ImGui::GetColorU32(ImGuiCol_Text),name);
    if(hot)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);return hit;
}
// Small theme swatch (dot + line) for the watermark colours.
static bool themeSwatch(int index,bool selected){
    ImGui::PushID(index+950);ImVec2 p=ImGui::GetCursorScreenPos(),size(36,28);bool hit=ImGui::InvisibleButton("theme",size);bool hot=ImGui::IsItemHovered();ImGui::PopID();
    auto *d=ImGui::GetWindowDrawList();ImU32 c0=MenuTheme::accentColor(0,1,index),c1=MenuTheme::accentColor(1,1,index);
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(18,19,28,255)),7);d->AddRect(p,p+size,selected?fadeColor(c0):fadeColor(IM_COL32(255,255,255,hot?50:20)),7,0,selected?2.f:1.f);
    d->AddCircleFilled(p+ImVec2(10,10),3,fadeColor(c0),12);
    int start=d->VtxBuffer.Size;d->AddRectFilled(p+ImVec2(7,size.y-9),p+ImVec2(size.x-7,size.y-6),fadeColor(IM_COL32_WHITE),2);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,p+ImVec2(7,0),p+ImVec2(size.x-7,0),c0,c1);
    if(hot){ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);ImGui::SetTooltip("%s",MenuTheme::accentNames[index]);}return hit;
}
// Captures the next supported key press for the menu key.
static bool capturingMenuKey;
static void menuKeyButton(){
    const char *label=capturingMenuKey?L(u8"Нажми клавишу…","Press a key…"):MenuTheme::currentMenuKey().name;
    if(ImGui::Button((std::string(label)+"##menukey").c_str(),{ImGui::CalcItemWidth(),28}))capturingMenuKey=!capturingMenuKey;
    if(!capturingMenuKey)return;
    if(GetAsyncKeyState(VK_ESCAPE)&0x8000){capturingMenuKey=false;return;}
    for(const auto &k:MenuTheme::menuKeys)if(GetAsyncKeyState(k.vk)&0x8000){
        MenuTheme::menuKey=k.vk;capturingMenuKey=false;insertWasDown=true; // the press must not toggle the menu
        savePreferences();break;
    }
}
// Carbon 3XL has a material editor instead of the legacy rainbow palette.
static void carbonAppearancePage(){
    row=7;float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;bool edit=false;
    card(L(u8"Карбон и 3XL","Carbon and 3XL"),{w,h});
    ImGui::TextUnformatted("baLONCUaga 3XL");
    ImGui::TextWrapped("%s",L(u8"Чёрный карбон, холодное серебро и силуэт 3XL.","Black carbon, cold silver and the 3XL silhouette."));
    ImVec2 p=ImGui::GetCursorScreenPos(),area(ImGui::GetContentRegionAvail().x,166);auto *d=ImGui::GetWindowDrawList();ImGui::Dummy(area);
    d->AddRectFilled(p,p+area,fadeColor(IM_COL32(9,9,9,255)),10);
    d->PushClipRect(p+ImVec2(1,1),p+area-ImVec2(1,1),true);SneakerArt::carbon(d,p+ImVec2(1,1),p+area-ImVec2(1,1),MenuTheme::carbonStrength/100.f);
    SneakerArt::shoe(d,p+area*.5f,std::min(142.f,area.x*.44f),MenuTheme::reduceMotion?0.f:menuEffectTime,ImGui::GetStyle().Alpha);d->PopClipRect();
    edit|=ImGui::SliderInt(L(u8"Фактура карбона","Carbon texture"),&MenuTheme::carbonStrength,0,100,"%d%%");
    edit|=ImGui::Checkbox(L(u8"Серебряное свечение","Silver rim glow"),&MenuTheme::rimGlow);
    edit|=ImGui::SliderInt(L(u8"Плотность окна","Window opacity"),&MenuTheme::windowOpacity,60,100,"%d%%");
    ImGui::TextWrapped("%s",L(u8"Настройки цвета игровых функций доступны в их разделах.","Game feature colours are available in their own sections."));
    endCard();ImGui::SameLine();
    card(L(u8"Интерфейс","Interface"),{w,h});
    const char *framesNames[]={L(u8"Контуры","Outlines"),L(u8"Минимализм","Minimal")};int framesIndex=MenuTheme::frames?0:1;
    if(ImGui::Combo(L(u8"Панели","Panels"),&framesIndex,framesNames,2)){MenuTheme::frames=framesIndex==0;edit=true;}
    const char *logos[]={L(u8"Серебряный блик","Silver shine"),L(u8"Градиент","Gradient"),L(u8"Белый","White")};
    edit|=ImGui::Combo(L(u8"Логотип","Wordmark"),&MenuTheme::logoStyle,logos,MenuTheme::LogoCount);
    ImGui::TextUnformatted(L(u8"Язык","Language"));const char *languages[]={L(u8"Авто","Auto"),"EN","RU"};edit|=segmented("carbon language",&Lang::mode,languages,3);
    static int draft=100;static bool pending=false;if(!pending)draft=MenuTheme::auroraScale;
    int before=draft;ImGui::SliderInt(L(u8"Размер меню","Menu size"),&draft,70,150,"%d%%");if(before!=draft)pending=true;
    if(pending&&!ImGui::IsItemActive()){pending=false;MenuTheme::auroraScale=draft;edit=true;}
    ImGui::TextUnformatted(L(u8"Клавиша меню","Menu key"));menuKeyButton();
    ImGui::Separator();ImGui::TextWrapped("%s",L(u8"Скорость открытия, частицы и спокойный режим — во вкладке «Анимация».","Opening speed, particles and reduced motion are in the Motion tab."));
    if(ImGui::Button(L(u8"Настроить анимацию","Motion settings"),{ImGui::CalcItemWidth(),30})){ui::tabs[6].next_subtab=4;ui::content_anim_dst=0;}
    if(edit)savePreferences();endCard();
}
// ---- Settings -> Appearance (palettes, colours, interface) ----------------
static void appearancePage(){
    if(MenuTheme::isCarbon()){carbonAppearancePage();return;}
    row=7;float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;
    card(L(u8"Палитра","Palette"),{w,h});
    {float cw=(ImGui::GetContentRegionAvail().x-8)/2;
     for(int i=0;i<MenuTheme::AccentCount;++i){if(i%2)ImGui::SameLine(0,8);
        if(paletteCard(i,!MenuTheme::customColors&&MenuTheme::accent==i,{cw,74})){MenuTheme::accent=i;MenuTheme::customColors=false;savePreferences();}}}
    endCard();ImGui::SameLine();
    ImGui::BeginGroup();
    card(L(u8"Цвета","Colors"),{w,150});
    {bool edit=ImGui::Checkbox(L(u8"Свои цвета","Custom colors"),&MenuTheme::customColors);ImGui::SameLine(ImGui::GetContentRegionAvail().x-44);
     bool picked=swatch("##menu custom 0",MenuTheme::custom[0]);ImGui::SameLine(0,6);picked|=swatch("##menu custom 1",MenuTheme::custom[1]);
     if(picked){MenuTheme::customColors=true;edit=true;} // picking a colour switches to custom colours
     edit|=ImGui::Checkbox(L(u8"Градиент","Gradient"),&MenuTheme::gradient);
     int old=MenuTheme::windowOpacity;ImGui::SliderInt(L(u8"Окно","Window"),&MenuTheme::windowOpacity,60,100,"%d%%");edit|=old!=MenuTheme::windowOpacity;
     if(edit)savePreferences();}
    endCard();
    card(L(u8"Интерфейс","Interface"),{w,ImGui::GetContentRegionAvail().y});
    {const char *framesNames[]={L(u8"Рамки","Frames"),L(u8"Без рамок","No frames")};int framesIndex=MenuTheme::frames?0:1;
     const char *logos[]={L(u8"Блик","Shine"),L(u8"Градиент","Gradient"),L(u8"Обычный","Plain")};
     // Frames and logo share one row so the whole card fits without scrolling.
     float halfWidth=(ImGui::GetContentRegionAvail().x-8)/2;
     ImGui::BeginGroup();ImGui::PushItemWidth(halfWidth);
     if(ImGui::Combo(L(u8"Оформление","Style"),&framesIndex,framesNames,2)){MenuTheme::frames=framesIndex==0;savePreferences();}
     ImGui::PopItemWidth();ImGui::EndGroup();ImGui::SameLine(0,8);ImGui::BeginGroup();ImGui::PushItemWidth(halfWidth);
     if(ImGui::Combo(L(u8"Логотип","Logo"),&MenuTheme::logoStyle,logos,MenuTheme::LogoCount))savePreferences();
     ImGui::PopItemWidth();ImGui::EndGroup();
     ImGui::TextUnformatted(L(u8"Язык","Language"));const char *languages[]={L(u8"Авто","Auto"),"EN","RU"};
     if(segmented("language",&Lang::mode,languages,3))savePreferences();
     // The scale resizes the whole window. Applying it mid-drag moves the slider under
     // the cursor and the value jumps back and forth, so the drag edits a draft and the
     // new size is applied on release (the -/+ buttons apply at once).
     static int scaleDraft=100;static bool scalePending=false;
     if(!scalePending)scaleDraft=MenuTheme::auroraScale;
     int scaleBefore=scaleDraft;ImGui::SliderInt(L(u8"Масштаб","Scale"),&scaleDraft,70,150,"%d%%");
     if(scaleDraft!=scaleBefore)scalePending=true;   // -/+ change the value without reporting it
     if(scalePending&&!ImGui::IsItemActive()){scalePending=false;if(scaleDraft!=MenuTheme::auroraScale){MenuTheme::auroraScale=scaleDraft;savePreferences();}}
     ImGui::TextUnformatted(L(u8"Клавиша меню","Menu key"));menuKeyButton();}
    endCard();ImGui::EndGroup();
}
// Motion settings use the same renderer as the menu, including its live preview.
static void motionPage(){
    row=7;float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;
    card(L(u8"Анимация и эффекты","Motion and effects"),{w,h});bool edit=false;
    const char *templates[]={"Orbit","Aurora","Carbon 3XL"};
    edit|=ImGui::Combo(L(u8"Шаблон меню","Menu template"),&MenuTheme::menuTemplate,templates,MenuTheme::TemplateCount);
    ImGui::Spacing();edit|=ImGui::Checkbox(L(u8"Анимация открытия и закрытия","Opening and closing animation"),&MenuTheme::animations);
    edit|=ImGui::SliderInt(L(u8"Скорость анимации","Animation speed"),&MenuTheme::motionSpeed,60,160,"%d%%");
    ImGui::Separator();edit|=ImGui::Checkbox(L(u8"Свечение и движущийся контур","Glow and moving outline"),&MenuTheme::outerEffects);
    edit|=ImGui::Checkbox(L(u8"Частицы вокруг меню","Particles around menu"),&MenuTheme::particles);
    edit|=ImGui::Checkbox(L(u8"Спокойный режим","Reduced motion"),&MenuTheme::reduceMotion);
    if(edit)savePreferences();
    ImGui::Spacing();if(ImGui::Button(L(u8"Повторить открытие","Replay opening"),{ImGui::CalcItemWidth(),30}))menuMotion.reset();
    endCard();ImGui::SameLine();
    card(L(u8"Предпросмотр оформления","Style preview"),{w,h});
    ImGui::TextWrapped("%s",L(u8"Эффекты используют выбранные цвета меню. Все настройки сохраняются после выхода из игры.","Effects follow your menu colours. All settings persist after restarting the game."));
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),area(ImGui::GetContentRegionAvail().x,190);ImGui::Dummy(area);
    ImVec2 size(area.x-44,126),at=p+ImVec2(22,35);
    float a=ImGui::GetStyle().Alpha;
    if(MenuTheme::outerEffects)MenuMotion::drawDecor(d,at,size,MenuTheme::accentColor(0),MenuTheme::accentColor(1),a,menuEffectTime,1,MenuTheme::menuTemplate==MenuTheme::TemplateOrbit,MenuTheme::particles,MenuTheme::reduceMotion);
    d->AddRectFilled(at,at+size,fadeColor(MenuTheme::isCarbon()?IM_COL32(13,13,13,255):IM_COL32(13,16,25,255)),14);d->AddRect(at,at+size,fadeColor(MenuTheme::accentColor(0,.5f)),14);
    d->AddLine(at+ImVec2(15,34),at+ImVec2(size.x-15,34),fadeColor(IM_COL32(255,255,255,22)));
    d->AddText(at+ImVec2(15,11),ImGui::GetColorU32(ImGuiCol_Text),"baLONCUaga");
    if(MenuTheme::isCarbon())SneakerArt::shoe(d,at+ImVec2(size.x-47,19),27,MenuTheme::reduceMotion?0.f:menuEffectTime,a);
    d->AddRectFilled(at+ImVec2(14,49),at+ImVec2(70,110),fadeColor(MenuTheme::accentColor(0,.20f)),8);
    for(int i=0;i<3;++i)d->AddRectFilled(at+ImVec2(84,52+i*20.f),at+ImVec2(size.x-16,59+i*20.f),fadeColor(MenuTheme::isCarbon()?IM_COL32(119,119,119,100-i*18):IM_COL32(110,119,148,100-i*18)),3);
    ImGui::TextWrapped("%s",L(u8"Открытие: плавное появление и подъём. Закрытие: мягкое затухание. Повторное нажатие разворачивает переход без скачка.","Opening: fade and rise. Closing: smooth fade out. Pressing again reverses the transition without a jump."));
    endCard();
}
// ---- Settings -> Watermark ------------------------------------------------
static void watermarkPage(){
    row=7;float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;bool edit=false;
    card(L(u8"Ватермарк","Watermark"),{w,h});
    if(ImGui::Checkbox(L(u8"Включён","Enabled"),&Hud::watermarkOn))Hud::save();
    const char *styles[]={L(u8"Плашка","Plate"),L(u8"Текст","Text"),L(u8"Блок","Block"),L(u8"Чипы","Chips")};
    edit|=ImGui::Combo(L(u8"Стиль","Style"),&Watermark::style,styles,Watermark::StyleCount);
    ImGui::TextUnformatted(L(u8"Место","Position"));const char *places[]={L(u8"Слева","Left"),L(u8"Центр","Center"),L(u8"Справа","Right")};
    edit|=segmented("watermark place",&Watermark::place,places,3);
    edit|=ImGui::SliderInt(L(u8"Видимость","Opacity"),&Watermark::opacity,30,100,"%d%%");
    edit|=ImGui::SliderInt(L(u8"Размер","Size"),&Watermark::size,70,150,"%d%%");
    if(ImGui::Button(L(u8"Сбросить ватермарк","Reset watermark"),{ImGui::CalcItemWidth(),28})){Watermark::defaults();edit=true;}
    endCard();ImGui::SameLine();
    ImGui::BeginGroup();
    card(L(u8"Цвета","Colors"),{w,196});
    ImGui::TextUnformatted(L(u8"Тема","Theme"));
    for(int i=0;i<MenuTheme::AccentCount;++i){if(i%4)ImGui::SameLine(0,6);if(themeSwatch(i,Watermark::theme==i)){Watermark::applyTheme(i);edit=true;}}
    edit|=ImGui::Checkbox(L(u8"Два цвета","Two colors"),&Watermark::twoColors);ImGui::SameLine(ImGui::GetContentRegionAvail().x-44);
    edit|=swatch("##wm color 0",Watermark::colors[0]);ImGui::SameLine(0,6);edit|=swatch("##wm color 1",Watermark::colors[1]);
    endCard();
    card(L(u8"Поля","Fields"),{w,ImGui::GetContentRegionAvail().y});
    {struct F{int bit;const char *ru,*en;};const F list[]={{Watermark::Brand,u8"Бренд","Brand"},{Watermark::Nick,u8"Ник","Nick"},{Watermark::Map,u8"Карта","Map"},
        {Watermark::Fps,"FPS","FPS"},{Watermark::Ping,u8"Пинг","Ping"},{Watermark::Speed,u8"Скорость","Speed"},{Watermark::Time,u8"Время","Time"}};
     float right=ImGui::GetWindowPos().x+ImGui::GetWindowWidth()-14;bool first=true;
     for(const auto &f:list){const char *label=Lang::english()?f.en:f.ru;float cw=ImGui::CalcTextSize(label).x+38;
        if(!first){ImGui::SameLine(0,6);if(ImGui::GetCursorScreenPos().x+cw>right)ImGui::NewLine();}first=false;
        if(chip(label,Watermark::fields&f.bit)){Watermark::fields^=f.bit;edit=true;}}}
    endCard();ImGui::EndGroup();
    if(edit)Watermark::save();
}
// ---- Showkeys -------------------------------------------------------------
static void showkeysPage(){
    row=7;float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;bool edit=false;
    card(L(u8"Показ клавиш","Show keys"),{w,h});
    {auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),size(ImGui::GetContentRegionAvail().x,150);
     d->AddRectFilled(p,p+size,fadeColor(IM_COL32(8,8,12,255)),10);d->AddRect(p,p+size,fadeColor(IM_COL32(255,255,255,14)),10);
     float unit=Showkeys::unitFor(1);ImVec2 extent=Showkeys::extent(unit);extent.x+=extent.y*.2f;
     float fit=std::min({1.f,size.x*.9f/std::max(1.f,extent.x),size.y*.9f/std::max(1.f,extent.y)});
     d->PushClipRect(p,p+size,true);Showkeys::draw(d,p+size/2,unit*fit,ImGui::GetStyle().Alpha,true,frameTime);d->PopClipRect();
     ImGui::Dummy(size);}
    edit|=ImGui::Checkbox(L(u8"Показывать","Show on screen"),&Showkeys::enabled);
    const char *styles[]={L(u8"Клавиши","Keys"),L(u8"Заливка","Filled"),L(u8"Мягкий","Soft"),L(u8"Минимал","Minimal")};
    edit|=ImGui::Combo(L(u8"Стиль","Style"),&Showkeys::style,styles,Showkeys::StyleCount);
    edit|=ImGui::SliderInt(L(u8"Размер","Size"),&Showkeys::sizePercent,50,200,"%d%%");
    edit|=ImGui::Checkbox(L(u8"Наклон","Tilt"),&Showkeys::tilt);
    edit|=ImGui::Checkbox(L(u8"Колесо мыши","Mouse wheel"),&Showkeys::wheel);
    ImGui::TextUnformatted(L(u8"Цвета","Colors"));ImGui::SameLine(ImGui::GetContentRegionAvail().x-44);
    edit|=swatch("##sk accent",Showkeys::accent);ImGui::SameLine(0,6);edit|=swatch("##sk text",Showkeys::text);
    float half=(ImGui::CalcItemWidth()-8)/2;
    if(ImGui::Button(L(u8"По центру","Center"),{half,28})){Showkeys::posX=50;Showkeys::posY=78;edit=true;}ImGui::SameLine(0,8);
    if(ImGui::Button(L(u8"Сбросить","Reset"),{half,28})){bool on=Showkeys::enabled;Showkeys::defaults();Showkeys::enabled=on;edit=true;}
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",L(u8"Клавиши на экране двигаются мышью.","Drag the keys on screen to move them."));ImGui::PopStyleColor();
    endCard();ImGui::SameLine();
    card(L(u8"Клавиши","Keys"),{w,h});
    {std::vector<const char *> names;for(const auto &preset:Showkeys::presets())names.push_back(preset.name);names.push_back(L(u8"Своя","Custom"));
     int current=Showkeys::layout<0?static_cast<int>(names.size())-1:Showkeys::layout;
     if(ImGui::Combo(L(u8"Раскладка","Layout"),&current,names.data(),static_cast<int>(names.size()))&&current<static_cast<int>(Showkeys::presets().size())){Showkeys::applyPreset(current);edit=true;}
     auto addPopup=[&](const char *id,int rowIndex){
        if(!ImGui::BeginPopup(id))return;
        for(const auto &k:Showkeys::keys){if(ImGui::Selectable(k.label)){Showkeys::addKey(rowIndex,k.vk);edit=true;ImGui::CloseCurrentPopup();}}
        ImGui::EndPopup();
     };
     int removeRow=-1,removeIndex=-1; // removal is applied after the loop so indices stay valid
     for(int r=0;r<static_cast<int>(Showkeys::rows.size());++r){
        ImGui::PushID(r+1200);
        for(int i=0;i<static_cast<int>(Showkeys::rows[r].size());++i){
            auto *k=Showkeys::find(Showkeys::rows[r][i]);if(i)ImGui::SameLine(0,6);ImGui::PushID(i);
            ImGui::Button(k?k->label:"?",{std::max(34.f,ImGui::CalcTextSize(k?k->label:"?").x+20),30});
            if(ImGui::IsItemHovered()&&ImGui::IsMouseReleased(1)){removeRow=r;removeIndex=i;}
            if(ImGui::IsItemClicked(0))ImGui::OpenPopup("key options");
            if(ImGui::BeginPopup("key options")){
                if(ImGui::Selectable(L(u8"Удалить","Remove"))){removeRow=r;removeIndex=i;ImGui::CloseCurrentPopup();}
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if(r<static_cast<int>(Showkeys::rows.size())&&Showkeys::rows[r].size()<Showkeys::MaxPerRow){if(!Showkeys::rows[r].empty())ImGui::SameLine(0,6);
            if(ImGui::Button("+##add",{34,30}))ImGui::OpenPopup("add key");addPopup("add key",r);}
        ImGui::PopID();
     }
     if(Showkeys::rows.size()<Showkeys::MaxRows){if(ImGui::Button("+##row",{34,30}))ImGui::OpenPopup("add row");addPopup("add row",static_cast<int>(Showkeys::rows.size()));}
     if(removeRow>=0){Showkeys::removeKey(removeRow,removeIndex);edit=true;}
     ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
     ImGui::TextWrapped("%s",L(u8"ПКМ по клавише удаляет её.","Right-click a key to remove it."));ImGui::PopStyleColor();}
    endCard();
    if(edit)Showkeys::save();
}
static void displaySettingsPage(){
    row=7;card(L(u8"Время и наблюдатель","Time and spectator"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    bool edit=ImGui::Checkbox(L(u8"Часовой пояс Windows","Windows time zone"),&Hud::systemTime);
    if(!Hud::systemTime){
        static std::vector<std::string> labels;static std::vector<const char *> names;   // built once
        if(labels.empty()){for(int m=-720;m<=840;m+=15){char label[24];snprintf(label,sizeof(label),"UTC%c%02d:%02d",m<0?'-':'+',std::abs(m)/60,std::abs(m)%60);labels.emplace_back(label);}
            for(const auto &name:labels)names.push_back(name.c_str());}
        int index=std::clamp((Hud::utcOffset+720)/15,0,104);
        if(ImGui::Combo(L(u8"Часовой пояс","Time zone"),&index,names.data(),names.size())){Hud::utcOffset=index*15-720;edit=true;}
    }
    auto time=Hud::now();ImGui::Text(L(u8"Сейчас: %02u:%02u:%02u","Current time: %02u:%02u:%02u"),time.wHour,time.wMinute,time.wSecond);
    if(edit)Hud::save();
    ImGui::Separator();
    bool hide=SpectatorResource::enabled;
    if(ImGui::Checkbox(L(u8"Убрать чёрные полосы наблюдателя","Remove spectator black bars"),&hide)){if(SpectatorResource::setEnabled(hide))SpectatorResource::save();}
    ImGui::TextDisabled("%s",L(u8"После изменения перезапусти игру.","Restart the game after changing this."));
    const char *state=SpectatorResource::state;
    if(!strcmp(state,"restart"))ImGui::TextWrapped("%s",L(u8"Файл обновлён. Перезапусти игру.","File updated. Restart the game."));
    else if(!strcmp(state,"conflict"))ImGui::TextWrapped("%s",L(u8"Spectator.res изменён другим модом. Автоматическая замена отменена; резервная копия сохранена.","Spectator.res was changed by another mod. Replacement was cancelled; the backup was retained."));
    else if(*state&&strcmp(state,"off")&&strcmp(state,"on"))ImGui::TextWrapped("%s",L(u8"Не удалось изменить ресурс. Проверь доступ к Spectator.res и файлам .baloncuaga-v72 рядом с ним.","Could not update the resource. Check access to Spectator.res and its adjacent .baloncuaga-v72 files."));
    endCard();
}
// A small scene that flashes and fades out in the chosen colour, like in game.
static void flashPreview(){
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetCursorScreenPos(),size(ImGui::CalcItemWidth(),46);ImGui::Dummy(size);
    d->AddRectFilledMultiColor(p,p+ImVec2(size.x,size.y*.45f),fadeColor(IM_COL32(92,134,201,255)),fadeColor(IM_COL32(92,134,201,255)),fadeColor(IM_COL32(154,183,222,255)),fadeColor(IM_COL32(154,183,222,255)));
    d->AddRectFilledMultiColor(p+ImVec2(0,size.y*.45f),p+size,fadeColor(IM_COL32(201,180,138,255)),fadeColor(IM_COL32(201,180,138,255)),fadeColor(IM_COL32(110,93,60,255)),fadeColor(IM_COL32(110,93,60,255)));
    d->AddRectFilled(p+ImVec2(size.x*.18f,size.y*.25f),p+ImVec2(size.x*.3f,size.y*.75f),fadeColor(IM_COL32(120,96,62,255)));
    float t=std::fmod(frameTime,2.6f),alpha=t<.35f?1.f:std::max(0.f,1.f-(t-.35f)/1.7f);
    int r=255,g=255,b=255;if(Hud::flashTintOn){r=Hud::flashRgb[0];g=Hud::flashRgb[1];b=Hud::flashRgb[2];}
    d->AddRectFilled(p,p+size,fadeColor(IM_COL32(r,g,b,static_cast<int>(alpha*255))));
    d->AddRect(p,p+size,fadeColor(IM_COL32(255,255,255,30)),6);
}
static void settings(){
    row=7;card(L(u8"Общие","General"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    if(ImGui::Checkbox(L(u8"Включить цветной дым","Colored smoke"),&Hud::smokeTintOn))Hud::save();
    float smokeColor[4]={Hud::smokeRgb[0]/255.f,Hud::smokeRgb[1]/255.f,Hud::smokeRgb[2]/255.f,1};
    if(ui::color(L(u8"Цвет дыма","Smoke color"),smokeColor,false)){for(int i=0;i<3;++i)Hud::smokeRgb[i]=std::clamp(static_cast<int>(std::round(smokeColor[i]*255)),0,255);Hud::save();}
    ImGui::Separator();
    if(ImGui::Checkbox(L(u8"Цвет ослепления от флешки (вместо белого)","Flashbang blind colour (instead of white)"),&Hud::flashTintOn))Hud::save();
    float flashColor[4]={Hud::flashRgb[0]/255.f,Hud::flashRgb[1]/255.f,Hud::flashRgb[2]/255.f,1};
    // Picking a colour turns the option on, like the menu's own colours.
    if(ui::color(L(u8"Цвет ослепления","Blind colour"),flashColor,false)){for(int i=0;i<3;++i)Hud::flashRgb[i]=std::clamp(static_cast<int>(std::round(flashColor[i]*255)),0,255);Hud::flashTintOn=true;Hud::save();}
    flashPreview();
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",L(u8"Когда флешка ослепляет, экран заливается этим цветом, а не ярко-белым. Ослепление длится столько же, сколько в игре.","When a flashbang blinds you the screen fills with this colour instead of bright white. The blindness lasts as long as in the game."));ImGui::PopStyleColor();
    ImGui::Separator();
    if(ImGui::Button(L(u8"Сбросить настройки","Reset settings"),{ImGui::CalcItemWidth(),32})){fov=90;crossStyle=crossColor=0;crossPending=true;for(auto &c:choice)c=0;Preview::release();save();}
    if(ImGui::Button(L(u8"Выгрузить плагин (F11)","Unload plugin (F11)"),{ImGui::CalcItemWidth(),32}))unloadRequested.store(true,std::memory_order_release);
    if(ImGui::Button(L(u8"Закрыть меню","Close menu"),{ImGui::CalcItemWidth(),32})){opened=false;Mouse::sync();}endCard();
}
static void gareyPage(){
    row=7;card("CS Fix",{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    Garey::frame();auto visual=Garey::variable("fps_visual");
    const char *status=GareyRuntime::state==GareyRuntime::RestartRequired?L(u8"CS Fix обновлён. Полностью перезапусти игру для подключения FPS и голоса.","CS Fix updated. Completely restart the game to connect FPS and voice."):
        visual&&GareyRuntime::ready()?L(u8"Визуальный FPS подключён.","Visual FPS is connected."):
        GareyRuntime::minimal&&GareyRuntime::progress(0xa88000)==4?L(u8"CS Fix пропущен: эта сборка движка или клиента не поддерживается.","CS Fix skipped: this engine or client build is unsupported."):
        GareyRuntime::state==GareyRuntime::NotAutoloaded?L(u8"ASI-загрузчик не загрузил компонент CS Fix. Проверь установку комплекта FULL и оставь один ASI-загрузчик.","The ASI loader did not load CS Fix. Check the FULL installation and use one ASI loader."):
        L(u8"Ожидание инициализации встроенного CS Fix.","Waiting for the embedded CS Fix to initialize.");
    ImGui::TextWrapped("%s",status);
    if(resourceInstallError){
        ImGui::TextWrapped(L(u8"Ресурсы HUD: ошибка Windows %lu.","HUD resources: Windows error %lu."),static_cast<unsigned long>(resourceInstallError));
        if(!resourceInstallFile.empty())ImGui::TextWrapped("%s",resourceInstallFile.c_str());
        if(!resourceInstallRollbackOk)ImGui::TextWrapped("%s",L(u8"Не все файлы восстановлены. Резервные копии: .baloncuaga-install/backups.","Some files could not be restored. Backups: .baloncuaga-install/backups."));
        ImGui::Separator();
    }
    if(!visual&&(GareyRuntime::state==GareyRuntime::Failed||GareyRuntime::state==GareyRuntime::Conflict))ImGui::TextWrapped(L(u8"CS Fix не подключён. Код Windows: %lu.","CS Fix is not connected. Windows code: %lu."),static_cast<unsigned long>(GareyRuntime::error));
    int value=Garey::visual;
    if(ImGui::SliderInt(L(u8"Визуальный FPS","Visual FPS"),&value,20,1000,"%d",ImGuiSliderFlags_AlwaysClamp))Garey::setVisual(value);
    if(ImGui::Button(L(u8"Стандарт: 100","Default: 100"),{-1,30}))Garey::setVisual(100);
    ImGui::TextWrapped("%s",L(u8"Значение сохраняется в конфигурации HUD. Стандарт — 100.","Saved in the HUD configuration. Default: 100."));
    if(visual)ImGui::Text(L(u8"Движок принял fps_visual: %.0f","Engine fps_visual: %.0f"),double(visual->value));
    else if((GareyRuntime::state==GareyRuntime::Loaded||GareyRuntime::state==GareyRuntime::Existing)&&GareyRuntime::started&&GetTickCount64()-GareyRuntime::started>15000)ImGui::TextWrapped("%s",L(u8"Модуль не зарегистрировал fps_visual. Эта версия CS Fix пока не подключилась к клиенту.","The module has not registered fps_visual; this CS Fix build has not attached to the client."));
    ImGui::Separator();ImGui::TextUnformatted(L(u8"Голос","Voice"));
    auto voice=Garey::variable("voice_enable");auto volume=Garey::variable("voice_scale");
    if(voice){bool enabled=voice->value!=0;if(ImGui::Checkbox(L(u8"Голосовой чат","Voice chat"),&enabled))Garey::setVoice(enabled);}
    if(volume){int percent=std::clamp(int(std::lround(volume->value*100)),0,100);if(ImGui::SliderInt(L(u8"Громкость игроков","Player voice volume"),&percent,0,100,"%d%%"))Garey::setVolume(percent);}
    if(GareyRuntime::minimal){auto state=GareyRuntime::progress(0xa88004);
        ImGui::TextWrapped("%s",state==2?L(u8"Голос 48 кГц: подключён.","48 kHz voice: connected."):
            state==4?L(u8"Голос: стандартный. Фикс 48 кГц не подключён.","Voice: native. The 48 kHz fix is not connected."):
            L(u8"Голос 48 кГц: ожидание подключения.","48 kHz voice: waiting to connect."));
    }
    ImGui::Separator();ImGui::TextWrapped("%s",L(u8"F11 выгружает BALONCUAGA. CS Fix продолжает работать до закрытия игры.","F11 unloads BALONCUAGA. CS Fix stays active until the game closes."));
    endCard();
}
// Tab and subtab names follow the menu language (rebuilt only when it changes).
static void applyLanguage(){
    static int applied=-1;const bool e=Lang::english();if(applied==static_cast<int>(e)&&ui::tabs.size()>=10)return;applied=e;
    static const char *ru[]={u8"Модели",u8"Камера",u8"Прицел","HUD",u8"Тренажёр колеса",u8"Чат",u8"Настройки","Misc","Stats","Showkeys"};
    static const char *en[]={"Models","Camera","Crosshair","HUD","Scroll trainer","Chat","Settings","Misc","Stats","Showkeys"};
    for(size_t i=0;i<ui::tabs.size()&&i<10;++i)ui::tabs[i].m_name=e?en[i]:ru[i];
    ui::tabs[11].m_name=e?"Servers":u8"Серверы";
    ui::tabs[8].m_subtabs=e?std::vector<const char*>{"General"}:std::vector<const char*>{u8"Общая"};
    if(e){ui::tabs[0].m_subtabs={"Knife","HE grenade","Flashbang","Smoke","Team outfits"};ui::tabs[3].m_subtabs={"Radar","FOG","In-game HUD","Skybox"};ui::tabs[8].m_icon=ICON_FA_CHART_BAR;ui::tabs[5].m_subtabs={"Settings","History",u8"БРАТУХА"};ui::tabs[6].m_subtabs={"Appearance","Watermark","General","Screen","Motion"};}
    else{ui::tabs[0].m_subtabs={u8"Нож",u8"HE-граната",u8"Флешка",u8"Дымовая",u8"Одежда команд"};ui::tabs[3].m_subtabs={u8"Радар","FOG",u8"Игровой HUD",u8"Небо"};ui::tabs[5].m_subtabs={u8"Настройки",u8"История",u8"БРАТУХА"};ui::tabs[6].m_subtabs={u8"Оформление",u8"Ватермарк",u8"Общие",u8"Экран",u8"Анимация"};}
}
#include "menu_navigation.h"
#include "aurora_layout.h"
#include "carbon_3xl_layout.h"
static char configName[33]="default";
static void misc(){
    row=7;card(L(u8"Конфигурация","Configuration"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    if(ImGui::Button(L(u8"Сохранить текущие настройки","Save current settings"),{ImGui::CalcItemWidth(),32}))Config::saveCurrent();
    ImGui::Separator();ImGui::InputText(L(u8"Имя конфига","Config name"),configName,sizeof(configName));
    if(ImGui::Button(L(u8"Сохранить конфиг","Save config"),{ImGui::CalcItemWidth(),32}))Config::saveProfile(configName);
    if(ImGui::Button(L(u8"Загрузить конфиг","Load config"),{ImGui::CalcItemWidth(),32}))Config::loadProfile(configName);
    ImGui::TextWrapped("%s",Config::status());ImGui::TextDisabled("%s",L(u8"Имена: латиница, цифры, _ и -.","Names: Latin letters, digits, _ and -."));
    endCard();
}
#include "combat_hud_render.h"
#include "skybox_page.h"
#include "weave_overlay.h"
static bool buildDenseFonts(int wanted,int maximum){
    auto *fonts=ImGui::GetIO().Fonts;
    const float menuRasterScale=menuScaleFor(screenW,screenH);
    const float hudRasterScale=HudResolution::factor(screenW,screenH);
    // Chat can be enlarged to150% independently of the menu. Bake enough
    // glyph pixels for that setting rather than magnifying a12/18px bitmap.
    const float chatRasterScale=std::max(menuRasterScale,hudRasterScale)*1.5f;
    auto chatFace=[](ImFont *face){return face==chatBold||face==chatSmall||face==chatButtons;};
    std::vector<float> originalSizes;for(const auto &cfg:fonts->ConfigData)originalSizes.push_back(cfg.SizePixels);
    for(int density=wanted;density>=1;--density){
        const float quality=static_cast<float>(density)/wanted;
        auto rasterFor=[&](ImFont *face){
            if(CombatFonts::nativeFace(face))return hudRasterScale*quality;
            if(nativeMenuFont(face))return menuRasterScale*quality;
            if(chatFace(face))return chatRasterScale*quality;
            return static_cast<float>(density);
        };
        fonts->ClearTexData();
        // A narrow fallback atlas can overflow in height even when a wider
        // texture fits the same GPU limit. Keep the full available width.
        fonts->TexDesiredWidth=std::min(maximum,2048);
        for(auto &cfg:fonts->ConfigData)cfg.SizePixels=std::max(1.f,std::round(cfg.SizePixels*rasterFor(cfg.DstFont)));
        bool built=fonts->Build();
        for(int i=0;i<fonts->ConfigData.Size;++i)fonts->ConfigData[i].SizePixels=originalSizes[i];
        if(!built||fonts->TexWidth>maximum||fonts->TexHeight>maximum)continue;
        for(auto *font:fonts->Fonts){
            // All metrics stay in design units. Only the atlas bitmap uses
            // physical framebuffer pixels; never apply the UI scale twice.
            float logicalSize=0.f;
            for(int i=0;i<fonts->ConfigData.Size;++i)if(fonts->ConfigData[i].DstFont==font&&!fonts->ConfigData[i].MergeMode){logicalSize=originalSizes[i];break;}
            if(logicalSize<=0.f)continue;
            const float wantedSize=logicalSize;
            const float rasterScale=font->FontSize/wantedSize;
            font->FontSize/=rasterScale;font->Ascent/=rasterScale;font->Descent/=rasterScale;
            for(auto &glyph:font->Glyphs){glyph.X0/=rasterScale;glyph.X1/=rasterScale;glyph.Y0/=rasterScale;glyph.Y1/=rasterScale;glyph.AdvanceX/=rasterScale;}
            font->BuildLookupTable();
        }
        fontDensity=density;fontRequestDensity=wanted;nativeMenuRequestScale=menuRasterScale;nativeHudRequestScale=hudRasterScale;return true;
    }
    return false;
}
static bool initialize(){
    if(!wglGetCurrentContext())return false;
    if(!context){
        IMGUI_CHECKVERSION();context=ImGui::CreateContext();ImGui::SetCurrentContext(context);ImGui::StyleColorsDark();
        auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.BackendRendererName="BALONCUAGA OpenGL";io.BackendPlatformName="BALONCUAGA GoldSrc";
        io.Fonts->FontBuilderIO=ImGuiFreeType::GetBuilderForFreeType();
        ImFontConfig cfg;cfg.FontDataOwnedByAtlas=false;cfg.FontBuilderFlags=ImGuiFreeTypeBuilderFlags_LightHinting;static const ImWchar letters[]={0x20,0xFF,0x400,0x45F,0x2000,0x206F,0};
        io.Fonts->AddFontFromMemoryTTF(font_medium_bin,sizeof(font_medium_bin),14,&cfg,letters);
        ImFontConfig icons=cfg;icons.MergeMode=true;icons.PixelSnapH=true;static const ImWchar iconRange[]={0xf005,0xf005,0xf030,0xf030,0xf05b,0xf05b,0xf06e,0xf06e,0xf080,0xf080,0xf086,0xf086,0xf0ad,0xf0ad,0xf11c,0xf11c,0xf15b,0xf15b,0xf1de,0xf1de,0xf233,0xf233,0xf55d,0xf55d,0xf8cc,0xf8cc,0};
        io.Fonts->AddFontFromMemoryTTF(font_awesome_bin,sizeof(font_awesome_bin),13,&icons,iconRange);
        io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),14,&cfg,letters);
        io.Fonts->AddFontFromMemoryTTF(font_medium_bin,sizeof(font_medium_bin),17,&cfg,letters);
        io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),17,&cfg,letters);
        char windowsFolder[MAX_PATH]={};GetWindowsDirectoryA(windowsFolder,MAX_PATH);char fontPath[MAX_PATH];
        auto hudFontPath=[&](char *path,const char *preferred,const char *fallback){snprintf(path,MAX_PATH,"%s/Fonts/%s",windowsFolder,preferred);
            if(!CombatFonts::validFont(path))snprintf(path,MAX_PATH,"%s/Fonts/%s",windowsFolder,fallback);};
        hudFontPath(fontPath,"seguisb.ttf","tahomabd.ttf");ImFontConfig installedFont;installedFont.FontBuilderFlags=ImGuiFreeTypeBuilderFlags_LightHinting;
        chatBold=CombatFonts::systemFace(io.Fonts,fontPath,18,installedFont,letters);
        hudFontPath(fontPath,"segoeui.ttf","tahoma.ttf");
        chatSmall=CombatFonts::systemFace(io.Fonts,fontPath,14,installedFont,letters);
        chatButtons=CombatFonts::systemFace(io.Fonts,fontPath,14,installedFont,letters);
        if(!chatButtons)chatButtons=io.Fonts->Fonts[0];if(!chatBold)chatBold=io.Fonts->Fonts[3];if(!chatSmall)chatSmall=io.Fonts->Fonts[0];
        // BuildLookupTable also handles space/tab and a fallback character.
        // Without real space, a later rebuild can hide the fallback digit 9.
        static const ImWchar speedDigits[]={0x20,0x20,0x30,0x39,0x3F,0x3F,0};
        static const ImWchar hudLetters[]={0x20,0x20,0x2B,0x2B,0x2D,0x2E,0x30,0x39,0x41,0x41,0x44,0x47,0x4C,0x4C,0x4F,0x4F,0x52,0x52,0xB7,0xB7,0};
        char regularPath[MAX_PATH],boldPath[MAX_PATH];
        hudFontPath(regularPath,"segoeui.ttf","tahoma.ttf");hudFontPath(boldPath,"seguisb.ttf","tahomabd.ttf");
        bool hasRegular=CombatFonts::validFont(regularPath),hasBold=CombatFonts::validFont(boldPath);
        for(int i=0;i<hudFontCount;++i){
            float pixels=hudFontSizes[i];
            // The regular FOG face only draws speed digits. Keep its range
            // narrow so the quality bank fits legacy OpenGL texture limits.
            hudRegularSizes[i]=hasRegular?CombatFonts::systemFace(io.Fonts,regularPath,pixels,installedFont,speedDigits):nullptr;
            hudBoldSizes[i]=hasBold?CombatFonts::systemFace(io.Fonts,boldPath,pixels,installedFont,hudLetters):nullptr;
            if(!hudRegularSizes[i])hudRegularSizes[i]=io.Fonts->AddFontFromMemoryTTF(font_medium_bin,sizeof(font_medium_bin),pixels,&cfg,speedDigits);
            if(!hudBoldSizes[i])hudBoldSizes[i]=io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),pixels,&cfg,hudLetters);
            speedFontSizes[i]=hasBold?CombatFonts::systemFace(io.Fonts,boldPath,pixels,installedFont,speedDigits):nullptr;
            if(!speedFontSizes[i])speedFontSizes[i]=io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),pixels,&cfg,speedDigits);
        }
        // Four additional faces, appended after the historical indices used by
        // the HUD. Normal FreeType hinting and grayscale AA remain enabled.
        ImFontConfig menuCfg=cfg;menuCfg.PixelSnapH=true;menuCfg.FontBuilderFlags=0;
        static const ImWchar menuLetters[]={0x20,0xFF,0x400,0x45F,0x2013,0x2014,0x2026,0x2026,0};
        menuSmall=io.Fonts->AddFontFromMemoryTTF(font_medium_bin,sizeof(font_medium_bin),12,&menuCfg,menuLetters);
        menuBody=io.Fonts->AddFontFromMemoryTTF(font_medium_bin,sizeof(font_medium_bin),16,&menuCfg,menuLetters);
        ImFontConfig menuIcons=menuCfg;menuIcons.MergeMode=true;
        io.Fonts->AddFontFromMemoryTTF(font_awesome_bin,sizeof(font_awesome_bin),16,&menuIcons,iconRange);
        menuBold=io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),16,&menuCfg,menuLetters);
        menuTitle=io.Fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),24,&menuCfg,menuLetters);
        CombatFonts::add(io.Fonts,windowsFolder);
        hudRegular=hudRegularSizes[6];hudBold=hudBoldSizes[6];speedFont=speedFontSizes[6];
        io.Fonts->TexDesiredWidth=1024;
        for(int i=0;i<4;++i)ui::add_page(0,[i]{models(i);});ui::add_page(0,outfitsPage);ui::add_page(1,camera);ui::add_page(2,crosshair);
        for(int i=2;i<=3;++i)ui::add_page(3,[i]{hudPage(i);});ui::add_page(3,combatHudPage);ui::add_page(3,skyboxPage);ui::add_page(4,trainer);ui::add_page(5,chatSettingsPage);ui::add_page(5,historyPage);ui::add_page(5,bratuhaPage);
        ui::add_page(6,appearancePage);ui::add_page(6,watermarkPage);ui::add_page(6,settings);ui::add_page(6,displaySettingsPage);ui::add_page(6,motionPage);ui::add_page(7,misc);ui::add_page(8,statsPage);ui::tabs[8].m_subtabs={u8"Общая"};
        ui::tabs.push_back(tab_t(ICON_FA_KEYBOARD,"Showkeys",{}));ui::add_page(9,showkeysPage);
        ui::tabs.push_back(tab_t(ICON_FA_WRENCH,"CS Fix",{}));ui::add_page(10,gareyPage);
        ui::tabs.push_back(tab_t(ICON_FA_SERVER,u8"Серверы",{}));ui::add_page(11,serversPage);ui::tabs[8].m_icon=ICON_FA_CHART_BAR;
        ui::anim=ui::content_anim=1;ui::size={740,530};
        const char *navigation[]={u8"Модели",u8"Камера",u8"Прицел",u8"HUD",u8"Тренажёр колеса",u8"Чат",u8"Настройки","Misc"};
        const char *categoryIcons[]={ICON_FA_BRUSH,ICON_FA_CAMERA,ICON_FA_CROSSHAIRS,ICON_FA_EYE,ICON_FA_COMPUTER_MOUSE,ICON_FA_COMMENTS,ICON_FA_SLIDERS,ICON_FA_FILE};
        for(int i=0;i<8;++i){ui::tabs[i].m_name=navigation[i];ui::tabs[i].m_icon=categoryIcons[i];}ui::tabs[5].m_subtabs={u8"Настройки",u8"История",u8"БРАТУХА"};ui::tabs[6].m_subtabs={u8"Оформление",u8"Ватермарк",u8"Общие",u8"Экран",u8"Анимация"};
        ui::tabs[0].m_subtabs={u8"Нож",u8"HE-граната",u8"Флешка",u8"Дымовая",u8"Одежда команд"};ui::tabs[3].m_subtabs={u8"Радар","FOG",u8"Игровой HUD",u8"Небо"};
        if(MenuTheme::isCarbon()){ui::cur_tab=ui::next_tab=6;ui::tabs[6].cur_subtab=ui::tabs[6].next_subtab=0;}
    }
    ImGui::SetCurrentContext(context);
    // A new GL context (and HUD_VidInit on every map) only needs the atlas texture
    // uploaded again; rasterizing all fonts again froze the first frames of each map.
    if(glContext!=wglGetCurrentContext()){atlas=avatar=0;glContext=wglGetCurrentContext();}
    int wanted=HudResolution::density(screenW,screenH);
    if(fontRequestDensity!=wanted||std::abs(nativeMenuRequestScale-menuScaleFor(screenW,screenH))>.0001f||std::abs(nativeHudRequestScale-HudResolution::factor(screenW,screenH))>.0001f||!ImGui::GetIO().Fonts->IsBuilt()){
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(!buildDenseFonts(wanted,maximum)){logLine("HUD fonts could not fit available texture size.");return false;}
        if(atlas){glDeleteTextures(1,&atlas);atlas=0;}
    }
    if(!atlas){
        unsigned char *pixels=nullptr;int w=0,h=0;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(!pixels||w>maximum||h>maximum){logLine("ImGui font atlas exceeds OpenGL capabilities.");return false;}
        glPushAttrib(GL_TEXTURE_BIT);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        glGenTextures(1,&atlas);glBindTexture(GL_TEXTURE_2D,atlas);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        GLint uploadedWidth=0,uploadedHeight=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&uploadedWidth);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&uploadedHeight);
        if(uploadedWidth!=w||uploadedHeight!=h){if(atlas)glDeleteTextures(1,&atlas);atlas=0;glPopClientAttrib();glPopAttrib();logLine("HUD font texture upload failed; native HUD retained.");return false;}
        ImGui::GetIO().Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(atlas)));
        char fontStatus[160];snprintf(fontStatus,sizeof(fontStatus),"UI fonts: %dx%d, quality %d/%d, HUD %.2fx, menu %.2fx.",w,h,fontDensity,fontRequestDensity,nativeHudRequestScale,nativeMenuRequestScale);logLine(fontStatus);
        glPopClientAttrib();glPopAttrib();
    }return atlas!=0;
}
static void resetVideo(){
    SneakerArt::resetVideo();
    SkyboxMenu::resetPreview();
    if(glContext==wglGetCurrentContext()){if(atlas)glDeleteTextures(1,&atlas);if(avatar)glDeleteTextures(1,&avatar);}
    atlas=avatar=0;glContext=nullptr;lastFrame=0;menuMotion.reset();menuRequested=false;menuEffectTime=0;
}
static void shutdown(){
    SneakerArt::resetVideo();
    SkyboxMenu::resetPreview();
    if(context){ImGui::SetCurrentContext(context);ImGui::DestroyContext(context);context=nullptr;}fontDensity=fontRequestDensity=0;menuMotion.reset();menuRequested=false;
    hudRegular=hudBold=speedFont=nullptr;
    menuSmall=menuBody=menuBold=menuTitle=nullptr;nativeMenuRequestScale=nativeHudRequestScale=0.f;
    combatSmall=combatLabel=combatValue=nullptr;CombatFonts::reset();
    for(int i=0;i<hudFontCount;++i)hudRegularSizes[i]=hudBoldSizes[i]=speedFontSizes[i]=nullptr;
    std::lock_guard<std::mutex> lock(inputLock);inputs.clear();
}
// Native VGUI may finish a frame with a texture transform, stencil mask or a
// shader still selected. Attribute stacks restore most legacy state, but they
// neither reset a texture matrix nor save a shader program. Keep those changes
// local to this pass, including when a model-preview callback resets rendering.
struct RendererPassState {
    using TextureUnit=void(APIENTRY *)(GLenum);
    using BindBuffer=void(APIENTRY *)(GLenum,GLuint);
    using UseProgram=void(APIENTRY *)(GLuint);
    using GetHandle=GLuint(APIENTRY *)(GLenum);
    struct Api {
        HGLRC context=nullptr;TextureUnit active=nullptr,client=nullptr;BindBuffer bind=nullptr;
        UseProgram program=nullptr;GetHandle handle=nullptr;GLint units=1;
        bool secondary=false,fogCoord=false,vertexProgram=false,fragmentProgram=false;
    };
    static Api &api(){
        static Api a;
        if(a.context==wglGetCurrentContext())return a;
        a=Api{};a.context=wglGetCurrentContext();
        const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION));int major=0,minor=0;if(version)sscanf(version,"%d.%d",&major,&minor);
        auto atLeast=[&](int ma,int mi){return major>ma||(major==ma&&minor>=mi);};
        const char *extensions=reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        auto has=[&](const char *name){if(!extensions)return false;size_t length=strlen(name);const char *p=extensions;
            while((p=strstr(p,name))){if((p==extensions||p[-1]==' ')&&(p[length]==' '||p[length]==0))return true;p+=length;}return false;};
        auto proc=[](const char *name,const char *alias=nullptr){PROC p=wglGetProcAddress(name);
            auto valid=[](PROC value){return reinterpret_cast<uintptr_t>(value)>4&&reinterpret_cast<uintptr_t>(value)!=static_cast<uintptr_t>(-1);};
            if(!valid(p)&&alias)p=wglGetProcAddress(alias);return valid(p)?p:nullptr;};
        if(atLeast(1,3)||has("GL_ARB_multitexture")){
            a.active=reinterpret_cast<TextureUnit>(proc("glActiveTexture","glActiveTextureARB"));
            a.client=reinterpret_cast<TextureUnit>(proc("glClientActiveTexture","glClientActiveTextureARB"));
            if(a.active&&a.client)glGetIntegerv(0x84E2,&a.units);
        }
        a.units=std::clamp(a.units,1,32);
        if(atLeast(1,5)||has("GL_ARB_vertex_buffer_object"))a.bind=reinterpret_cast<BindBuffer>(proc("glBindBuffer","glBindBufferARB"));
        if(atLeast(2,0))a.program=reinterpret_cast<UseProgram>(proc("glUseProgram"));
        if(!a.program&&has("GL_ARB_shader_objects")){
            a.program=reinterpret_cast<UseProgram>(proc("glUseProgramObjectARB"));a.handle=reinterpret_cast<GetHandle>(proc("glGetHandleARB"));
            if(!a.handle)a.program=nullptr;
        }
        a.secondary=atLeast(1,4)||has("GL_EXT_secondary_color");a.fogCoord=atLeast(1,4)||has("GL_EXT_fog_coord");
        a.vertexProgram=has("GL_ARB_vertex_program");a.fragmentProgram=has("GL_ARB_fragment_program");return a;
    }
    Api &functions;GLint arrayBuffer=0,elementBuffer=0,clientUnit=0x84C0,program=0,matrixMode=GL_MODELVIEW;
    RendererPassState():functions(api()){
        if(functions.active)functions.active(0x84C0);
        if(functions.client)glGetIntegerv(0x84E1,&clientUnit);
        if(functions.bind){glGetIntegerv(0x8894,&arrayBuffer);glGetIntegerv(0x8895,&elementBuffer);}
        if(functions.program){if(functions.handle)program=functions.handle(0x8B40);else glGetIntegerv(0x8B8D,&program);}
        glGetIntegerv(GL_MATRIX_MODE,&matrixMode);glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();glMatrixMode(matrixMode);
    }
    void prepare(){
        if(functions.program)functions.program(0);
        if(functions.bind){functions.bind(0x8892,0);functions.bind(0x8893,0);}
        for(int i=1;i<functions.units;++i){
            if(functions.active){functions.active(0x84C0+i);glDisable(GL_TEXTURE_2D);}
            if(functions.client){functions.client(0x84C0+i);glDisableClientState(GL_TEXTURE_COORD_ARRAY);}
        }
        if(functions.active)functions.active(0x84C0);if(functions.client)functions.client(0x84C0);
        glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
        glDisable(GL_STENCIL_TEST);glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
        if(functions.vertexProgram)glDisable(0x8620);if(functions.fragmentProgram)glDisable(0x8804);
        glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_INDEX_ARRAY);glDisableClientState(GL_EDGE_FLAG_ARRAY);
        if(functions.secondary){glDisableClientState(0x845E);glDisable(0x8458);}if(functions.fogCoord)glDisableClientState(0x8457);
    }
    ~RendererPassState(){
        if(functions.active)functions.active(0x84C0);glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(matrixMode);
        if(functions.bind){functions.bind(0x8892,arrayBuffer);functions.bind(0x8893,elementBuffer);}
        if(functions.program)functions.program(static_cast<GLuint>(program));
        if(functions.client)functions.client(clientUnit);
    }
};
static void setup(){
    // GoldSrc may leave GL_FLAT enabled. Color-picker gradients (including the
    // translucent black SV overlay) require interpolated vertex colors.
    // renderDrawData's GL2D guard restores the game's shade model afterward.
    glShadeModel(GL_SMOOTH);
    glViewport(0,0,screenW,screenH);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glEnable(GL_SCISSOR_TEST);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,screenW/scale,screenH/scale,0,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
}
static void renderDrawData(){
    Hud::GL2D guard(screenW,screenH);
    RendererPassState pass;auto reset=[&]{pass.prepare();setup();};reset();auto *data=ImGui::GetDrawData();
    // The filter belongs to the atlas texture, so set it once per pass rather
    // than twice for every text command. Other texture filtering is untouched.
    if(atlas){glBindTexture(GL_TEXTURE_2D,atlas);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
    for(int n=0;n<data->CmdListsCount;++n){auto *list=data->CmdLists[n];
        for(const auto &cmd:list->CmdBuffer){
            if(cmd.UserCallback){if(cmd.UserCallback!=ImDrawCallback_ResetRenderState)cmd.UserCallback(list,&cmd);reset();continue;}
            auto r=cmd.ClipRect;int x=std::max(0,static_cast<int>(r.x*scale)),y=std::max(0,static_cast<int>(r.y*scale)),right=std::min(screenW,static_cast<int>(std::ceil(r.z*scale))),bottom=std::min(screenH,static_cast<int>(std::ceil(r.w*scale)));
            if(right<=x||bottom<=y)continue;glScissor(x,screenH-bottom,right-x,bottom-y);
            const ImDrawVert *v=list->VtxBuffer.Data+cmd.VtxOffset;
            glVertexPointer(2,GL_FLOAT,sizeof(ImDrawVert),&v->pos);glTexCoordPointer(2,GL_FLOAT,sizeof(ImDrawVert),&v->uv);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(ImDrawVert),&v->col);
            glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(reinterpret_cast<intptr_t>(cmd.GetTexID())));
            glDrawElements(GL_TRIANGLES,cmd.ElemCount,sizeof(ImDrawIdx)==2?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT,list->IdxBuffer.Data+cmd.IdxOffset);
        }
    }
}
static void glitchEffect(){
    ULONGLONG tick=GetTickCount64();unsigned phase=static_cast<unsigned>(tick%1350);if(phase>270)return;
    ImVec2 p(menuX/scale,menuY/scale),size(ui::size.x,ui::size.y);auto *d=ImGui::GetForegroundDrawList();
    d->PushClipRect(p,p+size,true);
    float shift=4.f+static_cast<float>(phase%13)/2.f;
    d->AddRect(p+ImVec2(-shift,2),p+size+ImVec2(-shift,2),IM_COL32(0,235,255,155),4,0,2);
    d->AddRect(p+ImVec2(shift,-2),p+size+ImVec2(shift,-2),IM_COL32(255,38,132,150),4,0,2);
    for(unsigned i=0;i<9;++i){unsigned seed=(phase*17+i*73+tick/1350*191)%static_cast<unsigned>(ui::size.y);float y=p.y+static_cast<float>(seed);float start=p.x+static_cast<float>((phase*11+i*109+tick/1350*47)%(static_cast<unsigned>(ui::size.x)*3/4));float width=std::min(size.x-(start-p.x),55.f+static_cast<float>((tick+i*83)%260));float height=3.f+static_cast<float>((i+phase/20)%8);
        if(width>0)d->AddRectFilled({start,y},{start+width,y+height},i%2?IM_COL32(0,225,255,175):IM_COL32(255,35,129,170));}
    for(unsigned i=0;i<4;++i){float y=p.y+static_cast<float>((phase*9+i*127+tick/1350*29)%static_cast<unsigned>(ui::size.y));d->AddRectFilled({p.x,y},{p.x+size.x,y+1.5f},IM_COL32(235,244,255,85));}
    d->PopClipRect();
}
static bool renderFrame(const SCREENINFO &screen,float time,bool showMenu){
    if(!preferencesLoaded){MenuTheme::load();Lang::load();Showkeys::load();Watermark::load();ChatBox::loadTheme();preferencesLoaded=true;useMenu=true;glitchMenu=GetPrivateProfileIntA("interface","glitch_menu",0,iniPath)!=0;menuX=static_cast<int>(GetPrivateProfileIntA("interface","menu_x",-1,iniPath));menuY=static_cast<int>(GetPrivateProfileIntA("interface","menu_y",-1,iniPath));ChatBox::timeOn=GetPrivateProfileIntA("interface","chat_time",1,iniPath)!=0;ChatBox::serverOn=GetPrivateProfileIntA("interface","chat_server",1,iniPath)!=0;ChatBox::deadOn=GetPrivateProfileIntA("interface","chat_dead",1,iniPath)!=0;Preview::spin=GetPrivateProfileIntA("interface","preview_spin",1,iniPath)!=0;Preview::showHands=GetPrivateProfileIntA("interface","preview_hands",1,iniPath)!=0;Trainer::enabled=GetPrivateProfileIntA("interface","trainer_enabled",0,iniPath)!=0;}
    const auto surface=UiSurface::current(screen.iWidth,screen.iHeight);
    screenW=surface.width;screenH=surface.height;
    static int loggedNativeW=-1,loggedNativeH=-1,loggedSurfaceW=-1,loggedSurfaceH=-1;
    if(screen.iWidth!=loggedNativeW||screen.iHeight!=loggedNativeH||screenW!=loggedSurfaceW||screenH!=loggedSurfaceH){
        char line[192];snprintf(line,sizeof(line),"UI surface: native HUD %dx%d, GL drawable %dx%d, HUD scale %.2fx.",screen.iWidth,screen.iHeight,screenW,screenH,HudResolution::factor(screenW,screenH));logLine(line);
        loggedNativeW=screen.iWidth;loggedNativeH=screen.iHeight;loggedSurfaceW=screenW;loggedSurfaceH=screenH;
    }
    if(screenW<=0||screenH<=0)return false;
    if(!initialize())return false;frameTime=time;ui::size=MenuTheme::size();
    auto &io=ImGui::GetIO();ULONGLONG tick=GetTickCount64();
    io.DeltaTime=lastFrame?std::max(.001f,std::min(.05f,(tick-lastFrame)/1000.f)):1.f/60;lastFrame=tick;
    // Input ownership changes immediately. Only the visuals live through closing.
    if(menuRequested&&!showMenu){
        ImGui::ClearActiveID();io.ClearInputKeys();io.ClearEventsQueue();ImGui::ClosePopupsOverWindow(nullptr,false);
        capturingMenuKey=false;if(moving){moving=false;clampPosition();savePosition();}
    }
    menuRequested=showMenu;menuMotion.update(showMenu,io.DeltaTime,MenuTheme::motionSpeed/100.f,MenuTheme::animations);
    const bool menuVisible=showMenu||menuMotion.visible();menuEffectTime=std::fmod(menuEffectTime+io.DeltaTime,3600.f);
    // Keep the complete window inside the framebuffer, also at low resolutions.
    scale=menuVisible?menuScaleFor(screenW,screenH):HudResolution::factor(screenW,screenH);
    io.DisplaySize={screenW/scale,screenH/scale};io.DisplayFramebufferScale={scale,scale};
    bool interactive=showMenu||Hud::chatOpened;io.MouseDrawCursor=interactive&&!outsideGame;io.AddMousePosEvent(interactive?UiSurface::mouseCoordinate(Mouse::x,screen.iWidth,screenW)/scale:-100,interactive?UiSurface::mouseCoordinate(Mouse::y,screen.iHeight,screenH)/scale:-100);
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool focused=pid==GetCurrentProcessId();if(!focused)combatScoresHeld=false;io.AddFocusEvent(focused);
    for(int i=0;i<3;++i){int vk=i==0?VK_LBUTTON:i==1?VK_RBUTTON:VK_MBUTTON;io.AddMouseButtonEvent(i,interactive&&focused&&(GetAsyncKeyState(vk)&0x8000));}
    io.AddKeyEvent(ImGuiMod_Ctrl,focused&&(GetAsyncKeyState(VK_CONTROL)&0x8000));io.AddKeyEvent(ImGuiMod_Shift,focused&&(GetAsyncKeyState(VK_SHIFT)&0x8000));io.AddKeyEvent(ImGuiMod_Alt,focused&&(GetAsyncKeyState(VK_MENU)&0x8000));
    std::deque<Input> pending;{std::lock_guard<std::mutex> lock(inputLock);pending.swap(inputs);}
    for(auto e:pending){
        if(!interactive)continue;
        if(e.message==WM_MOUSEWHEEL)io.AddMouseWheelEvent(0,static_cast<short>(HIWORD(e.value))/120.f);
        if(e.message==WM_CHAR){if(e.wide)io.AddInputCharacterUTF16(static_cast<ImWchar16>(e.value));else {char c=static_cast<char>(e.value);wchar_t w=0;if(MultiByteToWideChar(CP_ACP,0,&c,1,&w,1))io.AddInputCharacterUTF16(w);}}
        if(e.message==WM_KEYDOWN||e.message==WM_KEYUP){auto k=key(e.value);if(k!=ImGuiKey_None)io.AddKeyEvent(k,e.message==WM_KEYDOWN);}
    }
    if(menuVisible)Profile::update();ImGui::NewFrame();
    if(!outsideGame&&!menuOnly)combatHudOverlay(menuVisible);
    if(outsideGame&&outsideGameDecor)outsideGameDecor(ImGui::GetBackgroundDrawList(),{screenW/scale,screenH/scale},time);
    if(menuVisible){
        // Scope menu colors and geometry so game HUD/chat retain their settings.
        applyLanguage();if(!outsideGame&&!menuOnly&&showMenu)showkeysOverlay(true); // under the menu window
        MenuFadeScope menuFade;ImGuiStyle original=ImGui::GetStyle();MenuTheme::apply();
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha,1.f);ImGui::BeginDisabled(!showMenu);
        if(MenuTheme::isCarbon()){ui::menuTitleFont=menuBold;ImGui::PushFont(menuBody);carbon3XLLayout();ImGui::PopFont();ui::menuTitleFont=nullptr;}else auroraLayout();
        ImGui::EndDisabled();ImGui::PopStyleVar();
        if(nativeResourcesRestartPending||resourceInstallError){
            ImGui::SetNextWindowPos({screenW/scale*.5f,8.f},ImGuiCond_Always,{.5f,0});
            ImGui::SetNextWindowSize({std::max(160.f,std::min(560.f,screenW/scale-16.f)),0});
            ImGui::SetNextWindowBgAlpha(.94f);
            ImGui::Begin("##resource_restart_notice",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing);
            if(resourceInstallError){
                ImGui::TextWrapped(L(u8"Ресурсы HUD: ошибка %lu. Подробнее — CS Fix.","HUD resources: error %lu. Details in CS Fix."),static_cast<unsigned long>(resourceInstallError));
            }else ImGui::TextWrapped("%s",L(u8"HUD работает. Чтобы вернуть стандартное оформление окон CS, перезапусти игру один раз.","HUD is active. Restart CS once to restore standard native window styling."));
            ImGui::End();
        }
        menuFade.apply(menuMotion.alpha());preview.opacity*=menuMotion.alpha();
        ImGui::GetStyle()=original;ui::aurora=false;ui::carbonMoon=false;MenuTheme::carbonStyle=false;
        if(showMenu&&row!=10&&Hud::chatOpened)ChatEditor::close();
    }if(!outsideGame&&!menuOnly){jumpStatsOverlay();gameInfoOverlay();if(!showMenu)showkeysOverlay(false);speedometerOverlay(showMenu);if(!opened)chatOverlay();}ImGui::Render();renderDrawData();return true;
}
static bool draw(const SCREENINFO &screen,float time){return renderFrame(screen,time,opened);}
static bool drawOverlay(const SCREENINFO &screen,float time){return renderFrame(screen,time,false);}
}

