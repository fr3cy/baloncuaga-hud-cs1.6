#pragma once
// Cosmetic player substitution is scoped to the native StudioDrawPlayer call.
// Native visibility, animation state, player info and server hitboxes remain owned by CS.
namespace PlayerOutfits {
// Validated asset hashes are generated from the final authored MDL files.
#include "outfit_asset_manifest.h"
static bool enabled[3]={false,true,true},settingsLoaded,installed;
static model_t *cached[assetCount]={};static bool checked[assetCount]={};
static std::string message;
static thread_local volatile int playerScope=-1;
static int (*originalDraw)(int,entity_state_t*);
static model_t *(*originalSetup)(int);
static void loadSettings(){if(settingsLoaded)return;settingsLoaded=true;enabled[1]=GetPrivateProfileIntA("interface","outfits_tt",1,iniPath)!=0;enabled[2]=GetPrivateProfileIntA("interface","outfits_ct",1,iniPath)!=0;}
static void saveSettings(){if(!settingsLoaded)return;WritePrivateProfileStringA("interface","outfits_tt",enabled[1]?"1":"0",iniPath);WritePrivateProfileStringA("interface","outfits_ct",enabled[2]?"1":"0",iniPath);}
static int fallbackVariant(int side){return side==2?0:5;}
static const char *variantPath(int variant){return assets[variant].file;}
static const char *path(int side){return variantPath(fallbackVariant(side));}
static int nativeVariant(const model_t *model,int side){
    const char *name=model?strrchr(model->name,'/'):nullptr;name=name?name+1:model?model->name:"";
    for(int i=0;i<assetCount;++i){char label[64];snprintf(label,sizeof(label),"%s.mdl",assets[i].native);if(assets[i].side==side&&!_stricmp(name,label))return i;}
    return fallbackVariant(side);
}
static int nativeSide(const model_t *native){
    if(!native)return 0;const char *p=strrchr(native->name,'/');p=p?p+1:native->name;
    const char *ct[]={"gign.mdl","gsg9.mdl","sas.mdl","urban.mdl","vip.mdl"};
    const char *tt[]={"arctic.mdl","guerilla.mdl","leet.mdl","terror.mdl"};
    for(const char *name:ct)if(!_stricmp(p,name))return 2;for(const char *name:tt)if(!_stricmp(p,name))return 1;return 0;
}
static bool compatible(model_t *model,const entity_state_t &state){
    if(!model||!Models::api)return false;auto h=static_cast<studiohdr_t*>(Models::api->Mod_Extradata(model));
    return Hud::readable(h,sizeof(*h))&&h->id==0x54534449&&h->version==10&&h->numseq==111&&(h->numbones==53||h->numbones==55)&&h->numseqgroups==1&&state.sequence>=0&&state.sequence<h->numseq&&state.gaitsequence>=0&&state.gaitsequence<h->numseq;
}
static model_t *getVariant(int variant){
    if(variant<0||variant>=assetCount)return nullptr;
    if(checked[variant])return cached[variant];checked[variant]=true;const auto &assetInfo=assets[variant];
    std::string full=std::string(rootPath)+"/cstrike/"+variantPath(variant);Mdl::Asset asset;
    if(!Mdl::validate(full,asset,nullptr,true)){message=std::string(assetInfo.native)+": "+Mdl::error;logLine("Player outfits: invalid or missing MDL; native player model retained.");return nullptr;}
    if(asset.hash!=assetInfo.hash){message=u8"Одежда: файл модели отличается от проверенной сборки. Сохранена штатная модель.";return nullptr;}
    const auto *header=Mdl::at<studiohdr_t>(asset.main,0);
    if(!header||header->numseq!=111||header->numbones!=assetInfo.bones||header->numseqgroups!=1){message=u8"Одежда: несовместимый скелет модели. Сохранена штатная модель.";return nullptr;}
    cached[variant]=Models::load(variantPath(variant));
    if(cached[variant]){
        auto *loaded=static_cast<studiohdr_t*>(Models::api->Mod_Extradata(cached[variant]));
        if(!Hud::readable(loaded,sizeof(*loaded))||loaded->numbones!=assetInfo.bones||loaded->numseq!=111||loaded->numseqgroups!=1)cached[variant]=nullptr;
    }
    if(cached[variant]){message=u8"Одежда команд загружена.";logLine("Player outfits: native-geometry clothing variant loaded.");}
    else message=u8"Одежда: движок не загрузил модель. Сохранена штатная модель.";
    return cached[variant];
}
static model_t *get(int side){return getVariant(fallbackVariant(side));}
#if defined(__GNUC__) && defined(__i386__)
#define BALONCUAGA_OUTFIT_ALIGN __attribute__((force_align_arg_pointer))
#else
#define BALONCUAGA_OUTFIT_ALIGN
#endif
static model_t *BALONCUAGA_OUTFIT_ALIGN setup(int index){
    CallbackScope callback;model_t *native=originalSetup(index);
    if(!installed||!ready||unloading||unloadRequested||playerScope!=index||index<0||index>=32||!eng||!Models::api)return native;
    auto *entity=Models::api->GetCurrentEntity();if(!entity||!entity->player||entity->index!=index+1)return native;
    int side=Hud::team[index+1];if(side!=1&&side!=2)side=nativeSide(native);if(side<1||side>2||!enabled[side])return native;
    model_t *custom=getVariant(nativeVariant(native,side));return compatible(custom,entity->curstate)?custom:native;
}
static int BALONCUAGA_OUTFIT_ALIGN draw(int flags,entity_state_t *state){
    CallbackScope callback;int previous=playerScope;
    auto *entity=Models::api?Models::api->GetCurrentEntity():nullptr;
    playerScope=(installed&&ready&&!unloading&&!unloadRequested&&(flags&STUDIO_RENDER)&&state&&entity&&entity->player&&entity->index>=1&&entity->index<=32&&state->number==entity->index)?entity->index-1:-1;
    struct Scope{int previous;~Scope(){playerScope=previous;}}scope{previous};return originalDraw(flags,state);
}
#undef BALONCUAGA_OUTFIT_ALIGN
static int drawWorld(int flags,int (*original)(int)){
    auto *entity=Models::api?Models::api->GetCurrentEntity():nullptr;
    if(!installed||!ready||unloading||unloadRequested||!(flags&STUDIO_RENDER)||!entity||entity->player||!entity->model)return original(flags);
    int side=0;const char *ids[]={"arctic","guerilla","leet","terror","gign","gsg9","sas","urban","vip"};
    for(int i=0;i<9;++i){char canonical[96];snprintf(canonical,sizeof(canonical),"models/player/%s/%s.mdl",ids[i],ids[i]);if(!_stricmp(entity->model->name,canonical)){side=i<4?1:2;break;}}
    if(!side||!enabled[side])return original(flags);model_t *custom=getVariant(nativeVariant(entity->model,side));if(!compatible(custom,entity->curstate))return original(flags);
    model_t *native=entity->model;entity->model=custom;
    struct Restore {cl_entity_t *entity;model_t *native;~Restore(){entity->model=native;Models::api->SetRenderModel(native);Models::api->StudioSetHeader(Models::api->Mod_Extradata(native));}}restore{entity,native};
    return original(flags);
}
static bool install(){
    if(installed)return true;if(!Models::available||!Models::api||!Models::studioInterface)return false;
    HMODULE client=GetModuleHandleA("client.dll"),hw=GetModuleHandleA("hw.dll");
    auto *iface=Models::studioInterface;void *drawTarget=reinterpret_cast<void*>(iface->StudioDrawPlayer),*setupTarget=reinterpret_cast<void*>(Models::api->SetupPlayerModel);
    if(!executableInModule(drawTarget,client)||!executableInModule(setupTarget,hw))return false;
    if(MH_CreateHook(drawTarget,reinterpret_cast<void*>(draw),reinterpret_cast<void**>(&originalDraw))!=MH_OK)return false;
    if(MH_CreateHook(setupTarget,reinterpret_cast<void*>(setup),reinterpret_cast<void**>(&originalSetup))!=MH_OK){MH_RemoveHook(drawTarget);return false;}
    if(ready&&(MH_EnableHook(setupTarget)!=MH_OK||MH_EnableHook(drawTarget)!=MH_OK)){MH_DisableHook(setupTarget);MH_DisableHook(drawTarget);MH_RemoveHook(setupTarget);MH_RemoveHook(drawTarget);return false;}
    loadSettings();installed=true;logLine("Player outfits: scoped native player rendering installed; team and visibility retained.");return true;
}
static void ensure(){static ULONGLONG previous;ULONGLONG now=GetTickCount64();if(installed||unloading||unloadRequested||now-previous<1000)return;previous=now;install();}
static void reset(){for(int i=0;i<assetCount;++i){cached[i]=nullptr;checked[i]=false;}message.clear();}
static void release(){installed=false;reset();}
}
