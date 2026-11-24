#pragma once
static std::set<uint64_t> serverSelection;
static char serverSearch[96],serverManual[64];
static int serverFilter;
static void serversPage(){
    if(!Servers::requested)Servers::refresh();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{7,5});ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{7,4});ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{5,2});
    float pageHeight=ImGui::GetContentRegionAvail().y;ImVec2 pageStart=ImGui::GetCursorScreenPos();
    ImGui::BeginChild("##servers_page",{0,pageHeight},false);
    bool busy=Servers::busy.load();const char *filters[]={L(u8"Все","All"),L(u8"Избранное","Favorites"),L(u8"История","History")};
    for(int i=0;i<3;++i){if(i)ImGui::SameLine();bool current=serverFilter==i;if(current)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        if(ImGui::Button(filters[i]))serverFilter=i;if(current)ImGui::PopStyleColor();}
    ImGui::SameLine();ImGui::BeginDisabled(busy);if(ImGui::Button(L(u8"Обновить","Refresh")))Servers::refresh();ImGui::EndDisabled();ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(100.f,ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##servers_search",L(u8"Название или IP…","Name or IP…"),serverSearch,sizeof(serverSearch));
    auto list=Servers::snapshot();std::vector<Servers::Row> visible;auto query=Hud::wide(serverSearch);if(!query.empty())CharLowerBuffW(query.data(),query.size());
    uint32_t flags=serverFilter==1?Servers::Favorite:serverFilter==2?Servers::History:3;
    for(auto &r:list){if(!(r.flags&flags))continue;auto text=Hud::wide(r.name+" "+Servers::address(r.ip,r.port));if(!text.empty())CharLowerBuffW(text.data(),text.size());if(query.empty()||text.find(query)!=std::wstring::npos)visible.push_back(r);}
    auto message=Servers::message();if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
    ImGui::AlignTextToFramePadding();ImGui::TextDisabled(L(u8"Серверов: %d","Servers: %d"),int(visible.size()));ImGui::SameLine();
    ImGui::BeginDisabled(busy);if(ImGui::SmallButton(L(u8"Выделить все","Select all")))for(const auto &r:visible)serverSelection.insert(Servers::key(r.ip,r.port));ImGui::SameLine();
    if(ImGui::SmallButton(L(u8"Снять","Clear")))serverSelection.clear();
    if(busy||ServerNames::pending.load()){ImGui::SameLine();ImGui::TextDisabled("%s",busy?L(u8"Загрузка…","Loading…"):L(u8"Получаю названия…","Fetching names…"));}
    float footer=std::max(60.f,ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y*2);
    float used=ImGui::GetCursorScreenPos().y-pageStart.y;
    float height=std::max(80.f,pageHeight-used-footer-14);
    int favoriteAction=-1,connectAction=-1;bool favoriteAdd=false;
    if(ImGui::BeginTable("##server_list",4,ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Resizable,{0,height})){
        ImGui::TableSetupColumn("##check",ImGuiTableColumnFlags_WidthFixed,22);ImGui::TableSetupColumn("##favorite",ImGuiTableColumnFlags_WidthFixed,26);
        ImGui::TableSetupColumn(L(u8"Название сервера","Server name"),ImGuiTableColumnFlags_WidthStretch,1);
        ImGui::TableSetupColumn(L(u8"IP : порт","IP : port"),ImGuiTableColumnFlags_WidthFixed,140);ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(int(visible.size()));while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
            const auto &r=visible[i];auto k=Servers::key(r.ip,r.port);ImGui::PushID(int(r.ip));ImGui::PushID(int(r.port));ImGui::TableNextRow();ImGui::TableNextColumn();bool selected=serverSelection.count(k)!=0;
            if(ImGui::Checkbox("##select",&selected)){if(selected)serverSelection.insert(k);else serverSelection.erase(k);}ImGui::TableNextColumn();
            bool favorite=r.flags&Servers::Favorite;if(favorite)ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.83f,.35f,1));
            if(ImGui::SmallButton(favorite?ICON_FA_STAR:"+##favorite")){favoriteAction=i;favoriteAdd=!favorite;}if(favorite)ImGui::PopStyleColor();
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",favorite?L(u8"Убрать из избранного","Remove favorite"):L(u8"В избранное","Add favorite"));
            ImGui::TableNextColumn();auto at=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
            std::string name=r.name.empty()?L(u8"Без названия","Name unavailable"):r.name;
            if(ImGui::Selectable("##server_name",selected,ImGuiSelectableFlags_AllowDoubleClick,{0,ImGui::GetFrameHeight()})){
                serverSelection.clear();serverSelection.insert(k);if(ImGui::IsMouseDoubleClicked(0))connectAction=i;
            }
            bool hover=ImGui::IsItemHovered();auto end=at+ImVec2(width,ImGui::GetTextLineHeight());
            ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),at,end,end.x,end.x,name.c_str(),name.c_str()+name.size(),nullptr);
            if(hover){ImGui::BeginTooltip();ImGui::PushTextWrapPos(500);ImGui::TextUnformatted(name.c_str());ImGui::TextDisabled("%s",Servers::address(r.ip,r.port).c_str());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
            ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(Servers::address(r.ip,r.port).c_str());ImGui::PopID();ImGui::PopID();
        }ImGui::EndTable();
    }
    std::vector<Servers::Row> chosen;for(const auto &r:visible)if(serverSelection.count(Servers::key(r.ip,r.port)))chosen.push_back(r);
    ImGui::BeginDisabled(chosen.size()!=1);if(ImGui::Button(L(u8"Подключиться","Connect")))Servers::connect(chosen[0].ip,chosen[0].port);ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(chosen.empty());if(ImGui::Button(L(u8"Удалить выбранные","Delete selected"))){Servers::erase(chosen,flags);serverSelection.clear();}ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(!Servers::canUndo());if(ImGui::Button(L(u8"Отменить удаление","Undo deletion")))Servers::restore();ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("+ IP"))ImGui::OpenPopup("##add_server");
    if(ImGui::BeginPopup("##add_server")){
        ImGui::SetNextItemWidth(240);ImGui::InputTextWithHint("##server_address","IP:27015",serverManual,sizeof(serverManual));uint32_t ip=0;uint16_t port=0;bool valid=Servers::parse(serverManual,ip,port);
        ImGui::BeginDisabled(!valid);if(ImGui::Button(L(u8"В избранное","Add favorite"))){Servers::Row row;row.ip=ip;row.port=port;Servers::favorite(row,true);ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(ImGui::Button(L(u8"Подключиться","Connect"))){Servers::connect(ip,port);ImGui::CloseCurrentPopup();}ImGui::EndDisabled();ImGui::EndPopup();
    }
    ImGui::EndDisabled();ImGui::EndChild();ImGui::PopStyleVar(3);
    if(favoriteAction>=0)Servers::favorite(visible[favoriteAction],favoriteAdd);
    if(connectAction>=0)Servers::connect(visible[connectAction].ip,visible[connectAction].port);
}
