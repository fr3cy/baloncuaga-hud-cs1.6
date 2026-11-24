#pragma once
#include <cstddef>
namespace SmokeParticle {
// Valve CCoreTriangleEffect x86 layout, verified against this particleman.dll:
// CoreDraw reads renderFlags at +4, RGB at +0xb0 and model at +0xc0.
struct Particle {void *vtable;int flags;float nextPvs;byte inPvs;byte pad[3];int collision;float distance;float scalars[13];int fps,frames,frame,renderMode;float vectors[7][3];float rgb[3],mass;model_t *texture;float bounce;char classname[32];};
static_assert(offsetof(Particle,rgb)==0xb0&&offsetof(Particle,texture)==0xc0&&offsetof(Particle,classname)==0xc8,"Valve particle x86 layout");
using CoreDraw=void (__thiscall *)(void *,Particle *);using Draw=void (__thiscall *)(Particle *);
static CoreDraw originalCore;static Draw originalDraw;static bool installed;
static thread_local unsigned depth;
static bool drawing(){return depth!=0||(SmokeTint::inParticles&&SmokeTint::bound);}
struct Tint {
    Particle *particle=nullptr;int flags=0;float rgb[3]={};
    explicit Tint(Particle *value){
        if(!ready||unloading||!Hud::smokeTintOn||!Hud::readable(value,sizeof(*value)))return;
        if(Hud::readable(value->texture,sizeof(model_t))&&value->texture->type==mod_sprite)SmokeTint::diagnose(value->texture,"native particle seen");
        bool smoke=!strncmp(value->classname,"particle_smokegren",sizeof(value->classname));
        if(!smoke&&Hud::readable(value->texture,64))smoke=SmokeTint::model(value->texture);
        if(!smoke)return;particle=value;++depth;flags=value->flags;memcpy(rgb,value->rgb,sizeof(rgb));
        // CoreDraw uses LIGHT_COLOR/INTENSITY as divisors. LIGHT_NONE selects RGB
        // directly and prevents neutral lighting from washing out the chosen hue.
        value->flags=(flags&~0x70)|0x10;for(int c=0;c<3;++c)value->rgb[c]=float(Hud::smokeRgb[c]);
    }
    ~Tint(){if(particle){particle->flags=flags;memcpy(particle->rgb,rgb,sizeof(rgb));--depth;}}
};
static void __fastcall core(void *self,void *,Particle *particle){CallbackScope scope;Tint tint(particle);if(tint.particle){static bool logged;if(!logged){logged=true;char line[192];snprintf(line,sizeof(line),"SmokeParticle: actual CoreDraw class=%.31s RGB=%d,%d,%d",particle->classname,Hud::smokeRgb[0],Hud::smokeRgb[1],Hud::smokeRgb[2]);logLine(line);}}originalCore(self,particle);}
static void __fastcall draw(Particle *particle,void *){CallbackScope scope;Tint tint(particle);if(tint.particle){static bool logged;if(!logged){logged=true;logLine("SmokeParticle: CPartSmokeGrenade::Draw intercepted; native particle RGB restored after drawing.");}}originalDraw(particle);}
struct Region {byte *data;size_t size;};
static std::vector<Region> regions(HMODULE module){std::vector<Region> result;if(!module)return result;auto base=reinterpret_cast<byte *>(module);auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return result;auto nt=reinterpret_cast<IMAGE_NT_HEADERS *>(base+dos->e_lfanew);auto section=IMAGE_FIRST_SECTION(nt);for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){if(!(section[i].Characteristics&IMAGE_SCN_MEM_READ))continue;auto p=base+section[i].VirtualAddress;auto end=p+section[i].Misc.VirtualSize;while(p<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(p,&m,sizeof(m)))break;auto next=std::min(end,static_cast<byte *>(m.BaseAddress)+m.RegionSize);if(next<=p)break;if(m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))result.push_back({p,size_t(next-p)});p=next;}}return result;}
static void *findClassDraw(HMODULE client){
    auto areas=regions(client);const char name[]=".?AVCPartSmokeGrenade@@";byte *descriptor=nullptr;
    for(auto r:areas)for(size_t i=8;i+sizeof(name)<=r.size;++i)if(!memcmp(r.data+i,name,sizeof(name))){descriptor=r.data+i-8;break;}
    if(!descriptor)return nullptr;
    for(auto r:areas)for(size_t i=12;i+8<=r.size;i+=4){uintptr_t type;memcpy(&type,r.data+i,4);if(type!=reinterpret_cast<uintptr_t>(descriptor))continue;auto locator=r.data+i-12;if(*reinterpret_cast<unsigned *>(locator)!=0||*reinterpret_cast<unsigned *>(locator+4)!=0)continue;
        for(auto tableArea:areas)for(size_t j=0;j+20<=tableArea.size;j+=4){uintptr_t reference;memcpy(&reference,tableArea.data+j,4);if(reference!=reinterpret_cast<uintptr_t>(locator))continue;auto table=reinterpret_cast<void **>(tableArea.data+j+4);if(executableInModule(table[0],client)&&executableInModule(table[1],client)&&executableInModule(table[2],client))return table[2];}
    }return nullptr;
}
static bool install(){
    HMODULE module=GetModuleHandleA("particleman.dll"),client=GetModuleHandleA("client.dll");if(!module||!client)return false;
    using Factory=void *(__cdecl *)(const char *,int *);auto factory=reinterpret_cast<Factory>(GetProcAddress(module,"CreateInterface"));if(!factory)return false;
    void *object=factory("create_particleman",nullptr);if(!Hud::readable(object,sizeof(void*)))return false;auto table=*static_cast<void ***>(object);if(!Hud::readable(table,12*sizeof(void*)))return false;
    auto target=table[11];auto code=static_cast<byte *>(target);if(!executableInModule(target,module)||memcmp(code,"\x8b\x4c\x24\x04",4))return false;
    // The class search reads all of client.dll; after three failed hook attempts it is
    // not repeated every second for the rest of the game.
    static int failedAttempts;if(failedAttempts>=3)return false;
    auto classTarget=findClassDraw(module);if(!classTarget)classTarget=findClassDraw(client);
    if(MH_CreateHook(target,reinterpret_cast<void *>(core),reinterpret_cast<void **>(&originalCore))!=MH_OK){++failedAttempts;return false;}
    if(classTarget&&MH_CreateHook(classTarget,reinterpret_cast<void *>(draw),reinterpret_cast<void **>(&originalDraw))!=MH_OK)classTarget=nullptr;
    if(ready&&(MH_EnableHook(target)!=MH_OK||(classTarget&&MH_EnableHook(classTarget)!=MH_OK))){if(classTarget)MH_RemoveHook(classTarget);MH_RemoveHook(target);return false;}
    installed=true;SmokeGL::scopeActive=drawing;logLine(classTarget?"SmokeParticle: native CoreDraw and CPartSmokeGrenade::Draw hooks installed.":"SmokeParticle: native CoreDraw installed; specific class draw unavailable.");return true;
}
static void ensure(){static ULONGLONG checked;auto now=GetTickCount64();if(installed||unloading||now-checked<1000)return;checked=now;install();}
}

