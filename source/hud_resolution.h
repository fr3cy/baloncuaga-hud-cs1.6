#pragma once
#include <algorithm>
#include <cmath>
#ifdef clamp
#undef clamp
#endif
namespace HudResolution {
// 16:9 keeps the 720p baseline; 4:3 can use a 1024-unit wide layout.
// Below 640x480 the complete HUD shrinks together rather than overlapping.
static float factor(int width,int height){
    if(width<=0||height<=0)return 1.f;
    const float fit=std::min(width/640.f,height/480.f);
    if(fit<1.f)return std::max(.1f,fit);
    return std::clamp(std::min(width/1024.f,height/720.f),1.f,3.f);
}
static int density(int width,int height){return std::clamp(static_cast<int>(std::ceil(factor(width,height))),2,3);}
static int segments(float radius){return std::clamp(static_cast<int>(std::ceil(6.2831853f*std::max(1.f,radius)/3)),32,1024);}
}
