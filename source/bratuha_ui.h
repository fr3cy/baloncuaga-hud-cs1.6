#pragma once
static char bratuhaInput[3001];
static void bratuhaPage(){
    Bratuha::load();Bratuha::tick();card(u8"БРАТУХА",{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    if(ImGui::Checkbox(L(u8"Разрешить мат","Allow swearing"),&Bratuha::swearing))WritePrivateProfileStringA("interface","bratuha_swearing",Bratuha::swearing?"1":"0",iniPath);ImGui::SameLine();
    if(ImGui::SmallButton(L(u8"Очистить переписку","Clear conversation")))Bratuha::clear();
    float height=std::max(80.f,ImGui::GetContentRegionAvail().y-75);
    ImGui::BeginChild("##bratuha_messages",{0,height},true);
    for(size_t i=0;i<Bratuha::history.size();++i){const auto &m=Bratuha::history[i];ImGui::PushID(int(i));
        ImGui::TextColored(m.user?ImVec4(.75f,.8f,.9f,1):ImVec4(.6f,.95f,.75f,1),"%s",m.user?L(u8"Ты","You"):u8"БРАТУХА");ImGui::TextWrapped("%s",m.text.c_str());ImGui::Separator();ImGui::PopID();}
    if(!Bratuha::pending.empty())ImGui::TextDisabled("%s",L(u8"БРАТУХА печатает…","BRATUKHA is typing…"));
    if(Bratuha::scroll){ImGui::SetScrollHereY(1);Bratuha::scroll=false;}ImGui::EndChild();
    bool waiting=!Bratuha::pending.empty();ImGui::BeginDisabled(waiting);ImGui::SetNextItemWidth(std::max(100.f,ImGui::GetContentRegionAvail().x-115));
    bool send=ImGui::InputTextWithHint("##bratuha_input",L(u8"Напиши братухе…","Message BRATUKHA…"),bratuhaInput,sizeof(bratuhaInput),ImGuiInputTextFlags_EnterReturnsTrue);ImGui::SameLine();
    send|=ImGui::Button(L(u8"Отправить","Send"),{105,0});if(send&&Bratuha::send(bratuhaInput))bratuhaInput[0]=0;ImGui::EndDisabled();
    endCard();
}
