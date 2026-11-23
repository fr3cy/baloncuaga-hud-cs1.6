#pragma once
#include <algorithm>
#include <atomic>
#include <cstring>
#include <type_traits>

// Cosmetic native-HUD tint only. Sprite sheets also contain tactical markers,
// so neither SPR_Set nor an entire texture is recoloured. Exact stock regions
// are tinted for one native HUD draw and the previous sprite colour is restored.
// Include after the SDK, CallbackScope, engine pointer and tracked MinHook API.
namespace NativeHudStyle {
using LoadFn=HSPRITE (*)(const char *);
using SetFn=void (*)(HSPRITE,int,int,int);
using DrawFn=void (*)(int,int,int,const wrect_t *);
using GenericFn=void (*)(int,int,int,const wrect_t *,int,int,int,int);
static LoadFn originalLoad;
static SetFn originalSet;
static DrawFn originalDraw,originalHoles,originalAdditive;
static GenericFn originalGeneric;
static std::atomic<HSPRITE> handles[2];
static bool installed;
static unsigned primeAttempts;
struct Selection {HSPRITE sprite;int r,g,b;bool valid;};
// GoldSrc's loader can provide an x86 TLS base aligned to only eight bytes.
// GCC vectorised an aggregate TLS clear into MOVAPS despite that runtime base.
// Keep a trivial, constant-zero TLS object and make every field access volatile:
// this forces scalar accesses for reads, writes and resets, including in -O2.
static_assert(std::is_trivial<Selection>::value,"TLS selection must have no runtime constructor");
static_assert(alignof(Selection)<=4,"Native TLS needs only four-byte alignment");
static thread_local volatile Selection selected={};
static Selection snapshotSelection(const volatile Selection &value){
    Selection result;result.sprite=value.sprite;result.r=value.r;result.g=value.g;result.b=value.b;result.valid=value.valid;return result;
}
static void writeSelection(volatile Selection &value,HSPRITE sprite,int r,int g,int b,bool valid){
    value.valid=false;value.sprite=sprite;value.r=r;value.g=g;value.b=b;value.valid=valid;
}
static void writeSelection(volatile Selection &value,const Selection &source){writeSelection(value,source.sprite,source.r,source.g,source.b,source.valid);}
static thread_local unsigned frameDepth;
static thread_local bool forwarding;
static thread_local volatile int screenWidth,screenHeight;
static uint64_t publishedSerial,publishedEpoch,currentSerial,currentEpoch;
static uint32_t publishedMask,suppressionMask;
struct Anchor {int x,y,width,height;uint64_t serial,epoch;bool valid;};
static Anchor anchors[5];

struct Region {int atlas;wrect_t rect;bool clipped;};
// x/y/width/height from the stock cstrike/sprites/hud.txt and weapon*.txt.
// atlas 0:640hud7, atlas 1:320hud2. Excludes crosshair/radar, objectives,
// team/status/kill-feed markers, weapon selection art and all world textures.
static const Region regions[]={
    {0,{0,20,0,25},false},{0,{23,43,0,25},false},{0,{47,67,0,25},false},
    {0,{70,90,0,25},false},{0,{95,115,0,25},false},{0,{119,139,0,25},false},
    {0,{144,164,0,25},false},{0,{169,189,0,25},false},{0,{192,212,0,25},false},{0,{216,236,0,25},false},
    {0,{240,242,0,40},false}, // number divider
    {0,{48,72,25,49},false}, // health cross, not the aiming crosshair
    {0,{192,210,25,50},false},{0,{210,223,25,48},false},{0,{223,240,25,48},false}, // money
    {0,{0,24,25,49},true},{0,{24,48,25,49},true}, // armour fill may crop vertically
    {0,{0,24,124,148},true},{0,{24,48,124,148},true},
    {0,{144,168,72,96},false}, // round clock
    {0,{0,24,72,96},false},{0,{24,48,72,96},false},{0,{48,72,72,96},false}, // ammunition
    {0,{72,96,72,96},false},{0,{96,120,72,96},false},{0,{120,144,72,96},false},
    {0,{0,24,96,120},false},{0,{24,48,96,120},false},{0,{48,72,96,120},false},
    {0,{72,96,96,120},false},{0,{96,120,96,120},false},{0,{120,144,96,120},false},{0,{144,168,96,120},false},
    {1,{0,12,0,16},false},{1,{12,24,0,16},false},{1,{24,36,0,16},false},
    {1,{36,48,0,16},false},{1,{48,60,0,16},false},{1,{60,72,0,16},false},
    {1,{72,84,0,16},false},{1,{84,96,0,16},false},{1,{96,108,0,16},false},{1,{108,120,0,16},false},
    {1,{120,121,0,20},false},{1,{0,16,72,88},false},
    {1,{59,74,72,88},false},{1,{76,88,72,88},false},{1,{91,105,72,88},false},
    {1,{0,16,88,104},true},{1,{16,32,88,104},true},{1,{32,48,88,104},true},{1,{48,64,88,104},true},
    {1,{64,80,88,104},false},{1,{0,18,16,34},false}
};

static int atlas(const char *path){
    if(!path)return -1;
    const char *expected[]={"sprites/640hud7.spr","sprites/320hud2.spr"};
    for(int id=0;id<2;++id){unsigned n=0;for(;n<32;++n){unsigned char c=static_cast<unsigned char>(path[n]);if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;if(c!=static_cast<unsigned char>(expected[id][n]))break;if(!c)return id;}}
    return -1;
}
static bool same(const wrect_t &a,const wrect_t &b){return a.left==b.left&&a.right==b.right&&a.top==b.top&&a.bottom==b.bottom;}
static int regionIndex(HSPRITE sprite,const wrect_t *rect){
    if(!sprite||!rect||rect->right<=rect->left||rect->bottom<=rect->top)return -1;
    for(unsigned i=0;i<sizeof(regions)/sizeof(regions[0]);++i){const auto &region=regions[i];if(sprite!=handles[region.atlas].load(std::memory_order_relaxed))continue;
        if(same(*rect,region.rect))return int(i);
        // Native armour fill changes only the vertical crop, not the columns.
        if(region.clipped&&rect->left==region.rect.left&&rect->right==region.rect.right&&rect->top>=region.rect.top&&rect->bottom<=region.rect.bottom)return int(i);
    }
    return -1;
}
static bool target(HSPRITE sprite,const wrect_t *rect){return regionIndex(sprite,rect)>=0;}
static uint32_t markerRole(int region){
    if(region==11||region==44)return GameHudState::Health;
    if((region>=15&&region<=18)||(region>=48&&region<=51))return GameHudState::Armor;
    if((region>=12&&region<=14)||(region>=45&&region<=47))return GameHudState::Money;
    if(region==19||region==52)return GameHudState::Timer;
    if((region>=20&&region<=32)||region==53)return GameHudState::Ammo;
    return 0;
}
static int componentIndex(uint32_t role){for(int i=0;i<5;++i)if(role==(1u<<i))return i;return -1;}
static bool nativeZone(uint32_t role,int x,int y){
    const int w=screenWidth,h=screenHeight;if(w<=0||h<=0||x<0||y<0||x>=w||y>=h)return false;
    const float bottomBand=(std::max)(64.f,h*.065f);
    if(role==GameHudState::Health)return x<w*.20f&&y>h-bottomBand;
    if(role==GameHudState::Armor)return x>w*.10f&&x<w*.40f&&y>h-bottomBand;
    if(role==GameHudState::Ammo)return x>w*.55f&&y>h-bottomBand;
    if(role==GameHudState::Money)return x>w*.55f&&y>h-(std::max)(160.f,h*.17f)&&y<h-(std::max)(48.f,h*.045f);
    if(role==GameHudState::Timer)return x>w*.33f&&x<w*.67f&&y>h-bottomBand;
    return false;
}
static uint32_t componentAt(int index,int x,int y,const wrect_t *rect){
    uint32_t marker=markerRole(index);if(marker){if(!nativeZone(marker,x,y))return 0;
        auto &anchor=anchors[componentIndex(marker)];const auto &canonical=regions[index].rect;
        anchor.x=x;anchor.y=y-(rect->top-canonical.top);anchor.width=canonical.right-canonical.left;anchor.height=canonical.bottom-canonical.top;anchor.serial=currentSerial;anchor.epoch=currentEpoch;anchor.valid=true;return marker;}
    // Numerals share a sheet with other counters. Suppress them only beside a
    // verified health/armour/ammo/money/clock marker observed this or last frame.
    if(!((index>=0&&index<=10)||(index>=33&&index<=43)))return 0;
    const float cx=x+(rect->right-rect->left)*.5f,cy=y+(rect->bottom-rect->top)*.5f;float best=1e9f,second=1e9f;int selectedComponent=-1;
    for(int i=0;i<5;++i){const auto &anchor=anchors[i];if(!anchor.valid||anchor.epoch!=currentEpoch||anchor.serial>currentSerial||currentSerial-anchor.serial>1)continue;
        float dy=std::abs(cy-(anchor.y+anchor.height*.5f));int glyph=(std::max)(rect->right-rect->left,anchor.width);
        float dx=std::abs(cx-(anchor.x+anchor.width*.5f));if(dy>(std::max)(8.f,anchor.height*.8f)||dx>glyph*10.f)continue;
        float score=dx+dy*4.f;if(score<best){second=best;best=score;selectedComponent=i;}else if(score<second)second=score;}
    if(selectedComponent<0||second-best<4.f)return 0;uint32_t role=1u<<selectedComponent;return nativeZone(role,x,y)?role:0;
}
static void clearReplacement(){publishedSerial=publishedEpoch=0;publishedMask=suppressionMask=0;for(auto &anchor:anchors)anchor.valid=false;}
static void replacementRendered(uint32_t mask,uint64_t serial){
    auto state=GameHudState::snapshot();if(serial!=state.frameSerial){clearReplacement();return;}publishedSerial=serial;publishedEpoch=state.epoch;publishedMask=mask&state.eligibleMask&GameHudState::All;
}
static bool suppress(uint32_t role){
    if(!role||frameDepth!=1||!ready||unloading||unloadRequested||!installed||forwarding||!(suppressionMask&role))return false;
    auto state=GameHudState::snapshot();return state.epoch==currentEpoch&&state.frameSerial==currentSerial&&(state.eligibleMask&role)!=0;
}
static bool warningRed(const Selection &s){return s.r>0&&s.g>=0&&s.b>=0&&s.g<=s.r/4&&s.b<=s.r/4;}
static bool shouldTint(const wrect_t *rect,const Selection &value){return ready&&!unloading&&installed&&frameDepth&&!forwarding&&value.valid&&!warningRed(value)&&target(value.sprite,rect);}
static int intensity(const Selection &s){return (std::clamp)((std::max)({s.r,s.g,s.b}),0,255);}

#if defined(__GNUC__)
#define NATIVE_HUD_ALIGN __attribute__((force_align_arg_pointer))
#else
#define NATIVE_HUD_ALIGN
#endif
NATIVE_HUD_ALIGN static HSPRITE load(const char *path){CallbackScope scope;HSPRITE sprite=originalLoad(path);int id=atlas(path);if(sprite&&id>=0)handles[id].store(sprite,std::memory_order_relaxed);return sprite;}
NATIVE_HUD_ALIGN static void set(HSPRITE sprite,int r,int g,int b){CallbackScope scope;writeSelection(selected,sprite,r,g,b,true);originalSet(sprite,r,g,b);}
template<typename Draw> static void withTint(const wrect_t *rect,int x,int y,Draw draw){
    const Selection previous=snapshotSelection(selected);
    if(frameDepth==1&&!forwarding&&ready&&!unloading&&installed){int index=regionIndex(previous.sprite,rect);if(index>=0&&suppress(componentAt(index,x,y,rect)))return;}
    if(!shouldTint(rect,previous)){draw();return;}
    const int grey=intensity(previous);forwarding=true;
    originalSet(previous.sprite,grey,grey,grey);draw();originalSet(previous.sprite,previous.r,previous.g,previous.b);writeSelection(selected,previous);forwarding=false;
}
NATIVE_HUD_ALIGN static void draw(int frame,int x,int y,const wrect_t *rect){CallbackScope scope;withTint(rect,x,y,[&]{originalDraw(frame,x,y,rect);});}
NATIVE_HUD_ALIGN static void holes(int frame,int x,int y,const wrect_t *rect){CallbackScope scope;withTint(rect,x,y,[&]{originalHoles(frame,x,y,rect);});}
NATIVE_HUD_ALIGN static void additive(int frame,int x,int y,const wrect_t *rect){CallbackScope scope;withTint(rect,x,y,[&]{originalAdditive(frame,x,y,rect);});}
NATIVE_HUD_ALIGN static void generic(int frame,int x,int y,const wrect_t *rect,int src,int dest,int width,int height){CallbackScope scope;withTint(rect,x,y,[&]{originalGeneric(frame,x,y,rect,src,dest,width,height);});}
#undef NATIVE_HUD_ALIGN
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
static_assert(std::is_same<decltype(&load),decltype(eng->pfnSPR_Load)>::value,"SPR_Load ABI");
static_assert(std::is_same<decltype(&set),decltype(eng->pfnSPR_Set)>::value,"SPR_Set ABI");
static_assert(std::is_same<decltype(&draw),decltype(eng->pfnSPR_Draw)>::value,"SPR_Draw ABI");
static_assert(std::is_same<decltype(&generic),decltype(eng->pfnSPR_DrawGeneric)>::value,"SPR_DrawGeneric ABI");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void resetVideo(){for(auto &handle:handles)handle.store(0,std::memory_order_relaxed);primeAttempts=0;writeSelection(selected,0,0,0,0,false);clearReplacement();screenWidth=screenHeight=0;}
// Call on the native render thread, never the loader/init worker. SPR_Load uses
// the engine's existing cache, so this also discovers sheets loaded before ASI.
static void ensure(){
    if(!installed||!ready||unloading||!originalLoad||primeAttempts>=3)return;
    if(handles[0].load(std::memory_order_relaxed)&&handles[1].load(std::memory_order_relaxed))return;
    ++primeAttempts;const char *paths[]={"sprites/640hud7.spr","sprites/320hud2.spr"};
    for(int id=0;id<2;++id)if(!handles[id].load(std::memory_order_relaxed)){HSPRITE sprite=originalLoad(paths[id]);if(sprite)handles[id].store(sprite,std::memory_order_relaxed);}
}
struct Frame {
    Frame(){ensure();++frameDepth;if(frameDepth==1){auto state=GameHudState::snapshot();currentSerial=state.frameSerial;currentEpoch=state.epoch;
        suppressionMask=publishedSerial&&publishedSerial+1==currentSerial&&publishedEpoch==currentEpoch?publishedMask&state.eligibleMask:0;
        SCREENINFO screen={};screen.iSize=sizeof(screen);if(eng&&eng->pfnGetScreenInfo&&eng->pfnGetScreenInfo(&screen)&&screen.iWidth>0&&screen.iHeight>0){if(screenWidth!=screen.iWidth||screenHeight!=screen.iHeight){for(auto &anchor:anchors)anchor.valid=false;}screenWidth=screen.iWidth;screenHeight=screen.iHeight;}else{screenWidth=screenHeight=0;suppressionMask=0;}}}
    ~Frame(){if(frameDepth)--frameDepth;}
    Frame(const Frame &)=delete;Frame &operator=(const Frame &)=delete;
};
static bool install(HMODULE hw){
    if(installed)return true;if(!eng||!hw)return false;
    struct Hook {void *target,*detour;void **original;};
    Hook hooks[]={
        {reinterpret_cast<void *>(eng->pfnSPR_Load),reinterpret_cast<void *>(load),reinterpret_cast<void **>(&originalLoad)},
        {reinterpret_cast<void *>(eng->pfnSPR_Set),reinterpret_cast<void *>(set),reinterpret_cast<void **>(&originalSet)},
        {reinterpret_cast<void *>(eng->pfnSPR_Draw),reinterpret_cast<void *>(draw),reinterpret_cast<void **>(&originalDraw)},
        {reinterpret_cast<void *>(eng->pfnSPR_DrawHoles),reinterpret_cast<void *>(holes),reinterpret_cast<void **>(&originalHoles)},
        {reinterpret_cast<void *>(eng->pfnSPR_DrawAdditive),reinterpret_cast<void *>(additive),reinterpret_cast<void **>(&originalAdditive)},
        {reinterpret_cast<void *>(eng->pfnSPR_DrawGeneric),reinterpret_cast<void *>(generic),reinterpret_cast<void **>(&originalGeneric)}
    };
    // Never guess offsets, vtables or renderer internals. Unsupported or aliased
    // SDK functions leave the native HUD completely unchanged.
    for(unsigned i=0;i<6;++i){if(!hooks[i].target||!executableInModule(hooks[i].target,hw)){logLine("Native HUD: unsupported sprite API; native colours retained.");return false;}
        for(unsigned j=0;j<i;++j)if(hooks[i].target==hooks[j].target){logLine("Native HUD: aliased sprite API; native colours retained.");return false;}}
    unsigned count=0;for(auto &hook:hooks){if(MH_CreateHook(hook.target,hook.detour,hook.original)!=MH_OK){while(count)MH_RemoveHook(hooks[--count].target);logLine("Native HUD: sprite hook unavailable; native colours retained.");return false;}++count;}
    resetVideo();installed=true;logLine("Native HUD: scoped monochrome health, armour, ammunition, money and clock; red warnings preserved.");return true;
}
} // namespace NativeHudStyle
