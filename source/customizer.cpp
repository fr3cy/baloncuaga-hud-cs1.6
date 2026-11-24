#include "app_version.h"
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include "startup_worker.h"
#include "startup_log.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <tlhelp32.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <limits>
typedef float vec_t;
typedef float vec3_t[3];
typedef int (*pfnUserMsgHook)(const char *, int, void *);
typedef struct rect_s { int left, right, top, bottom; } wrect_t;
enum netsrc_s { NS_CLIENT, NS_SERVER, NS_MULTICAST };
#include "cdll_int.h"
#include "com_model.h"
#include "cl_entity.h"
#include "r_efx.h"
#include "studio.h"
#include "usercmd.h"
#include "r_studioint.h"
#include "triangleapi.h"
#include "demo_api.h"
#include "cvardef.h"
#include "MinHook.h"
// Every hook this plugin creates goes through a small forwarder in memory of its own,
// "jmp [slot]" with the slot holding our function. Another plugin (an overlay, CS_Patches)
// that hooks a function on top of ours keeps a jump to the forwarder in its trampoline, and
// may switch its hook on again later (on joining a server, for example). On F11 every slot
// is pointed at the game's original code and the forwarders and MinHook's trampolines stay
// in memory, so no jump of any plugin ever lands in the freed DLL.
static void *makeStub(void *to,void *volatile *&slot){
    static SRWLOCK allocationLock=SRWLOCK_INIT;
    struct AllocationGuard{SRWLOCK *lock;AllocationGuard(SRWLOCK *value):lock(value){AcquireSRWLockExclusive(lock);}~AllocationGuard(){ReleaseSRWLockExclusive(lock);}} guard(&allocationLock);
    static unsigned char *page;static size_t used;
    if(!page||used+16>4096){page=static_cast<unsigned char *>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));used=0;if(!page)return nullptr;}
    unsigned char *code=page+used;used+=16;
    slot=reinterpret_cast<void *volatile *>(code+8);*slot=to;
    uintptr_t at=reinterpret_cast<uintptr_t>(code+8);code[0]=0xFF;code[1]=0x25;memcpy(code+2,&at,4);code[6]=code[7]=0xCC;   // jmp dword ptr [slot]
    FlushInstructionCache(GetCurrentProcess(),code,16);return code;
}
struct OwnHook {void *target,*detour,*stub;void *volatile *slot;void *trampoline;};
static std::vector<OwnHook> ownHooks;
static MH_STATUS trackedCreateHook(LPVOID target,LPVOID detour,LPVOID *original){
    void *volatile *slot=nullptr;void *stub=makeStub(detour,slot);if(!stub)return MH_ERROR_MEMORY_ALLOC;
    MH_STATUS status=MH_CreateHook(target,stub,original);if(status==MH_OK)ownHooks.push_back({target,detour,stub,slot,original?*original:nullptr});
    return status;   // (a forwarder that was not used stays: 16 bytes)
}
static MH_STATUS trackedRemoveHook(LPVOID target){
    MH_STATUS status=MH_RemoveHook(target);
    if(status==MH_OK)for(auto it=ownHooks.begin();it!=ownHooks.end();++it)if(it->target==target){ownHooks.erase(it);break;}
    return status;
}
#define MH_CreateHook trackedCreateHook
#define MH_RemoveHook trackedRemoveHook
// MinHook on x86 writes "jmp detour" at the function, or a short jump back to a
// "jmp detour" placed in the padding just above it (hot-patch form).
static bool jumpsTo(const unsigned char *code,const void *detour){
    for(int hop=0;hop<2;++hop){
        MEMORY_BASIC_INFORMATION info;if(!VirtualQuery(code,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        if(code[0]==0xE9){int32_t rel;memcpy(&rel,code+1,4);return code+5+rel==detour;}
        if(code[0]==0xEB){code=code+2+static_cast<int8_t>(code[1]);continue;}
        return false;
    }
    return false;
}
static bool hooksStillOurs(){for(const auto &h:ownHooks)if(!jumpsTo(static_cast<const unsigned char *>(h.target),h.stub))return false;return true;}
// F11, first: every hook forwards to the game's original code, also when reached through
// another plugin's hook layered on ours, now or later.
static void forwardHooksToOriginals(){for(auto &h:ownHooks)if(h.trampoline)InterlockedExchangePointer(reinterpret_cast<void *volatile *>(h.slot),h.trampoline);}
// F11, once no callback is inside the plugin: the original bytes go back wherever the function
// still starts with our jump; under another plugin's hook ours stays, forwarding.
static void restoreUnsharedHooks(int &restored,int &shared){
    restored=shared=0;
    for(auto &h:ownHooks){if(!h.trampoline)continue;
        if(!jumpsTo(static_cast<const unsigned char *>(h.target),h.stub))++shared;   // another plugin's hook on top (or never switched on)
        else if(MH_DisableHook(h.target)==MH_OK)++restored;
        else ++shared;}
}

// Cosmetics are local render substitutions; server weapon/player logic stays native.
static cl_enginefunc_t *eng;
static HMODULE selfModule;
static char iniPath[MAX_PATH], rootPath[MAX_PATH];
static void logLine(const char *s);
// Every INI access below goes through the in-memory settings cache.
#include "ini_cache.h"
static const char *categories[] = {"knife", "he", "flash", "smoke"};
static const char *titles[] = {"Knife", "HE grenade", "Flashbang", "Smoke grenade"};
static const char *stockNames[] = {"v_knife.mdl", "v_hegrenade.mdl", "v_flashbang.mdl", "v_smokegrenade.mdl"};
static constexpr int limit = 64;
static char files[4][limit][64];
static int counts[4], choice[4], row, fov = 90;
static int crossStyle, crossColor;
static bool crossPending, crossCaptured;
static char originalCrossSize[16]="auto",originalCrossColor[64]="50 250 50";
static int originalDynamic=1,originalCrossEnabled=1,originalTranslucent=1;
static const char *crossStyles[]={"Game default","Small / Static","Medium / Static","Large / Static","Small / Dynamic","Medium / Dynamic","Large / Dynamic","CSGO / Classic","CSGO / Compact","CSGO / Dot","CSGO / T shape","CSGO / Dynamic","CS2 / Configurable"};
#include "crosshair_config.h"
static const char *crossColors[]={"Game default","White","Green","Cyan","Red","Yellow","Purple","Orange","Pink"};
static const int palette[][3]={{50,250,50},{255,255,255},{50,250,50},{70,220,255},{255,65,65},{255,225,60},{170,100,255},{255,150,45},{255,100,195}};
static bool opened;
static std::atomic<bool> ready{false},unloadRequested{false},unloading{false};
static std::atomic<long> activeCallbacks{0};
static bool f11WasDown;
static HANDLE initThread;
static HANDLE instanceMutex;
static bool nativeResourcesRestartPending;
struct CallbackScope {CallbackScope(){activeCallbacks.fetch_add(1,std::memory_order_acq_rel);}~CallbackScope(){activeCallbacks.fetch_sub(1,std::memory_order_acq_rel);}};
static bool insertWasDown;
static bool insertSeenInClient;
static char status[128] = "Models affect your first-person view only.";
static model_t *baseModel, *replacement;
static cl_entity_t *lastViewModel;
static int (*origRedraw)(float, int);
static int (*origUpdate)(client_data_t *, float);
static void (*origRefdef)(ref_params_t *);
static int (*origKey)(int, int, const char *);
static int (*origVidInit)();
using PostRunCmdFn=void (*)(local_state_t *,local_state_t *,usercmd_t *,int,double,unsigned int);
static PostRunCmdFn origPostRunCmd;
static void postRunCmd(local_state_t *,local_state_t *,usercmd_t *,int,double,unsigned int);
using AddVisibleEntityFn=int (*)(cl_entity_t *);
using PlayTempSoundFn=void (*)(TEMPENTITY *,float);
using TempEntUpdateFn=void (*)(double,double,double,TEMPENTITY **,TEMPENTITY **,AddVisibleEntityFn,PlayTempSoundFn);
static TempEntUpdateFn origTempEntUpdate;
using AddEntityFn=int (*)(int,cl_entity_t *,const char *);
static AddEntityFn origAddEntity;
static int addEntity(int,cl_entity_t *,const char *);
static int smokeTintVisible(cl_entity_t *entity);
static void tempEntUpdate(double,double,double,TEMPENTITY **,TEMPENTITY **,AddVisibleEntityFn,PlayTempSoundFn);
static bool executableInModule(void *, HMODULE);
namespace Hud { static void paint(int,int,const char *,bool);static void save(); }
namespace Weave {static void savePreferences();}
namespace Garey {static void save();static void frame();static void load();static void restore();}
namespace JumpStats {static void save();static void load();static void reset();}
namespace Showkeys {static void wheelEvent(int);}
namespace Models {static void ensureInstalled();}
namespace PlayerOutfits {static void loadSettings();static void saveSettings();static bool install();static void ensure();static void reset();static void release();static int drawWorld(int,int(*)(int));}
namespace SmokeGL {static void probe();static void observe(model_t *);}
namespace SmokeParticle {static void ensure();}
namespace FlashTint {static void apply();static void message(const void *data,int size);}
namespace GameHudState {static void installMessages();}
namespace Skybox {static void load();static void save();static void frame();static void resetVideo();static void release();}
namespace GlassScoreboard {static bool install(HMODULE);static void frame();static void resetVideo();static void release();static bool available();static const char* status();}
static void skyboxFrameForDC(HDC);
static void beginUnload();
static BOOL (WINAPI *originalSwap)(HDC), (WINAPI *originalWglSwap)(HDC);
static thread_local bool swapping;
static ULONGLONG lastRedrawTick;
static void menuOutsideGame();
static bool gameLevelActive(){if(!eng||!eng->pfnGetLevelName)return false;const char *level=eng->pfnGetLevelName();return level&&*level;}
static BOOL swapFrame(HDC dc,BOOL (WINAPI *original)(HDC)){
    CallbackScope scope;
    if(swapping)return original(dc);
    swapping=true;
    // Cover the real presentation as well: GDI SwapBuffers may enter the WGL
    // export that we also hook. The nested call must never submit a second UI.
    struct PresentScope {~PresentScope(){swapping=false;}} presentScope;
    if(ready&&!unloading){IniCache::tick();Garey::frame();GameHudState::installMessages();if(gameLevelActive()){Models::ensureInstalled();PlayerOutfits::ensure();SmokeParticle::ensure();SmokeGL::probe();}DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);bool down=pid==GetCurrentProcessId()&&(GetAsyncKeyState(VK_F11)&0x8000);if(down&&!f11WasDown)unloadRequested=true;f11WasDown=down;
        if(unloadRequested&&(!initThread||WaitForSingleObject(initThread,0)==WAIT_OBJECT_0))beginUnload();
        else if(!unloadRequested&&dc==wglGetCurrentDC()){if(gameLevelActive())skyboxFrameForDC(dc);menuOutsideGame();}}
    return original(dc);
}
#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
static BOOL WINAPI swapBuffers(HDC dc){return swapFrame(dc,originalSwap);}
#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
static BOOL WINAPI wglSwapBuffersHook(HDC dc){return swapFrame(dc,originalWglSwap);}


static void logLine(const char *s) { StartupLog::write(s); }
static void startupFailure(const char *details,const wchar_t *message) {
    logLine(details);
    std::wstring text=message;
    text+=L"\n\nФайловая диагностика отключена для защиты личных данных.";
    MessageBoxW(nullptr,text.c_str(),L"BALONCUAGA — ошибка запуска",MB_OK|MB_ICONERROR);
}
static void save() {
    char value[20]; snprintf(value, sizeof(value), "%d", fov);
    WritePrivateProfileStringA("settings", "fov", value, iniPath);
    snprintf(value,sizeof(value),"%d",crossStyle);WritePrivateProfileStringA("crosshair","style",value,iniPath);
    snprintf(value,sizeof(value),"%d",crossColor);WritePrivateProfileStringA("crosshair","color",value,iniPath);
    CrossConfig::save();Skybox::save();PlayerOutfits::saveSettings();Garey::save();JumpStats::save();
    for (int i = 0; i < 4; ++i)
        WritePrivateProfileStringA("models", categories[i], choice[i] ? files[i][choice[i]-1] : "", iniPath);
    Hud::save();
    Weave::savePreferences();
}
struct CrossValues {char size[16],color[64];int dynamic,enabled,translucent;};
// Native cvar values that show the menu's crosshair choice.
static CrossValues wantedCrosshair(){
    CrossValues v={};const char *sizes[]={"small","medium","large"};
    snprintf(v.size,sizeof(v.size),"%s",crossStyle?sizes[(crossStyle-1)%3]:originalCrossSize);
    if(crossStyle>=7)strcpy(v.color,"0 0 0");
    else if(crossColor)snprintf(v.color,sizeof(v.color),"%d %d %d",palette[crossColor][0],palette[crossColor][1],palette[crossColor][2]);
    else snprintf(v.color,sizeof(v.color),"%s",originalCrossColor);
    v.dynamic=crossStyle?crossStyle>=4:originalDynamic;
    v.enabled=crossStyle>=7?0:crossStyle>0?1:originalCrossEnabled;
    v.translucent=crossStyle>=7?1:originalTranslucent;
    return v;
}
static ULONGLONG crossAppliedTick,crossCheckTick;
static void applyCrosshair() {
    if(!crossPending || !eng->pfnClientCmd || !eng->pfnGetCvarString || !eng->pfnGetCvarFloat)return;
    crossPending=false;
    if(!crossCaptured) {
        if(!crossStyle && !crossColor)return;
        const char *size=eng->pfnGetCvarString(const_cast<char *>("cl_crosshair_size"));
        if(size && (!_stricmp(size,"auto") || !_stricmp(size,"small") || !_stricmp(size,"medium") || !_stricmp(size,"large")))
            snprintf(originalCrossSize,sizeof(originalCrossSize),"%s",size);
        const char *color=eng->pfnGetCvarString(const_cast<char *>("cl_crosshair_color"));
        if(color && strlen(color)<sizeof(originalCrossColor) && strspn(color,"0123456789 ")==strlen(color))
            strcpy(originalCrossColor,color);
        originalCrossEnabled=eng->pfnGetCvarFloat(const_cast<char *>("crosshair"))!=0;
        originalTranslucent=eng->pfnGetCvarFloat(const_cast<char *>("cl_crosshair_translucent"))!=0;
        originalDynamic=eng->pfnGetCvarFloat(const_cast<char *>("cl_dynamiccrosshair"))!=0;
        crossCaptured=true;
        WritePrivateProfileStringA("original_crosshair","size",originalCrossSize,iniPath);
        WritePrivateProfileStringA("original_crosshair","color",originalCrossColor,iniPath);
        WritePrivateProfileStringA("original_crosshair","dynamic",originalDynamic?"1":"0",iniPath);
        WritePrivateProfileStringA("original_crosshair","enabled",originalCrossEnabled?"1":"0",iniPath);
        WritePrivateProfileStringA("original_crosshair","translucent",originalTranslucent?"1":"0",iniPath);
        WritePrivateProfileStringA("original_crosshair","captured","1",iniPath);
    }
    auto want=wantedCrosshair();char command[192];
    snprintf(command,sizeof(command),"cl_crosshair_size \"%s\";cl_dynamiccrosshair %d;cl_crosshair_color \"%s\"\n",want.size,want.dynamic,want.color);
    eng->pfnClientCmd(command);
    snprintf(command,sizeof(command),"crosshair %d;cl_crosshair_translucent %d\n",want.enabled,want.translucent);eng->pfnClientCmd(command);
    crossAppliedTick=GetTickCount64();
}
// The game's Options dialog writes its own crosshair cvars back on OK/Apply, which put
// the green native crosshair over the menu's crosshair. While the menu controls the
// crosshair, a change made from outside becomes the new game default (restored on
// unload) and the menu's values are applied again.
static bool sameColor(const char *a,const char *b){int x[3],y[3];return a&&b&&sscanf(a,"%d %d %d",&x[0],&x[1],&x[2])==3&&sscanf(b,"%d %d %d",&y[0],&y[1],&y[2])==3&&x[0]==y[0]&&x[1]==y[1]&&x[2]==y[2];}
static bool validCrossSize(const char *s){return s&&(!_stricmp(s,"auto")||!_stricmp(s,"small")||!_stricmp(s,"medium")||!_stricmp(s,"large"));}
static void guardCrosshair(){
    if(!crossCaptured||crossPending||(!crossStyle&&!crossColor)||!eng||!eng->pfnGetCvarPointer)return;
    ULONGLONG now=GetTickCount64();if(now-crossAppliedTick<1500||now-crossCheckTick<250)return;crossCheckTick=now;
    static const char *names[5]={"crosshair","cl_crosshair_size","cl_crosshair_color","cl_dynamiccrosshair","cl_crosshair_translucent"};
    static cvar_t *cvars[5];for(int i=0;i<5;++i)if(!cvars[i])cvars[i]=eng->pfnGetCvarPointer(names[i]);
    auto want=wantedCrosshair();bool changed=false;
    if(cvars[0]&&(cvars[0]->value!=0)!=(want.enabled!=0)){originalCrossEnabled=cvars[0]->value!=0;changed=true;}
    if(cvars[1]&&cvars[1]->string&&_stricmp(cvars[1]->string,want.size)){if(validCrossSize(cvars[1]->string))snprintf(originalCrossSize,sizeof(originalCrossSize),"%s",cvars[1]->string);changed=true;}
    if(cvars[2]&&cvars[2]->string&&!sameColor(cvars[2]->string,want.color)){
        const char *c=cvars[2]->string;if(strlen(c)<sizeof(originalCrossColor)&&strspn(c,"0123456789 ")==strlen(c)&&sameColor(c,c))strcpy(originalCrossColor,c);changed=true;}
    if(cvars[3]&&(cvars[3]->value!=0)!=(want.dynamic!=0)){originalDynamic=cvars[3]->value!=0;changed=true;}
    if(cvars[4]&&(cvars[4]->value!=0)!=(want.translucent!=0)){originalTranslucent=cvars[4]->value!=0;changed=true;}
    if(!changed)return;
    // Something that keeps rewriting a cvar must not get a command every 1.5 s forever.
    static ULONGLONG windowStart;static int reapplied;if(now-windowStart>60000){windowStart=now;reapplied=0;}
    if(++reapplied>6){if(reapplied==7)logLine("Crosshair: native cvars keep changing; menu crosshair re-apply paused for a minute.");return;}
    WritePrivateProfileStringA("original_crosshair","size",originalCrossSize,iniPath);
    WritePrivateProfileStringA("original_crosshair","color",originalCrossColor,iniPath);
    WritePrivateProfileStringA("original_crosshair","dynamic",originalDynamic?"1":"0",iniPath);
    WritePrivateProfileStringA("original_crosshair","enabled",originalCrossEnabled?"1":"0",iniPath);
    WritePrivateProfileStringA("original_crosshair","translucent",originalTranslucent?"1":"0",iniPath);
    static bool logged;if(!logged){logged=true;logLine("Crosshair: game settings re-enabled the native crosshair; menu crosshair applied again.");}
    crossPending=true;applyCrosshair();
}
#include "model_validation.h"
#include "embedded_installer.h"
static bool ensureModelFolders(){
    bool success=true;
    const char *folders[]={"cstrike","cstrike/models","cstrike/models/customizer",
        "cstrike/models/customizer/knife","cstrike/models/customizer/flash",
        "cstrike/models/customizer/he","cstrike/models/customizer/smoke","cstrike/models/customizer/cache"};
    for(const char *folder:folders){
        char path[MAX_PATH];int n=snprintf(path,sizeof(path),"%s/%s",rootPath,folder);
        if(!*rootPath||n<0||n>=static_cast<int>(sizeof(path))){logLine("Models: folder path is too long or game root is missing.");success=false;continue;}
        if(CreateDirectoryA(path,nullptr))continue;
        DWORD error=GetLastError(),attributes=GetFileAttributesA(path);
        if(error==ERROR_ALREADY_EXISTS&&attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY))continue;
        char message[MAX_PATH+96];snprintf(message,sizeof(message),"Models: cannot create folder '%s' (Windows error %lu).",path,static_cast<unsigned long>(error));logLine(message);success=false;
    }
    return success;
}
static void scanModels() {
    ensureModelFolders();
    for (int i = 0; i < 4; ++i) {
        char folder[MAX_PATH], mask[MAX_PATH], chosen[64];
        snprintf(folder, sizeof(folder), "%s\\cstrike\\models\\customizer\\%s", rootPath, categories[i]);
        snprintf(mask, sizeof(mask), "%s\\*.mdl", folder);
        GetPrivateProfileStringA("models", categories[i], "", chosen, sizeof(chosen), iniPath);
        counts[i] = choice[i] = 0;
        WIN32_FIND_DATAA item; HANDLE handle = FindFirstFileA(mask, &item);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                // Engine receives a short hashed cache name; source basenames may use 63 bytes.
                char relative[128], full[MAX_PATH];
                snprintf(relative, sizeof(relative), "models/customizer/%s/%s", categories[i], item.cFileName);
                snprintf(full, sizeof(full), "%s\\%s", folder, item.cFileName);
                size_t len = strlen(item.cFileName);
                if (len < 4 || len >= 64 ||
                    _stricmp(item.cFileName + len - 4, ".mdl")) continue;
                FILE *probe=fopen(full,"rb");studiohdr_t h={};
                if(probe){bool headerRead=fread(&h,1,sizeof(h),probe)==sizeof(h);fclose(probe);if(headerRead&&h.id==0x54534449&&h.version==10&&h.numseq==0)continue;}
                if (counts[i] < limit) strcpy(files[i][counts[i]++], item.cFileName);
            } while (FindNextFileA(handle, &item));
            FindClose(handle);
        }
        // Keep invalid assets visible so Apply can explain the exact validation error.
        for (int a = 0; a < counts[i]; ++a) for (int b = a+1; b < counts[i]; ++b)
            if (_stricmp(files[i][a], files[i][b]) > 0) {
                char temp[64]; strcpy(temp, files[i][a]); strcpy(files[i][a], files[i][b]); strcpy(files[i][b], temp);
            }
        for (int n = 0; n < counts[i]; ++n) if (!_stricmp(chosen, files[i][n])) choice[i] = n+1;
    }
}
static void text(int x, int y, const char *s, bool selected = false) {
    Hud::paint(x,y,s,selected);
}
#include "model_preview.h"
#include "speedometer.h"
#include "hud_resolution.h"
#include "movement_hud.h"
#include "jump_stats.h"
#include "server_strafes.h"
#include "movement_stats.h"
#include "interface_clock.h"
#include "hud_features.h"
#include "garey_bridge.h"
#include "garey_runtime.h"
#include "spectator_resource.h"
#include "smoke_tint.h"
#include "smoke_gl.h"
#include "smoke_particle.h"
#include "scroll_trainer.h"
#include "chat_editor.h"
#include "menu_mouse.h"
namespace Weave {static void event(UINT,WPARAM,bool);static bool acceptsInput();}
#include "chat_scroll.h"
static thread_local AddVisibleEntityFn tempAddVisible;
static int smokeTintVisible(cl_entity_t *entity){
    if(!tempAddVisible)return 0;
    if(!ready||unloading||!Hud::smokeTintOn||!entity||!entity->model||entity->model->type!=mod_sprite)return tempAddVisible(entity);
    SmokeTint::diagnose(entity->model,"temp sprite seen");
    if(!SmokeTint::model(entity->model)){SmokeTint::consider(entity,GetTickCount64());if(!SmokeTint::model(entity->model))return tempAddVisible(entity);}
    SmokeGL::observe(entity->model);SmokeTint::diagnose(entity->model,"temp entity");SmokeTint::remember(entity);
    entity->curstate.rendercolor.r=static_cast<byte>(Hud::smokeRgb[0]);
    entity->curstate.rendercolor.g=static_cast<byte>(Hud::smokeRgb[1]);
    entity->curstate.rendercolor.b=static_cast<byte>(Hud::smokeRgb[2]);
    static bool logged=false;if(!logged){logged=true;logLine("Smoke: matched gas-puff temp entity; RGB retained through world rendering.");}return tempAddVisible(entity);
}
static int addEntity(int type,cl_entity_t *entity,const char *name){
    CallbackScope scope;int accepted=origAddEntity(type,entity,name);
    if(!accepted||!ready||unloading||!Hud::smokeTintOn||!entity||!Hud::readable(entity->model,sizeof(model_t)))return accepted;
    // Smoke grenades mark where a server's own smoke sprites can be learned.
    if(entity->model->type==mod_studio){if(SmokeTint::grenadeModel(entity->model))SmokeTint::seeGrenade(entity,GetTickCount64());return accepted;}
    if(entity->model->type!=mod_sprite)return accepted;
    SmokeTint::diagnose(entity->model,"server sprite seen");
    if(!SmokeTint::model(entity->model))SmokeTint::consider(entity,GetTickCount64());
    if(SmokeTint::model(entity->model)){SmokeGL::observe(entity->model);SmokeTint::remember(entity);entity->curstate.rendercolor.r=byte(Hud::smokeRgb[0]);entity->curstate.rendercolor.g=byte(Hud::smokeRgb[1]);entity->curstate.rendercolor.b=byte(Hud::smokeRgb[2]);}
    return accepted;
}
static void tempEntUpdate(double frameTime,double clientTime,double gravity,TEMPENTITY **freeList,TEMPENTITY **activeList,AddVisibleEntityFn addVisible,PlayTempSoundFn playSound){
    CallbackScope callbackScope;if(!origTempEntUpdate)return;SmokeTint::restoreEntities();
    AddVisibleEntityFn previous=tempAddVisible;tempAddVisible=addVisible;
    origTempEntUpdate(frameTime,clientTime,gravity,freeList,activeList,addVisible?smokeTintVisible:nullptr,playSound);
    tempAddVisible=previous;
}
#include "csgo_crosshair.h"
using PlayerMoveFn=void (*)(playermove_t *,int);
static PlayerMoveFn origPlayerMove;
static void playerMove(playermove_t *pm,int server){CallbackScope scope;
    bool fresh=ready&&pm&&!server&&!pm->server&&pm->runfuncs;
    auto before=fresh?MovementHud::sample(pm):MovementHud::Frame{};
    origPlayerMove(pm,server);
    if(fresh){auto after=MovementHud::sample(pm);auto sequence=MovementStats::attemptSequence;MovementStats::observe(before,after,true);if(sequence!=MovementStats::attemptSequence)MovementHud::rememberCaAttempt(MovementStats::lastAttempt.ideal);MovementHud::observe(before,after,true,GetTickCount64());Hud::observePredicted(pm->origin,(pm->flags&(1<<14))!=0,pm->onground!=-1);}   // FL_DUCKING
}
static void postRunCmd(local_state_t *from,local_state_t *to,usercmd_t *cmd,int runfuncs,double time,unsigned int seed){CallbackScope scope;origPostRunCmd(from,to,cmd,runfuncs,time,seed);if(ready){ULONGLONG tick=GetTickCount64();GoCross::observe(from,to,cmd,runfuncs,time,tick);Speedometer::observe(to,tick);}}
#include "chat_box.h"
namespace Config {static bool saveProfile(const char *);static bool loadProfile(const char *);static bool saveCurrent();static const char *status();}
#include "model_runtime.h"
#include "player_outfits.h"
#include "sound_audit.h"
#include "flash_tint.h"
#include "game_hud_state.h"
#include "native_hud_style.h"
#include "skybox_runtime.h"
// Auxiliary overlay windows may own their own GL contexts. Only this game's
// foreground host is allowed to commit/recreate sky resources from SwapBuffers.
static void skyboxFrameForDC(HDC dc){
    static std::atomic<HWND> knownHost{nullptr};HWND host=knownHost.load(std::memory_order_acquire);
    HWND surface=WindowFromDC(dc);if(!surface)return;
    if(!host||!IsWindow(host)){
        HWND foreground=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);
        if(pid!=GetCurrentProcessId()||(surface!=foreground&&!IsChild(foreground,surface)))return;
        host=GetAncestor(surface,GA_ROOT);if(!host)host=surface;
        if(GetWindowThreadProcessId(host,nullptr)!=GetCurrentThreadId())return;
        knownHost.store(host,std::memory_order_release);
    }
    if(GetWindowThreadProcessId(host,nullptr)==GetCurrentThreadId()&&(surface==host||IsChild(host,surface))){Skybox::frame();}
}
#include "weave_menu.h"

#include "glass_scoreboard.h"
#include "config_profiles.h"
static void crossSample(int x,int y,float time) {
    if(crossStyle>=7){GoCross::sample(x,y,Preview::spin?100.f:0.f,true);return;}
    int size=crossStyle?(crossStyle-1)%3:1,length=5+size*3,gap=4;
    if(crossStyle>=4)gap+=static_cast<int>((std::sin(time*3)+1)*3);
    int r=palette[crossColor][0],g=palette[crossColor][1],b=palette[crossColor][2];
    eng->pfnFillRGBA(x-gap-length,y-1,length,2,r,g,b,255);
    eng->pfnFillRGBA(x+gap,y-1,length,2,r,g,b,255);
    eng->pfnFillRGBA(x-1,y-gap-length,2,length,r,g,b,255);
    eng->pfnFillRGBA(x-1,y+gap,2,length,r,g,b,255);
}
static void toggleMenu() { opened = !opened;if(opened)ChatEditor::close();Mouse::sync(); }
static int menuScreenW,menuScreenH;
static void panel(int x, int y, int w, int h, int radius, int r, int g, int b, int a = 255) {
    const auto &theme=Hud::legacyTheme();
    if(r==126&&g==191&&b==235){r=theme.rgb[0];g=theme.rgb[1];b=theme.rgb[2];}
    if(Hud::inMenu) {
        if(Hud::look==4 && r==theme.rgb[0]&&g==theme.rgb[1]&&b==theme.rgb[2]){r=42;g=48;b=54;}
        if(Hud::look==0)radius=2;else if(Hud::look==1||Hud::look==4||Hud::look==5)radius=0;
        else if(Hud::look==2)radius=std::max(radius,14);else radius=std::min(radius,Hud::look==6?4:6);
        if(r<65&&g<80&&b<90){a=255;if(Hud::look==5){r=7;g=14;b=11;}else if(Hud::look==4){r=18;g=21;b=24;}}
        if(Hud::look==3 && (r==39&&g==49&&b==61)) {r=theme.rgb[0]/4+16;g=theme.rgb[1]/4+16;b=theme.rgb[2]/4+16;}
    }
    if(Hud::look==6 && Hud::inMenu){
        if(r==39&&g==49&&b==61){r=87;g=55;b=15;}
        else if(r<65 && g<80 && b<90){r=std::max(9,r/2);g=std::max(12,g/2);b=std::max(15,b/2);}
    }
    if(Hud::inMenu)a=255;
    auto draw=[&](bool direct) {
        auto fill=[&](int px,int py,int pw,int ph,int red,int green,int blue,int alpha) {
            if(pw<=0 || ph<=0)return;
            if(!direct){eng->pfnFillRGBA(px,py,pw,ph,red,green,blue,255);return;}
            glColor4ub(red,green,blue,255);
            glBegin(GL_QUADS);glVertex2i(px,py);glVertex2i(px+pw,py);glVertex2i(px+pw,py+ph);glVertex2i(px,py+ph);glEnd();
        };
    radius = std::max(0, std::min(radius, std::min(w, h)/2));
    if (w <= 0 || h <= 0) return;
    fill(x, y+radius, w, h-2*radius, r, g, b, a);
    for (int line = 0; line < radius; ++line) {
        float dy = radius-line-0.5f;
        int inset = radius-static_cast<int>(std::sqrt(radius*radius-dy*dy));
        fill(x+inset, y+line, w-2*inset, 1, r, g, b, a);
        fill(x+inset, y+h-line-1, w-2*inset, 1, r, g, b, a);
    }
    };
    if(wglGetCurrentContext() && menuScreenW>0 && menuScreenH>0){
        Hud::GL2D state(menuScreenW,menuScreenH);glViewport(0,0,menuScreenW,menuScreenH);
        glDisable(GL_BLEND);draw(true);
    }else draw(false);
    if(Hud::look==6 && Hud::inMenu && w>100 && h>50 && r<100 && g<100 && b<100){
        const int border=34;eng->pfnFillRGBA(x+3,y,w-6,1,border,border+3,border+6,255);
        eng->pfnFillRGBA(x+3,y+h-1,w-6,1,border,border+3,border+6,255);
        eng->pfnFillRGBA(x,y+3,1,h-6,border,border+3,border+6,255);
        eng->pfnFillRGBA(x+w-1,y+3,1,h-6,border,border+3,border+6,255);
    }
}
static void extraUiAction(int value){if(value==8)ChatEditor::begin(ChatEditor::team);else if(value<10)ChatBox::action(value);else if(value==10)Trainer::toggle();else if(value==11)Trainer::reset();else if(value==12){Weave::useMenu=true;WritePrivateProfileStringA("interface","weave_menu","1",iniPath);}}
static void trainerPanel(int x,int y,int w,int h){
    text(x+20,y+18,"SCROLL TRAINER",true);text(x+20,y+44,"Target: 20 ms +/- 2 ms | pattern gap: 100 ms");
    panel(x+16,y+76,112,28,5,39,49,61);text(x+26,y+83,Trainer::enabled?"Pause":"Start",true);Mouse::target(x+16,y+76,112,28,Mouse::Custom,10);
    panel(x+140,y+76,112,28,5,29,33,39);text(x+153,y+83,"Reset");Mouse::target(x+140,y+76,112,28,Mouse::Custom,11);
    char line[160];snprintf(line,sizeof(line),"Session: %llu patterns | %llu wheel events",static_cast<unsigned long long>(Trainer::totalPatterns),static_cast<unsigned long long>(Trainer::totalScrolls));text(x+20,y+121,line);
    snprintf(line,sizeof(line),"Up: %llu | Down: %llu | Accuracy: %.1f%%",static_cast<unsigned long long>(Trainer::upPatterns),static_cast<unsigned long long>(Trainer::downPatterns),Trainer::intervals?100.*Trainer::validIntervals/Trainer::intervals:0);text(x+20,y+145,line);
    auto pattern=Trainer::describe(Trainer::current);if(pattern.events.empty()&&!Trainer::history.empty())pattern=Trainer::history.front();
    text(x+20,y+174,"CURRENT PATTERN / LAST PATTERN",true);text(x+20,y+198,Trainer::sequence(pattern).c_str());
    snprintf(line,sizeof(line),"Average: %.2f ms | Accuracy: %.1f%%",pattern.average,pattern.accuracy);text(x+20,y+222,line);
    int tx=x+20,ty=y+248;for(size_t i=pattern.delays.size()>12?pattern.delays.size()-12:0;i<pattern.delays.size();++i){char value[24];snprintf(value,sizeof(value),"%.1f",pattern.delays[i]);Hud::colorText(tx,ty,value,std::abs(pattern.delays[i]-20)<=2?90:244,std::abs(pattern.delays[i]-20)<=2?220:100,105);tx+=43;if(tx>x+w-50){tx=x+20;ty+=18;}}
    if(h>340){text(x+20,y+h-78,"HISTORY",true);int count=0;for(const auto &past:Trainer::history){if(count++>=2)break;snprintf(line,sizeof(line),"%s | %.2f ms | %.1f%%",Trainer::sequence(past).c_str(),past.average,past.accuracy);text(x+20,y+h-56+(count-1)*18,Hud::fitPixels(line,w-40).c_str());}}
    text(x+20,y+h-18,Trainer::enabled?(Trainer::hookReady?"Recording wheel timings. No automated input.":"Mouse capture unavailable; see log."):"Press Start, then scroll in this tab or in-game.");
}
// The engine stops calling HUD_Redraw in the CS 1.6 main menu (no server).
// There the ImGui menu is drawn from SwapBuffers, so Insert works before joining.
static void menuOutsideGame(){
    if(!ready||unloading||!eng||!wglGetCurrentContext())return;
    const bool level=gameLevelActive();
    static bool previousLevel=false;
    if(!level)Mouse::menuScreen=true;
    else if(!previousLevel)Mouse::menuScreen=false;
    else if(!opened){CURSORINFO cursor={};cursor.cbSize=sizeof(cursor);Mouse::menuScreen=GetCursorInfo(&cursor)&&(cursor.flags&CURSOR_SHOWING);}
    previousLevel=level;
    ChatScroll::install();
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    bool down=pid==GetCurrentProcessId()&&(GetAsyncKeyState(MenuTheme::menuKey)&0x8000);
    if(down&&!insertWasDown&&!insertSeenInClient)toggleMenu();
    insertWasDown=down;insertSeenInClient=false;Mouse::sync();
    if(!opened&&!Weave::isAnimating())return;
    SCREENINFO screen={};screen.iSize=sizeof(screen);
    if(!eng->pfnGetScreenInfo||!eng->pfnGetScreenInfo(&screen)||screen.iWidth<=0||screen.iHeight<=0){
        RECT rect={};auto window=WindowFromDC(wglGetCurrentDC());if(!window||!GetClientRect(window,&rect)||rect.right<=0||rect.bottom<=0)return;
        screen.iWidth=rect.right;screen.iHeight=rect.bottom;
    }
    bool previousOutside=Weave::outsideGame,previousOnly=Weave::menuOnly;
    Weave::outsideGame=!level;Weave::menuOnly=true;
    Mouse::begin(screen);menuScreenW=screen.iWidth;menuScreenH=screen.iHeight;
    Weave::draw(screen,GetTickCount64()/1000.f);
    Weave::outsideGame=previousOutside;Weave::menuOnly=previousOnly;
}

#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
static int redraw(float time, int intermission) {
    CallbackScope callbackScope;
    if(ready){DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        bool f11Down=foregroundPid==GetCurrentProcessId()&&(GetAsyncKeyState(VK_F11)&0x8000);
        if(f11Down&&!f11WasDown)unloadRequested.store(true,std::memory_order_release);f11WasDown=f11Down;
        if(unloadRequested.load(std::memory_order_acquire))return origRedraw(time,intermission);
    }
    lastRedrawTick=GetTickCount64();
    if(ready&&!gameLevelActive()){return origRedraw(time,intermission);}
    if(ready){applyCrosshair();guardCrosshair();FlashTint::apply();Hud::chatPresentationReady=wglGetCurrentContext()!=nullptr;Hud::frame();ChatScroll::install();Trainer::frame();}
    if(ready)GameHudState::frame(time,intermission);
    int result;{NativeHudStyle::Frame nativeHud;JumpStats::NativeScope jumpStats;result=origRedraw(time,intermission);}
    if (!ready) return result;
    Mouse::sync();
    SCREENINFO screen = {}; screen.iSize = sizeof(screen);
    if (!eng->pfnGetScreenInfo(&screen)) return result;
    if(!opened&&!Weave::isAnimating()){Mouse::begin(screen);Hud::drawRadar(screen);if(!Weave::drawOverlay(screen,time)){ChatBox::draw(screen);Hud::watermark(screen);if(Hud::chatOpened)Mouse::finish(screen);}GoCross::draw(screen);return result;}
    return result; // Menu is submitted once, after native VGUI, from SwapBuffers.
}
static int update(client_data_t *data, float time) {
    CallbackScope callbackScope;
    int result = origUpdate(data, time);
    GoCross::zoomed=data && data->fov>0 && data->fov<85;
    // Leave zoom and server-requested special FOV values untouched.
    if (ready && data && fov != 90 && (data->fov == 0 || data->fov == 90)) {
        data->fov = static_cast<float>(fov); result=1;
    }
    return result;
}
static void restoreModel() {lastViewModel=nullptr;baseModel=replacement=nullptr;}
static void refdef(ref_params_t *params) {CallbackScope scope;origRefdef(params);}
static bool captured[256];
static int keyEvent(int down, int key, const char *binding) {
    CallbackScope callbackScope;
    static int scoresKey=-1;
    if(binding&&!_stricmp(binding,"+showscores")){Weave::combatScoresHeld=down!=0;scoresKey=down?key:-1;}
    else if(!down&&key==scoresKey){Weave::combatScoresHeld=false;scoresKey=-1;}
    if (!ready) return origKey(down, key, binding);
    if(key==145){if(down)unloadRequested.store(true,std::memory_order_release);return 0;}
    bool chatBinding=binding && (!_stricmp(binding,"messagemode") || !_stricmp(binding,"messagemode2"));
    if(gameLevelActive() && !opened && Hud::chatOn && Hud::chatPresentationReady && Hud::oldSay && ChatScroll::window && down && chatBinding){ChatEditor::begin(!_stricmp(binding,"messagemode2"));Mouse::sync();return 0;}
    if(!opened && Hud::chatOpened && !Mouse::flushing){
        if(key==149||key==150||key==239||key==240){if(down)ChatScroll::scroll((key==150||key==240)?(key>=239?2:5):-(key>=239?2:5));return 0;}
        if(down){if(key==13)ChatEditor::send();else if(key==27)ChatEditor::close();else if(key==8)ChatEditor::key(VK_BACK);}
        return 0;
    }
    if(opened && Weave::context && Weave::useMenu && !Mouse::flushing && key!=MenuTheme::menuEngineKey()){
        if(down && key==27){opened=false;Mouse::sync();}
        return 0;
    }
    // GoldSrc key codes: arrows 128..131, Insert 147, F5 139, F6 140.
    bool rotate=opened && row<4 && (key=='a'||key=='d'||key=='w'||key=='s'||key==' '||key=='h');
    bool historyKey=opened&&row==10&&(key==149||key==150||key==151||key==152||key==9||key==13);
    bool optionKey=opened&&row==9&&key==9;
    if(opened && !Mouse::flushing && ((key>=241 && key<=245) || (key>=200 && key<=204)))return 0;
    if(opened && down && (key==239 || key==240)){
        if(row==10){if(Hud::historyDetails)Hud::detailScroll=std::max(0,Hud::detailScroll+(key==240?-3:3));else Hud::navigateHistory(key==240?1:-1);}
        else if(row<4){choice[row]=std::max(0,std::min(counts[row],choice[row]+(key==240?-1:1)));save();}
        else if(row==5 || row==6){int &value=row==5?crossStyle:crossColor;value=std::max(0,std::min(row==5?12:8,value+(key==240?-1:1)));crossPending=true;save();}
        return 0;
    }
    if(opened && (key==239 || key==240))return 0;
    bool control = key == MenuTheme::menuEngineKey() || rotate || historyKey || optionKey || (opened && (key == 27 || (key >= 128 && key <= 131) || key == 139 || key == 140));
    if (!down && key >= 0 && key < 256 && captured[key]) { captured[key] = false; return 0; }
    if (!control) return origKey(down, key, binding);
    if (!down) return 0;
    if (key >= 0 && key < 256) captured[key] = true;
    if (key == MenuTheme::menuEngineKey()) {
        if (!insertWasDown) toggleMenu();
        insertSeenInClient = true;
        insertWasDown = true;
    }
    else if (key == 27) {opened = false;Mouse::sync();}
    else if (key == 128) row = (row+11)%12;
    else if (key == 129) row = (row+1)%12;
    else if(optionKey)Hud::option=(Hud::option+1)%5;
    else if(historyKey){
        if(key==13){Hud::historyDetails=!Hud::historyDetails;Hud::detailScroll=0;}
        else if(key==9){Hud::historySelected=(Hud::historySelected+1)%Hud::historyRows;Hud::historyDetails=false;}
        else if(Hud::historyDetails && (key==150||key==149)){Hud::detailScroll=std::max(0,Hud::detailScroll+(key==150?-3:3));}
        else if(key==150)Hud::navigateHistory(1);else if(key==149)Hud::navigateHistory(-1);
        else if(key==151){Hud::historyOffset=std::max(0,static_cast<int>(Hud::history.size())-1);Hud::historyDetails=false;}
        else {Hud::historyOffset=0;Hud::historyDetails=false;}
    }
    else if(rotate) {
        if(key==' ')Preview::spin=!Preview::spin;
        else if(key=='h')Preview::showHands=!Preview::showHands;
        else if(key=='a'||key=='d'){Preview::yaw+=key=='a'?-15:15;Preview::spin=false;}
        else {Preview::pitch=std::max(-80.f,std::min(80.f,Preview::pitch+(key=='w'?10:-10)));Preview::spin=false;}
    }
    else if (key == 130 || key == 131) {
        int direction = key == 131 ? 1 : -1;
        if (row == 4) fov = std::max(60, std::min(120, fov+direction*5));
        else if(row<4) choice[row] = (choice[row]+direction+counts[row]+1) % (counts[row]+1);
        else if(row==5){crossStyle=(crossStyle+direction+13)%13;crossPending=true;}
        else if(row==6){crossColor=(crossColor+direction+9)%9;crossPending=true;}
        else if(row==7){Hud::legacyPalette=(Hud::legacyPalette+direction+8)%8;Hud::customLegacy=false;}
        else if(row==8)Hud::look=(Hud::look+direction+7)%7;
        else if(row==9)Hud::changeOption(direction);
        else Hud::navigateHistory(-direction);
        save();
    } else if (key == 139) {
        save(); scanModels(); Preview::release(); strcpy(status, "Model folders rescanned.");
    } else if (key == 140) {
        fov = 90; crossStyle=crossColor=0;crossPending=true;
        for (int &n : choice) n = 0; Preview::release();save(); strcpy(status, "Default settings restored.");
    }
    return 0;
}
static int vidInit() {
    CallbackScope callbackScope;
    NativeHudStyle::resetVideo();Skybox::resetVideo();GlassScoreboard::resetVideo();
    Weave::combatScoresHeld=false;PlayerOutfits::reset();
    // Map changes invalidate model pointers; don't dereference the previous map.
    lastViewModel = nullptr; baseModel = replacement = nullptr;
    SmokeGL::invalidate();SmokeGL::observed.clear();SmokeTint::newMap();
    Preview::release();
    Weave::resetVideo();
    Hud::resetVideo();
    GoCross::reset();
    MovementHud::reset();JumpStats::reset();MovementStats::resetTracking();Hud::predictedTick=0;
    return origVidInit();
}
static bool executableInModule(void *p, HMODULE module) {
    MEMORY_BASIC_INFORMATION info;
    return VirtualQuery(p, &info, sizeof(info)) && info.AllocationBase == module && info.State == MEM_COMMIT &&
        !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
// Executable code inside any loaded module (another plugin may wrap an engine callback).
static bool executableImage(void *p) {
    MEMORY_BASIC_INFORMATION info;
    return p && VirtualQuery(p, &info, sizeof(info)) && info.Type == MEM_IMAGE && info.State == MEM_COMMIT &&
        !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
// Another ASI may have hooked client Initialize, so its first bytes in memory are a jump.
// The original instructions are read from client.dll on disk and relocated to the loaded base.
static bool initializeFromFile(HMODULE client, unsigned char *code, size_t size) {
    auto base = reinterpret_cast<unsigned char *>(client);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + reinterpret_cast<IMAGE_DOS_HEADER *>(base)->e_lfanew);
    auto init = reinterpret_cast<unsigned char *>(GetProcAddress(client, "Initialize"));
    if (!init) return false;
    DWORD rva = static_cast<DWORD>(init - base), offset = 0; bool mapped = false;
    auto section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        if (rva >= section->VirtualAddress && rva + size <= section->VirtualAddress + section->SizeOfRawData) {
            offset = rva - section->VirtualAddress + section->PointerToRawData; mapped = true; break;
        }
    wchar_t path[MAX_PATH]; if (!mapped || !GetModuleFileNameW(client, path, MAX_PATH)) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD read = 0; bool ok = SetFilePointer(file, offset, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(file, code, static_cast<DWORD>(size), &read, nullptr) && read == size;
    CloseHandle(file); if (!ok) return false;
    // The immediate operands on disk use the preferred image base.
    intptr_t delta = reinterpret_cast<intptr_t>(base) - static_cast<intptr_t>(nt->OptionalHeader.ImageBase);
    for (size_t i = 0; i + 7 <= size; ++i) if (code[i] == 0xBF && code[i+5] == 0xF3 && code[i+6] == 0xA5) {
        uintptr_t address; memcpy(&address, code+i+1, 4); address += delta; memcpy(code+i+1, &address, 4);
    }
    return true;
}
static cl_enginefunc_t *findEngine(HMODULE client, HMODULE hw) {
    auto init = reinterpret_cast<unsigned char *>(GetProcAddress(client, "Initialize"));
    if (!init) return nullptr;
    // Steam Legacy Initialize copies the engine API with mov edi,imm32; rep movsd.
    // No fixed addresses are used. Refuse unsupported initialization code.
    unsigned char original[64];
    if (init[0] == 0xE9 || init[0] == 0xE8 || init[0] == 0xFF || init[0] == 0x68) {
        static bool noted; if (!noted) { noted = true; logLine("Client Initialize is hooked by another plugin; reading its original code from client.dll."); }
        if (!initializeFromFile(client, original, sizeof(original))) return nullptr;
        init = original;
    }
    for (int i = 0; i < 64-7; ++i) if (init[i] == 0xBF && init[i+5] == 0xF3 && init[i+6] == 0xA5) {
        uintptr_t address; memcpy(&address, init+i+1, sizeof(address));
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery(reinterpret_cast<void *>(address), &info, sizeof(info)) ||
            info.AllocationBase != client || info.State != MEM_COMMIT ||
            !(info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) ||
            address + 82*sizeof(void *) > reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize) return nullptr;
        auto table = reinterpret_cast<cl_enginefunc_t *>(address);
        // Optional, unused callbacks can be null or belong to another engine module.
        // Validate only the callbacks we actually use.
        void *required[] = {
            reinterpret_cast<void *>(table->pfnGetScreenInfo),
            reinterpret_cast<void *>(table->pfnFillRGBA),
            reinterpret_cast<void *>(table->pfnDrawSetTextColor),
            reinterpret_cast<void *>(table->pfnDrawConsoleString),
            reinterpret_cast<void *>(table->GetViewModel),
            reinterpret_cast<void *>(table->CL_LoadModel),
            reinterpret_cast<void *>(table->pfnAddCommand)
            ,reinterpret_cast<void *>(table->pfnClientCmd)
            ,reinterpret_cast<void *>(table->pfnGetCvarString)
            ,reinterpret_cast<void *>(table->pfnGetCvarFloat)
            ,reinterpret_cast<void *>(table->COM_LoadFile)
            ,reinterpret_cast<void *>(table->COM_FreeFile)
            ,reinterpret_cast<void *>(table->pfnHookUserMsg)
            ,reinterpret_cast<void *>(table->pfnGetPlayerInfo)
            ,reinterpret_cast<void *>(table->GetLocalPlayer)
            ,reinterpret_cast<void *>(table->GetViewAngles)
            ,reinterpret_cast<void *>(table->pfnGetLevelName)
            ,reinterpret_cast<void *>(table->pfnGetCvarPointer)
            ,reinterpret_cast<void *>(table->pfnTextMessageGet)
        };
        // Other plugins may wrap some engine callbacks with their own code. Accept that as
        // long as every callback is real code and most of them still belong to hw.dll.
        int inEngine = 0, total = static_cast<int>(sizeof(required) / sizeof(*required));
        for (void *callback : required) { if (!executableImage(callback)) return nullptr; if (executableInModule(callback, hw)) ++inEngine; }
        if (inEngine * 3 < total * 2) return nullptr;
        if (inEngine < total) { static bool noted; if (!noted) { noted = true; char line[96]; snprintf(line, sizeof(line), "Engine table: %d of %d callbacks are wrapped by another plugin.", total - inEngine, total); logLine(line); } }
        return table;
    }
    return nullptr;
}
// The game and the Steam overlay load and hook their DLLs in the first seconds. Loading DLLs on
// the plugin's thread meanwhile (the licence check opens Windows' crypto providers) could
// deadlock the start: the overlay waited for its hooking lock ("Failure waiting for hooking
// mutex!") and the game never showed a window. So nothing is done until the engine, the client
// and the game's window are up (at most two minutes).
static BOOL CALLBACK visibleWindowOfProcess(HWND window,LPARAM found){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid==GetCurrentProcessId()&&IsWindowVisible(window)){*reinterpret_cast<bool *>(found)=true;return FALSE;}
    return TRUE;
}
static bool gameWindowShown(){bool found=false;EnumWindows(visibleWindowOfProcess,reinterpret_cast<LPARAM>(&found));return found;}
static bool waitForGameStart(){
    for(int attempt=0;attempt<600;++attempt){
        if(GetModuleHandleA("hw.dll")&&GetModuleHandleA("client.dll")&&gameWindowShown()){Sleep(300);return true;}
        if(attempt>=100&&GetModuleHandleA("sw.dll")&&!GetModuleHandleA("hw.dll")&&gameWindowShown()){
            startupFailure("Startup: software renderer detected; OpenGL is required.",
                L"Включите OpenGL в настройках видео CS 1.6 и перезапустите игру.");
            return false;
        }
        Sleep(100);
    }
    startupFailure("Startup: engine/client or game window did not appear within 60 seconds.",
        L"Не удалось дождаться запуска клиента CS 1.6 с OpenGL. Проверьте, что ASI находится в папке запускаемой игры.");
    return false;
}
static DWORD WINAPI initialize(void *) {
    IniCache::deferred=true;
    logLine("Startup stage: waiting for the game window and OpenGL client.");
    if(!waitForGameStart())return 0;
    logLine("Startup stage: installing embedded resources.");
    const auto installation=EmbeddedInstaller::installForGame();
    if(!installation.ok){
        char line[128];snprintf(line,sizeof(line),"Automatic assets: failed; error=%lu rollback=%d.",static_cast<unsigned long>(installation.error),int(installation.rollbackOk));
        startupFailure(line,L"Не удалось установить файлы BALONCUAGA. Проверьте доступ к папке игры. Резервные копии: .baloncuaga-install/backups.");
        return 0;
    }
    nativeResourcesRestartPending=installation.restart;
    if(installation.restart)
        logLine("Automatic assets: installed with backups; continuing HUD initialization. Only native resource styling needs a restart.");
    logLine("Startup stage: locating the engine API.");
    ensureModelFolders();
    logLine("baLONCUaga " BALONCUAGA_VERSION " standalone build starting.");
    HMODULE client = nullptr, hw = nullptr;
    for (int attempt = 0; attempt < 600; ++attempt) {
        client = GetModuleHandleA("client.dll"); hw = GetModuleHandleA("hw.dll");
        if (client && hw && (eng = findEngine(client, hw))) break;
        Sleep(100);
    }
    if (!eng) {
        startupFailure("Unsupported engine/client or initialization timeout. No hooks installed.",
            L"Не удалось подключиться к этой версии клиента. Нужна CS 1.6 Steam Legacy x86 с OpenGL. Если это Steam Legacy, проверьте версию клиента.");
        return 0;
    }
    GareyRuntime::startForGame();
    for(int attempt=0;attempt<50&&!Garey::variable("fps_visual")&&(GareyRuntime::state==GareyRuntime::Loaded||GareyRuntime::state==GareyRuntime::Existing);++attempt)Sleep(20);
    fov = std::max(60, std::min(120, static_cast<int>(GetPrivateProfileIntA("settings", "fov", 90, iniPath))));
    crossStyle=std::max(0,std::min(12,static_cast<int>(GetPrivateProfileIntA("crosshair","style",0,iniPath))));
    CrossConfig::load();
    crossColor=std::max(0,std::min(8,static_cast<int>(GetPrivateProfileIntA("crosshair","color",0,iniPath))));
    crossPending=true;
    if(GetPrivateProfileIntA("original_crosshair","captured",0,iniPath)){
        char size[16],color[64];GetPrivateProfileStringA("original_crosshair","size","auto",size,sizeof(size),iniPath);GetPrivateProfileStringA("original_crosshair","color","50 250 50",color,sizeof(color),iniPath);
        if((!_stricmp(size,"auto")||!_stricmp(size,"small")||!_stricmp(size,"medium")||!_stricmp(size,"large")) && *color && strspn(color,"0123456789 ")==strlen(color)){
            strcpy(originalCrossSize,size);strcpy(originalCrossColor,color);crossCaptured=true;
            originalDynamic=GetPrivateProfileIntA("original_crosshair","dynamic",1,iniPath)!=0;
            originalCrossEnabled=GetPrivateProfileIntA("original_crosshair","enabled",1,iniPath)!=0;
            originalTranslucent=GetPrivateProfileIntA("original_crosshair","translucent",1,iniPath)!=0;
        }
    }
    Hud::load();SpectatorResource::load();
    // CS FIX (v81-v89) and fps_visual (v81-v83) were removed; drop their settings.
    WritePrivateProfileStringA("cs_fix",nullptr,nullptr,iniPath);WritePrivateProfileStringA("fps_visual",nullptr,nullptr,iniPath);
    // Load the configured opener before the first menu frame. Otherwise the
    // main-menu poll still waits for Insert after a saved key change.
    MenuTheme::load();MenuTheme::nativeBackgroundAnimation=false;Skybox::load();Garey::load();JumpStats::load();
    logLine("Build: " BALONCUAGA_VERSION "-stock-native-menu; local-fog; smoke-fix; scalar TLS access.");
    scanModels();
    if (MH_Initialize() != MH_OK) {
        startupFailure("MinHook initialization failed.",L"Не удалось подготовить обработчики BALONCUAGA.");return 0;
    }
    struct Hook { const char *name; void *callback; void **original; };
    Hook hooks[] = {
        {"HUD_Redraw", reinterpret_cast<void *>(redraw), reinterpret_cast<void **>(&origRedraw)},
        {"HUD_UpdateClientData", reinterpret_cast<void *>(update), reinterpret_cast<void **>(&origUpdate)},
        {"V_CalcRefdef", reinterpret_cast<void *>(refdef), reinterpret_cast<void **>(&origRefdef)},
        {"HUD_Key_Event", reinterpret_cast<void *>(keyEvent), reinterpret_cast<void **>(&origKey)},
        {"HUD_VidInit", reinterpret_cast<void *>(vidInit), reinterpret_cast<void **>(&origVidInit)},
        {"HUD_TempEntUpdate", reinterpret_cast<void *>(tempEntUpdate), reinterpret_cast<void **>(&origTempEntUpdate)},
        {"HUD_AddEntity", reinterpret_cast<void *>(addEntity), reinterpret_cast<void **>(&origAddEntity)}
    };
    for (auto &hook : hooks) {
        void *target = reinterpret_cast<void *>(GetProcAddress(client, hook.name));
        if (!target || MH_CreateHook(target, hook.callback, hook.original) != MH_OK) {
            char line[192];snprintf(line,sizeof(line),"Required client hook failed: %s.",hook.name);
            MH_Uninitialize();startupFailure(line,L"Не удалось подключить обязательный обработчик клиента. Не удалось установить обработчик.");return 0;
        }
    }
    ServerStrafes::install(client);JumpStats::install();
    SoundAudit::install(hw);
    if(!SmokeGL::install())logLine("SmokeParticle: scoped GL support unavailable; CPU tint remains active.");
    if(!SmokeParticle::install())logLine("SmokeParticle: waiting for native particle module.");
    void *postTarget=reinterpret_cast<void *>(GetProcAddress(client,"HUD_PostRunCmd"));
    void *moveTarget=reinterpret_cast<void *>(GetProcAddress(client,"HUD_PlayerMove"));
    if(moveTarget&&MH_CreateHook(moveTarget,reinterpret_cast<void *>(playerMove),reinterpret_cast<void **>(&origPlayerMove))==MH_OK)logLine("Movement HUD: native movement capture installed.");
    else logLine("Movement HUD: unsupported client; takeoff data unavailable.");
    if(!postTarget||MH_CreateHook(postTarget,reinterpret_cast<void *>(postRunCmd),reinterpret_cast<void **>(&origPostRunCmd))!=MH_OK)logLine("Prediction capture unavailable: speedometer and crosshair shooting animation disabled.");
    if(!Models::install(client,hw))logLine("Models: studio API not initialized yet; retrying on the render thread.");
    PlayerOutfits::install();
    if(!SmokeTint::install(client))logLine("Smoke: particle capture unavailable; temp-entity recoloring remains available.");
    Mouse::activate=reinterpret_cast<void(*)()>(GetProcAddress(client,"IN_ActivateMouse"));
    Mouse::deactivate=reinterpret_cast<void(*)()>(GetProcAddress(client,"IN_DeactivateMouse"));
    Mouse::clearStates=reinterpret_cast<void(*)()>(GetProcAddress(client,"IN_ClearStates"));
    void *mouseEvent=reinterpret_cast<void *>(GetProcAddress(client,"IN_MouseEvent"));
    if(!Mouse::activate || !Mouse::deactivate || !Mouse::clearStates || !mouseEvent ||
       MH_CreateHook(reinterpret_cast<void *>(Mouse::activate),reinterpret_cast<void *>(Mouse::activateHook),reinterpret_cast<void **>(&Mouse::origActivate))!=MH_OK ||
       MH_CreateHook(mouseEvent,reinterpret_cast<void *>(Mouse::eventHook),reinterpret_cast<void **>(&Mouse::origMouseEvent))!=MH_OK){
        MH_Uninitialize();startupFailure("Cannot install menu mouse input hooks; modifications disabled.",
            L"Не удалось подключить управление мышью для меню. Файловая диагностика отключена.");return 0;
    }
    void *swapTarget=reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("gdi32.dll"),"SwapBuffers"));
    void *wglTarget=reinterpret_cast<void *>(GetProcAddress(GetModuleHandleA("opengl32.dll"),"wglSwapBuffers"));
    bool swapOk=swapTarget&&MH_CreateHook(swapTarget,reinterpret_cast<void *>(swapBuffers),reinterpret_cast<void **>(&originalSwap))==MH_OK;
    bool wglOk=wglTarget&&wglTarget!=swapTarget&&MH_CreateHook(wglTarget,reinterpret_cast<void *>(wglSwapBuffersHook),reinterpret_cast<void **>(&originalWglSwap))==MH_OK;
    if(!swapOk&&!wglOk){MH_Uninitialize();startupFailure("Cannot install OpenGL presentation hooks.",
        L"Не удалось подключить отрисовку OpenGL. Проверьте режим видео игры.");return 0;}
    NativeHudStyle::install(hw);Skybox::install(hw);
    logLine("Main menu: native CS 1.6 UI; no BALONCUAGA menu/background hooks.");
    // Native CS scoreboard resources/fonts are restored; no glass-TAB hooks.
    ready = true;
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        ready = false; MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
        startupFailure("Enabling hooks failed.",L"Не удалось включить обработчики BALONCUAGA.");return 0;
    }
    Mouse::extraAction=extraUiAction;Trainer::start();
    Hud::installMessages();GameHudState::installMessages();
    logLine("Initialized. Insert opens the menu in-game; native CS main menu remains unchanged.");
    if(nativeResourcesRestartPending)MessageBoxW(nullptr,L"Стандартные фон и окна CS 1.6 восстановлены. Один раз полностью перезапустите игру, чтобы обновить уже загруженные ресурсы.",L"BALONCUAGA",MB_OK|MB_ICONINFORMATION);
    return 0;
}
// Once entry points are detached, the counter can reach zero a few instructions
// before a callback's epilogue leaves the module. Inspect live instruction pointers
// as well; never free trampolines while another thread is still in this DLL.
static bool threadsOutsideModule(){
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(selfModule);auto nt=reinterpret_cast<IMAGE_NT_HEADERS *>(reinterpret_cast<byte *>(selfModule)+dos->e_lfanew);
    uintptr_t first=reinterpret_cast<uintptr_t>(selfModule),last=first+nt->OptionalHeader.SizeOfImage;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE)return false;
    THREADENTRY32 entry={sizeof(entry)};bool clean=true;
    if(!Thread32First(snapshot,&entry))clean=false;else do{if(entry.th32OwnerProcessID!=GetCurrentProcessId()||entry.th32ThreadID==GetCurrentThreadId())continue;
        HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);if(!thread){clean=false;continue;}
        if(SuspendThread(thread)==DWORD(-1)){if(WaitForSingleObject(thread,0)!=WAIT_OBJECT_0)clean=false;CloseHandle(thread);continue;}
        CONTEXT context={};context.ContextFlags=CONTEXT_CONTROL;if(!GetThreadContext(thread,&context))clean=false;else if(context.Eip>=first&&context.Eip<last)clean=false;
        ResumeThread(thread);CloseHandle(thread);
    }while(Thread32Next(snapshot,&entry));CloseHandle(snapshot);return clean;
}
static std::atomic<bool> unloadPrepared{false};
static DWORD WINAPI unloadWorker(void *parameter){
    HANDLE gate=static_cast<HANDLE>(parameter);WaitForSingleObject(gate,INFINITE);CloseHandle(gate);
    if(!unloadPrepared)return 0;
    StartupWorker::join(initThread);
    forwardHooksToOriginals();   // from here every game function runs its own code again
    Trainer::stop();
    // Worker threads run this DLL's code: once no game callback is inside the plugin,
    // finish the radar builder and the I/O thread before the DLL is freed.
    while(activeCallbacks.load(std::memory_order_acquire)!=0)Sleep(2);
    Hud::stopRadarWork();BackgroundIo::stop();
    while(activeCallbacks.load(std::memory_order_acquire)!=0||!threadsOutsideModule())Sleep(2);
    ChatScroll::release();
    // MinHook is not uninitialised: its trampolines and the forwarders stay for other plugins' jumps.
    int restored=0,shared=0;restoreUnsharedHooks(restored,shared);
    {char line[192];snprintf(line,sizeof(line),"F11: %d game functions restored, %d left forwarding to the game's own code (under other plugins' hooks); freeing the DLL.",restored,shared);logLine(line);}
    if(instanceMutex){CloseHandle(instanceMutex);instanceMutex=nullptr;}
    FreeLibraryAndExitThread(selfModule,0);return 0;
}
static void beginUnload(){
    if(unloading.exchange(true))return;
    if(!wglGetCurrentContext()){unloading=false;return;}
    // Another plugin's hooks on top of ours stay working: ours forward to the game's code.
    if(!hooksStillOurs())logLine("F11: another plugin hooked some of the same game functions after BALONCUAGA; those keep forwarding to the game's own code.");
    // Reserve the worker before any irreversible teardown, so thread creation
    // failure leaves the active plugin intact.
    HANDLE gate=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!gate){unloading=false;return;}
    HANDLE worker=CreateThread(nullptr,0,unloadWorker,gate,0,nullptr);
    if(!worker){CloseHandle(gate);unloading=false;logLine("F11: worker creation failed; plugin retained.");return;}CloseHandle(worker);
    if(!ChatScroll::uninstall()){
        logLine("F11: window subclass restoration blocked; DLL retained safely.");SetEvent(gate);unloadRequested=false;unloading=false;return;
    }
    if(!GameHudState::restore()){logLine("F11: combat HUD message restoration blocked; DLL retained safely.");SetEvent(gate);unloadRequested=false;unloading=false;return;}
    if(!Hud::restoreDirectMessages()){logLine("F11: native message restoration blocked; DLL retained safely.");SetEvent(gate);unloadRequested=false;unloading=false;return;}
    save();IniCache::flushNow();Garey::restore();JumpStats::restoreNativeText();opened=false;ChatEditor::close();Hud::chatOpened=false;restoreModel();SmokeTint::restoreEntities();Mouse::sync();
    if(crossCaptured&&eng&&eng->pfnClientCmd){char command[256];snprintf(command,sizeof(command),
        "cl_crosshair_size \"%s\";cl_dynamiccrosshair %d;cl_crosshair_color \"%s\";crosshair %d;cl_crosshair_translucent %d\n",
        originalCrossSize,originalDynamic,originalCrossColor,originalCrossEnabled,originalTranslucent);eng->pfnClientCmd(command);}


    Trainer::allowed=false;Trainer::enabled=false;ready=false;
    PlayerOutfits::release();GlassScoreboard::release();Skybox::release();Preview::release();Weave::resetVideo();Hud::resetVideo();Weave::shutdown();Models::reset();Mouse::hits.clear();Mouse::extraAction=nullptr;
    unloadPrepared=true;SetEvent(gate);
}
static DWORD WINAPI startupEntry(void *) {
    StartupLog::configure(selfModule);
    logLine("baLONCUaga " BALONCUAGA_VERSION " x86: deferred startup worker entered.");
    DWORD moduleLength=GetModuleFileNameA(selfModule,iniPath,sizeof(iniPath));
    if(!moduleLength||moduleLength>=sizeof(iniPath)){
        startupFailure("Startup: ASI path exceeds the supported engine path length.",L"Путь к ASI слишком длинный для этой версии движка.");return 0;
    }
    char *slash=strrchr(iniPath,'\\');if(!slash)return 0;slash[1]=0;
    if(strlen(iniPath)+strlen("CS16Customizer.ini")>=sizeof(iniPath)){
        startupFailure("Startup: settings path is too long.",L"Путь к папке игры слишком длинный.");return 0;
    }
    strcat(iniPath,"CS16Customizer.ini");
    moduleLength=GetModuleFileNameA(nullptr,rootPath,sizeof(rootPath));
    if(!moduleLength||moduleLength>=sizeof(rootPath)){
        startupFailure("Startup: game path is too long.",L"Путь к папке игры слишком длинный.");return 0;
    }
    slash=strrchr(rootPath,'\\');if(!slash)return 0;*slash=0;
    char mutexName[64];snprintf(mutexName,sizeof(mutexName),"Local\\BALONCUAGA_CS16Customizer_%lu",GetCurrentProcessId());
    instanceMutex=CreateMutexA(nullptr,FALSE,mutexName);
    if(instanceMutex&&GetLastError()==ERROR_ALREADY_EXISTS){
        CloseHandle(instanceMutex);instanceMutex=nullptr;
        logLine("Another copy of BALONCUAGA is already loaded in this game; this copy stays inactive.");return 0;
    }
    if(!instanceMutex){startupFailure("Startup: cannot create the single-instance guard.",L"Не удалось создать защиту от двойной загрузки ASI.");return 0;}
    return initialize(nullptr);
}
extern "C" __declspec(dllexport) void InitializeASI() {
    StartupWorker::schedule(startupEntry,initThread);
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        selfModule=module;DisableThreadLibraryCalls(module);
        StartupWorker::schedule(startupEntry,initThread);
    }
    // The game is closing: settings changed in the last moment are still written.
    if (reason == DLL_PROCESS_DETACH && reserved) IniCache::emergencyFlush();
    // All initialization runs on the tracked worker, never in DllMain.
    // InitializeASI is retained for ASI loaders which explicitly call it.
    return TRUE;
}












