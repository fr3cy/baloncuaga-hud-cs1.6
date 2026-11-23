#pragma once
// Included inside Weave, beside the existing first-person model page.
static int outfitPreviewSide=2,outfitPreviewCT=0,outfitPreviewTT=0;
static void outfitPreviewCallback(const ImDrawList *,const ImDrawCmd *){
    bool hands=Preview::showHands;Preview::showHands=true;
    modelCallback(nullptr,nullptr);Preview::showHands=hands;
}
// This ImGui fork applies BeginChild size after Begin. Set it before Begin too,
// so the two new cards get valid clipping/content bounds on their first frame.
static void outfitCard(const char *title,const char *id,ImVec2 size){
    ImGui::SetNextWindowSize(size);ImGui::BeginChild((std::string(id)+"decor").c_str(),size,false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground);
    auto *d=ImGui::GetWindowDrawList();ImVec2 p=ImGui::GetWindowPos(),z=ImGui::GetWindowSize();auto &style=ImGui::GetStyle();
    d->AddRectFilled(p,p+z,ImGui::GetColorU32(ImGuiCol_ChildBg),style.ChildRounding);
    d->AddRectFilled(p,{p.x+z.x,p.y+33},ImGui::GetColorU32(ImGuiCol_Header),style.ChildRounding,ImDrawFlags_RoundCornersTop);
    d->AddLine({p.x,p.y+33},{p.x+z.x,p.y+33},ImGui::GetColorU32(ImGuiCol_BorderShadow));
    ImFont *face=ui::carbonMoon&&ui::menuTitleFont?ui::menuTitleFont:font(1);
    d->AddText(face,face->FontSize,{p.x+13,p.y+16.5f-face->CalcTextSizeA(face->FontSize,FLT_MAX,0,title).y/2},ImGui::GetColorU32(ImGuiCol_Text),title);
    ImGui::SetCursorPos({0,34});ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{13,15});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,14});
    ImVec2 inner(std::max(4.f,ImGui::GetContentRegionAvail().x-1),std::max(4.f,ImGui::GetContentRegionAvail().y-1));
    ImGui::SetNextWindowSize(inner);ImGui::BeginChild(id,inner,false,ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoBackground);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
}
static void outfitsPage(){
    PlayerOutfits::loadSettings();float width=(ImGui::GetContentRegionAvail().x-10)/2;
    outfitCard(L(u8"Одежда команд","Team outfits"),"##outfit controls",{width,ImGui::GetContentRegionAvail().y});
    bool edit=ImGui::Checkbox(L(u8"Одежда всех КТ","All CT outfits"),&PlayerOutfits::enabled[2]);
    ImGui::TextWrapped("%s",L(u8"Vetements FAN, Mowalola Atlanta, Balenciaga Stomper. Капюшон надет.","Vetements FAN, Mowalola Atlanta, Balenciaga Stomper. Hood up."));
    ImGui::Separator();edit|=ImGui::Checkbox(L(u8"Одежда всех ТТ","All T outfits"),&PlayerOutfits::enabled[1]);
    ImGui::TextWrapped("%s",L(u8"Enfants Riches Déprimés, Rick Owens Bolan Banana, Rick Owens Geobasket. Капюшон надет.","Enfants Riches Déprimés, Rick Owens Bolan Banana, Rick Owens Geobasket. Hood up."));
    if(edit){PlayerOutfits::saveSettings();save();}
    if(ImGui::Button(L(u8"Стандартная одежда","Default outfits"),{ImGui::CalcItemWidth(),30})){PlayerOutfits::enabled[1]=PlayerOutfits::enabled[2]=false;PlayerOutfits::saveSettings();save();}
    ImGui::Spacing();ImGui::TextWrapped("%s",L(u8"Заменяет внешний вид всех моделей выбранной команды только на твоём экране.","Changes every model of the selected team on your screen."));
    if(!PlayerOutfits::installed)ImGui::TextWrapped("%s",L(u8"Обработчик игроков пока недоступен.","Player renderer is not available yet."));
    if(!PlayerOutfits::message.empty())ImGui::TextWrapped("%s",PlayerOutfits::message.c_str());
    endCard();ImGui::SameLine();outfitCard(L(u8"3D-просмотр одежды","3D outfit preview"),"##outfit preview",{width,ImGui::GetContentRegionAvail().y});
    if(ImGui::RadioButton(L(u8"КТ","CT"),outfitPreviewSide==2)){outfitPreviewSide=2;Preview::release();}ImGui::SameLine();
    if(ImGui::RadioButton(L(u8"ТТ","T"),outfitPreviewSide==1)){outfitPreviewSide=1;Preview::release();}
    const char *ctNames[]={"Urban","GIGN","GSG9","SAS","VIP"},*ttNames[]={"Terror","Leet","Arctic","Guerilla"};
    int &selected=outfitPreviewSide==2?outfitPreviewCT:outfitPreviewTT;
    if(ImGui::Combo("##native outfit variant",&selected,outfitPreviewSide==2?ctNames:ttNames,outfitPreviewSide==2?5:4))Preview::release();
    changed(ImGui::Checkbox(L(u8"Автовращение","Auto rotate"),&Preview::spin));
    ImGui::SameLine();if(ImGui::Button(L(u8"Сбросить ракурс","Reset view"))){Preview::yaw=Preview::pitch=0;}
    ImGui::TextDisabled("%s",L(u8"Зажми ЛКМ на модели, чтобы повернуть","Drag the model to rotate"));
    auto p=ImGui::GetCursorScreenPos(),a=ImGui::GetContentRegionAvail();a.y=std::max(35.f,a.y);
    preview={static_cast<int>(p.x*scale),static_cast<int>(p.y*scale),static_cast<int>(a.x*scale),static_cast<int>(a.y*scale),PlayerOutfits::variantPath((outfitPreviewSide==2?0:5)+selected),ImGui::GetStyle().Alpha};
    ImGui::GetWindowDrawList()->AddCallback(outfitPreviewCallback,nullptr);ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
    ImGui::InvisibleButton("##outfit orbit",a);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
        auto delta=ImGui::GetIO().MouseDelta;Preview::yaw=std::remainder(Preview::yaw+delta.x*scale*.55f,360.f);Preview::pitch=std::clamp(Preview::pitch-delta.y*scale*.4f,-80.f,80.f);
    }
    if(ImGui::IsItemHovered()||ImGui::IsItemActive())ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);endCard();
}
