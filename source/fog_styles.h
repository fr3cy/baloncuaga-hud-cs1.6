#pragma once
struct FogArt {
    MovementHud::Result result;float speed,gain;bool top,bottom,showGain;
    char number[32],delta[32],pre[32],count[24],label[40],full[80];
    FogArt(MovementHud::Result r,float s,float g,bool t,bool b,bool show):result(r),speed(s),gain(g),top(t),bottom(b),showGain(show&&t){
        snprintf(number,sizeof(number),"%.0f",s);snprintf(delta,sizeof(delta),"%+.0f",std::fabs(g)<.5f?0.f:g);
        snprintf(pre,sizeof(pre),"%.3f",r.pre);snprintf(count,sizeof(count),r.kind==MovementHud::Ladder?"L":"%d",r.fog);
        if(r.kind==MovementHud::Ladder)snprintf(label,sizeof(label),"LADDER");else snprintf(label,sizeof(label),"FOG %d",r.fog);
        MovementHud::formatResult(r,full,sizeof(full));
    }
};
static ImFont *fogRegular(){return hudRegular?hudRegular:chatButtons?chatButtons:ImGui::GetFont();}
static ImFont *fogBold(){return hudBold?hudBold:chatBold?chatBold:ImGui::GetFont();}
static float fogWidth(ImFont *f,float size,const char *s){return hudFontFor(f,size)->CalcTextSizeA(size,FLT_MAX,0,s).x;}
static ImVec2 fogArtSize(int style,float fs,const FogArt &v){
    float lo=fs*.58f,gap=fs*.35f,top=v.top?fogWidth(fogRegular(),fs,v.number)+(v.showGain?gap+fogWidth(fogBold(),fs*.46f,v.delta):0):0;
    if(!v.bottom)return {top,fs};
    float label=fogWidth(fogBold(),lo,v.label),pre=fogWidth(fogBold(),lo,v.pre),row=label+gap+pre;
    if(style==MovementHud::Inline)return {top+(v.top?gap:0)+row+fs*.6f,fs*1.05f};
    if(style==MovementHud::Split&&v.top)return {std::max(fogWidth(fogRegular(),fs,v.number),v.showGain?fogWidth(fogBold(),lo,v.delta):0)+gap*2+std::max(label,pre),fs*1.9f};
    float badge=std::max(fs*.9f,fogWidth(fogBold(),lo,v.count)+fs*.3f);
    if(style==MovementHud::Badge||style==MovementHud::Ring)row=badge+fs*.1f+gap+pre;
    if(style==MovementHud::Classic)row=fogWidth(fogBold(),lo,v.full);
    if(style==MovementHud::Underline)row=std::max(row,fs*6.5f);
    if(style==MovementHud::Card)return {std::max(fs*7,top+label+gap*2)+fs*.6f,fs*2.6f};
    return {std::max(top,row+(style==MovementHud::Capsule?fs*.75f:0)),(v.top?fs*1.65f:0)+((style==MovementHud::Badge||style==MovementHud::Ring)?std::max(fs*.95f,badge):fs*.95f)};
}
static void fogVideoBackdrop(ImDrawList *d,ImVec2 p,int style,float fs,const FogArt &v,ImVec2 size,ULONGLONG now,bool animate,bool foreground=false){
    if(!MovementHud::videoEffects||!v.bottom)return;
    const int *rgb=MovementHud::resultAccent(v.result);float pulse=animate?MovementHud::perfectPulse(v.result,now):0;
    bool inlineStyle=style==MovementHud::Inline||style==MovementHud::Split||style==MovementHud::Card;
    float y=p.y+(v.top&&!inlineStyle?fs*1.65f:0),bottom=p.y+size.y+fs*.02f;
    ImVec2 a(p.x-fs*.28f,y-fs*.08f),b(p.x+size.x+fs*.28f,bottom);
    float fade=std::min(fs*.9f,(b.x-a.x)*.24f);ImU32 clear=IM_COL32(5,8,12,0),black=IM_COL32(5,8,12,165);
    if(!foreground){
        d->AddRectFilledMultiColor(a,{a.x+fade,b.y},clear,black,black,clear);
        d->AddRectFilled({a.x+fade,a.y},{b.x-fade,b.y},black);
        d->AddRectFilledMultiColor({b.x-fade,a.y},b,black,clear,clear,black);return;
    }
    float mid=(a.x+b.x)/2,lineY=bottom-fs*.035f;
    auto strip=[&](float thickness,int alpha){ImU32 c=IM_COL32(rgb[0],rgb[1],rgb[2],alpha);
        d->AddRectFilledMultiColor({a.x,lineY-thickness/2},{mid,lineY+thickness/2},0,c,c,0);
        d->AddRectFilledMultiColor({mid,lineY-thickness/2},{b.x,lineY+thickness/2},c,0,0,c);
    };
    strip(std::max(1.f/scale,fs*.035f),205);
    strip(fs*(.16f+.18f*pulse),static_cast<int>(16+60*pulse));
    if(pulse>.001f){
        // Ideal uses a green travelling highlight and a soft, decaying wash.
        ImU32 wash=IM_COL32(rgb[0],rgb[1],rgb[2],static_cast<int>(40*pulse));
        d->AddRectFilledMultiColor(a,b,0,0,wash,wash);
    }
    float t=MovementHud::animationAge(now,v.result.tick);
    if(animate&&t>=0&&t<1){
        float x=a.x+(b.x-a.x)*t,half=fs*.7f;float strength=(1-t)*(v.result.ideal?.95f:.4f);
        ImU32 shine=IM_COL32(std::min(255,rgb[0]+65),std::min(255,rgb[1]+65),std::min(255,rgb[2]+65),static_cast<int>(230*strength));
        d->PushClipRect({a.x,lineY-fs*.18f},{b.x,lineY+fs*.18f},true);
        d->AddRectFilledMultiColor({x-half,lineY-fs*.04f},{x,lineY+fs*.04f},0,shine,shine,0);
        d->AddRectFilledMultiColor({x,lineY-fs*.04f},{x+half,lineY+fs*.04f},shine,0,0,shine);d->PopClipRect();
    }
}
struct FogVideoFinish {
    ImDrawList *d;ImVec2 p;int style;float fs;const FogArt &v;ImVec2 size;ULONGLONG now;bool animate;
    ~FogVideoFinish(){fogVideoBackdrop(d,p,style,fs,v,size,now,animate,true);}
};
static void fogArt(ImDrawList *d,ImVec2 p,int style,float fs,const FogArt &v,ULONGLONG now,bool glow=true){
    ImFont *regular=fogRegular(),*bold=fogBold();float lo=fs*.58f,gap=fs*.35f;
    auto size=fogArtSize(style,fs,v);const int *rgb=MovementHud::resultAccent(v.result);ImU32 accent=IM_COL32(rgb[0],rgb[1],rgb[2],255);
    ImU32 white=IM_COL32(Speedometer::rgb[0],Speedometer::rgb[1],Speedometer::rgb[2],255),muted=IM_COL32(118,121,130,255);
    ImU32 gain=v.gain<-.5f?IM_COL32(245,121,133,255):IM_COL32(MovementHud::accent[0],MovementHud::accent[1],MovementHud::accent[2],255);
    float pulse=MovementHud::videoEffects?.28f+.72f*MovementHud::perfectPulse(v.result,now):MovementHud::glowStrength(now,v.result.tick);bool halo=glow&&v.result.kind!=MovementHud::Ladder;
    if(MovementHud::videoEffects&&glow){float t=std::clamp(MovementHud::animationAge(now,v.result.tick)*5.f,0.f,1.f);p.y+=fs*.1f*(1-t)*(1-t);}
    fogVideoBackdrop(d,p,style,fs,v,size,now,glow);
    FogVideoFinish finish{d,p,style,fs,v,size,now,glow};
    auto text=[&](ImFont *font,float sz,ImVec2 at,ImU32 color,const char *s,bool lit=false){
        font=hudFontFor(font,sz);
        at={std::round(at.x*scale)/scale,std::round(at.y*scale)/scale};
        if(lit&&halo)fogGlowText(d,font,sz,at,color,s,pulse,fs*(.07f+.055f*pulse));
        if(s==v.number)speedometerBorder(d,font,sz,at,s);
        else hudDrawText(d,font,sz,at+ImVec2(.7f/scale,.7f/scale),IM_COL32(0,0,0,150),s);
        hudDrawText(d,font,sz,at,color,s);
    };
    auto line=[&](ImVec2 a,ImVec2 b){
        if(halo)for(int i=3;i>0;--i)d->AddLine(a,b,(accent&~IM_COL32_A_MASK)|IM_COL32(0,0,0,static_cast<int>((16-i*3)*pulse)),fs*(.05f+i*.05f*pulse));
        d->AddLine(a,b,accent,std::max(.8f,fs*.055f));
    };
    auto top=[&](ImVec2 at){text(regular,fs,at,white,v.number);if(v.showGain)text(bold,fs*.46f,at+ImVec2(fogWidth(regular,fs,v.number)+gap,fs*.22f),gain,v.delta);};
    float tw=v.top?fogWidth(regular,fs,v.number)+(v.showGain?gap+fogWidth(bold,fs*.46f,v.delta):0):0;
    float lw=fogWidth(bold,lo,v.label),pw=fogWidth(bold,lo,v.pre);
    if(!v.bottom){if(v.top)top(p);return;}
    if(style==MovementHud::Inline){
        if(v.top)top(p);float x=p.x+tw+(v.top?gap:0),h=fs*.9f,bw=lw+fs*.5f;
        d->AddRectFilled({x,p.y},{x+bw,p.y+h},accent,fs*.18f);text(bold,lo,{x+fs*.25f,p.y+fs*.14f},IM_COL32(11,22,16,255),v.label);
        text(bold,lo,{x+bw+gap,p.y+fs*.18f},accent,v.pre,true);return;
    }
    if(style==MovementHud::Split&&v.top){
        float left=std::max(fogWidth(regular,fs,v.number),v.showGain?fogWidth(bold,lo,v.delta):0);
        text(regular,fs,p,white,v.number);if(v.showGain)text(bold,lo,p+ImVec2(left-fogWidth(bold,lo,v.delta),fs*1.22f),gain,v.delta);
        line(p+ImVec2(left+gap,0),p+ImVec2(left+gap,size.y));
        text(bold,lo,p+ImVec2(left+gap*2,fs*.18f),accent,v.label,true);text(bold,lo,p+ImVec2(left+gap*2,fs*1.25f),accent,v.pre,true);return;
    }
    if(style==MovementHud::Card){
        d->AddRectFilled(p,p+size,IM_COL32(10,11,16,225),fs*.45f);d->AddRect(p,p+size,IM_COL32(255,255,255,25),fs*.45f);
        ImVec2 inner=p+ImVec2(fs*.4f,fs*.3f);if(v.top)top(inner);
        text(bold,lo,{p.x+size.x-lw-fs*.4f,p.y+fs*.42f},accent,v.label,true);
        float y=p.y+fs*1.65f;d->AddLine({inner.x,y},{p.x+size.x-fs*.4f,y},IM_COL32(42,44,51,255),fs*.09f);
        line({inner.x,y},{inner.x+(size.x-fs*.8f)*std::clamp(v.result.pre/300.f,0.f,1.f),y});
        text(bold,lo,{inner.x,p.y+fs*1.95f},accent,v.pre,true);text(bold,lo,{p.x+size.x-fogWidth(bold,lo,"300")-fs*.4f,p.y+fs*1.95f},muted,"300");return;
    }
    if(v.top)top(p+ImVec2((size.x-tw)/2,0));float y=p.y+(v.top?fs*1.65f:0);
    if(style==MovementHud::Capsule){
        float w=lw+pw+gap+fs*.75f,x=p.x+(size.x-w)/2;
        d->AddRectFilled({x,y},{x+w,y+fs*.9f},IM_COL32(7,10,14,225),fs*.3f);
        line({x+fs*.2f,y+fs*.2f},{x+fs*.2f,y+fs*.68f});
        text(bold,lo,{x+fs*.43f,y+fs*.15f},accent,v.label,true);
        float divider=x+fs*.43f+lw+gap*.45f;d->AddLine({divider,y+fs*.22f},{divider,y+fs*.68f},IM_COL32(134,140,146,90));
        text(bold,lo,{divider+gap*.55f,y+fs*.15f},accent,v.pre,true);
    }else if(style==MovementHud::Badge||style==MovementHud::Ring){
        float diameter=std::max(fs*.9f,fogWidth(bold,lo,v.count)+fs*.3f),row=diameter+fs*.1f+gap+pw,x=p.x+(size.x-row)/2;
        if(style==MovementHud::Badge)d->AddRectFilled({x,y},{x+diameter,y+diameter},accent,fs*.22f);
        else{if(halo)d->AddCircle({x+diameter/2,y+diameter/2},diameter/2+fs*.07f, (accent&~IM_COL32_A_MASK)|IM_COL32(0,0,0,static_cast<int>(32*pulse)),32,fs*.12f);d->AddCircle({x+diameter/2,y+diameter/2},diameter/2,accent,32,std::max(1.f,fs*.07f));}
        text(bold,lo,{x+(diameter-fogWidth(bold,lo,v.count))/2,y+(diameter-lo)/2},style==MovementHud::Badge?IM_COL32(11,22,16,255):accent,v.count,style==MovementHud::Ring);
        text(bold,lo,{x+diameter+fs*.1f+gap,y+(diameter-lo)/2},accent,v.pre,true);
    }else if(style==MovementHud::Classic){
        if(v.top)line(p+ImVec2(size.x*.33f,fs*1.25f),p+ImVec2(size.x*.67f,fs*1.25f));
        text(bold,lo,{p.x+(size.x-fogWidth(bold,lo,v.full))/2,y},accent,v.full,true);
    }else{
        float x=p.x+(size.x-lw-gap-pw)/2;text(bold,lo,{x,y},accent,v.label,true);text(bold,lo,{x+lw+gap,y},accent,v.pre,true);
        float mid=p.x+size.x/2,base=y+fs*.77f;
        auto fade=[&](float thickness,int alpha){ImU32 color=(accent&~IM_COL32_A_MASK)|IM_COL32(0,0,0,alpha);
            d->AddRectFilledMultiColor({p.x,base-thickness/2},{mid,base+thickness/2},0,color,color,0);
            d->AddRectFilledMultiColor({mid,base-thickness/2},{p.x+size.x,base+thickness/2},color,0,0,color);
        };
        if(halo)fade(fs*.16f,static_cast<int>(35*pulse));fade(std::max(.8f,fs*.045f),235);
    }
}
static const char *fogStyleName(int style){static const char *en[]={"Capsule","Underline","Classic","Single line","Split","Badge","Ring","Card"};return Lang::english()?en[style]:MovementHud::styleNames[style];}
static void fogStyleGallery(){
    card(L(u8"Предпросмотр стилей","Style preview"),{0,ImGui::GetContentRegionAvail().y});
    auto local=MovementHud::displayResult();bool measured=MovementHud::visible(GetTickCount64());
    if(measured)ImGui::Text(L(u8"FOG: %d","FOG: %d"),local.fog);
    else{
        ImGui::TextWrapped("%s",L(u8"Демонстрация оформления. Нет свежего результата прыжка.","Style demonstration. No recent jump result."));
        if(ImGui::SliderInt(L(u8"FOG в примере","Preview FOG"),&MovementHud::previewFog,1,6,"%d"))MovementHud::previewEpoch=0;
        if(MovementHud::videoEffects&&ImGui::Checkbox(L(u8"Ideal bhop в примере","Preview Ideal bhop"),&MovementHud::previewIdeal))MovementHud::previewEpoch=0;
    }
    ULONGLONG now=GetTickCount64();if(MovementHud::videoEffects&&now-MovementHud::previewTakeoff(now)>static_cast<ULONGLONG>(MovementHud::animationMs+900))MovementHud::previewEpoch=now;
    MovementHud::Result demo={MovementHud::Bhop,MovementHud::previewFog,284.106f,MovementHud::previewTakeoff(now),true,MovementHud::videoEffects&&MovementHud::previewIdeal};
    FogArt sample(measured?local:demo,Speedometer::fresh(now)?Speedometer::speed:286.f,measured?MovementHud::track.gain:45.f,true,true,true);
    for(int style=0;style<MovementHud::StyleCount;++style){
        ImGui::PushID(style);ImVec2 p=ImGui::GetCursorScreenPos();float w=std::max(32.f,ImGui::GetContentRegionAvail().x),h=104;
        // fogArt is also used by the in-game HUD, where its colours stay native.
        // Fade only this menu preview's newly emitted vertices, exactly once.
        auto *previewDraw=ImGui::GetWindowDrawList();int firstVertex=previewDraw->VtxBuffer.Size;
        float previewAlpha=ImGui::GetStyle().Alpha;ImGui::PushStyleVar(ImGuiStyleVar_Alpha,1.f);
        if(ImGui::InvisibleButton("##fog style",{w,h})){MovementHud::style=style;save();}
        auto *d=ImGui::GetWindowDrawList();bool selected=MovementHud::style==style;
        d->AddRectFilled(p,p+ImVec2(w,h),IM_COL32(15,15,20,255),6);d->AddRect(p,p+ImVec2(w,h),selected?ImGui::GetColorU32(ImGuiCol_Scheme):IM_COL32(255,255,255,25),6,0,selected?1.5f:1.f);
        d->AddText(p+ImVec2(9,7),ImGui::GetColorU32(selected?ImGuiCol_Text:ImGuiCol_TextDisabled),fogStyleName(style));
        float fs=24;auto extent=fogArtSize(style,fs,sample);float fit=std::min(1.f,std::min((w-24)/extent.x,66.f/extent.y));fs*=std::max(.15f,fit);extent=fogArtSize(style,fs,sample);
        d->PushClipRect(p+ImVec2(3,24),p+ImVec2(w-3,h-3),true);fogArt(d,p+ImVec2((w-extent.x)/2,30+(66-extent.y)/2),style,fs,sample,now,selected);d->PopClipRect();
        ImGui::PopStyleVar();
        for(int i=firstVertex;i<d->VtxBuffer.Size;++i){auto &color=d->VtxBuffer[i].col;
            ImU32 alpha=static_cast<ImU32>(((color>>IM_COL32_A_SHIFT)&255)*previewAlpha+.5f);
            color=(color&~IM_COL32_A_MASK)|(alpha<<IM_COL32_A_SHIFT);}
        ImGui::PopID();
    }endCard();
}
