#pragma once
// Top info bar ("watermark"): baLONCUaga brand plus nick, map, FPS, ping,
// speed and time, in four styles, three positions and the menu palettes.
namespace Watermark {
static ImFont *modernBrand,*modernValue,*modernUnit;static bool modernChrome;
enum { Plate, Text, Block, Chips, StyleCount };
enum { Left, Center, Right };
enum { Brand=1, Nick=2, Map=4, Fps=8, Ping=16, Speed=32, Time=64, AllFields=127 };
static int style=Plate,place=Right,opacity=96,size=100,theme=0,fields=AllFields;
static bool twoColors=true;
static int colors[2][3]={{124,108,255},{56,201,255}};
static void applyTheme(int index){theme=std::clamp(index,0,MenuTheme::AccentCount-1);for(int i=0;i<2;++i)for(int c=0;c<3;++c)colors[i][c]=MenuTheme::accents[theme][i][c];}
static void defaults(){style=Plate;place=Right;opacity=96;size=100;fields=AllFields;twoColors=true;applyTheme(0);}
static void save(){
    auto put=[](const char *key,int value){char t[16];snprintf(t,sizeof(t),"%d",value);WritePrivateProfileStringA("interface",key,t,iniPath);};
    put("watermark_style",style);put("watermark_place",place);put("watermark_opacity",opacity);put("watermark_size",size);
    put("watermark_theme",theme);put("watermark_fields",fields);put("watermark_two_colors",twoColors);
    for(int i=0;i<2;++i)for(int c=0;c<3;++c){char k[40];snprintf(k,sizeof(k),"watermark_color_%d_%d",i,c);put(k,colors[i][c]);}
}
static void load(){
    auto get=[](const char *key,int fallback,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath)),lo,hi);};
    defaults();style=get("watermark_style",Plate,0,StyleCount-1);place=get("watermark_place",Right,0,2);opacity=get("watermark_opacity",96,30,100);
    size=get("watermark_size",100,70,150);applyTheme(get("watermark_theme",0,0,MenuTheme::AccentCount-1));fields=get("watermark_fields",AllFields,0,AllFields);
    twoColors=get("watermark_two_colors",1,0,1)!=0;
    for(int i=0;i<2;++i)for(int c=0;c<3;++c){char k[40];snprintf(k,sizeof(k),"watermark_color_%d_%d",i,c);colors[i][c]=get(k,colors[i][c],0,255);}
}
struct Field {std::string value,unit;};
static ImU32 color(int which,float a){const int *c=colors[twoColors?which:0];return IM_COL32(c[0],c[1],c[2],static_cast<int>(std::clamp(a,0.f,1.f)*255));}
static ImU32 white(float a){return IM_COL32(240,240,246,static_cast<int>(std::clamp(a,0.f,1.f)*255));}
static ImU32 dim(float a){return IM_COL32(160,160,172,static_cast<int>(std::clamp(a,0.f,1.f)*255));}
// Gradient bar from colour 1 to colour 2 (or a solid bar with one colour).
static void bar(ImDrawList *d,ImVec2 a,ImVec2 b,float alpha,float rounding=1){
    int start=d->VtxBuffer.Size;d->AddRectFilled(a,b,IM_COL32(255,255,255,static_cast<int>(255*alpha)),rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,a,{b.x,a.y},color(0,1),color(1,1));
}
struct Fonts {ImFont *brand,*value,*unit;float brandSize,valueSize,unitSize;};
static float textWidth(ImFont *f,float s,const std::string &t){return t.empty()?0:f->CalcTextSizeA(s,FLT_MAX,0,t.c_str()).x;}
static float brandWidth(const Fonts &f){return f.brandSize*2.20f+textWidth(f.brand,f.brandSize,"baLONCUaga");}
static void drawBrand(ImDrawList *d,const Fonts &f,ImVec2 p,float alpha){
    // The visible game HUD and Insert share one embedded shoe texture. Closed
    // native main-menu frames do not call this renderer or initialize the image.
    SneakerArt::shoe(d,p+ImVec2(f.brandSize*.925f,f.brandSize*.47f),f.brandSize*.925f,MenuTheme::reduceMotion?0.f:float(ImGui::GetTime()),alpha);
    p.x+=f.brandSize*2.20f;
    if(style==Text||style==Block)d->AddText(f.brand,f.brandSize,p+ImVec2(1,1),IM_COL32(0,0,0,static_cast<int>(150*alpha)),"baLONCUaga"); // readable over bright scenes
    d->AddText(f.brand,f.brandSize,p,white(alpha),"baLONCUaga");
}
static float fieldWidth(const Fonts &f,const Field &x){return textWidth(f.value,f.valueSize,x.value)+(x.unit.empty()?0:3+textWidth(f.unit,f.unitSize,x.unit));}
static void drawField(ImDrawList *d,const Fonts &f,ImVec2 p,float lineH,const Field &x,float alpha){
    if(style==Text||style==Block){ImVec2 s(1,1);d->AddText(f.value,f.valueSize,ImVec2(p.x,p.y+(lineH-f.valueSize)/2)+s,IM_COL32(0,0,0,static_cast<int>(150*alpha)),x.value.c_str());
        if(!x.unit.empty())d->AddText(f.unit,f.unitSize,ImVec2(p.x+textWidth(f.value,f.valueSize,x.value)+3,p.y+(lineH-f.unitSize)/2+1)+s,IM_COL32(0,0,0,static_cast<int>(150*alpha)),x.unit.c_str());}
    d->AddText(f.value,f.valueSize,{p.x,p.y+(lineH-f.valueSize)/2},white(alpha),x.value.c_str());
    if(!x.unit.empty())d->AddText(f.unit,f.unitSize,{p.x+textWidth(f.value,f.valueSize,x.value)+3,p.y+(lineH-f.unitSize)/2+1},dim(alpha),x.unit.c_str());
}
// Draws at the top of the given area; `fieldsIn` are already filtered.
static ImVec2 draw(ImDrawList *d,ImVec2 areaMin,ImVec2 areaMax,const std::vector<Field> &list,bool brand,float alpha,float scale){
    float s=scale*std::clamp(size,70,150)/100.f;auto *io=ImGui::GetIO().Fonts;
    Fonts f{io->Fonts[3],io->Fonts[1],io->Fonts[0],15*s,13*s,11*s};
    if(modernChrome&&modernBrand&&modernValue&&modernUnit)f={modernBrand,modernValue,modernUnit,modernBrand->FontSize*s,modernValue->FontSize*s,modernUnit->FontSize*s};
    float pad=10*s,sep=16*s,h=26*s;ImVec2 extent;
    float fieldsW=0;for(size_t i=0;i<list.size();++i)fieldsW+=fieldWidth(f,list[i])+(i?sep:0);
    float bw=brand?brandWidth(f):0;
    if(style==Plate)extent={pad*2+(brand?bw+(list.empty()?0:sep):0)+fieldsW,h};
    else if(style==Text)extent={(brand?bw+(list.empty()?0:sep):0)+fieldsW,h};
    else if(style==Block){float lineW=fieldsW;extent={8*s+std::max(bw,lineW),(brand?f.brandSize+6*s:0)+f.valueSize+4*s};}
    else{float w=0;if(brand)w+=bw+pad*2;for(const auto &x:list)w+=(w>0?5*s:0)+fieldWidth(f,x)+pad*1.6f;extent={w,h};}
    // Keep all selected fields on narrow screens, including the compact 3XL
    // badge. A single proportional fit preserves the four layouts and spacing.
    float available=areaMax.x-areaMin.x-20*s;
    if(available>0&&extent.x>available){
        float fit=available/extent.x;s*=fit;pad*=fit;sep*=fit;h*=fit;
        f.brandSize*=fit;f.valueSize*=fit;f.unitSize*=fit;fieldsW*=fit;bw*=fit;extent*=fit;
    }
    float margin=10*s;float x=place==Left?areaMin.x+margin:place==Center?(areaMin.x+areaMax.x-extent.x)/2:areaMax.x-margin-extent.x;
    ImVec2 p(x,areaMin.y+margin);
    if(style==Plate){
        d->AddRectFilled(p,p+extent,IM_COL32(17,19,23,static_cast<int>((modernChrome?130:225)*alpha)),h/2);
        d->AddRect(p,p+extent,IM_COL32(255,255,255,static_cast<int>(20*alpha)),h/2);
        bar(d,{p.x+h/2,p.y+h-2.2f*s},{p.x+extent.x-h/2,p.y+h-.6f*s},alpha);
        float cx=p.x+pad;
        if(brand){drawBrand(d,f,{cx,p.y+(h-f.brandSize)/2},alpha);cx+=bw;}
        for(size_t i=0;i<list.size();++i){if(brand||i){d->AddLine({cx+sep/2,p.y+7*s},{cx+sep/2,p.y+h-7*s},IM_COL32(255,255,255,static_cast<int>(40*alpha)));cx+=sep;}
            drawField(d,f,{cx,p.y},h,list[i],alpha);cx+=fieldWidth(f,list[i]);}
    }else if(style==Text){
        float cx=p.x;
        if(brand){drawBrand(d,f,{cx,p.y+(h-f.brandSize)/2},alpha);cx+=bw;}
        for(size_t i=0;i<list.size();++i){if(brand||i){d->AddCircleFilled({cx+sep/2,p.y+h/2},1.4f*s,dim(alpha),8);cx+=sep;}
            drawField(d,f,{cx,p.y},h,list[i],alpha);cx+=fieldWidth(f,list[i]);}
    }else if(style==Block){
        int start=d->VtxBuffer.Size;d->AddRectFilled(p,{p.x+2.5f*s,p.y+extent.y},IM_COL32(255,255,255,static_cast<int>(255*alpha)));
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(d,start,d->VtxBuffer.Size,p,{p.x,p.y+extent.y},color(0,1),color(1,1));
        float cy=p.y;if(brand){drawBrand(d,f,{p.x+8*s,cy},alpha);cy+=f.brandSize+6*s;}
        float cx=p.x+8*s,lh=f.valueSize+4*s;
        for(size_t i=0;i<list.size();++i){if(i){d->AddLine({cx+sep/2,cy+3*s},{cx+sep/2,cy+lh-3*s},IM_COL32(255,255,255,static_cast<int>(45*alpha)));cx+=sep;}
            drawField(d,f,{cx,cy},lh,list[i],alpha);cx+=fieldWidth(f,list[i]);}
    }else{
        float cx=p.x;
        auto chip=[&](float w){d->AddRectFilled({cx,p.y},{cx+w,p.y+h},IM_COL32(40,28,30,static_cast<int>(215*alpha)),6*s);d->AddRect({cx,p.y},{cx+w,p.y+h},IM_COL32(255,255,255,static_cast<int>(22*alpha)),6*s);};
        if(brand){float w=bw+pad*2;chip(w);drawBrand(d,f,{cx+pad,p.y+(h-f.brandSize)/2-1*s},alpha);bar(d,{cx+pad,p.y+h-4*s},{cx+w-pad,p.y+h-2.4f*s},alpha);cx+=w;}
        for(const auto &x:list){if(cx>p.x)cx+=5*s;float w=fieldWidth(f,x)+pad*1.6f;chip(w);drawField(d,f,{cx+pad*.8f,p.y},h,x,alpha);cx+=w;}
    }
    return extent;
}
}
