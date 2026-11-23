#pragma once
// Included inside Weave: selection UI and one small, lazily loaded panorama.
namespace SkyboxMenu {
struct Preset {const char *id,*ru,*en,*descriptionRu,*descriptionEn;};
static constexpr Preset presets[]={
    {"",u8"Небо карты","Map default",u8"Оригинальный скайбокс текущей карты.","The current map's original skybox."},
    {"sunny",u8"Солнечный день","Sunny day",u8"Голубое небо, солнечный свет и мягкие облака.","Blue sky, sunlight and soft clouds."},
    {"night",u8"3XL в небе","3XL night",u8"Светящийся baLONCUaga 3XL с серебристым ореолом среди звёзд.","A glowing baLONCUaga 3XL with a silver halo among the stars."},
    {"sunset",u8"Закат","Sunset",u8"Тёплый закат и розово-золотые облака.","A warm sunset with rose-gold clouds."},
    {"overcast",u8"Пасмурное небо","Overcast",u8"Слоистые облака и спокойные серо-синие оттенки.","Layered clouds in calm blue-grey tones."},
    {"aurora",u8"Северное сияние","Northern lights",u8"Ночное небо с зелёными и фиолетовыми лентами сияния.","A night sky with green and violet auroral ribbons."},
    {"space",u8"Космос","Deep space",u8"Звёзды и цветные облака далёкой туманности.","Stars and colourful clouds of a distant nebula."}
};
static int browsing=-1,observedSelection=-1,previewIndex=-1,failedPreview=-1;
static GLuint texture;static HGLRC textureContext;
static const char *name(int index){index=std::clamp(index,0,6);return L(presets[index].ru,presets[index].en);}
static void resetPreview(){
    if(texture&&textureContext==wglGetCurrentContext())glDeleteTextures(1,&texture);
    texture=0;textureContext=nullptr;previewIndex=failedPreview=-1;
}
static GLuint preview(int index){
    if(index<1||index>6||!wglGetCurrentContext())return 0;
    if(textureContext!=wglGetCurrentContext()){texture=0;previewIndex=failedPreview=-1;textureContext=wglGetCurrentContext();}
    if(previewIndex==index)return texture;if(failedPreview==index)return 0;
    if(texture){glDeleteTextures(1,&texture);texture=0;}previewIndex=-1;
    char path[MAX_PATH];int n=snprintf(path,sizeof(path),"baloncuaga_skyboxes/%s/preview.tga",presets[index].id);
    if(n<0||n>=int(sizeof(path))){failedPreview=index;return 0;}
    Skybox::Image image;if(!Skybox::readTga(path,image)){failedPreview=index;return 0;}
    if(image.width!=512||image.height!=256){failedPreview=index;return 0;}
    glPushAttrib(GL_TEXTURE_BIT);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
    GLint uploaded=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&uploaded);
    if(uploaded!=image.width){glDeleteTextures(1,&texture);texture=0;failedPreview=index;}else{previewIndex=index;failedPreview=-1;}
    glPopClientAttrib();glPopAttrib();return texture;
}
}
static void skyboxPage(){
    card(L(u8"Небо / Скайбокс","Sky / Skybox"),{ImGui::GetContentRegionAvail().x,ImGui::GetContentRegionAvail().y});
    int current=std::clamp(Skybox::selected(),0,6);
    if(SkyboxMenu::browsing<0||(current!=SkyboxMenu::observedSelection&&SkyboxMenu::browsing==SkyboxMenu::observedSelection))SkyboxMenu::browsing=current;
    SkyboxMenu::observedSelection=current;
    int &index=SkyboxMenu::browsing;
    ImGui::TextWrapped("%s: %s",L(u8"Сейчас выбрано","Current selection"),SkyboxMenu::name(Skybox::selected()));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if(ImGui::BeginCombo("##SkyboxPreset",SkyboxMenu::name(index))){
        for(int i=0;i<7;++i){bool selected=index==i;if(ImGui::Selectable(SkyboxMenu::name(i),selected))index=i;if(selected)ImGui::SetItemDefaultFocus();}ImGui::EndCombo();
    }
    const auto &item=SkyboxMenu::presets[index];ImGui::TextWrapped("%s",L(item.descriptionRu,item.descriptionEn));
    float w=ImGui::GetContentRegionAvail().x,h=std::clamp(ImGui::GetContentRegionAvail().y-154.f,80.f,190.f);
    ImVec2 area(std::min(w,h*2),h),origin=ImGui::GetCursorScreenPos();auto *d=ImGui::GetWindowDrawList();origin.x+=std::max(0.f,(w-area.x)*.5f);
    d->AddRectFilled(origin,origin+area,IM_COL32(11,14,21,255),8);
    GLuint texture=SkyboxMenu::preview(index);
    if(texture)d->AddImageRounded(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)),origin,origin+area,{0,0},{1,1},IM_COL32_WHITE,8);
    else{const char *label=index?L(u8"Предпросмотр недоступен","Preview unavailable"):L(u8"Оригинальное небо карты","Original map sky");ImVec2 size=ImGui::CalcTextSize(label);d->AddText(origin+(area-size)/2,IM_COL32(171,180,194,255),label);}
    d->AddRect(origin,origin+area,IM_COL32(185,201,221,75),8);ImGui::Dummy({w,area.y+8});
    float buttonW=std::max(80.f,(w-8)*.5f);bool supported=Skybox::available();
    ImGui::BeginDisabled(!supported&&index!=0);
    if(ImGui::Button(L(u8"Применить","Apply"),{buttonW,32}))Skybox::request(index);ImGui::EndDisabled();ImGui::SameLine(0,8);
    if(ImGui::Button(L(u8"Небо карты","Map default"),{buttonW,32})){index=0;Skybox::request(0);}
    ImGui::TextWrapped("%s",Skybox::status());
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s",L(u8"Выбор сохраняется. Свет карты и работа сервера не меняются.","Selection is saved. Map lighting and server behaviour are unchanged."));ImGui::PopStyleColor();
    endCard();
}
