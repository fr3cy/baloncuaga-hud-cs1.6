#pragma once
// Control only public game cvars. The external CS Fix binary owns its patches
// and lifetime; BALONCUAGA never maps or frees a running CS Fix module.
namespace Garey {
static int visual=100,voiceEnabled=-1,voiceVolume=-1;
static bool loaded,pending=true,pendingVoice,pendingVolume;
static ULONGLONG nextPoll;
static bool verified;
struct Original {const char *name;float value=0;cvar_t *pointer=nullptr;};
static Original originals[]={{"fps_visual"},{"voice_enable"},{"voice_scale"}};
static int clampVisual(int n){return std::clamp(n,20,1000);}
static cvar_t *variable(const char *name){
    if(!eng||!eng->pfnGetCvarPointer)return nullptr;
    auto value=eng->pfnGetCvarPointer(name);
    return value&&Hud::readable(value,sizeof(*value))&&std::isfinite(value->value)?value:nullptr;
}
static bool write(const char *name,float value){
    if(!eng||!std::isfinite(value))return false;
    if(eng->Cvar_SetValue){eng->Cvar_SetValue(const_cast<char *>(name),value);return true;}
    if(eng->pfnClientCmd){char command[80];snprintf(command,sizeof(command),"%s %.6g\n",name,double(value));eng->pfnClientCmd(command);return true;}
    return false;
}
static bool apply(Original &original,float value){
    auto current=variable(original.name);if(!current)return false;
    if(!original.pointer){original.pointer=current;original.value=current->value;}
    return write(original.name,value);
}
static void save(){
    char value[24];snprintf(value,sizeof(value),"%d",visual);WritePrivateProfileStringA("interface","garey_fps_visual",value,iniPath);
    if(voiceEnabled>=0){snprintf(value,sizeof(value),"%d",voiceEnabled);WritePrivateProfileStringA("interface","garey_voice_enable",value,iniPath);}
    if(voiceVolume>=0){snprintf(value,sizeof(value),"%d",voiceVolume);WritePrivateProfileStringA("interface","garey_voice_volume",value,iniPath);}
}
static void load(){
    visual=clampVisual(static_cast<int>(GetPrivateProfileIntA("interface","garey_fps_visual",100,iniPath)));
    voiceEnabled=static_cast<int>(GetPrivateProfileIntA("interface","garey_voice_enable",UINT(-1),iniPath));
    voiceVolume=static_cast<int>(GetPrivateProfileIntA("interface","garey_voice_volume",UINT(-1),iniPath));
    if(voiceEnabled>=0)voiceEnabled=std::clamp(voiceEnabled,0,1);
    if(voiceVolume>=0)voiceVolume=std::clamp(voiceVolume,0,100);
    pending=true;pendingVoice=voiceEnabled>=0;pendingVolume=voiceVolume>=0;nextPoll=0;loaded=true;
}
static void frame(){
    if(!loaded)load();ULONGLONG now=GetTickCount64();if(now<nextPoll)return;nextPoll=now+250;
    auto value=variable("fps_visual");if(!value)verified=false;
    if(value){
        if(pending){
            if(apply(originals[0],float(visual))){auto current=variable("fps_visual");verified=current&&std::fabs(current->value-visual)<.01f;pending=!verified;}
        }
        else if(value->value>=20&&value->value<=1000)visual=clampVisual(int(std::lround(value->value)));
    }
    if(pendingVoice&&apply(originals[1],float(voiceEnabled)))pendingVoice=false;
    if(pendingVolume&&apply(originals[2],voiceVolume/100.f))pendingVolume=false;
}
static void setVisual(int value){visual=clampVisual(value);pending=true;nextPoll=0;save();frame();}
static void setVoice(bool value){voiceEnabled=value?1:0;pendingVoice=true;nextPoll=0;save();frame();}
static void setVolume(int value){voiceVolume=std::clamp(value,0,100);pendingVolume=true;nextPoll=0;save();frame();}
static void restore(){
    for(auto &value:originals)if(value.pointer&&variable(value.name)==value.pointer)write(value.name,value.value);
    // No FreeLibrary: CS Fix remains independently loaded until process exit.
}
}
