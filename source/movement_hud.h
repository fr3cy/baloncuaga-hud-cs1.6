#pragma once
#include "pm_defs.h"
#include "in_buttons.h"
#ifdef clamp
#undef clamp
#endif
namespace MovementHud {
enum Kind {None,Jump,Bhop,StandBhop,CountJump,MultiCountJump,StandCountJump,MultiStandCountJump,Duck,StandDuck,Ladder};
static bool enabled=true,showGain=true,keep=false;
static int accent[3]={134,220,159},holdMs=5000;
static int lateAccent[3]={245,92,105};
static constexpr bool videoEffects=true;
static bool previewIdeal=true;
static int animationMs=850,perfectRgb[3]={90,245,145};
enum Style {Capsule,Underline,Classic,Inline,Split,Badge,Ring,Card,StyleCount};
static int style=Classic,previewFog=1;
static ULONGLONG previewEpoch=0;
static ULONGLONG previewTakeoff(ULONGLONG now){if(!previewEpoch||now<previewEpoch)previewEpoch=now;return previewEpoch;}
static const char *styleNames[]={u8"Капсула",u8"Подчёркивание",u8"Классический",u8"Одна строка",u8"Разделитель",u8"Бейдж",u8"Кольцо",u8"Карточка"};
struct Frame {
    bool ground=false,crouched=false,alive=false;int type=0,water=0,buttons=0,oldbuttons=0,flags=0;
    float x=0,y=0,z=0,vx=0,vy=0,vz=0,dt=.01f,maxSpeed=250,jumpTimer=0;
};
struct Result {Kind kind=None;int fog=0;float pre=0;ULONGLONG tick=0;bool hasFog=false;bool ideal=false;};
static bool lateFog(const Result &value){return value.kind!=Ladder&&value.fog>=3;}
static const int *resultAccent(const Result &value){
    if(videoEffects){
        static const int colors[][3]={{115,224,231},{247,214,119},{246,159,89},{242,100,111},{150,227,247}};
        if(value.kind==Ladder)return colors[4];
        if(value.ideal)return perfectRgb;
        return colors[value.fog<=1?0:value.fog==2?1:value.fog==3?2:3];
    }
    return lateFog(value)?lateAccent:accent;
}
static float animationAge(ULONGLONG now,ULONGLONG tick){return now>=tick?(now-tick)/static_cast<float>(std::clamp(animationMs,300,1600)):0.f;}
static float perfectPulse(const Result &value,ULONGLONG now){
    if(!videoEffects||!value.ideal||now<value.tick)return 0;
    float t=animationAge(now,value.tick);if(t>=1)return 0;
    float wave=std::sin(std::min(1.f,t*5)*1.570796327f);return wave*(1-t)*(1-t);
}
static float glowStrength(ULONGLONG now,ULONGLONG takeoff){
    double age=now>=takeoff?(now-takeoff)/1000.0:0;
    return .55f+.25f*static_cast<float>(std::cos(age*6.28318530718/1.2))+.20f*static_cast<float>(std::exp(-age/0.18));
}
struct Tracker {
    bool primed=false,ground=false,landCrouched=false,hasLanding=false,duckArmed=false,flight=false,duckStand=false;
    int frames=0,previousButtons=0,ducks=0;
    float duckAge=0,age=0,takeoff=0,gain=0,x=0,y=0,z=0;
    Result duckCandidate;
    bool idealPending=false;int idealAge=0;
    float candidateAge=0;
    bool candidateStand=false;
};
static Tracker track;
static void rememberCaAttempt(bool ideal){track.idealPending=ideal;track.idealAge=0;}
static Result result;
static Result displayResult(){return result;}
static unsigned long long publication=0;
static bool showDetails(const Result &value){return value.hasFog||value.kind==Ladder;}
static void reset(){track={};result={};}
static void resetSequence(){float gain=track.gain;track={};track.gain=gain;}
static bool visible(ULONGLONG now){return result.kind!=None&&now>=result.tick&&(keep||now-result.tick<static_cast<ULONGLONG>(holdMs));}
static void formatResult(const Result &value,char *text,size_t capacity){
    if(value.kind==Ladder)snprintf(text,capacity,"LADDER  ·  %.3f",value.pre);
    else snprintf(text,capacity,"FOG %d  ·  %.3f",value.fog,value.pre);
}
static Frame sample(const playermove_t *pm){Frame f;if(!pm)return f;
    f.ground=pm->onground!=-1;f.crouched=pm->usehull==1||(pm->flags&FL_DUCKING)!=0;
    f.alive=!pm->dead&&!pm->deadflag&&!pm->spectator&&!pm->iuser1;
    f.type=pm->movetype;f.water=pm->waterlevel;f.buttons=pm->cmd.buttons;f.oldbuttons=pm->oldbuttons;f.flags=pm->flags;
    f.x=pm->origin[0];f.y=pm->origin[1];f.z=pm->origin[2];f.vx=pm->velocity[0];f.vy=pm->velocity[1];f.vz=pm->velocity[2];
    // client_analyzer reads pev_maxspeed (the player's cap). pm->maxspeed is
    // the shared movement cap before PM_CheckParameters applies clientmaxspeed.
    // Stats samples before movement, so using maxspeed here changes thresholds.
    f.maxSpeed=std::isfinite(pm->clientmaxspeed)&&pm->clientmaxspeed>0?pm->clientmaxspeed:pm->maxspeed;
    f.jumpTimer=pm->fuser2;
    f.dt=pm->frametime>0?pm->frametime:pm->cmd.msec*.001f;return f;
}
static void publish(Kind kind,int fog,bool hasFog,float speed,ULONGLONG tick){
    ++publication;result={kind,fog,speed,tick,hasFog};track.takeoff=speed;track.gain=0;track.flight=true;track.idealPending=false;track.idealAge=0;
}
static void observe(const Frame &before,const Frame &after,bool fresh,ULONGLONG tick){
    // runfuncs identifies new prediction commands. Equal timestamps are allowed:
    // they are not a reliable command identifier at high frame rates.
    if(!fresh)return;
    if(track.idealPending&&++track.idealAge>6)track.idealPending=false;
    if(!before.alive||!after.alive){reset();return;}
    float speed=std::hypot(after.vx,after.vy);
    if(!std::isfinite(speed)||!std::isfinite(after.vz)||!std::isfinite(before.dt)||before.dt<=0||before.dt>.25f||
       !std::isfinite(before.x)||!std::isfinite(before.y)||!std::isfinite(before.z)||!std::isfinite(after.x)||!std::isfinite(after.y)||!std::isfinite(after.z)||
       before.water>=2||after.water>=2){resetSequence();return;}
    if(std::hypot(after.x-before.x,after.y-before.y)>128||std::fabs(after.z-before.z)>128||
       (track.primed&&(std::hypot(before.x-track.x,before.y-track.y)>128||std::fabs(before.z-track.z)>128))){resetSequence();return;}
    if(before.type==MOVETYPE_FLY&&after.type==MOVETYPE_WALK&&!after.ground){
        resetSequence();publish(Ladder,0,false,speed,tick);track.primed=true;track.x=after.x;track.y=after.y;track.z=after.z;return;
    }
    if(before.type!=MOVETYPE_WALK||after.type!=MOVETYPE_WALK){resetSequence();return;}
    if(!track.primed){track.primed=true;track.ground=before.ground;track.frames=before.ground?1:0;track.previousButtons=before.oldbuttons;}
    // Count completed movement commands, not the pre-prediction ground flag:
    // the latter can remain set after departure and replay a false takeoff.
    bool previousGround=track.ground;
    bool pressDuck=(before.buttons&IN_DUCK)&&!(track.previousButtons&IN_DUCK);
    bool releaseDuck=!(before.buttons&IN_DUCK)&&((track.previousButtons|before.oldbuttons)&IN_DUCK);
    // Client hull changes can report the departure on the crouch press rather
    // than on standing up. Keep it provisional until a short release confirms
    // an actual duck cycle; never classify a held crouch from the press alone.
    if(track.duckCandidate.kind!=None){
        track.candidateAge+=before.dt;
        if(track.candidateAge>.25f||(before.buttons&IN_JUMP)){track.duckCandidate={};}
        else if(releaseDuck&&!after.crouched){
            ++publication;result=track.duckCandidate;result.kind=track.candidateStand?StandDuck:Duck;
            track.idealPending=false;track.idealAge=0;
            track.takeoff=result.pre;track.gain=after.ground?0:speed-result.pre;track.flight=!after.ground;
            ++track.ducks;track.duckStand|=track.candidateStand;track.duckCandidate={};
        }
    }
    if(pressDuck){track.duckArmed=true;track.duckAge=0;}
    if(track.duckArmed){track.duckAge+=before.dt;if(track.duckAge>.25f)track.duckArmed=false;}
    if(after.ground){
        if(!track.ground){track.frames=1;track.hasLanding=true;track.landCrouched=before.crouched||after.crouched;}
        else track.frames=std::min(10000,track.frames+1);
        track.flight=false;track.age=0;
        // JumpStats treats a long ground run as a new prestrafe, not a bhop.
        if(track.frames>10){track.ducks=0;track.duckStand=false;track.hasLanding=false;track.idealPending=false;}
    }else{
        if(previousGround){
            int fog=track.frames;
            bool jump=(before.buttons&IN_JUMP)&&after.vz>0;
            bool duck=releaseDuck&&track.duckArmed&&!after.crouched&&!jump;
            if(jump){
                Kind kind=Jump;bool shortContact=track.hasLanding&&fog<=10;
                if(shortContact&&track.ducks){kind=track.duckStand?(track.ducks>1?MultiStandCountJump:StandCountJump):(track.ducks>1?MultiCountJump:CountJump);}
                else if(shortContact){kind=track.landCrouched?StandBhop:Bhop;}
                // An ordinary jump after a ground run is not a bhop FOG. Keep
                // takeoff speed/air gain, but do not label the entire LJ run-up
                // as e.g. FOG 106. Do not clamp real short-contact counts.
                bool ideal=shortContact&&kind!=Jump&&track.idealPending;
                publish(kind,fog,shortContact,speed,tick);result.ideal=ideal;track.idealPending=false;track.ducks=0;track.duckStand=false;
            }else if(duck){
                bool stand=track.hasLanding&&track.landCrouched;track.duckStand|=stand;
                ++track.ducks;publish(stand?StandDuck:Duck,fog,fog<=10,speed,tick);
            }else{
                if(track.duckArmed&&(before.buttons&IN_DUCK)&&(before.crouched||after.crouched)){
                    track.duckCandidate={Duck,fog,speed,tick,fog<=10};track.candidateAge=0;track.candidateStand=track.hasLanding&&track.landCrouched;
                }
                track.ducks=0;track.duckStand=false;track.flight=false;
            }
            track.frames=0;track.hasLanding=false;track.duckArmed=false;
        }else if(track.flight){track.gain=speed-track.takeoff;}
        track.age+=before.dt;
    }
    track.ground=after.ground;track.previousButtons=before.buttons;track.x=after.x;track.y=after.y;track.z=after.z;
}
static void save(){
    char selected[12];snprintf(selected,sizeof(selected),"%d",style);WritePrivateProfileStringA("interface","movement_style",selected,iniPath);
    const char *keys[]={"movement_hud","movement_gain","movement_keep","movement_hold_ms","movement_rgb_0","movement_rgb_1","movement_rgb_2"};
    int values[]={enabled,showGain,keep,holdMs,accent[0],accent[1],accent[2]};
    for(int i=0;i<7;++i){char text[24];snprintf(text,sizeof(text),"%d",values[i]);WritePrivateProfileStringA("interface",keys[i],text,iniPath);}
    for(int i=0;i<3;++i){char key[32],text[24];snprintf(key,sizeof(key),"movement_late_rgb_%d",i);snprintf(text,sizeof(text),"%d",lateAccent[i]);WritePrivateProfileStringA("interface",key,text,iniPath);}
    WritePrivateProfileStringA("interface","movement_video_effects",nullptr,iniPath);
    char duration[16];snprintf(duration,sizeof(duration),"%d",animationMs);WritePrivateProfileStringA("interface","movement_animation_ms",duration,iniPath);
    for(int i=0;i<3;++i){char key[40],text[16];snprintf(key,sizeof(key),"movement_perfect_rgb_%d",i);snprintf(text,sizeof(text),"%d",perfectRgb[i]);WritePrivateProfileStringA("interface",key,text,iniPath);}
}
static void defaults(){style=Classic;enabled=showGain=true;keep=false;holdMs=5000;accent[0]=134;accent[1]=220;accent[2]=159;lateAccent[0]=245;lateAccent[1]=92;lateAccent[2]=105;animationMs=850;perfectRgb[0]=90;perfectRgb[1]=245;perfectRgb[2]=145;}
static void load(){auto get=[](const char *key,int value,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,value,iniPath)),lo,hi);};
    style=get("movement_style",Classic,0,StyleCount-1);
    enabled=get("movement_hud",1,0,1);showGain=get("movement_gain",1,0,1);keep=get("movement_keep",0,0,1);holdMs=get("movement_hold_ms",5000,1000,15000);
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"movement_rgb_%d",i);accent[i]=get(key,i==0?134:i==1?220:159,0,255);}
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"movement_late_rgb_%d",i);lateAccent[i]=get(key,i==0?245:i==1?92:105,0,255);}
    animationMs=get("movement_animation_ms",850,300,1600);
    for(int i=0;i<3;++i){char key[40];snprintf(key,sizeof(key),"movement_perfect_rgb_%d",i);perfectRgb[i]=get(key,i==0?90:i==1?245:145,0,255);}
}
}
