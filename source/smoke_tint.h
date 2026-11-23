#pragma once
namespace SmokeTint {
static bool textureTintAvailable;
struct EntityColor {cl_entity_t *entity;model_t *model;byte original[3],tinted[3];};static std::vector<EntityColor> entityColors;
static void remember(cl_entity_t *entity){EntityColor saved={entity,entity->model,{entity->curstate.rendercolor.r,entity->curstate.rendercolor.g,entity->curstate.rendercolor.b},{static_cast<byte>(Hud::smokeRgb[0]),static_cast<byte>(Hud::smokeRgb[1]),static_cast<byte>(Hud::smokeRgb[2])}};entityColors.push_back(saved);}
static void restoreEntities(){for(const auto &saved:entityColors){auto *entity=saved.entity;if(!Hud::readable(entity,sizeof(*entity))||entity->model!=saved.model)continue;auto &color=entity->curstate.rendercolor;if(color.r==saved.tinted[0]&&color.g==saved.tinted[1]&&color.b==saved.tinted[2]){color.r=saved.original[0];color.g=saved.original[1];color.b=saved.original[2];}}entityColors.clear();}
static std::string lowerName(model_t *value){
    if(!value||!Hud::readable(value,64))return {};
    size_t length=0;while(length<sizeof(value->name)&&value->name[length])++length;
    if(length==sizeof(value->name))return {};
    std::string name(value->name,length);for(char &c:name){if(c=='\\')c='/';c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));}
    return name;
}
static bool spriteName(const std::string &name){return name.size()>=4&&!name.compare(name.size()-4,4,".spr");}
// Weapon muzzle smoke, fire, blood, sparks and the like are never smoke clouds.
static bool excluded(const std::string &name){
    for(auto word:{"pistol","rifle","gun","muzzle","fire","flame","poison","bonus_platform","blood","spark","laser","beam","xplode","explo","flash","shell","bullet","hit","dot","glow","light","flare","lamp","bulb"})
        if(name.find(word)!=std::string::npos)return true;
    return false;
}
// ---- Smoke from the servers' own plugins ---------------------------------------------------
// Servers with custom smoke draw it with sprites of their own (steam1.spr through TE_SMOKE,
// fog, "xsmoke" and so on), which the name test below does not know. Around a smoke grenade that
// lies still (or lay there until a moment ago) a sprite kept on screen for a while by three or
// more objects at once is that server's smoke: it is learned for the rest of the map.
struct Grenade {cl_entity_t *entity;float pos[3];ULONGLONG seen,still;};    // still: since when it has not moved
struct Puff {cl_entity_t *entity;ULONGLONG first,last;};
struct Candidate {model_t *model;std::vector<Puff> puffs;};
static std::vector<Grenade> grenades;static std::vector<Candidate> candidates;static std::vector<model_t *> learned;
static constexpr float zoneRadius=450;
// (Answers are cached per model: every studio model on screen passes here every frame.)
static std::vector<std::pair<model_t *,bool>> grenadeKnown;   // cleared with the map: model slots are reused
static bool grenadeModel(model_t *value){
    for(const auto &k:grenadeKnown)if(k.first==value)return k.second;
    if(!value||!Hud::readable(value,64)||value->type!=mod_studio)return false;std::string name=lowerName(value);
    bool smoke=name.find("smokegren")!=std::string::npos||name.find("smoke_gren")!=std::string::npos;if(grenadeKnown.size()<512)grenadeKnown.push_back({value,smoke});return smoke;
}
static void seeGrenade(cl_entity_t *entity,ULONGLONG now){
    for(auto &g:grenades)if(g.entity==entity){float dx=entity->origin[0]-g.pos[0],dy=entity->origin[1]-g.pos[1],dz=entity->origin[2]-g.pos[2];
        if(dx*dx+dy*dy+dz*dz>4){g.still=now;for(int i=0;i<3;++i)g.pos[i]=entity->origin[i];}g.seen=now;return;}
    if(grenades.size()<32)grenades.push_back({entity,{entity->origin[0],entity->origin[1],entity->origin[2]},now,now});
}
// Where smoke can be: a grenade at rest for a quarter second, or one gone less than 30 s ago
// (plugins often remove the grenade when its smoke starts).
static bool nearSmoke(const float *origin,ULONGLONG now){
    grenades.erase(std::remove_if(grenades.begin(),grenades.end(),[&](const Grenade &g){return now-g.seen>30000;}),grenades.end());
    for(const auto &g:grenades){const bool present=now-g.seen<500;if(present&&now-g.still<250)continue;
        float dx=origin[0]-g.pos[0],dy=origin[1]-g.pos[1],dz=origin[2]-g.pos[2];if(dx*dx+dy*dy+dz*dz<zoneRadius*zoneRadius)return true;}
    return false;
}
static bool isLearned(model_t *value){return std::find(learned.begin(),learned.end(),value)!=learned.end();}
static void consider(cl_entity_t *entity,ULONGLONG now){
    model_t *value=entity->model;if(isLearned(value)||learned.size()>=16)return;
    std::string name=lowerName(value);if(!spriteName(name)||excluded(name)||!nearSmoke(entity->origin,now))return;
    Candidate *c=nullptr;for(auto &k:candidates)if(k.model==value){c=&k;break;}
    if(!c){if(candidates.size()>=64)return;candidates.push_back({value,{}});c=&candidates.back();}
    Puff *p=nullptr;for(auto &k:c->puffs)if(k.entity==entity){p=&k;break;}
    if(!p){if(c->puffs.size()>=32)return;c->puffs.push_back({entity,now,now});p=&c->puffs.back();}
    if(now-p->last>250)p->first=now;p->last=now;                     // a pooled temp entity reused later starts again
    int lasting=0;for(const auto &k:c->puffs)if(now-k.last<=250&&k.last-k.first>=800)++lasting;   // impacts and sparks live shorter
    if(lasting>=3){learned.push_back(value);char line[192];snprintf(line,sizeof(line),"SmokeCustom: learned the server's smoke sprite %.63s (kept on screen around a smoke grenade).",value->name);logLine(line);}
}
static void newMap(){grenades.clear();candidates.clear();learned.clear();grenadeKnown.clear();}
static bool model(model_t *value){
    std::string name=lowerName(value);if(!spriteName(name))return false;
    if(isLearned(value))return true;
    if(name.find("pistol")!=std::string::npos||name.find("rifle")!=std::string::npos||name.find("gun")!=std::string::npos||name.find("muzzle")!=std::string::npos||
       name.find("fire")!=std::string::npos||name.find("flame")!=std::string::npos||name.find("poison")!=std::string::npos||name.find("bonus_platform")!=std::string::npos)return false;
    return name.find("smoke")!=std::string::npos||name.find("gas_puff")!=std::string::npos;
}
static void diagnose(model_t *value,const char *path){static std::vector<std::string> logged;if(!value||logged.size()>=128)return;std::string name(value->name);if(std::find(logged.begin(),logged.end(),name)!=logged.end())return;logged.push_back(name);char line[192];snprintf(line,sizeof(line),"SmokeCustom: %s sprite=%.63s",path,value->name);logLine(line);}

static void tint(float &r,float &g,float &b){float light=std::clamp(std::max(r,std::max(g,b)),0.f,1.f);r=Hud::smokeRgb[0]/255.f*light;g=Hud::smokeRgb[1]/255.f*light;b=Hud::smokeRgb[2]/255.f*light;}
static int (*originalSprite)(model_t *,int);
static void (*originalColor)(float,float,float,float),(*originalModeColor)(float,float,float,float,int),(*originalTriangles)();
static void (*originalByteColor)(unsigned char,unsigned char,unsigned char,unsigned char);
static thread_local bool inParticles,bound,forwarding;
// Win32 dynamic TLS may be aligned to 8 bytes. Volatile scalar accesses
// prevent aligned 16-byte SIMD stores into this per-thread colour bank.
static thread_local volatile float color[4]={1,1,1,1};static thread_local int kind,mode;
struct Forward {bool previous;Forward():previous(forwarding){forwarding=true;}~Forward(){forwarding=previous;}};
static void emit(){float r=color[0],g=color[1],b=color[2];if(inParticles&&bound&&Hud::smokeTintOn&&!textureTintAvailable)tint(r,g,b);Forward guard;
    if(kind==2&&originalModeColor)originalModeColor(r,g,b,color[3],mode);
    else if(kind==1&&originalByteColor)originalByteColor(static_cast<byte>(std::round(r*255)),static_cast<byte>(std::round(g*255)),static_cast<byte>(std::round(b*255)),static_cast<byte>(std::round(color[3]*255)));
    else if(originalColor)originalColor(r,g,b,color[3]);
}
static void colorFloat(float r,float g,float b,float a){CallbackScope scope;if(forwarding){originalColor(r,g,b,a);return;}color[0]=r;color[1]=g;color[2]=b;color[3]=a;kind=0;emit();}
static void colorByte(unsigned char r,unsigned char g,unsigned char b,unsigned char a){CallbackScope scope;if(forwarding){originalByteColor(r,g,b,a);return;}color[0]=r/255.f;color[1]=g/255.f;color[2]=b/255.f;color[3]=a/255.f;kind=1;emit();}
static void colorMode(float r,float g,float b,float a,int renderMode){CallbackScope scope;if(forwarding){originalModeColor(r,g,b,a,renderMode);return;}color[0]=r;color[1]=g;color[2]=b;color[3]=a;kind=2;mode=renderMode;emit();}
static int sprite(model_t *value,int frame){CallbackScope scope;int result=originalSprite(value,frame);bound=inParticles&&result!=0&&model(value);if(bound&&Hud::smokeTintOn)diagnose(value,"triangle particle");if(inParticles)emit();static bool logged=false;if(bound&&Hud::smokeTintOn&&!logged){logged=true;logLine("Smoke: matched gas-puff sprite in particle renderer.");}return result;}
static void triangles(){CallbackScope scope;inParticles=true;bound=false;kind=0;color[0]=color[1]=color[2]=color[3]=1;originalTriangles();bound=false;emit();inParticles=false;}
static bool install(HMODULE client){
    auto *api=eng->pTriAPI;if(!api)return false;
    struct Hook {void *target,*replacement;void **old;};
    Hook hooks[]={{reinterpret_cast<void *>(api->SpriteTexture),reinterpret_cast<void *>(sprite),reinterpret_cast<void **>(&originalSprite)},
        {reinterpret_cast<void *>(api->Color4f),reinterpret_cast<void *>(colorFloat),reinterpret_cast<void **>(&originalColor)},
        {reinterpret_cast<void *>(api->Color4ub),reinterpret_cast<void *>(colorByte),reinterpret_cast<void **>(&originalByteColor)},
        {reinterpret_cast<void *>(api->Color4fRendermode),reinterpret_cast<void *>(colorMode),reinterpret_cast<void **>(&originalModeColor)},
        {reinterpret_cast<void *>(GetProcAddress(client,"HUD_DrawTransparentTriangles")),reinterpret_cast<void *>(triangles),reinterpret_cast<void **>(&originalTriangles)}};
    int installed=0;for(auto &hook:hooks){if(!hook.target||MH_CreateHook(hook.target,hook.replacement,hook.old)!=MH_OK){for(int i=0;i<installed;++i){MH_RemoveHook(hooks[i].target);*hooks[i].old=nullptr;}return false;}++installed;}
    logLine("Smoke: gas-puff particle rendering capture installed.");return true;
}
}


