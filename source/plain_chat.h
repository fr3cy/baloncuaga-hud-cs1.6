#pragma once
// Compact, ordinary chat: one translucent background per wrapped text row.
struct PlainChatRow {const Hud::ChatLine *message;std::vector<Hud::ChatSpan> spans;float width;};
// Newest rows first, stopping once `wanted` rows exist: only what can be shown is wrapped.
static std::vector<PlainChatRow> plainChatRows(float width,bool fullHistory,ULONGLONG now,size_t wanted=static_cast<size_t>(-1)){
    const float fontSize=14*ChatBox::textScale();
    std::vector<PlainChatRow> rows;
    for(auto it=Hud::history.rbegin();it!=Hud::history.rend()&&rows.size()<wanted;++it){const auto &message=*it;
        if(message.serial<=ChatBox::cleared)continue;
        if(!fullHistory&&(!message.tick||now<message.tick||now-message.tick>=12000)){if(message.tick&&now>=message.tick)break;continue;}
        // CSS presentation filters and its collapsed state do not hide messages
        // in the ordinary theme, which has no corresponding buttons.
        auto lines=Hud::wrapChatSpans(message,width-14,[fontSize](const std::string &text){return chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,text.c_str()).x;});
        for(size_t i=lines.size();i-->0;){float measured=0;for(const auto &span:lines[i])measured+=chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,span.text.c_str()).x;
            rows.push_back({&message,std::move(lines[i]),measured});}
    }std::reverse(rows.begin(),rows.end());return rows;
}
static void plainChatPanel(bool embedded){
    auto &io=ImGui::GetIO();bool interactive=embedded||Hud::chatOpened;
    const float fontSize=14*ChatBox::textScale(),rowHeight=fontSize+8,inputHeight=fontSize+10;
    float w=embedded?ImGui::GetContentRegionAvail().x:std::min(ChatBox::panelWidth(560),io.DisplaySize.x-24);
    float h=embedded?std::min(300.f,ImGui::GetContentRegionAvail().y):std::min(interactive?(ChatBox::lineCount+4)*rowHeight+inputHeight+2:ChatBox::lineCount*rowHeight,io.DisplaySize.y-80);
    if(!embedded)h=std::min(h,std::max(0.f,io.DisplaySize.y-std::max(48.f,combatChatInset/scale)-8));
    if(w<120||h<(interactive?inputHeight+rowHeight:rowHeight))return;
    float area=h-(interactive?inputHeight+2:0);int fits=std::max(1,static_cast<int>(area/rowHeight));
    auto rows=plainChatRows(w,interactive,GetTickCount64(),static_cast<size_t>(fits+(interactive?std::max(0,Hud::chatScroll)+12:0)));if(!embedded&&rows.empty()&&!Hud::chatOpened)return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    int flags=ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    if(embedded)ImGui::BeginChild("##PlainChat",{w,h},false,flags);
    else{
        ImGui::SetNextWindowPos({12,std::max(8.f,io.DisplaySize.y-h-std::max(48.f,combatChatInset/scale))},ImGuiCond_Always);ImGui::SetNextWindowSize({w,h});
        flags|=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
        if(!interactive)flags|=ImGuiWindowFlags_NoInputs;ImGui::Begin("##BALONCUAGAPlainChat",nullptr,flags);
    }
    ImGui::PopStyleVar(2);auto p=ImGui::GetWindowPos();auto *d=ImGui::GetWindowDrawList();
    float textArea=h-(interactive?inputHeight+2:0);int capacity=std::max(1,static_cast<int>(textArea/rowHeight));
    Hud::chatScrollLimit=std::max(0,static_cast<int>(rows.size())-capacity);Hud::chatScroll=std::clamp(Hud::chatScroll,0,Hud::chatScrollLimit);
    if(interactive&&ImGui::IsWindowHovered()&&io.MouseWheel!=0)Hud::chatScroll=std::clamp(Hud::chatScroll+static_cast<int>(io.MouseWheel*2),0,Hud::chatScrollLimit);
    int end=static_cast<int>(rows.size())-(interactive?Hud::chatScroll:0),begin=std::max(0,end-capacity);
    float y=embedded?p.y+2:p.y+textArea-(end-begin)*rowHeight;
    d->PushClipRect(p,p+ImVec2(w,textArea),true);
    for(int i=begin;i<end;++i){const auto &line=rows[i];float x=p.x+6;
        d->AddRectFilled({p.x,y},{p.x+std::min(w,line.width+12),y+rowHeight-2},IM_COL32(15,15,12,145));
        for(const auto &span:line.spans){int r=228,g=228,b=220;
            if(span.code==4){r=62;g=169;b=66;}else if(span.code==3)Hud::nativeChatRgb(span.code,line.message->side,r,g,b);
            referenceText(d,chatSmall,fontSize,{x,y+3},IM_COL32(r,g,b,255),span.text);x+=chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,span.text.c_str()).x;
        }y+=rowHeight;
    }d->PopClipRect();
    if(interactive){
        float fy=h-inputHeight;ImVec2 field=p+ImVec2(0,fy);d->AddRectFilled(field,field+ImVec2(w,inputHeight),IM_COL32(15,15,12,145));
        const char *prefix=ChatEditor::team?u8"Команде: ":u8"Сказать: ";float prefixWidth=chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,prefix).x;
        referenceText(d,chatSmall,fontSize,field+ImVec2(6,5),IM_COL32(228,228,220,255),prefix);
        ImGui::SetCursorPos({0,fy});if(ImGui::InvisibleButton("##plain chat input",{w,inputHeight})&&!Hud::chatOpened)ChatEditor::begin(ChatEditor::team);
        size_t caret=std::min(ChatEditor::caret,ChatEditor::input.size()),first=0;float available=std::max(20.f,w-prefixWidth-18);
        while(first<caret&&chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,Hud::utf8(ChatEditor::input.substr(first,caret-first)).c_str()).x>available){
            if(ChatEditor::input[first]>=0xD800&&ChatEditor::input[first]<=0xDBFF&&first+1<caret)first+=2;else ++first;
        }
        auto text=Hud::utf8(ChatEditor::input.substr(first));ImVec2 start=field+ImVec2(6+prefixWidth,5);
        d->PushClipRect(field+ImVec2(6+prefixWidth,0),field+ImVec2(w-6,inputHeight),true);referenceText(d,chatSmall,fontSize,start,IM_COL32(228,228,220,255),text);
        if(Hud::chatOpened&&(GetTickCount64()/500)%2==0){float cx=start.x+chatSmall->CalcTextSizeA(fontSize,FLT_MAX,0,Hud::utf8(ChatEditor::input.substr(first,caret-first)).c_str()).x;d->AddLine({cx,field.y+4},{cx,field.y+rowHeight-2},IM_COL32(228,228,220,255));}
        d->PopClipRect();
    }
    if(embedded)ImGui::EndChild();else ImGui::End();
}
