#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <atomic>

// Read-only presentation state. Every captured user message is forwarded to
// its previous native callback unchanged, including malformed/unknown packets.
// Requires Hud::messageList/readable/executable/slotFor/restoreSlot helpers.
namespace GameHudState {
enum Component : uint32_t {Health=1,Armor=2,Ammo=4,Money=8,Timer=16,All=31};
enum Hide : unsigned {HideWeapons=1,HideAll=4,HideHealth=8,HideTimer=16,HideMoney=32};
static constexpr int maxWeapons=64,maxAmmo=64;
struct Weapon {char name[64];int ammo1,ammo2,max1,max2,slot,position,flags;bool valid;};
struct Snapshot {
    int health,armor,money,weaponId,clip,reserve,ammoType,armorType,roundSeconds;
    char weaponName[64];uint32_t validMask,eligibleMask;uint64_t frameSerial,epoch;
    bool alive,spectating,inGame,intermission,noHud,hasClip;int hideFlags,observerMode,observedPlayer;
};
static_assert(std::is_trivial<Snapshot>::value,"HUD snapshots are plain data");
struct State {
    int health,armor,money,weaponId,clip,armorType,hideFlags,ammo[maxAmmo];
    bool healthValid,healthMessage,armorValid,moneyValid,weaponValid,clipValid,timerValid,ammoValid[maxAmmo],localSpecValid,localSpec;
    Weapon weapons[maxWeapons];double roundEnd,frameTime;bool timerNeedsAnchor;int pendingRound;
    uint64_t frameSerial,epoch;bool inGame,alive,spectating,intermission,noHud;int localIndex,observerMode,observedPlayer;char map[260];
};
static State state={};static SRWLOCK stateLock=SRWLOCK_INIT;
struct Write {Write(){AcquireSRWLockExclusive(&stateLock);}~Write(){ReleaseSRWLockExclusive(&stateLock);}};
struct Read {Read(){AcquireSRWLockShared(&stateLock);}~Read(){ReleaseSRWLockShared(&stateLock);}};
static std::atomic<bool> stopped{false};static ULONGLONG registryTick;static bool captureLogged;
static SRWLOCK registryLock=SRWLOCK_INIT;

struct Reader {
    const unsigned char *data;int size,pos=0;bool valid=true;
    int byteValue(){if(pos>=size){valid=false;return 0;}return data[pos++];}
    int shortValue(){int lo=byteValue(),hi=byteValue();return static_cast<int16_t>(lo|(hi<<8));}
    int longValue(){uint32_t a=byteValue(),b=byteValue(),c=byteValue(),d=byteValue();return static_cast<int32_t>(a|(b<<8)|(c<<16)|(d<<24));}
    bool string(char *out,size_t capacity){size_t n=0;while(pos<size){unsigned char c=static_cast<unsigned char>(byteValue());if(!c){out[n]=0;return true;}if(c<32||c>126||n+1>=capacity){valid=false;return false;}out[n++]=static_cast<char>(c);}valid=false;return false;}
    bool end()const{return valid&&pos==size;}
};
enum Message {MHealth,MBattery,MMoney,MAmmoX,MCurWeapon,MWeaponList,MHideWeapon,MRoundTime,MResetHUD,MInitHUD,MSpectator,MArmorType,MHudText,MHudTextPro,MHudTextArgs,MTextMsg,MessageCount};
struct Binding {const char *name;pfnUserMsgHook ours,old,stub;Hud::UserMsg *node;bool installed;};
static Binding bindings[MessageCount];

static void clearValues(bool definitions){
    ++state.epoch;state.health=state.armor=state.money=state.weaponId=state.armorType=state.hideFlags=0;state.clip=-1;
    state.healthValid=state.healthMessage=state.armorValid=state.moneyValid=state.weaponValid=state.clipValid=state.timerValid=false;
    state.roundEnd=0;state.timerNeedsAnchor=false;state.pendingRound=0;state.alive=false;state.localSpecValid=state.localSpec=false;
    for(int i=0;i<maxAmmo;++i){state.ammo[i]=0;state.ammoValid[i]=false;}
    if(definitions)for(auto &weapon:state.weapons)weapon={};
}
static void observeMap(){
    if(!eng||!eng->pfnGetLevelName)return;const char *level=eng->pfnGetLevelName();if(!level)level="";size_t length=0;while(length<sizeof(state.map)&&level[length])++length;if(length>=sizeof(state.map))return;
    if(!length){if(state.inGame){clearValues(true);state.map[0]=0;}state.inGame=false;return;}
    if(state.map[0]&&strcmp(state.map,level))clearValues(true);
    memcpy(state.map,level,length+1);state.inGame=true;
}
static double messageTime(bool &known){
    if(eng&&eng->GetClientTime){double time=eng->GetClientTime();if(std::isfinite(time)&&time>=0){known=true;return time;}}
    known=state.frameSerial!=0;return state.frameTime;
}
static void consume(Message id,int size,const void *payload){
    if(size<0||size>256||(size&&(!payload||!Hud::readable(payload,size))))return;
    // No engine text lookup is performed while holding the HUD state lock.
    if(id==MHudText||id==MHudTextPro||id==MHudTextArgs){ServerStrafes::token(size,payload);return;}
    if(id==MTextMsg)return;
    if(id==MResetHUD||id==MInitHUD){MovementHud::reset();JumpStats::reset();}
    Write guard;observeMap();Reader p{static_cast<const unsigned char *>(payload),size};
    switch(id){
    case MHealth:state.healthValid=false;state.healthMessage=true;state.alive=false;if(size==1){state.health=p.byteValue();state.healthValid=true;state.alive=state.health>0;}break;
    case MBattery:state.armorValid=false;if(size==2){int value=p.shortValue();if(p.end()&&value>=0){state.armor=value;state.armorValid=true;}}break;
    case MMoney:state.moneyValid=false;if(size==5){int value=p.longValue();int flash=p.byteValue();if(p.end()&&value>=0&&value<=100000000&&flash<=1){state.money=value;state.moneyValid=true;}}break;
    case MAmmoX:if(size==2){int type=p.byteValue(),value=p.byteValue();if(p.end()&&type<maxAmmo){state.ammo[type]=value;state.ammoValid[type]=true;}}else if(size>0){int type=p.byteValue();if(type<maxAmmo)state.ammoValid[type]=false;}break;
    case MCurWeapon:if(size==3){int active=p.byteValue(),weapon=p.byteValue(),clip=p.byteValue();if(p.end()&&active<=3){
        if(weapon==255){state.weaponValid=state.clipValid=false;state.weaponId=0;state.clip=-1;}
        else if((active&1)&&weapon>0&&weapon<maxWeapons){state.weaponId=weapon;state.clip=clip==255?-1:clip;state.weaponValid=state.clipValid=true;}
        else if(weapon==state.weaponId&&state.weaponValid){state.clip=clip==255?-1:clip;state.clipValid=true;}}else state.weaponValid=state.clipValid=false;}else state.weaponValid=state.clipValid=false;break;
    case MWeaponList:{Weapon weapon={};int ident=0;bool accepted=false;if(size>=9&&p.string(weapon.name,sizeof(weapon.name))){int a1=p.byteValue(),m1=p.byteValue(),a2=p.byteValue(),m2=p.byteValue();weapon.ammo1=a1==255?-1:a1;weapon.max1=m1==255?-1:m1;weapon.ammo2=a2==255?-1:a2;weapon.max2=m2==255?-1:m2;weapon.slot=p.byteValue();weapon.position=p.byteValue();ident=p.byteValue();weapon.flags=p.byteValue();
        if(p.end()&&weapon.name[0]&&ident>0&&ident<maxWeapons&&(weapon.ammo1==-1||weapon.ammo1<maxAmmo)&&(weapon.ammo2==-1||weapon.ammo2<maxAmmo)&&weapon.slot<16&&weapon.position<64){weapon.valid=true;state.weapons[ident]=weapon;accepted=true;}}
        if(!accepted)state.weaponValid=state.clipValid=false;}break;
    case MHideWeapon:state.hideFlags=size==1?p.byteValue():HideAll;break;
    case MRoundTime:state.timerValid=false;if(size==2){int seconds=p.shortValue();if(p.end()&&seconds>=0){bool known=false;double now=messageTime(known);state.roundEnd=now+seconds;state.pendingRound=seconds;state.timerNeedsAnchor=!known;state.timerValid=true;}}break;
    case MResetHUD:if(size==0||size==1)clearValues(false);break;
    case MInitHUD:if(size==0)clearValues(true);break;
    case MSpectator:if(size==2){int index=p.byteValue(),spectator=p.byteValue();if(p.end()&&index==state.localIndex&&spectator<=1){state.localSpecValid=true;state.localSpec=spectator!=0;state.spectating=state.localSpec;}}break;
    case MArmorType:if(size==1){int type=p.byteValue();if(type<=1)state.armorType=type;}break;
    default:break;
    }
}
#if defined(__GNUC__)
#define GAMEHUD_ALIGN __attribute__((force_align_arg_pointer))
#else
#define GAMEHUD_ALIGN
#endif
template<int Id> GAMEHUD_ALIGN static int message(const char *name,int size,void *payload){
    CallbackScope scope;if(ready&&!unloading&&!stopped)consume(static_cast<Message>(Id),size,payload);pfnUserMsgHook old=bindings[Id].old;return old?old(name,size,payload):1;
}
#undef GAMEHUD_ALIGN
static void prepareBindings(){
    if(bindings[0].name)return;
    Binding entries[]={{"Health",message<MHealth>},{"Battery",message<MBattery>},{"Money",message<MMoney>},{"AmmoX",message<MAmmoX>},{"CurWeapon",message<MCurWeapon>},{"WeaponList",message<MWeaponList>},{"HideWeapon",message<MHideWeapon>},{"RoundTime",message<MRoundTime>},{"ResetHUD",message<MResetHUD>},{"InitHUD",message<MInitHUD>},{"Spectator",message<MSpectator>},{"ArmorType",message<MArmorType>},{"HudText",message<MHudText>},{"HudTextPro",message<MHudTextPro>},{"HudTextArgs",message<MHudTextArgs>},{"TextMsg",message<MTextMsg>}};
    for(int i=0;i<MessageCount;++i)bindings[i]=entries[i];
}
static bool writableCallback(Hud::UserMsg *node){MEMORY_BASIC_INFORMATION info={};return VirtualQuery(&node->callback,&info,sizeof(info))&&info.State==MEM_COMMIT&&!(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY));}
static void installMessages(){
    if(stopped||!eng||!TryAcquireSRWLockExclusive(&registryLock))return;struct Guard {~Guard(){ReleaseSRWLockExclusive(&registryLock);}} guard;if(stopped)return;
    prepareBindings();ULONGLONG now=GetTickCount64();if(now-registryTick<500)return;registryTick=now;auto head=Hud::messageList();if(!head)return;
    auto node=*head;for(int walked=0;node&&walked<512;++walked){if(!Hud::readable(node,sizeof(*node))||!memchr(node->name,0,sizeof(node->name)))break;
        for(auto &binding:bindings)if(!strcmp(node->name,binding.name)&&Hud::executable(reinterpret_cast<void *>(node->callback))&&writableCallback(node)){
            if(node->callback==binding.stub&&binding.stub){binding.node=node;binding.installed=true;continue;}
            // A later plugin may wrap our stable stub. Preserve its chain.
            if(binding.installed&&binding.node==node&&node->callback!=binding.old)continue;
            if(binding.installed&&node->callback!=binding.old)continue;
            pfnUserMsgHook previous=node->callback,forward=Hud::slotFor(binding.ours);if(!forward||forward==binding.ours)continue; // no unsafe raw DLL callback fallback
            binding.old=previous;binding.stub=forward;void *found=InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(&node->callback),reinterpret_cast<void *>(forward),reinterpret_cast<void *>(previous));
            if(found==reinterpret_cast<void *>(previous)){binding.node=node;binding.installed=true;}
        }node=node->next;
    }
    if(!captureLogged&&bindings[MHealth].installed){captureLogged=true;logLine("Game HUD: read-only native message capture installed; all messages forwarded.");}
}
static void frame(float time,int intermission=0){
    if(ready&&!unloading)installMessages();Write guard;observeMap();++state.frameSerial;state.frameTime=std::isfinite(time)&&time>=0?time:state.frameTime;state.intermission=intermission!=0;state.noHud=false;state.spectating=state.localSpecValid&&state.localSpec;state.observerMode=state.observedPlayer=0;
    if(state.timerNeedsAnchor&&state.timerValid){state.roundEnd=state.frameTime+state.pendingRound;state.timerNeedsAnchor=false;}
    if(eng&&eng->pfnGetCvarFloat)state.noHud=eng->pfnGetCvarFloat(const_cast<char *>("hud_draw"))==0;
    auto local=eng&&eng->GetLocalPlayer?eng->GetLocalPlayer():nullptr;state.localIndex=0;
    if(local&&Hud::readable(local,sizeof(*local))&&local->player&&local->index>=1&&local->index<=32){
        state.localIndex=local->index;int mode=local->curstate.iuser1;state.observerMode=mode>=0&&mode<=6?mode:0;state.observedPlayer=local->curstate.iuser2>=1&&local->curstate.iuser2<=32?local->curstate.iuser2:0;state.spectating=state.spectating||local->curstate.spectator!=0||state.observerMode!=0;
        if(!state.healthMessage){int health=local->curstate.health;if(health>0&&health<=32767){state.health=health;state.healthValid=true;}else state.healthValid=false;}
    }
    if(eng&&eng->IsSpectateOnly&&eng->IsSpectateOnly())state.spectating=true;
    state.alive=state.healthValid&&state.health>0&&state.localIndex!=0;
}
static Weapon standardWeapon(int id){
    struct Entry {int id,ammo;const char *name;};
    static const Entry entries[]={{1,9,"weapon_p228"},{3,2,"weapon_scout"},{4,12,"weapon_hegrenade"},{5,5,"weapon_xm1014"},{6,14,"weapon_c4"},{7,6,"weapon_mac10"},{8,4,"weapon_aug"},{9,13,"weapon_smokegrenade"},{10,10,"weapon_elite"},{11,7,"weapon_fiveseven"},{12,6,"weapon_ump45"},{13,4,"weapon_sg550"},{14,4,"weapon_galil"},{15,4,"weapon_famas"},{16,6,"weapon_usp"},{17,10,"weapon_glock18"},{18,1,"weapon_awp"},{19,10,"weapon_mp5navy"},{20,3,"weapon_m249"},{21,5,"weapon_m3"},{22,4,"weapon_m4a1"},{23,10,"weapon_tmp"},{24,2,"weapon_g3sg1"},{25,11,"weapon_flashbang"},{26,8,"weapon_deagle"},{27,4,"weapon_sg552"},{28,2,"weapon_ak47"},{29,-1,"weapon_knife"},{30,7,"weapon_p90"}};
    Weapon weapon={};weapon.ammo1=weapon.ammo2=-1;for(const auto &entry:entries)if(entry.id==id){weapon.valid=true;weapon.ammo1=entry.ammo;strcpy(weapon.name,entry.name);break;}return weapon;
}
static Snapshot snapshot(){
    Read guard;Snapshot out={};out.health=state.health;out.armor=state.armor;out.money=state.money;out.weaponId=state.weaponId;out.clip=state.clip;out.reserve=-1;out.ammoType=-1;out.armorType=state.armorType;out.frameSerial=state.frameSerial;out.epoch=state.epoch;out.alive=state.alive;out.spectating=state.spectating;out.inGame=state.inGame;out.intermission=state.intermission;out.noHud=state.noHud;out.hideFlags=state.hideFlags;out.observerMode=state.observerMode;out.observedPlayer=state.observedPlayer;out.hasClip=state.clipValid&&state.clip>=0;
    if(state.healthValid)out.validMask|=Health;if(state.armorValid)out.validMask|=Armor;if(state.moneyValid)out.validMask|=Money;if(state.timerValid){out.validMask|=Timer;out.roundSeconds=int((std::max)(0.,std::ceil(state.roundEnd-state.frameTime)));}
    if(state.weaponValid&&state.weaponId>0&&state.weaponId<maxWeapons){Weapon weapon=state.weapons[state.weaponId].valid?state.weapons[state.weaponId]:standardWeapon(state.weaponId);if(weapon.valid){memcpy(out.weaponName,weapon.name,sizeof(out.weaponName));out.ammoType=weapon.ammo1;
        if(weapon.ammo1>=0&&weapon.ammo1<maxAmmo&&state.ammoValid[weapon.ammo1]&&state.clipValid){out.reserve=state.ammo[weapon.ammo1];out.validMask|=Ammo;}}}
    if(!stopped&&state.inGame&&state.alive&&state.healthValid&&state.health>0&&!state.spectating&&!state.intermission&&!state.noHud&&!(state.hideFlags&HideAll)){
        out.eligibleMask=out.validMask;if(state.hideFlags&HideHealth)out.eligibleMask&=~uint32_t(Health|Armor);if(state.hideFlags&HideWeapons)out.eligibleMask&=~uint32_t(Ammo);if(state.hideFlags&HideMoney)out.eligibleMask&=~uint32_t(Money);if(state.hideFlags&HideTimer)out.eligibleMask&=~uint32_t(Timer);
    }return out;
}
static bool restore(){
    stopped=true;AcquireSRWLockExclusive(&registryLock);struct Guard {~Guard(){ReleaseSRWLockExclusive(&registryLock);}} registryGuard;{Write guard;clearValues(false);}prepareBindings();auto head=eng?Hud::messageList():nullptr;
    for(auto &binding:bindings)if(binding.installed&&binding.old){
        // Redirect every permanent forwarder even if the engine rebuilt its
        // registry or a later plugin now owns the callback slot.
        for(auto &forward:Hud::slotForwards)if(forward.ours==binding.ours&&forward.slot)InterlockedExchangePointer(reinterpret_cast<void *volatile *>(forward.slot),reinterpret_cast<void *>(binding.old));
        if(head){auto node=*head;for(int i=0;node&&i<512;++i){if(!Hud::readable(node,sizeof(*node))||!memchr(node->name,0,sizeof(node->name)))break;if(!strcmp(node->name,binding.name)&&writableCallback(node))Hud::restoreSlot(node,binding.ours,binding.old);node=node->next;}}
        binding.installed=false;
    }return true;
}
} // namespace GameHudState
