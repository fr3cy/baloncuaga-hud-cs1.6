#pragma once
namespace Models {
static engine_studio_api_t *api;
static r_studio_interface_t *studioInterface;
static int (*drawOriginal)(int);
static bool available;
static int pending[4];
static std::string loadedName[4],cachedName[4];
static std::vector<int> maps[4];
static std::string message;
static model_t *load(const std::string &path){
    if(!api||!api->Mod_ForName||!api->Mod_Extradata)return nullptr;
    auto value=api->Mod_ForName(path.c_str(),0);
    if(!value){char msg[192];snprintf(msg,sizeof(msg),"Models: Mod_ForName failed for %s",path.c_str());logLine(msg);return nullptr;}
    auto header=static_cast<studiohdr_t *>(api->Mod_Extradata(value));
    if(!Hud::readable(header,sizeof(*header))||header->id!=0x54534449||header->version!=10){char msg[192];snprintf(msg,sizeof(msg),"Models: invalid loaded studio data: %s type=%d",path.c_str(),int(value->type));logLine(msg);return nullptr;}
    return value;
}
static Mdl::Bytes stock(int category){char p[96];snprintf(p,sizeof(p),"models/%s",stockNames[category]);return Preview::read(p);}
static bool apply(int category,int selected){
    if(!available){message=u8"Обработчик моделей недоступен на этой версии клиента.";return false;}
    if(selected<0||selected>counts[category])return false;
    if(!selected){choice[category]=pending[category]=0;loadedName[category].clear();cachedName[category].clear();maps[category].clear();save();message=u8"Восстановлена стандартная модель.";return true;}
    if(Preview::context==wglGetCurrentContext())Preview::release();
    std::string name=files[category][selected-1];std::string full=std::string(rootPath)+"/cstrike/models/customizer/"+categories[category]+"/"+name;
    Mdl::Asset asset;auto native=stock(category);if(!Mdl::validate(full,asset,&native)){message=name+": "+Mdl::error;return false;}
    // Preview parser also verifies the first pose and actual drawable geometry.
    Preview::Model previewCheck;if(!Preview::parse(asset.main,asset.texture,previewCheck)){message=u8"Предпросмотр не поддерживает эту модель; сохранена прежняя модель.";return false;}
    std::string staged;if(!Mdl::stage(asset,staged)){message=Mdl::error;return false;}
    auto model=load(staged);
    if(!model){message=u8"Студийный загрузчик не открыл модель; подробности в CS16Customizer.log. Прежний выбор сохранён.";return false;}
    loadedName[category]=name;cachedName[category]=staged;maps[category]=asset.sequenceMap;choice[category]=pending[category]=selected;save();message=u8"Применено: "+name;return true;
}
static void refresh(){save();scanModels();for(int c=0;c<4;++c)pending[c]=choice[c];Preview::release();message=u8"Список обновлён. Выберите модель и нажмите «Применить».";}
#if defined(__GNUC__) && defined(__i386__)
__attribute__((force_align_arg_pointer))
#endif
static int draw(int flags){
    CallbackScope scope;if(!ready||unloading||!api||!eng)return drawOriginal(flags);auto entity=api->GetCurrentEntity();auto view=eng->GetViewModel();
    if(!entity||entity!=view||!view->model)return PlayerOutfits::drawWorld(flags,drawOriginal);
    const char *n=strrchr(view->model->name,'/');n=n?n+1:view->model->name;int c=-1;for(int i=0;i<4;++i)if(!_stricmp(n,stockNames[i])){c=i;break;}
    if(c<0||!choice[c]||choice[c]>counts[c])return drawOriginal(flags);
    if(loadedName[c]!=files[c][choice[c]-1]){int wanted=choice[c];if(!apply(c,wanted)){choice[c]=0;save();return drawOriginal(flags);}}
    auto custom=load(cachedName[c]);
    if(!custom||view->curstate.sequence<0||size_t(view->curstate.sequence)>=maps[c].size())return drawOriginal(flags);
    // Events always run on the native MDL, never on untrusted/custom sound events.
    int result=0;if(flags&STUDIO_EVENTS)result=drawOriginal(flags&~STUDIO_RENDER);
    if(!(flags&STUDIO_RENDER))return result;
    cl_entity_t original=*view;view->model=custom;view->curstate.sequence=maps[c][original.curstate.sequence];
    view->curstate.body=0;view->curstate.skin=0;
    if(original.latched.prevsequence>=0&&size_t(original.latched.prevsequence)<maps[c].size())view->latched.prevsequence=maps[c][original.latched.prevsequence];else view->latched.prevsequence=view->curstate.sequence;
    result=drawOriginal(flags&~STUDIO_EVENTS);*view=original;
    api->SetRenderModel(original.model);api->StudioSetHeader(api->Mod_Extradata(original.model));return result;
}
static bool install(HMODULE client,HMODULE hw){
    auto code=reinterpret_cast<byte *>(GetProcAddress(client,"HUD_GetStudioModelInterface"));if(!code)return false;
    // Decode the client's exported Initialize-style API copy, then validate every
    // pointer against its owner. No build-specific virtual addresses are used.
    r_studio_interface_t *iface=nullptr;engine_studio_api_t *candidate=nullptr;
    for(size_t off=0;off<80;){hde32s op={};unsigned len=hde32_disasm(code+off,&op);if(!len||op.flags&F_ERROR)break;
        if(op.opcode==0xBF){auto ptr=reinterpret_cast<engine_studio_api_t *>(op.imm.imm32);if(Hud::readable(ptr,sizeof(*ptr)))candidate=ptr;}
        if(op.opcode==0xC7&&op.modrm==0x00){auto ptr=reinterpret_cast<r_studio_interface_t *>(op.imm.imm32);if(Hud::readable(ptr,sizeof(*ptr))&&ptr->version==1)iface=ptr;}
        if(op.opcode==0xC3&&off>30)break;off+=len;
    }
    static int reason;
    if(!candidate||!iface){if(reason!=1){reason=1;logLine("Models: client studio interface signature not found.");}return false;}
    if(!executableInModule(reinterpret_cast<void *>(iface->StudioDrawModel),client)||!executableInModule(reinterpret_cast<void *>(candidate->Mod_ForName),hw)||!executableInModule(reinterpret_cast<void *>(candidate->GetCurrentEntity),hw)||!executableInModule(reinterpret_cast<void *>(candidate->Mod_Extradata),hw)||!executableInModule(reinterpret_cast<void *>(candidate->StudioSetHeader),hw)||!executableInModule(reinterpret_cast<void *>(candidate->SetRenderModel),hw)){if(reason!=2){reason=2;logLine("Models: studio API not ready or callback owner mismatch.");}return false;}
    auto hookStatus=MH_CreateHook(reinterpret_cast<void *>(iface->StudioDrawModel),reinterpret_cast<void *>(draw),reinterpret_cast<void **>(&drawOriginal));
    if(hookStatus!=MH_OK){if(reason!=3){reason=3;char msg[128];snprintf(msg,sizeof(msg),"Models: studio draw hook rejected: MinHook status %d",int(hookStatus));logLine(msg);}return false;}
    if(ready&&MH_EnableHook(reinterpret_cast<void *>(iface->StudioDrawModel))!=MH_OK){MH_RemoveHook(reinterpret_cast<void *>(iface->StudioDrawModel));drawOriginal=nullptr;return false;}
    api=candidate;studioInterface=iface;available=true;for(int c=0;c<4;++c)pending[c]=choice[c];logLine("Models: render-only studio hook installed; native animation and sound events retained.");return true;
}
static void ensureInstalled(){static ULONGLONG checked;ULONGLONG now=GetTickCount64();if(available||unloading||now-checked<1000)return;checked=now;install(GetModuleHandleA("client.dll"),GetModuleHandleA("hw.dll"));}
static void reset(){for(int c=0;c<4;++c){loadedName[c].clear();cachedName[c].clear();maps[c].clear();}}
}
