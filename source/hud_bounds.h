#pragma once
#include <algorithm>
#include <cmath>
namespace HudBounds {
static float fit(float width,float height,float itemWidth,float itemHeight,float margin){
    if(itemWidth<=0||itemHeight<=0)return 1.f;
    return std::clamp(std::min((width-2*margin)/itemWidth,(height-2*margin)/itemHeight),.001f,1.f);
}
static float axis(float viewport,float extent,float margin,int percent){
    margin=std::clamp(margin,0.f,std::max(0.f,(viewport-extent)*.5f));
    const float travel=std::max(0.f,viewport-extent-margin*2);
    return margin+travel*std::clamp(percent,0,100)/100.f;
}
}
