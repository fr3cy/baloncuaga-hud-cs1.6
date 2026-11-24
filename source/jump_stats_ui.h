#pragma once
static const char *jumpStyleName(int n){
    const char *ru[]={u8"Таблица",u8"Мини",u8"Строки",u8"Контраст",u8"Синк",u8"Нативный"};
    const char *en[]={"Table","Mini","Lines","Contrast","Sync","Native"};return Lang::english()?en[n]:ru[n];
}
static ImU32 strafeColor(int r,int g,int b){return IM_COL32(r,g,b,std::clamp(JumpStats::opacity,20,100)*255/100);}
static ImU32 strafeAccent(){return strafeColor(JumpStats::rgb[0],JumpStats::rgb[1],JumpStats::rgb[2]);}
static ImFont *strafeFont(){return chatBold?chatBold:ImGui::GetFont();}
static std::string strafeLine(const JumpStats::Row &r){return r.keys+"  +"+r.gain+" / -"+r.loss+"  "+r.frames+"f  "+r.sync;}
struct StrafeLayout {float widths[6]={},gap=0,row=0,header=0;ImVec2 size;int limit=0;bool index=false,labels=false;};
static StrafeLayout strafeLayout(const std::vector<JumpStats::Row> &rows,float fs,int limit,int style){
    StrafeLayout out;out.limit=std::min(limit,int(rows.size()));out.gap=fs*.65f;out.row=fs+(style==JumpStats::Mini?1.f:3.f);out.header=style==JumpStats::Native||style==JumpStats::Lines?fs+4:2*(fs+4);
    out.index=style==JumpStats::Table||style==JumpStats::Contrast||style==JumpStats::SyncBars;out.labels=style!=JumpStats::Native&&style!=JumpStats::Lines;
    auto *face=strafeFont();auto width=[&](const std::string &text){return face->CalcTextSizeA(fs,FLT_MAX,0,text.c_str()).x;};
    const char *headers[]={"#",L(u8"Кл.","Key"),"Gain","Loss",L(u8"Кдр","Fr"),"Sync"};
    for(int col=0;col<6;++col)if(out.labels)out.widths[col]=width(headers[col]);
    for(int i=0;i<out.limit;++i){const auto &r=rows[i];std::string values[]={std::to_string(r.index?r.index:i+1),r.keys,r.gain,r.loss,r.frames,r.sync};
        for(int col=0;col<6;++col)out.widths[col]=std::max(out.widths[col],width(values[col]));
        if(style==JumpStats::Lines)out.size.x=std::max(out.size.x,width(strafeLine(r)));
    }
    if(style!=JumpStats::Lines){int first=out.index?0:1;for(int col=first;col<6;++col)out.size.x+=out.widths[col]+(col==5?0:out.gap);}
    if(style==JumpStats::Lines)out.size.x=std::max(out.size.x,width(L(u8"Стрейфы","Strafes")));
    out.size.y=out.header+out.limit*out.row+(out.limit<int(rows.size())?fs+3:0);return out;
}
static void strafeDraw(ImDrawList *d,ImVec2 p,const std::vector<JumpStats::Row> &rows,float fs,int limit,int style){
    auto layout=strafeLayout(rows,fs,limit,style);auto *font=strafeFont();ImU32 ink=strafeColor(235,240,242),muted=strafeColor(160,177,180);
    auto text=[&](ImVec2 at,ImU32 color,const std::string &value){hudDrawText(d,font,fs,at,color,value.c_str());};
    const char *headers[]={"#",L(u8"Кл.","Key"),"Gain","Loss",L(u8"Кдр","Fr"),"Sync"};
    int total=int(rows.size());for(const auto &r:rows)total=std::max(total,r.index);
    text(p,muted,std::string(L(u8"Стрейфы: ","Strafes: "))+std::to_string(total));
    if(layout.labels){float x=p.x;for(int col=layout.index?0:1;col<6;++col){text({x,p.y+fs+4},muted,headers[col]);x+=layout.widths[col]+layout.gap;}}
    for(int i=0;i<layout.limit;++i){auto &r=rows[i];ImVec2 at={p.x,p.y+layout.header+i*layout.row};
        if(style==JumpStats::Lines){text(at,ink,strafeLine(r));continue;}
        std::string values[]={std::to_string(r.index?r.index:i+1),r.keys,r.gain,r.loss,r.frames,r.sync};
        for(int col=layout.index?0:1;col<6;++col){
            auto color=style==JumpStats::Native?strafeAccent():col==1?strafeAccent():ink;
            if(style==JumpStats::Contrast&&col==2)color=strafeColor(132,235,174);
            if(style==JumpStats::Contrast&&col==3&&strtod(r.loss.c_str(),nullptr)>0)color=strafeColor(247,150,150);
            float w=font->CalcTextSizeA(fs,FLT_MAX,0,values[col].c_str()).x;float x=at.x+((col>=2)?layout.widths[col]-w:0);text({x,at.y},color,values[col]);
            if(style==JumpStats::SyncBars&&col==5){float sync=std::clamp(float(strtod(r.sync.c_str(),nullptr))/100.f,0.f,1.f);d->AddLine({at.x,at.y+fs+1},{at.x+layout.widths[col]*sync,at.y+fs+1},strafeAccent(),1.f);}
            at.x+=layout.widths[col]+layout.gap;
        }
    }
    if(layout.limit<int(rows.size()))text({p.x,p.y+layout.header+layout.limit*layout.row},muted,std::string("+")+std::to_string(rows.size()-layout.limit));
}
static ImVec2 strafeOverlayPosition(ImVec2 screen,ImVec2 extent){
    float x=screen.x*JumpStats::tableX/100.f-extent.x,y=screen.y*JumpStats::tableY/100.f;
    return {std::clamp(x,6.f,std::max(6.f,screen.x-extent.x-6)),std::clamp(y,6.f,std::max(6.f,screen.y-extent.y-6))};
}
static void jumpStatsOverlay(){
    JumpStats::presentable.store(true,std::memory_order_release);
    if(outsideGame||opened||!JumpStats::visible(GetTickCount64()))return;
    auto value=JumpStats::snapshot();auto screen=ImGui::GetIO().DisplaySize;float unit=HudResolution::factor(screenW,screenH)/scale,fs=JumpStats::size*unit;
    int limit=std::min(32,int(value.rows.size()));auto layout=strafeLayout(value.rows,fs,limit,JumpStats::style);
    for(int pass=0;pass<40&&(layout.size.x>screen.x*.39f||layout.size.y>screen.y*.60f);++pass){
        if(fs>9*unit)fs=std::max(9*unit,fs*.9f);else if(limit>3)--limit;else break;layout=strafeLayout(value.rows,fs,limit,JumpStats::style);
    }
    auto at=strafeOverlayPosition(screen,layout.size);strafeDraw(ImGui::GetBackgroundDrawList(),at,value.rows,fs,limit,JumpStats::style);
}
static JumpStats::Snapshot jumpDemo(){
    auto value=JumpStats::snapshot();if(!value.rows.empty())return value;
    value.rows={{"A","12.1","0.0","12","66%",1},{"D","11.1","0.0","9","100%",2},{"A","7.0","0.0","8","75%",3},{"D","5.7","0.0","9","55%",4},{"A","2.2","0.0","7","28%",5},{"D","2.9","0.0","11","27%",6},{"A","4.2","0.0","14","71%",7}};return value;
}
static void jumpStatsPage(){
    float w=(ImGui::GetContentRegionAvail().x-10)/2,h=ImGui::GetContentRegionAvail().y;
    card(L(u8"Стрейфы","Strafes"),{w,h});
    bool edit=ImGui::Checkbox(L(u8"Кастомная стата стрейфов","Custom strafe stats"),&JumpStats::enabled);if(edit)JumpStats::syncNativeText();
    const char *names[6];for(int i=0;i<6;++i)names[i]=jumpStyleName(i);
    edit|=ImGui::Combo(L(u8"Стиль","Style"),&JumpStats::style,names,6);
    edit|=ImGui::SliderInt(L(u8"Размер текста","Text size"),&JumpStats::size,10,18,"%d px");
    edit|=ImGui::SliderInt(L(u8"Видимость текста","Text opacity"),&JumpStats::opacity,20,100,"%d%%");
    float color[3]={JumpStats::rgb[0]/255.f,JumpStats::rgb[1]/255.f,JumpStats::rgb[2]/255.f};
    if(ImGui::ColorEdit3(L(u8"Цвет","Colour"),color)){for(int i=0;i<3;++i)JumpStats::rgb[i]=std::clamp(int(std::lround(color[i]*255)),0,255);edit=true;}
    edit|=ImGui::SliderInt(L(u8"Справа по X","Right anchor X"),&JumpStats::tableX,35,100,"%d%%");
    edit|=ImGui::SliderInt("Y##strafes",&JumpStats::tableY,0,100,"%d%%");
    edit|=ImGui::SliderInt(L(u8"Время показа","Display time"),&JumpStats::holdMs,1000,12000,L(u8"%d мс","%d ms"));
    if(ImGui::Button(L(u8"Сбросить положение","Reset position"))){JumpStats::tableX=98;JumpStats::tableY=18;JumpStats::size=12;edit=true;}
    if(!JumpStats::hooksAvailable)ImGui::TextWrapped("%s",L(u8"Этот клиент не поддерживает замену HUD-текста.","HUD text replacement is unavailable for this client."));
    if(edit)JumpStats::save();endCard();ImGui::SameLine();
    card(L(u8"Стили","Styles"),{w,h});auto sample=jumpDemo();
    for(int i=0;i<6;++i){ImGui::PushID(i);
        if(ImGui::Selectable(names[i],JumpStats::style==i,0,{0,23})){JumpStats::style=i;JumpStats::save();}
        float fs=12;auto layout=strafeLayout(sample.rows,fs,4,i);float available=ImGui::GetContentRegionAvail().x-8;
        if(layout.size.x>available)fs*=available/layout.size.x;layout=strafeLayout(sample.rows,fs,4,i);auto p=ImGui::GetCursorScreenPos();
        ImGui::Dummy({0,layout.size.y+10});auto *d=ImGui::GetWindowDrawList();d->PushClipRect(p,p+ImVec2(available,layout.size.y),true);strafeDraw(d,p+ImVec2(3,0),sample.rows,fs,4,i);d->PopClipRect();ImGui::PopID();
    }endCard();
}
