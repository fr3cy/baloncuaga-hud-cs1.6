#pragma once
namespace Config {
static std::string message=u8"Настройки автоматически восстанавливаются при запуске игры.";
static const char *status(){return message.c_str();}
static bool saveCurrent(){save();bool written=WritePrivateProfileStringA("interface","config_version",BALONCUAGA_VERSION_NUMBER,iniPath)!=0;WritePrivateProfileStringA(nullptr,nullptr,nullptr,iniPath);if(!written){message=u8"Не удалось записать конфигурацию. Проверь доступ к папке игры.";return false;}message=u8"Настройки сохранены. Они загрузятся при следующем запуске.";return true;}
static bool validName(const char *name){if(!name||!*name||strlen(name)>32)return false;for(const unsigned char *p=reinterpret_cast<const unsigned char *>(name);*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'))return false;return true;}
static std::string profilePath(const char *name){std::string dir=rootPath;dir+="\\baloncuaga_configs";return dir+"\\"+name+".ini";}
static const char *sections[]={"settings","models","crosshair","interface"};
static bool readSections(const char *file,std::vector<std::vector<char>> &data){for(const char *section:sections){std::vector<char> buffer(32768);DWORD n=GetPrivateProfileSectionA(section,buffer.data(),buffer.size(),file);if(!n||n>=buffer.size()-2)return false;data.push_back(std::move(buffer));}return true;}
static bool saveProfile(const char *name){
    if(!validName(name)){message=u8"Имя конфига: 1–32 латинские буквы, цифры, _ или -.";return false;}
    if(!saveCurrent())return false;std::string dir=std::string(rootPath)+"\\baloncuaga_configs";
    if(!CreateDirectoryA(dir.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){message=u8"Не удалось создать папку конфигов.";return false;}
    std::vector<std::vector<char>> data;if(!readSections(iniPath,data)){message=u8"Не удалось прочитать настройки для сохранения.";return false;}
    std::string target=profilePath(name),temp=target+".tmp";DeleteFileA(temp.c_str());
    bool ok=true;for(size_t i=0;i<data.size();++i)if(!WritePrivateProfileSectionA(sections[i],data[i].data(),temp.c_str()))ok=false;
    if(!WritePrivateProfileStringA("baloncuaga","format","1",temp.c_str()))ok=false;WritePrivateProfileStringA(nullptr,nullptr,nullptr,temp.c_str());
    if(!ok||!MoveFileExA(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileA(temp.c_str());message=u8"Не удалось сохранить профиль.";return false;}
    message=std::string(u8"Конфиг сохранён: ")+name;return true;
}
static void apply(){
    Garey::load();JumpStats::load();MenuTheme::load();Lang::load();Showkeys::load();Watermark::load();Skybox::load();
    Weave::combatSettingsLoaded=false;Weave::loadCombatHud();
    PlayerOutfits::settingsLoaded=false;PlayerOutfits::loadSettings();PlayerOutfits::reset();
    ChatBox::loadTheme();
    fov=std::clamp(static_cast<int>(GetPrivateProfileIntA("settings","fov",90,iniPath)),60,120);
    crossStyle=std::clamp(static_cast<int>(GetPrivateProfileIntA("crosshair","style",12,iniPath)),0,12);crossColor=std::clamp(static_cast<int>(GetPrivateProfileIntA("crosshair","color",0,iniPath)),0,8);
    CrossConfig::load();Hud::load(false);SpectatorResource::load();ChatEditor::close();
    ChatBox::timeOn=GetPrivateProfileIntA("interface","chat_time",1,iniPath)!=0;ChatBox::serverOn=GetPrivateProfileIntA("interface","chat_server",1,iniPath)!=0;ChatBox::deadOn=GetPrivateProfileIntA("interface","chat_dead",1,iniPath)!=0;
    Weave::useMenu=true;Weave::glitchMenu=GetPrivateProfileIntA("interface","glitch_menu",0,iniPath)!=0;Weave::menuX=static_cast<int>(GetPrivateProfileIntA("interface","menu_x",-1,iniPath));Weave::menuY=static_cast<int>(GetPrivateProfileIntA("interface","menu_y",-1,iniPath));Weave::preferencesLoaded=true;
    Preview::spin=GetPrivateProfileIntA("interface","preview_spin",1,iniPath)!=0;Preview::showHands=GetPrivateProfileIntA("interface","preview_hands",1,iniPath)!=0;Trainer::enabled=GetPrivateProfileIntA("interface","trainer_enabled",0,iniPath)!=0;
    scanModels();Preview::release();Hud::freeOverview();GoCross::reset();crossPending=true;
}
static bool loadProfile(const char *name){
    if(!validName(name)){message=u8"Недопустимое имя конфига.";return false;}std::string path=profilePath(name);
    if(GetPrivateProfileIntA("baloncuaga","format",0,path.c_str())!=1){message=u8"Такой конфиг не найден или имеет неверный формат.";return false;}
    std::vector<std::vector<char>> data;if(!readSections(path.c_str(),data)){message=u8"Конфиг повреждён: настройки не изменены.";return false;}
    // Stage the replacement beside the active file. Preserve native crosshair
    // restoration data and unrelated sections rather than importing them.
    WritePrivateProfileStringA(nullptr,nullptr,nullptr,iniPath);std::string temp=std::string(iniPath)+".loading";if(!CopyFileA(iniPath,temp.c_str(),FALSE)){message=u8"Не удалось подготовить загрузку.";return false;}
    bool ok=true;for(size_t i=0;i<data.size();++i)if(!WritePrivateProfileSectionA(sections[i],data[i].data(),temp.c_str()))ok=false;
    WritePrivateProfileStringA(nullptr,nullptr,nullptr,temp.c_str());
    if(!ok||!MoveFileExA(temp.c_str(),iniPath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileA(temp.c_str());message=u8"Не удалось применить конфиг.";return false;}
    IniCache::invalidate();apply();message=std::string(u8"Конфиг загружен: ")+name;return true;
}
}
