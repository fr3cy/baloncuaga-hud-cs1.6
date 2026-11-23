#pragma once
#include <algorithm>
#include <cmath>
#include "weave/imgui/imgui.h"

// Transparent, non-interactive animation for the native main-menu background.
// Native VGUI controls stay in charge. No full-screen fill, input capture,
// textures, hooks, callbacks or threads are owned by this helper.
namespace MainMenuArt {
inline float unit(float x){return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}
inline ImU32 silver(int grey,float alpha){grey=std::clamp(grey,0,255);return IM_COL32(grey,grey,grey,static_cast<int>(unit(alpha)*255.f+.5f));}
inline void draw(ImDrawList *draw,ImVec2 screen,float time,float alpha=1.f,bool nativeDialogVisible=false){
    alpha=unit(alpha);
    if(!draw||alpha<=0.f||nativeDialogVisible||!std::isfinite(screen.x)||!std::isfinite(screen.y)||screen.x<160||screen.y<120)return;
    if(!std::isfinite(time))time=0.f;const float t=std::fmod(time,600.f);
    // Keep motion outside the native button column and ordinary centred
    // dialogs. The caller can suppress it completely for modal/native dialogs.
    const int count=12;
    for(int i=0;i<count;++i){
        const float seed=i*.61803398875f,phase=seed*6.28318530718f;
        float x,y;
        if(i<7){x=screen.x*(.945f+.035f*std::fmod(seed,1.f));y=screen.y*(.10f+.80f*std::fmod(seed+t*(.0018f+i*.0001f),1.f));}
        else{x=screen.x*(.10f+.78f*std::fmod(seed+t*.0011f,1.f));y=screen.y*(.020f+.016f*(.5f+.5f*std::sin(phase)));}
        const float life=.35f+.65f*(.5f+.5f*std::sin(t*.65f+phase));
        const float opacity=alpha*life*.42f,radius=i%4==0?1.35f:.85f;
        draw->AddCircleFilled({x,y},radius+2.f,silver(235,opacity*.045f),8);
        draw->AddCircleFilled({x,y},radius,silver(235,opacity),8);
        if(i%4==0){draw->AddLine({x-2.5f,y},{x+2.5f,y},silver(235,opacity*.28f),1.f);draw->AddLine({x,y-2.5f},{x,y+2.5f},silver(235,opacity*.28f),1.f);}
    }
    // A slowly travelling silver rim occupies only the extreme right edge.
    float center=screen.y*(.18f+.62f*(.5f+.5f*std::sin(t*.12f)));
    float x=screen.x-std::max(3.f,screen.x*.004f),span=std::min(90.f,screen.y*.10f);
    for(int i=0;i<10;++i){float y=center-span*.5f+i*span/10.f,k=std::sin((i+.5f)*3.14159265359f/10.f);
        draw->AddLine({x,y},{x,y+span/10.f},silver(225,alpha*k*.11f),1.f);}
}
} // namespace MainMenuArt
