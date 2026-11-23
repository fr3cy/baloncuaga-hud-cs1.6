#pragma once
static void referenceText(ImDrawList *d,ImFont *font,float size,ImVec2 p,ImU32 color,const std::string &text,bool shadow=true){
    p={std::round(p.x*scale)/scale,std::round(p.y*scale)/scale};
    if(shadow)hudDrawText(d,font,size,p+ImVec2(1/scale,1/scale),IM_COL32(0,0,0,190),text.c_str());hudDrawText(d,font,size,p,color,text.c_str());
}
static bool referenceButton(const char *id,ImVec2 local,ImVec2 size,const char *label,bool active,bool tab=false){
    ImGui::SetCursorPos(local);bool pressed=ImGui::InvisibleButton(id,size);auto p=ImGui::GetItemRectMin();auto *d=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered()||active)d->AddRectFilled(p,p+size,IM_COL32(118,176,46,active?24:16),3);
    if(tab&&active)d->AddLine(p+ImVec2(8,size.y-2),p+ImVec2(size.x-8,size.y-2),IM_COL32(158,209,77,255),2);
    auto textSize=chatButtons->CalcTextSizeA(14,FLT_MAX,0,label);d->AddText(chatButtons,14,p+(size-textSize)/2,active?IM_COL32(205,229,169,255):IM_COL32(204,207,204,255),label);return pressed;
}
#include "plain_chat.h"
static void chatPanel(bool embedded){
    if(ChatBox::theme==ChatBox::Plain){plainChatPanel(embedded);return;}
    auto &io=ImGui::GetIO();bool interactive=embedded||Hud::chatOpened;
    const float textSize=18*ChatBox::textScale(),smallSize=12*ChatBox::textScale(),rowHeight=std::max(27.f,textSize+9),timeWidth=58*ChatBox::textScale();
    float w=embedded?ImGui::GetContentRegionAvail().x:std::min(ChatBox::panelWidth(760),io.DisplaySize.x-24);
    float h=embedded?std::min(300.f,ImGui::GetContentRegionAvail().y):interactive?123.f+ChatBox::lineCount*rowHeight:16.f+ChatBox::lineCount*rowHeight;
    if(!embedded)h=std::min(h,std::max(0.f,io.DisplaySize.y-std::max(48.f,combatChatInset/scale)-8));
    if(w<280||h<(interactive?106+rowHeight:rowHeight+16))return;
    struct Row {const Hud::ChatLine *message;std::vector<Hud::ChatSpan> spans;bool first;};std::vector<Row> rows;
    ULONGLONG now=GetTickCount64();float textWidth=w-30-(ChatBox::timeOn?timeWidth:0);
    // Only the rows that can be shown are wrapped, newest first. Wrapping the whole
    // history (up to 2000 messages) every frame cut the frame rate while chat was open.
    int capacity=std::min(ChatBox::lineCount,std::max(1,static_cast<int>((h-(interactive?106:16))/rowHeight)));
    size_t wanted=interactive?static_cast<size_t>(capacity+std::max(0,Hud::chatScroll)+12):static_cast<size_t>(capacity);
    size_t unread=0;
    for(auto it=Hud::history.rbegin();it!=Hud::history.rend();++it){const auto &message=*it;
        if(rows.size()>=wanted){unread=static_cast<size_t>(std::distance(it,Hud::history.rend()));break;}
        if(!ChatBox::visible(message))continue;
        if(!interactive&&(!message.tick||now<message.tick||now-message.tick>=12000)){if(message.tick&&now>=message.tick)break;continue;}
        auto lines=Hud::wrapChatSpans(message,textWidth,[textSize](const std::string &s){return chatBold->CalcTextSizeA(textSize,FLT_MAX,0,s.c_str()).x;});
        for(size_t i=lines.size();i-->0;)rows.push_back({&message,std::move(lines[i]),i==0});
    }
    std::reverse(rows.begin(),rows.end());
    if(!embedded&&rows.empty()&&!Hud::chatOpened)return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.f);
    if(embedded)ImGui::BeginChild("##ChatPanel",{w,h},false,ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    else{ImGui::SetNextWindowPos({12,std::max(8.f,io.DisplaySize.y-h-std::max(48.f,combatChatInset/scale))},ImGuiCond_Always);ImGui::SetNextWindowSize({w,h});
        int flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
        if(!interactive)flags|=ImGuiWindowFlags_NoInputs;ImGui::Begin("##BALONCUAGAChat",nullptr,flags);
    }
    ImGui::PopStyleVar(2);ImVec2 p=ImGui::GetWindowPos();auto *d=ImGui::GetWindowDrawList();
    if(!interactive){
        if(!ChatBox::collapsed){int end=static_cast<int>(rows.size()),begin=std::max(0,end-capacity);float y=p.y+8;
            d->PushClipRect(p+ImVec2(12,4),p+ImVec2(w-14,h-4),true);
            for(int i=begin;i<end;++i){const auto &line=rows[i];float x=p.x+16;
                if(ChatBox::timeOn){if(line.first)referenceText(d,chatSmall,smallSize,{x,y+3},IM_COL32(146,151,147,255),line.message->date.size()>=16?line.message->date.substr(11,5):"",false);x+=timeWidth;}
                for(const auto &span:line.spans){int r,g,b;Hud::nativeChatRgb(span.code,line.message->side,r,g,b);referenceText(d,chatBold,textSize,{x,y},IM_COL32(r,g,b,255),span.text);x+=chatBold->CalcTextSizeA(textSize,FLT_MAX,0,span.text.c_str()).x;}y+=rowHeight;
            }d->PopClipRect();
        }ImGui::End();return;
    }
    d->AddRectFilled(p,p+ImVec2(w,h),IM_COL32(24,26,24,125),10);
    d->AddLine(p+ImVec2(1,44),p+ImVec2(w-1,44),IM_COL32(163,173,160,65));
    if(referenceButton("##time",{10,5},{60,36},"Time",ChatBox::timeOn,true))ChatBox::action(0);
    if(referenceButton("##server",{74,5},{68,36},"Server",ChatBox::serverOn,true))ChatBox::action(1);
    if(referenceButton("##dead",{146,5},{60,36},"Dead",ChatBox::deadOn,true))ChatBox::action(2);
    if(referenceButton("##hide",{w-150,5},{88,36},ChatBox::collapsed?"Show chat":"Hide chat",false))ChatBox::action(3);
    if(referenceButton("##clear",{w-62,5},{54,36},"Clear",false))ChatBox::action(4);
    Hud::chatScrollLimit=std::max(0,static_cast<int>(rows.size())-capacity);Hud::chatScroll=std::clamp(Hud::chatScroll,0,Hud::chatScrollLimit);
    if(interactive&&ImGui::IsWindowHovered()&&io.MouseWheel!=0)Hud::chatScroll=std::clamp(Hud::chatScroll+static_cast<int>(io.MouseWheel*2),0,Hud::chatScrollLimit);
    if(!ChatBox::collapsed){int end=static_cast<int>(rows.size())-Hud::chatScroll,begin=std::max(0,end-capacity);float y=p.y+57;
        d->PushClipRect(p+ImVec2(12,46),p+ImVec2(w-14,h-54),true);
        for(int i=begin;i<end;++i){const auto &line=rows[i];float x=p.x+16;if(ChatBox::timeOn){if(line.first)referenceText(d,chatSmall,smallSize,{x,y+3},IM_COL32(146,151,147,255),line.message->date.size()>=16?line.message->date.substr(11,5):"",false);x+=timeWidth;}
            for(const auto &span:line.spans){int r,g,b;Hud::nativeChatRgb(span.code,line.message->side,r,g,b);referenceText(d,chatBold,textSize,{x,y},IM_COL32(r,g,b,255),span.text);x+=chatBold->CalcTextSizeA(textSize,FLT_MAX,0,span.text.c_str()).x;}y+=rowHeight;
        }d->PopClipRect();
        // Older messages not wrapped yet count as one row each for the scroll bar.
        if(interactive&&Hud::chatScrollLimit>0){size_t total=rows.size()+unread;float track=h-106,thumb=std::max(14.f,track*capacity/std::max<size_t>(1,total));
            float offset=(track-thumb)*(1.f-static_cast<float>(Hud::chatScroll)/std::max<size_t>(1,total-std::min<size_t>(total,capacity)));d->AddRectFilled(p+ImVec2(w-7,53+offset),p+ImVec2(w-4,53+offset+thumb),IM_COL32(176,187,166,140),2);}
    }
    float fy=h-47,channelWidth=ChatEditor::team?96.f:66.f;ImVec2 from=p+ImVec2(12,fy),to=p+ImVec2(w-12,h-12);
    d->AddRectFilled(from,to,IM_COL32(30,32,30,65),6);d->AddRect(from,to,IM_COL32(150,158,146,170),6,0,1);
    if(referenceButton("##channel",{14,fy+2},{channelWidth,30},ChatEditor::team?u8"КОМАНДА ▾":u8"ВСЕ ▾",false))ChatBox::action(5);
    float ix=14+channelWidth+12,right=w-86;d->AddLine(p+ImVec2(ix-7,fy+7),p+ImVec2(ix-7,h-19),IM_COL32(151,156,145,140));
    ImGui::SetCursorPos({ix,fy+1});if(ImGui::InvisibleButton("##chatInput",{std::max(30.f,right-ix),32})&&!Hud::chatOpened)ChatEditor::begin(ChatEditor::team);
    auto value=ChatEditor::input;size_t hidden=0;while(!value.empty()&&chatBold->CalcTextSizeA(textSize,FLT_MAX,0,Hud::utf8(value).c_str()).x>right-ix-5){value.erase(0,1);++hidden;}
    bool placeholder=value.empty();std::string text=placeholder?(ChatEditor::team?u8"Сказать команде…":u8"Сказать всем…"):Hud::utf8(value);
    d->PushClipRect(p+ImVec2(ix,fy+1),p+ImVec2(right,h-14),true);referenceText(d,chatBold,textSize,p+ImVec2(ix,fy+7),placeholder?IM_COL32(139,146,138,255):IM_COL32(218,220,215,255),text,false);
    if(Hud::chatOpened&&(GetTickCount64()/500)%2==0){size_t caret=ChatEditor::caret>hidden?ChatEditor::caret-hidden:0;float cx=ix+chatBold->CalcTextSizeA(textSize,FLT_MAX,0,Hud::utf8(value.substr(0,caret)).c_str()).x;d->AddLine(p+ImVec2(cx,fy+6),p+ImVec2(cx,fy+28),IM_COL32(220,226,217,255));}d->PopClipRect();
    auto count=std::to_string(ChatEditor::input.size())+"/100";referenceText(d,chatSmall,smallSize,p+ImVec2(w-65,fy+11),IM_COL32(148,155,145,255),count,false);
    d->AddRect(p+ImVec2(1,1),p+ImVec2(w-1,h-1),IM_COL32(Hud::chatBorderRgb[0],Hud::chatBorderRgb[1],Hud::chatBorderRgb[2],255),10,0,1.3f);
    if(embedded)ImGui::EndChild();else ImGui::End();
}
static void chatOverlay(){if(Hud::chatOn&&Hud::oldSay&&Hud::chatPresentationReady)chatPanel(false);}
