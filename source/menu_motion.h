#pragma once
#include <algorithm>
#include <cmath>
#include "weave/imgui/imgui.h"

// Menu presentation only. The owning code retains control of mouse capture and
// input; a visible closing tail must never make the menu interactive again.
namespace MenuMotion {
inline float unit(float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
inline float smooth(float v){v=unit(v);return v*v*(3.f-2.f*v);}
struct State {
    float progress=0.f;
    bool target=false;
    void reset(){progress=0.f;target=false;}
    void update(bool open,float dt,float speed=1.f,bool animated=true){
        target=open;progress=unit(progress);
        if(!animated){progress=open?1.f:0.f;return;}
        if(!std::isfinite(dt)||dt<=0.f)return;
        if(!std::isfinite(speed))speed=1.f;
        speed=std::clamp(speed,.2f,3.f);
        // Time is integrated rather than lerped per frame. Reversing the target
        // continues from the same position, even when Insert is pressed rapidly.
        const float step=std::min(dt,10.f)*speed/(open?.38f:.24f);
        progress=unit(progress+(open?step:-step));
        if(progress<.000001f)progress=0.f;
        if(progress>.999999f)progress=1.f;
    }
    bool visible()const{return std::isfinite(progress)&&progress>0.f;}
    float alpha()const{return smooth(progress);}
    float offsetY()const{return (1.f-alpha())*22.f;}
};

inline ImU32 tint(ImU32 color,float alpha){
    const int a=static_cast<int>(((color>>IM_COL32_A_SHIFT)&255)*unit(alpha)+.5f);
    return (color&~IM_COL32_A_MASK)|(static_cast<ImU32>(a)<<IM_COL32_A_SHIFT);
}
inline ImU32 mix(ImU32 a,ImU32 b,float t,float alpha=1.f){
    t=unit(t);ImU32 out=0;
    for(int shift:{IM_COL32_R_SHIFT,IM_COL32_G_SHIFT,IM_COL32_B_SHIFT,IM_COL32_A_SHIFT}){
        const float x=static_cast<float>((a>>shift)&255),y=static_cast<float>((b>>shift)&255);
        out|=static_cast<ImU32>(x+(y-x)*t+.5f)<<shift;
    }
    return tint(out,alpha);
}
struct Point {ImVec2 pos,normal;};
// Constant-speed rounded-rectangle perimeter. The normal keeps each particle
// outside the panel, including through its corners; no particles cover controls.
inline Point perimeter(ImVec2 origin,ImVec2 size,float rounding,float distance){
    const float pi=3.14159265358979323846f;
    const float r=std::max(0.f,std::min(rounding,std::min(size.x,size.y)*.5f));
    const float w=std::max(0.f,size.x-2*r),h=std::max(0.f,size.y-2*r),arc=pi*r*.5f;
    const float length=std::max(1.f,2*(w+h)+4*arc);
    float d=std::fmod(std::isfinite(distance)?distance:0.f,length);if(d<0)d+=length;
    auto corner=[&](float cx,float cy,float start,float local){
        const float angle=start+(r>0?local/r:0);
        ImVec2 normal(std::cos(angle),std::sin(angle));
        return Point{ImVec2(origin.x+cx+r*normal.x,origin.y+cy+r*normal.y),normal};
    };
    if(d<w)return {{origin.x+r+d,origin.y},{0,-1}};d-=w;
    if(d<arc)return corner(size.x-r,r,-pi*.5f,d);d-=arc;
    if(d<h)return {{origin.x+size.x,origin.y+r+d},{1,0}};d-=h;
    if(d<arc)return corner(size.x-r,size.y-r,0,d);d-=arc;
    if(d<w)return {{origin.x+size.x-r-d,origin.y+size.y},{0,1}};d-=w;
    if(d<arc)return corner(r,size.y-r,pi*.5f,d);d-=arc;
    if(d<h)return {{origin.x,origin.y+size.y-r-d},{-1,0}};d-=h;
    return corner(r,r,pi,std::min(d,arc));
}
inline float perimeterLength(ImVec2 s,float r=14.f){
    r=std::max(0.f,std::min(r,std::min(s.x,s.y)*.5f));
    return 2*(s.x+s.y-4*r)+6.28318530718f*r;
}
inline void beam(ImDrawList *draw,ImVec2 origin,ImVec2 size,float head,float length,ImU32 color,float alpha){
    if(alpha<=0.f)return;
    // A short gradient tail, sampled at fixed distances. Three beams cost less
    // than 1,200 vertices even on large windows and 4K output.
    constexpr int count=14;ImVec2 points[count+1];
    for(int i=0;i<=count;++i)points[i]=perimeter(origin,size,14,head-length+length*i/count).pos;
    for(int i=0;i<count;++i){
        float k=(i+.5f)/count;k=k*k;
        draw->AddLine(points[i],points[i+1],tint(color,alpha*.11f*k),7.f);
        draw->AddLine(points[i],points[i+1],tint(color,alpha*.40f*k),3.f);
        draw->AddLine(points[i],points[i+1],tint(color,alpha*.95f*k),1.f);
    }
}
// Colors are raw, un-faded accents. Alpha is applied exactly once here; this
// function does not read ImGui style alpha and does not allocate external assets.
inline void drawDecor(ImDrawList *draw,ImVec2 origin,ImVec2 size,ImU32 primary,ImU32 secondary,
                      float alpha,float time,float openingProgress,bool orbitStyle,bool particles,bool reduceMotion){
    alpha=unit(alpha);openingProgress=unit(openingProgress);
    if(!draw||alpha<=0.f||!std::isfinite(origin.x)||!std::isfinite(origin.y)||
       !std::isfinite(size.x)||!std::isfinite(size.y)||size.x<40||size.y<40)return;
    if(!std::isfinite(time))time=0.f;
    // The sine frequencies share this period; reduce their arguments without
    // resetting travel along the perimeter during a long-running session.
    const double clock=reduceMotion?0.:static_cast<double>(time);
    const float t=static_cast<float>(std::fmod(clock,125.66370614359173));
    const float pulse=reduceMotion?1.f:.86f+.14f*std::sin(t*1.2f);
    for(int layer=8;layer>=1;--layer){
        const float e=2.f+layer*2.6f;
        const ImVec2 a(origin.x-e,origin.y-e+5),b(origin.x+size.x+e,origin.y+size.y+e+5);
        draw->AddRectFilled(a,b,tint(IM_COL32(0,0,0,255),alpha*(9-layer)*.006f),14+e);
    }
    const int layers=orbitStyle?5:3;
    for(int layer=layers;layer>=1;--layer){
        float e=layer*2.2f;
        ImU32 c=mix(primary,secondary,.5f,alpha*pulse*(layers-layer+1.f)/(layers*35.f));
        draw->AddRect({origin.x-e,origin.y-e},{origin.x+size.x+e,origin.y+size.y+e},c,14+e,0,2.f);
    }
    draw->AddRect(origin,{origin.x+size.x,origin.y+size.y},mix(primary,secondary,.35f,alpha*.24f),14,0,1.f);
    if(!reduceMotion){
        const float length=perimeterLength(size),head=static_cast<float>(std::fmod(clock*105.,length));
        const int beams=orbitStyle?3:2;
        for(int i=0;i<beams;++i)beam(draw,origin,size,head+i*length/beams,orbitStyle?128.f:88.f,i%2?secondary:primary,alpha);
        // An expanding echo exists only during the transition, then settles to
        // the restrained moving border. The menu never flashes at full opacity.
        if(openingProgress<.999f){
            float e=3.f+18.f*smooth(openingProgress),k=alpha*(1.f-smooth(openingProgress));
            draw->AddRect({origin.x-e,origin.y-e},{origin.x+size.x+e,origin.y+size.y+e},tint(primary,k*.30f),14+e,0,1.2f);
        }
    }
    if(!particles||reduceMotion)return;
    const int count=orbitStyle?20:12;
    const float length=perimeterLength(size),transition=1.f-smooth(openingProgress);
    for(int i=0;i<count;++i){
        const float seed=i*.61803398875f,phase=seed*6.28318530718f;
        const float travel=static_cast<float>(std::fmod(clock*(10.+(i%5)*2.)*(i%3==0?-1.:1.),length));
        Point pt=perimeter(origin,size,14,seed*length+travel);
        const float away=12.f+(i%4)*6.f+5.f*std::sin(t*.65f+phase)+transition*15.f;
        ImVec2 p(pt.pos.x+pt.normal.x*away,pt.pos.y+pt.normal.y*away);
        const float life=.35f+.65f*(.5f+.5f*std::sin(t*.9f+phase));
        const float a=alpha*life*(orbitStyle?.70f:.43f),radius=i%5==0?1.8f:1.15f;
        ImU32 col=mix(primary,secondary,.5f+.5f*std::sin(phase));
        draw->AddCircleFilled(p,radius+3.2f,tint(col,a*.06f),10);
        draw->AddCircleFilled(p,radius+1.4f,tint(col,a*.13f),10);
        draw->AddCircleFilled(p,radius,tint(col,a),8);
        if(i%5==0){
            draw->AddLine({p.x-3.5f,p.y},{p.x+3.5f,p.y},tint(col,a*.45f),1.f);
            draw->AddLine({p.x,p.y-3.5f},{p.x,p.y+3.5f},tint(col,a*.45f),1.f);
        }
    }
}
} // namespace MenuMotion
