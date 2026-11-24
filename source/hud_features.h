#pragma once
#include <cfloat>
#include <deque>
#include <memory>
#include <sstream>
#include <unordered_map>
#include "cvardef.h"
#include "hde32.h"
#ifdef clamp
#undef clamp
#endif

namespace Hud {
struct Theme { const char *name; int rgb[3],second[3]; };
static const Theme themes[]={
    {"Aurora",{112,171,255},{159,99,255}}, {"Crimson",{245,77,100},{255,142,58}},
    {"Mint",{85,255,45},{40,220,172}}, {"Violet",{174,104,255},{255,94,203}},
    {"Sunset",{255,155,69},{255,83,105}}, {"Ocean",{89,199,246},{87,146,255}},
    {"Mono",{225,235,239},{131,149,157}}, {"Toxic",{175,245,54},{35,189,117}}
};
static const char *looks[]={"Frame","Instrument","Soft","Tiles","Minimal","Terminal","Amber"};
static int theme=4,look=6,opacity=100,range=550,radarSizePercent=0,radarArrowPercent=60,option,historyOffset;
static int legacyPalette=4;
static bool customHud=true,customLegacy=false;
static int hudRgb[3]={215,96,20},legacyRgb[3]={255,155,69};
static int chatBorderRgb[3]={118,176,46};
static bool radarBorderOn=true;
static int radarBorderRgb[3]={30,45,48},radarBorderWidth=2,radarBorderOpacity=100;
static bool smokeTintOn=false;
static int smokeRgb[3]={40,190,255};
static bool flashTintOn=false;
static int flashRgb[3]={36,40,52};   // dark by default: the point is to stop the bright white glare
static const Theme &hudTheme(){static Theme value;value=themes[theme];if(customHud){value.name="Custom";for(int i=0;i<3;++i)value.rgb[i]=hudRgb[i];}return value;}
static const Theme &legacyTheme(){static Theme value;value=themes[legacyPalette];if(customLegacy){value.name="Custom";for(int i=0;i<3;++i)value.rgb[i]=legacyRgb[i];}return value;}
static int historySelected,detailScroll,historyRows=6;
static uint64_t chatSerial;
static bool historyDetails;
static bool chatOpened;static int chatScroll,chatScrollLimit;
static bool chatOn=true,chatPresentationReady=false,radarOn=true,radar3D=true,watermarkOn=true,inMenu;
static bool radarCentered=false;
static bool radarInsideOnly=true;   // in a building the radar shows only that building
static int radarCenteredZoom=100;
static int utcOffset=180;
static bool systemTime=false;
static int team[33];
struct RadarDot { float xyz[3]={}; ULONGLONG tick=0; bool dead=false; };
static RadarDot dots[33];
struct ChatLine { std::string date,nick,map,body; int side=0; ULONGLONG tick=0;uint64_t serial=0; std::string colored;bool serverMessage=false,deadMessage=false,radioMessage=false; };
static std::deque<ChatLine> history;
static std::string pendingLocalBody,pendingLocalNick;static ULONGLONG pendingLocalTick;
static constexpr size_t maxHistory=2000;
static pfnUserMsgHook oldSay,oldText,oldRadar,oldTeam,oldAttrib,oldReset;
static bool directSayInstalled,directTextInstalled,directRadarInstalled,directTeamInstalled;
static ULONGLONG hookCheck;
static bool hookLogged;
static int registryAttempts;
static int lastRadarMode=-1;
static double fps;
static LARGE_INTEGER lastCounter,frequency;
static unsigned frameCount;
static GLuint fontBase;
static HGLRC fontContext;
static HDC fontDc;
static HFONT font;
static int fontLook=-1;
static bool localizationTried;
static std::unordered_map<std::string,std::string> localized;

static std::wstring wide(const std::string &s) {
    if(s.empty())return {};
    UINT code=CP_UTF8;int n=MultiByteToWideChar(code,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n){code=1251;n=MultiByteToWideChar(code,0,s.data(),static_cast<int>(s.size()),nullptr,0);}
    std::wstring result(n,L'\0');
    if(n)MultiByteToWideChar(code,0,s.data(),static_cast<int>(s.size()),result.data(),n);
    return result;
}
static std::string utf8(const std::wstring &s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string result(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),result.data(),n,nullptr,nullptr);return result;
}
static std::string clean(const std::string &s,size_t maximum=384) {
    auto w=wide(s);std::wstring out;
    for(wchar_t c:w)if(c>=32 && c!=127){out+=c;if(out.size()>=maximum)break;}
    return utf8(out);
}
static std::string trim(const std::string &s,int count) {
    auto w=wide(s);if(static_cast<int>(w.size())>count){w.resize(std::max(1,count-3));w+=L"...";}return utf8(w);
}
// Preserve native color controls while converting each text segment to UTF-8.
static std::string chatColored(const std::string &s) {
    std::string out,part;auto flush=[&]{out+=clean(part,1024);part.clear();};
    for(unsigned char c:s){if(c>=1&&c<=4){flush();out+=static_cast<char>(c);}else if(c=='\n'||c=='\r'||c=='\t')part+=' ';else if(c>=32&&c!=127)part+=static_cast<char>(c);}flush();return out;
}
static std::string hexChat(const std::string &s){static const char digits[]="0123456789abcdef";std::string out;for(unsigned char c:s){out+=digits[c>>4];out+=digits[c&15];}return out;}
static std::string unhexChat(const std::string &s){if(s.size()%2||s.size()>8192)return {};std::string out;auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};for(size_t i=0;i<s.size();i+=2){int a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)return {};out+=static_cast<char>((a<<4)|b);}return chatColored(out);}
struct ChatSpan {std::string text;int code=1;};
static std::vector<ChatSpan> chatSpans(const ChatLine &line){
    std::string text=line.colored;if(text.empty())text=(line.nick=="Server"||line.nick.empty())?line.body:std::string(1,char(3))+line.nick+char(1)+" : "+line.body;
    std::vector<ChatSpan> spans;std::string part;int code=1;bool implicitName=false;
    auto flush=[&]{if(!part.empty()){spans.push_back({part,code});part.clear();}};
    for(unsigned char c:text){if(c>=1&&c<=4){flush();implicitName=c==2;code=c==2?3:c;}else if(implicitName&&c==':'){flush();implicitName=false;code=1;part+=':';}else part+=static_cast<char>(c);}flush();return spans;
}
static void nativeChatRgb(int code,int side,int &r,int &g,int &b){
    if(code==3){if(side==1){r=255;g=64;b=64;}else if(side==2){r=153;g=204;b=255;}else r=g=b=204;return;}
    if(code==4){r=0;g=255;b=0;return;}
    r=255;g=180;b=30;const char *color=eng&&eng->pfnGetCvarString?eng->pfnGetCvarString(const_cast<char *>("con_color")):nullptr;
    if(color){int cr,cg,cb;if(sscanf(color,"%d %d %d",&cr,&cg,&cb)==3){r=std::clamp(cr,0,255);g=std::clamp(cg,0,255);b=std::clamp(cb,0,255);}}
}
template<class Measure> static std::vector<std::vector<ChatSpan>> wrapChatSpans(const ChatLine &line,float maximum,Measure measure){
    struct Cached {float width,metric;std::string text;std::vector<std::vector<ChatSpan>> rows;};static std::unordered_map<uint64_t,Cached> cache;
    float metric=measure("MW09");
    std::string signature=line.colored.empty()?line.nick+"\n"+line.body:line.colored;
    auto found=cache.find(line.serial);if(line.serial&&found!=cache.end()&&found->second.width==maximum&&found->second.metric==metric&&found->second.text==signature)return found->second.rows;
    std::vector<std::vector<ChatSpan>> rows(1);float used=0;maximum=std::max(30.f,maximum);
    for(const auto &span:chatSpans(line)){auto value=wide(span.text);for(size_t i=0;i<value.size();++i){std::wstring glyph(1,value[i]);if(value[i]>=0xD800&&value[i]<=0xDBFF&&i+1<value.size())glyph+=value[++i];std::string s=utf8(glyph);float advance=measure(s);
        if(used+advance>maximum&&used>0){rows.emplace_back();used=0;if(s==" ")continue;}
        auto &row=rows.back();if(row.empty()||row.back().code!=span.code)row.push_back({s,span.code});else row.back().text+=s;used+=advance;
    }}if(line.serial){if(cache.size()>2100)cache.clear();cache[line.serial]={maximum,metric,std::move(signature),rows};}return rows;
}
static std::vector<std::string> wrap(const std::string &s,int count) {
    std::wstring remaining=wide(s);std::vector<std::string> lines;count=std::max(12,count);
    while(!remaining.empty()) {
        size_t length=std::min(static_cast<size_t>(count),remaining.size());
        if(length<remaining.size()){size_t space=remaining.rfind(L' ',length);if(space!=std::wstring::npos&&space>length/2)length=space;}
        lines.push_back(utf8(remaining.substr(0,length)));remaining.erase(0,length);while(!remaining.empty()&&remaining.front()==L' ')remaining.erase(0,1);
    }return lines;
}
static SYSTEMTIME now() {
    return InterfaceClock::now(utcOffset,systemTime);
}
static std::string mapName() {
    const char *raw=eng->pfnGetLevelName?eng->pfnGetLevelName():nullptr;
    if(!raw || !*raw)return "menu";
    std::string name=raw;size_t slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);
    size_t dot=name.find_last_of('.');if(dot!=std::string::npos)name.resize(dot);
    return clean(name,48);
}
// Set while our menu is drawn from SwapBuffers in the game's main menu.
static bool outsideGame;
static int localIndex() {
    // Without a loaded level the client entity array does not exist yet and
    // GetLocalPlayer() returns an invalid pointer, so never touch it there.
    const char *level=eng->pfnGetLevelName?eng->pfnGetLevelName():nullptr;
    if(outsideGame||!level||!*level)return 0;
    auto *entity=eng->GetLocalPlayer?eng->GetLocalPlayer():nullptr;
    if(entity && entity->index>=1 && entity->index<=32)return entity->index;
    if(eng->pfnGetPlayerInfo)for(int i=1;i<=32;++i){hud_player_info_t info={};eng->pfnGetPlayerInfo(i,&info);if(info.thisplayer)return i;}
    return 0;
}
static std::string playerName(int index) {
    hud_player_info_t info={};if(index>0 && index<=32 && eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(index,&info);
    return info.name?clean(info.name,48):index?"Player "+std::to_string(index):"Server";
}
static void push(ChatLine line,bool persist) {
    if(line.body.empty())return;
    if(line.body.rfind("#Spec_",0)==0)return;
    line.serial=++chatSerial;history.push_back(line);if(history.size()>maxHistory)history.pop_front();
    if(chatOpened && chatScroll>0)chatScroll=std::min(static_cast<int>(history.size())-1,chatScroll+1);
    if(historyOffset || historyDetails)historyOffset=std::min(static_cast<int>(history.size())-1,historyOffset+1);
    // Chat history exists only in memory; never save message bodies or identities.

}
static void record(int index,const std::string &nick,const std::string &body,const std::string &colored={},bool serverMessage=false,bool deadMessage=false,bool radioMessage=false) {
    auto time=now();char stamp[32];snprintf(stamp,sizeof(stamp),"%04u-%02u-%02u %02u:%02u:%02u",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
    ChatLine line={stamp,clean(nick,48),mapName(),clean(body,1024),index>=1 && index<=32?team[index]:0,GetTickCount64()};line.colored=chatColored(colored);line.serverMessage=serverMessage;line.deadMessage=deadMessage;line.radioMessage=radioMessage;push(std::move(line),true);
}
static void recordLocal(const std::string &body) {
    int index=localIndex();std::string nick=playerName(index);
    pendingLocalBody=body;pendingLocalNick=nick;pendingLocalTick=GetTickCount64();bool teamOnly=body.rfind("[Team] ",0)==0;
    record(index,nick,body,std::string(1,char(1))+(teamOnly?"(TEAM) ":"")+char(3)+nick+char(1)+" : "+(teamOnly?body.substr(7):body));
}
static bool isLocalEcho(int index,const std::string &nick,const std::string &body) {
    ULONGLONG tick=GetTickCount64();
    if(index==localIndex()&&tick>=pendingLocalTick&&tick-pendingLocalTick<5000&&nick==pendingLocalNick&&body==pendingLocalBody){
        pendingLocalBody.clear();pendingLocalNick.clear();pendingLocalTick=0;return true;
    }
    return false;
}
static void save() {
    WritePrivateProfileStringA("interface","clock_system_time",systemTime?"1":"0",iniPath);
    Speedometer::save();
    MovementHud::save();
    const char *keys[]={"theme","look","opacity","radar_range","chat","radar","watermark","utc_offset_minutes","radar_3d","radar_size_percent","smoke_color_enabled","radar_arrow_percent"};
    int values[]={theme,look,opacity,range,chatOn,radarOn,watermarkOn,utcOffset,radar3D,radarSizePercent,smokeTintOn,radarArrowPercent};char number[24];
    for(int i=0;i<12;++i){snprintf(number,sizeof(number),"%d",values[i]);WritePrivateProfileStringA("interface",keys[i],number,iniPath);}
    WritePrivateProfileStringA("interface","radar_centered",radarCentered?"1":"0",iniPath);
    WritePrivateProfileStringA("interface","radar_inside_only",radarInsideOnly?"1":"0",iniPath);
    snprintf(number,sizeof(number),"%d",radarCenteredZoom);WritePrivateProfileStringA("interface","radar_centered_zoom",number,iniPath);
    const char *borderKeys[]={"radar_border","radar_border_width","radar_border_opacity","radar_border_rgb_0","radar_border_rgb_1","radar_border_rgb_2"};
    int borderValues[]={radarBorderOn,radarBorderWidth,radarBorderOpacity,radarBorderRgb[0],radarBorderRgb[1],radarBorderRgb[2]};
    for(int i=0;i<6;++i){snprintf(number,sizeof(number),"%d",borderValues[i]);WritePrivateProfileStringA("interface",borderKeys[i],number,iniPath);}
    snprintf(number,sizeof(number),"%d",legacyPalette);WritePrivateProfileStringA("interface","legacy_palette",number,iniPath);
    WritePrivateProfileStringA("interface","custom_hud",customHud?"1":"0",iniPath);WritePrivateProfileStringA("interface","custom_legacy",customLegacy?"1":"0",iniPath);
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"hud_rgb_%d",i);snprintf(number,sizeof(number),"%d",hudRgb[i]);WritePrivateProfileStringA("interface",key,number,iniPath);
        snprintf(key,sizeof(key),"legacy_rgb_%d",i);snprintf(number,sizeof(number),"%d",legacyRgb[i]);WritePrivateProfileStringA("interface",key,number,iniPath);}
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"chat_border_rgb_%d",i);snprintf(number,sizeof(number),"%d",chatBorderRgb[i]);WritePrivateProfileStringA("interface",key,number,iniPath);}
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"smoke_rgb_%d",i);snprintf(number,sizeof(number),"%d",smokeRgb[i]);WritePrivateProfileStringA("interface",key,number,iniPath);}
    WritePrivateProfileStringA("interface","flash_color_enabled",flashTintOn?"1":"0",iniPath);
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"flash_rgb_%d",i);snprintf(number,sizeof(number),"%d",flashRgb[i]);WritePrivateProfileStringA("interface",key,number,iniPath);}
}
static void load(bool readHistory=true) {
    Speedometer::load();
    MovementHud::load();
    bool migrated=false;
    auto setting=[](const char *key,int fallback,int lo,int hi){return std::max(lo,std::min(hi,static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath))));};
    theme=setting("theme",4,0,7);look=setting("look",6,0,6);opacity=100;range=setting("radar_range",550,200,3000);
    radarBorderOn=setting("radar_border",1,0,1)!=0;
    radarBorderWidth=setting("radar_border_width",2,1,10);radarBorderOpacity=setting("radar_border_opacity",100,0,100);
    const int borderDefaults[3]={30,45,48};
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"radar_border_rgb_%d",i);radarBorderRgb[i]=setting(key,borderDefaults[i],0,255);}
    chatOn=setting("chat",1,0,1)!=0;
    if(setting("chat_revision",0,0,999)<33){chatOn=true;WritePrivateProfileStringA("interface","chat","1",iniPath);WritePrivateProfileStringA("interface","chat_revision","33",iniPath);}
    radarOn=setting("radar",1,0,1);radar3D=setting("radar_3d",1,0,1)!=0;radarSizePercent=setting("radar_size_percent",0,0,100);radarArrowPercent=setting("radar_arrow_percent",60,30,150);smokeTintOn=setting("smoke_color_enabled",0,0,1)!=0;watermarkOn=setting("watermark",1,0,1);utcOffset=setting("utc_offset_minutes",180,-720,840);systemTime=setting("clock_system_time",0,0,1)!=0;
    radarCentered=setting("radar_centered",0,0,1)!=0;radarCenteredZoom=setting("radar_centered_zoom",100,100,400);
    radarInsideOnly=setting("radar_inside_only",1,0,1)!=0;
    if(setting("style_revision",0,0,999)<11){theme=4;look=6;range=800;WritePrivateProfileStringA("interface","style_revision","11",iniPath);migrated=true;}
    if(setting("radar_zoom_revision",0,0,999)<13){range=550;WritePrivateProfileStringA("interface","radar_zoom_revision","13",iniPath);migrated=true;}
    legacyPalette=setting("legacy_palette",theme,0,7);customHud=setting("custom_hud",1,0,1);customLegacy=setting("custom_legacy",0,0,1);
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"hud_rgb_%d",i);hudRgb[i]=setting(key,hudRgb[i],0,255);snprintf(key,sizeof(key),"legacy_rgb_%d",i);legacyRgb[i]=setting(key,legacyRgb[i],0,255);}
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"chat_border_rgb_%d",i);chatBorderRgb[i]=setting(key,chatBorderRgb[i],0,255);}
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"smoke_rgb_%d",i);smokeRgb[i]=setting(key,smokeRgb[i],0,255);}
    flashTintOn=setting("flash_color_enabled",0,0,1)!=0;
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"flash_rgb_%d",i);flashRgb[i]=setting(key,flashRgb[i],0,255);}
    if(migrated)save();
    // Do not load or create transcripts from earlier releases.
}

struct Packet {
    const byte *data;int size,pos=0;bool valid=true;
    int byteValue(){if(pos>=size){valid=false;return 0;}return data[pos++];}
    int shortValue(){int a=byteValue(),b=byteValue();return static_cast<short>(a|(b<<8));}
    std::string string(){int first=pos;while(pos<size && data[pos])++pos;if(pos>=size){valid=false;return {};}std::string s(reinterpret_cast<const char *>(data+first),pos-first);++pos;return s;}
};
static std::string expanded(const std::string &,const std::vector<std::string> &);
static std::string expandedColored(const std::string &,const std::vector<std::string> &);
static bool formatOnly(const std::string &value){
    size_t first=value.find_first_not_of(" \t\r\n"),last=value.find_last_not_of(" \t\r\n");
    if(first==std::string::npos)return false;std::string token=value.substr(first,last-first+1);
    return token=="%s"||token=="%s1"||token=="%s2"||token=="%s3"||token=="%s4";
}
static bool sayPacket(int size,void *buffer,bool *recorded=nullptr) {
    if(recorded)*recorded=false;
    if(!buffer || size<2 || size>8192)return false;
    Packet p{static_cast<const byte *>(buffer),size};int sender=p.byteValue();std::string raw=chatColored(p.string()),text=clean(raw);if(!p.valid)return false;
    std::vector<std::string> args;while(p.pos<size && args.size()<4){args.push_back(p.string());if(!p.valid)return false;}
    std::string nick=sender>=1&&sender<=32?playerName(sender):"Server",body,colored;bool serverMessage=sender<1||sender>32,deadMessage=false;
    if(text.rfind("#Spec_",0)==0){
        // Some community servers use #Spec_PlayerItem as a localized "%s"
        // envelope for their complete, colored player-chat line. Only discard
        // actual item metadata, not a supplied "name : speech" string.
        for(const auto &arg:args){std::string candidate=chatColored(arg),plain=clean(candidate);size_t colon=plain.find(':');
            bool color=candidate.find_first_of(std::string("\x01\x02\x03\x04",4))!=std::string::npos;
            if(colon!=std::string::npos&&colon+1<plain.size()&&(color||plain.substr(0,colon).find(nick)!=std::string::npos)){
                colored=candidate;body=plain.substr(colon+1);size_t first=body.find_first_not_of(" ");body=first==std::string::npos?"":body.substr(first);
                std::string prefix=plain.substr(0,colon);deadMessage=prefix.find("*DEAD*")!=std::string::npos||prefix.find("[Dead]")!=std::string::npos;
                if(deadMessage)body="[Dead] "+body;break;
            }
        }
        if(colored.empty())return true;
    }else if(text.rfind("#Cstrike_Chat_",0)==0) {
        if(args.size()>=2){if(!args[0].empty()&&!formatOnly(args[0]))nick=clean(args[0]);
            // Native CS: name (often empty), speech, optional map location.
            body=args[1];}
        else if(!args.empty())body=args[0];else return false;
        std::string prefix;bool teamOnly=text.find("_CT")!=std::string::npos || text.find("_T_")!=std::string::npos || text=="#Cstrike_Chat_T";
        if(teamOnly)prefix="(TEAM) ";
        if(text.find("Dead")!=std::string::npos){prefix+="*DEAD* ";deadMessage=true;}
        if(text.find("Spec")!=std::string::npos)prefix+="(SPEC) ";
        std::string location=text.find("_Loc")!=std::string::npos&&args.size()>=3?std::string(" @ ")+char(4)+chatColored(args[2])+char(1):"";
        colored=std::string(1,char(1))+prefix+char(3)+nick+char(1)+location+" : "+chatColored(body);
        if(deadMessage)body="[Dead] "+clean(body);
        if(text.find("Spec")!=std::string::npos)body="[Spectator] "+body;
        if(teamOnly)body="[Team] "+body;
    } else {
        if(!text.empty() && text[0]=='#'){
            colored=expandedColored(text,args);body=clean(colored);if(body.empty())return false;nick="Server";serverMessage=true;
        }else{if(!args.empty()&&args[0].empty()&&sender>=1&&sender<=32)args[0]=nick;colored=args.empty()?raw:expandedColored(raw,args);body=clean(colored);}
        deadMessage=body.find("*DEAD*")!=std::string::npos || body.find("[Dead]")!=std::string::npos;std::string known=clean(nick);
        if(body.rfind(known,0)==0){body.erase(0,known.size());size_t first=body.find_first_not_of(" :");body=first==std::string::npos?"":body.substr(first);}
    }
    if(body.empty()||formatOnly(clean(body)))return false;
    if(!isLocalEcho(sender,nick,clean(body)))record(sender,nick,body,colored,serverMessage,deadMessage);
    else for(auto it=history.rbegin();it!=history.rend();++it)if(it->nick==nick&&it->body==clean(body)){it->colored=colored;it->side=sender>=1&&sender<=32?team[sender]:0;break;}
    if(recorded)*recorded=true;return true;
}
static int say(const char *name,int size,void *data){
    CallbackScope callbackScope;
    static bool receivedLogged=false;if(!receivedLogged){receivedLogged=true;logLine("Chat: received an incoming SayText packet from the engine.");}
    bool recorded=false,handled=sayPacket(size,data,&recorded);
    static unsigned rejects=0;if(!handled&&++rejects<=3){char message[96];snprintf(message,sizeof(message),"Chat: unrecognized SayText packet (%d bytes); delegated to native handler.",size);logLine(message);}
    bool metadata=false;
    if(data&&size>=2&&size<=8192){Packet packet{static_cast<const byte *>(data),size};packet.byteValue();std::string token=clean(packet.string());metadata=packet.valid&&token.rfind("#Spec_",0)==0;}
    if(chatOn && chatPresentationReady && handled && (recorded||metadata)){
        // Replacement chat is presentation-only. Do not start a sound for each
        // packet: server-replaced talk samples can replay unrelated audio here.
        return 1; // Only consume messages that were captured for the replacement chat.
    }
    return oldSay?oldSay(name,size,data):1;
}
static void loadLocalization() {
    if(localizationTried)return;localizationTried=true;
    auto bytes=Preview::read("resource/cstrike_english.txt");if(bytes.empty())return;
    std::string text;
    if(bytes.size()>=2&&bytes[0]==0xFF&&bytes[1]==0xFE){std::wstring value((bytes.size()-2)/2,L'\0');memcpy(value.data(),bytes.data()+2,value.size()*2);text=utf8(value);}
    else text=std::string(bytes.begin(),bytes.end());
    std::istringstream file(text);std::string line;
    while(std::getline(file,line)) {
        size_t p=0;
        auto quoted=[&]() {std::string result;p=line.find('"',p);if(p==std::string::npos)return result;++p;
            while(p<line.size()){char c=line[p++];if(c=='"')break;if(c=='\\'&&p<line.size()){c=line[p++];if(c=='n'||c=='t'||c=='r')c=' ';}result+=c;}return result;};
        std::string key=quoted(),value=quoted();if(!key.empty()&&!value.empty())localized[key]=chatColored(value);
    }
}
static std::string translated(std::string token) {
    loadLocalization();
    for(int depth=0;depth<4 && !token.empty() && token[0]=='#';++depth){
        std::string key=token.substr(1);auto found=localized.find(key);
        if(found!=localized.end()){token=found->second;continue;}
        if(eng->pfnTextMessageGet){auto *m=eng->pfnTextMessageGet(key.c_str());if(m&&m->pMessage&&token!=m->pMessage){token=m->pMessage;continue;}}
        break;
    }return chatColored(token);
}
static std::string expandedColored(const std::string &format,const std::vector<std::string> &args){
    std::string value=translated(format);
    if(!format.empty() && format[0]=='#' && !value.empty() && value[0]=='#')return {};
    // Parse placeholders before inserting parameters, so user '%' text is literal.
    std::string result;size_t plain=0;
    for(size_t i=0;i<value.size();++i){
        if(value[i]=='%' && i+1<value.size() && value[i+1]=='s'){
            size_t index=plain++;size_t length=2;
            if(i+2<value.size() && value[i+2]>='1' && value[i+2]<='4'){index=value[i+2]-'1';length=3;}
            if(index>=args.size())return {};result+=translated(args[index]);i+=length-1;
        }else result+=value[i];
    }return chatColored(result);
}
static std::string expanded(const std::string &format,const std::vector<std::string> &args){return clean(expandedColored(format,args));}
static int textMessage(const char *name,int size,void *data) {
    CallbackScope callbackScope;
    if(data && size>=2 && size<=8192){Packet p{static_cast<const byte *>(data),size};int destination=p.byteValue(),sender=0;std::string format=chatColored(p.string());
        // CS radio destination 5 has a sender-index STRING before its format token.
        if(destination==5){sender=atoi(format.c_str());format=chatColored(p.string());}
        if((destination==3||destination==5) && p.valid){std::vector<std::string> args;while(p.pos<size && args.size()<4){args.push_back(p.string());if(!p.valid)break;}
            std::string token=clean(format);
            if(p.valid&&token.rfind("#Spec_",0)==0&&chatOn&&chatPresentationReady)return 1;
            if(p.valid){bool radioMessage=destination==5||token=="#Game_radio"||token=="#Game_radio_location";std::string colored,nick="Server",body;
                if(radioMessage&&args.size()>=2&&(token=="#Game_radio"||token=="#Game_radio_location")){
                    nick=clean(args[0]);size_t radioIndex=token=="#Game_radio_location"?2:1;std::string radio=radioIndex<args.size()?translated(args[radioIndex]):"";
                    if(!radio.empty()&&radio[0]!='#'){std::string location=token=="#Game_radio_location"&&args.size()>=3?" @ "+translated(args[1]):"";
                        colored=std::string(1,char(3))+nick+char(1)+location+" (RADIO): "+radio;body="[RADIO] "+clean(radio);}
                }else{colored=expandedColored(format,args);body=clean(colored);if(radioMessage&&!args.empty())nick=clean(args[0]);}
                if(body.empty()||body[0]=='#'||formatOnly(body))return oldText?oldText(name,size,data):1;
                record(sender,nick,body,colored,!radioMessage,false,radioMessage);
                if(chatOn&&chatPresentationReady&&oldSay)return 1;
            }
        }
    }return oldText?oldText(name,size,data):1;
}
static int radar(const char *name,int size,void *data) {
    CallbackScope callbackScope;
    static bool logged=false;if(data && size==7 && !logged){logged=true;logLine("Radar: received native position packet (7 bytes).");}
    if(data && size==7){Packet p{static_cast<const byte *>(data),size};int index=p.byteValue();float xyz[3];for(float &c:xyz)c=p.shortValue()/8.f;
        if(index>=1 && index<=32 && p.valid){memcpy(dots[index].xyz,xyz,sizeof(xyz));dots[index].tick=GetTickCount64();}}
    return oldRadar?oldRadar(name,size,data):1;
}
static int teamInfo(const char *name,int size,void *data) {
    CallbackScope callbackScope;
    static bool logged=false;if(data && size>=2 && !logged){logged=true;logLine("Radar: received native TeamInfo packet.");}
    if(data&&size>=2&&size<128){Packet p{static_cast<const byte *>(data),size};int index=p.byteValue();std::string side=p.string();
        if(p.valid&&index>=1&&index<=32)team[index]=side=="TERRORIST"?1:side=="CT"?2:0;}
    return oldTeam?oldTeam(name,size,data):1;
}
static int attrib(const char *name,int size,void *data) {
    CallbackScope callbackScope;
    if(data&&size==2){auto *p=static_cast<byte *>(data);if(p[0]>=1&&p[0]<=32)dots[p[0]].dead=(p[1]&1)!=0;}
    return oldAttrib?oldAttrib(name,size,data):1;
}
static int reset(const char *name,int size,void *data){CallbackScope callbackScope;Speedometer::reset();MovementHud::reset();for(auto &dot:dots)dot=RadarDot{};return oldReset?oldReset(name,size,data):1;}
// Flashbang colour: the message tells a flashbang from other fades (in any colour the server
// sends); the new fade is coloured before its first frame is drawn.
static pfnUserMsgHook oldFade;static bool directFadeInstalled;
static int fade(const char *name,int size,void *data){CallbackScope callbackScope;const bool active=ready&&!unloading;if(active)::FlashTint::message(data,size);
    int result=oldFade?oldFade(name,size,data):1;if(active)::FlashTint::apply();return result;}
struct UserMsg {int id,size;char name[16];UserMsg *next;pfnUserMsgHook callback;};
static bool readable(const void *ptr,size_t count) {
    MEMORY_BASIC_INFORMATION info;uintptr_t address=reinterpret_cast<uintptr_t>(ptr);
    return ptr && VirtualQuery(ptr,&info,sizeof(info)) && info.State==MEM_COMMIT && !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
        address+count>=address && address+count<=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
}
static bool executable(const void *ptr){MEMORY_BASIC_INFORMATION info={};return ptr&&VirtualQuery(ptr,&info,sizeof(info))&&info.State==MEM_COMMIT&&!(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
static bool validMessageHead(UserMsg **head,HMODULE hw){
    MEMORY_BASIC_INFORMATION info={};if(!readable(head,sizeof(*head))||!VirtualQuery(head,&info,sizeof(info))||info.AllocationBase!=hw)return false;
    bool foundSay=false,foundText=false;UserMsg *node=*head;
    for(int i=0;node&&i<512;++i){if(!readable(node,sizeof(*node))||!memchr(node->name,0,sizeof(node->name)))return false;
        if(!strcmp(node->name,"SayText")&&executable(reinterpret_cast<void *>(node->callback)))foundSay=true;
        if(!strcmp(node->name,"TextMsg")&&executable(reinterpret_cast<void *>(node->callback)))foundText=true;
        if(foundSay&&foundText)return true;if(node->next==node)return false;node=node->next;}
    return false;
}
static UserMsg **messageList() {
    HMODULE hw=GetModuleHandleA("hw.dll");auto *entry=reinterpret_cast<byte *>(eng->pfnHookUserMsg);
    if(!hw || !entry)return nullptr;
    static UserMsg **cached; if(cached&&validMessageHead(cached,hw))return cached;
    // Follow engine wrappers, including tail jumps, and accept absolute loads
    // into any x86 register. Validate the linked list rather than one opcode.
    std::vector<byte *> pending{entry},seen;
    for(size_t cursor=0;cursor<pending.size()&&cursor<24;++cursor){byte *code=pending[cursor];if(!executableInModule(code,hw)||std::find(seen.begin(),seen.end(),code)!=seen.end())continue;seen.push_back(code);
        for(int offset=0;offset<256;){if(!readable(code+offset,16))break;hde32s op={};int n=hde32_disasm(code+offset,&op);if(!n||(op.flags&F_ERROR))break;
            uintptr_t address=0;if(op.opcode==0xA1)address=op.imm.imm32;
            else if((op.opcode==0x8B||op.opcode==0x89||op.opcode==0xFF)&&(op.modrm&0xC7)==0x05)address=op.disp.disp32;
            if(address){auto **head=reinterpret_cast<UserMsg **>(address);if(validMessageHead(head,hw)){cached=head;return head;}}
            if(op.opcode==0xE8||op.opcode==0xE9){auto *target=code+offset+n+static_cast<int32_t>(op.imm.imm32);if(executableInModule(target,hw)&&pending.size()<64)pending.push_back(target);}
            else if(op.opcode==0xEB){auto *target=code+offset+n+static_cast<int8_t>(op.imm.imm8);if(executableInModule(target,hw)&&pending.size()<64)pending.push_back(target);}
            else if(op.opcode==0xFF&&(op.modrm==0x25||op.modrm==0x15)&&readable(reinterpret_cast<void *>(address),sizeof(void *))){byte *target=*reinterpret_cast<byte **>(address);if(executableInModule(target,hw)&&pending.size()<64)pending.push_back(target);}
            if(op.opcode==0xC3||op.opcode==0xC2||op.opcode==0xE9||op.opcode==0xEB||(op.opcode==0xFF&&op.modrm==0x25))break;offset+=n;
        }
    }return nullptr;
}
// A message slot gets a forwarder (see makeStub) rather than our function itself: a plugin
// that wraps the slot after us keeps a pointer that stays valid after F11.
struct SlotForward {pfnUserMsgHook ours;void *stub;void *volatile *slot;};
static SlotForward slotForwards[32];
static SRWLOCK slotForwardLock=SRWLOCK_INIT;
static pfnUserMsgHook slotFor(pfnUserMsgHook ours){
    AcquireSRWLockExclusive(&slotForwardLock);struct Guard {~Guard(){ReleaseSRWLockExclusive(&slotForwardLock);}} guard;
    for(auto &f:slotForwards){if(f.ours==ours)return reinterpret_cast<pfnUserMsgHook>(f.stub);
        if(!f.ours){void *volatile *slot=nullptr;void *stub=makeStub(reinterpret_cast<void *>(ours),slot);if(!stub)return ours;f={ours,stub,slot};return reinterpret_cast<pfnUserMsgHook>(stub);}}
    return ours;
}
// F11: the slot gets the engine's handler back where it still holds ours; where another plugin
// wrapped it, its copy of our forwarder now leads straight to the engine's handler.
static void restoreSlot(UserMsg *node,pfnUserMsgHook ours,pfnUserMsgHook old){
    for(auto &f:slotForwards)if(f.ours==ours)InterlockedExchangePointer(reinterpret_cast<void *volatile *>(f.slot),reinterpret_cast<void *>(old));
    if(node->callback==ours||node->callback==slotFor(ours))InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&node->callback),reinterpret_cast<void *>(old));
}
static void installMessageList(UserMsg *node,HMODULE client) {
    struct Hook {const char *name;pfnUserMsgHook replacement;pfnUserMsgHook *old;};
    Hook hooks[]={{"SayText",say,&oldSay},{"TextMsg",textMessage,&oldText},{"Radar",radar,&oldRadar},{"TeamInfo",teamInfo,&oldTeam},{"ScoreAttrib",attrib,&oldAttrib},{"ResetHUD",reset,&oldReset},{"ScreenFade",fade,&oldFade}};
    for(int walked=0;node && walked<512;++walked) {
        // Bytes following the first NUL are unused and need not be zeroed.
        if(!readable(node,sizeof(*node)) || !memchr(node->name,0,sizeof(node->name)))break;
        if(!chatOn&&(!strcmp(node->name,"SayText")||!strcmp(node->name,"TextMsg"))){node=node->next;continue;}
        for(auto &hook:hooks)if(!strcmp(node->name,hook.name)&&executable(reinterpret_cast<void *>(node->callback))&&
            (!*hook.old || (node->callback==*hook.old&&((hook.replacement==say&&directSayInstalled)||(hook.replacement==textMessage&&directTextInstalled))))) {
            // Prefer replacing the engine's callback slot for chat messages. This
            // catches short or already-wrapped handlers that MinHook cannot patch.
            bool chatMessage=!strcmp(hook.name,"SayText")||!strcmp(hook.name,"TextMsg");
            // The engine's own ScreenFade handler is only wrapped through its slot, never patched.
            bool radarMessage=!strcmp(hook.name,"Radar")||!strcmp(hook.name,"TeamInfo")||!strcmp(hook.name,"ScreenFade");
            MEMORY_BASIC_INFORMATION memory={};bool writable=VirtualQuery(&node->callback,&memory,sizeof(memory)) && (memory.Protect&(PAGE_READWRITE|PAGE_WRITECOPY));
            if((chatMessage||radarMessage)&&writable){
                *hook.old=node->callback;
                InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&node->callback),reinterpret_cast<void *>(slotFor(hook.replacement)));
                if(!strcmp(hook.name,"SayText"))directSayInstalled=true;
                else if(!strcmp(hook.name,"TextMsg"))directTextInstalled=true;
                else if(!strcmp(hook.name,"Radar"))directRadarInstalled=true;
                else if(!strcmp(hook.name,"ScreenFade"))directFadeInstalled=true;
                else directTeamInstalled=true;
            } else if(chatMessage || !radarMessage) {
                if(MH_CreateHook(reinterpret_cast<void *>(node->callback),reinterpret_cast<void *>(hook.replacement),reinterpret_cast<void **>(hook.old))==MH_OK)
                    if(MH_EnableHook(reinterpret_cast<void *>(node->callback))!=MH_OK){MH_RemoveHook(reinterpret_cast<void *>(node->callback));*hook.old=nullptr;}
            }
            if(*hook.old){char message[96];snprintf(message,sizeof(message),"Native %s capture installed.",hook.name);logLine(message);}
        }
        node=node->next;
    }
    if(oldSay&&!hookLogged){hookLogged=true;logLine("Chat replacement installed: SayText and chat TextMsg are consumed; non-chat native messages are preserved.");}
}
static void installMessages() {
    ULONGLONG tick=GetTickCount64();if(tick-hookCheck<1000)return;hookCheck=tick;
    UserMsg **head=messageList();if(!head){if(++registryAttempts==10)logLine("Native message registry not yet available; chat/radar keep native HUD until capture is ready.");return;}
    installMessageList(*head,GetModuleHandleA("client.dll"));
}
static bool restoreDirectMessages(){
    UserMsg **head=messageList();if(!head&&(directSayInstalled||directTextInstalled||directRadarInstalled||directTeamInstalled||directFadeInstalled))return false;
    bool sayFound=false,textFound=false,radarFound=false,teamFound=false,fadeFound=false;if(head){UserMsg *node=*head;
        for(int walked=0;node&&walked<512;++walked){if(!readable(node,sizeof(*node))||!memchr(node->name,0,sizeof(node->name)))break;
            if(!strcmp(node->name,"SayText")&&oldSay&&directSayInstalled){restoreSlot(node,say,oldSay);sayFound=true;}
            else if(!strcmp(node->name,"TextMsg")&&oldText&&directTextInstalled){restoreSlot(node,textMessage,oldText);textFound=true;}
            else if(!strcmp(node->name,"Radar")&&oldRadar&&directRadarInstalled){restoreSlot(node,radar,oldRadar);radarFound=true;}
            else if(!strcmp(node->name,"TeamInfo")&&oldTeam&&directTeamInstalled){restoreSlot(node,teamInfo,oldTeam);teamFound=true;}
            else if(!strcmp(node->name,"ScreenFade")&&oldFade&&directFadeInstalled){restoreSlot(node,fade,oldFade);fadeFound=true;}
            node=node->next;
        }
    }
    if(sayFound){directSayInstalled=false;oldSay=nullptr;}if(textFound){directTextInstalled=false;oldText=nullptr;}
    if(radarFound){directRadarInstalled=false;oldRadar=nullptr;}if(teamFound){directTeamInstalled=false;oldTeam=nullptr;}
    if(fadeFound){directFadeInstalled=false;oldFade=nullptr;}
    if(directSayInstalled||directTextInstalled||directRadarInstalled||directTeamInstalled||directFadeInstalled){char message[192];snprintf(message,sizeof(message),"Unload registry: unresolved SayText=%d TextMsg=%d Radar=%d TeamInfo=%d ScreenFade=%d",int(directSayInstalled),int(directTextInstalled),int(directRadarInstalled),int(directTeamInstalled),int(directFadeInstalled));logLine(message);return false;}
    if(lastRadarMode==1&&eng&&eng->pfnClientCmd)eng->pfnClientCmd(const_cast<char *>("drawradar\n"));lastRadarMode=-1;
    return true;
}
static void freeFont() {
    if(fontBase&&fontContext==wglGetCurrentContext())glDeleteLists(fontBase,512);
    if(fontDc){DeleteDC(fontDc);fontDc=nullptr;}if(font){DeleteObject(font);font=nullptr;}
    fontBase=0;fontContext=nullptr;
}
struct GL2D {
    GLint mode,unit=0x84C0;using Active=void(APIENTRY *)(GLenum);Active active=nullptr;
    // Looked up once per GL context instead of on every 2D pass.
    static inline HGLRC procContext=nullptr;static inline Active procActive=nullptr;static inline GLint procUnits=1;
    GL2D(int sw,int sh){
        glGetIntegerv(GL_MATRIX_MODE,&mode);glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
        HGLRC current=wglGetCurrentContext();
        if(current!=procContext){procContext=current;procUnits=1;procActive=reinterpret_cast<Active>(wglGetProcAddress("glActiveTextureARB"));
            if(reinterpret_cast<uintptr_t>(procActive)<=4||reinterpret_cast<uintptr_t>(procActive)==static_cast<uintptr_t>(-1))procActive=nullptr;
            else glGetIntegerv(0x84E2,&procUnits);}
        active=procActive;
        if(active) {
            glGetIntegerv(0x84E0,&unit);
            for(int i=1;i<std::min(procUnits,16);++i){active(0x84C0+i);glDisable(GL_TEXTURE_2D);}active(0x84C0);
        }
        glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);glDisable(GL_SCISSOR_TEST);
        // Game rendering may leave stipple enabled; force a continuous chat/menu outline.
        glDisable(GL_LINE_STIPPLE);glDisable(GL_POLYGON_STIPPLE);glLineWidth(1.0f);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,sw,sh,0,-1,1);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    }
    ~GL2D(){glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glPopClientAttrib();glPopAttrib();if(active)active(unit);glMatrixMode(mode);}
};
struct FontCache {GLuint base=0;HGLRC context=nullptr;HDC dc=nullptr;HFONT face=nullptr;int style=-1;};
static FontCache chatFontCache,timeFontCache;static bool chatFontActive,timeFontActive;
struct ChatFontScope {
    ChatFontScope(){swap();chatFontActive=true;}
    ~ChatFontScope(){swap();chatFontActive=false;}
    void swap(){std::swap(fontBase,chatFontCache.base);std::swap(fontContext,chatFontCache.context);std::swap(fontDc,chatFontCache.dc);std::swap(font,chatFontCache.face);std::swap(fontLook,chatFontCache.style);}
};
struct TimeFontScope {
    TimeFontScope(){swap();timeFontActive=true;}~TimeFontScope(){swap();timeFontActive=false;}
    void swap(){std::swap(fontBase,timeFontCache.base);std::swap(fontContext,timeFontCache.context);std::swap(fontDc,timeFontCache.dc);std::swap(font,timeFontCache.face);std::swap(fontLook,timeFontCache.style);}
};
static bool ensureFont() {
    if(!wglGetCurrentContext())return false;
    int wanted=timeFontActive?3:chatFontActive?2:look==5?1:0;
    if(fontBase && fontContext==wglGetCurrentContext() && fontLook==wanted)return true;
    freeFont();fontContext=wglGetCurrentContext();fontLook=wanted;
    fontDc=CreateCompatibleDC(nullptr);if(!fontDc)return false;
    font=CreateFontW(fontLook==3?-12:fontLook==2?-16:-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,fontLook==1?L"Consolas":L"Tahoma");
    if(!font)return false;SelectObject(fontDc,font);fontBase=glGenLists(512);
    if(!fontBase || !wglUseFontBitmapsW(fontDc,0,256,fontBase) || !wglUseFontBitmapsW(fontDc,0x400,256,fontBase+256)){freeFont();return false;}return true;
}
static int width(const std::string &s) {
    auto value=wide(s);if(ensureFont()){SIZE size={};GetTextExtentPoint32W(fontDc,value.data(),static_cast<int>(value.size()),&size);return size.cx;}return static_cast<int>(value.size())*8;
}
static std::vector<std::string> wrapPixels(const std::string &s,int pixels) {
    if(!ensureFont())return wrap(s,pixels/8);
    std::wstring rest=wide(s);std::vector<std::string> result;
    while(!rest.empty()) {
        int fit=0;SIZE extent={};GetTextExtentExPointW(fontDc,rest.data(),static_cast<int>(rest.size()),pixels,&fit,nullptr,&extent);
        size_t count=std::max(1,fit);if(count<rest.size()){size_t space=rest.rfind(L' ',count);if(space!=std::string::npos&&space>count/2)count=space;}
        result.push_back(utf8(rest.substr(0,count)));rest.erase(0,count);while(!rest.empty()&&rest.front()==L' ')rest.erase(0,1);
    }return result;
}
static std::string fitPixels(const std::string &s,int pixels) {
    auto lines=wrapPixels(s,pixels);return lines.empty()?"":lines.front();
}
static void colorText(int x,int y,const std::string &s,int r,int g,int b,int alpha=255) {
    SCREENINFO screen={};screen.iSize=sizeof(screen);
    if(wglGetCurrentContext() && eng->pfnGetScreenInfo && eng->pfnGetScreenInfo(&screen)) {
        GL2D state(screen.iWidth,screen.iHeight);if(ensureFont()) {
            glColor4ub(r,g,b,alpha);glRasterPos2i(x,y+12);
            for(wchar_t c:wide(s)){GLuint glyph=c<256?fontBase+c:c>=0x400&&c<0x500?fontBase+256+c-0x400:fontBase+'?';glCallList(glyph);}return;
        }
    }
    eng->pfnDrawSetTextColor(r/255.f,g/255.f,b/255.f);eng->pfnDrawConsoleString(x,y,const_cast<char *>(s.c_str()));
}
static void paint(int x,int y,const char *s,bool selected) {
    const auto &t=inMenu?legacyTheme():hudTheme();
    if(inMenu){colorText(x+2,y+2,s,3,5,8);colorText(x+1,y+1,s,selected?t.rgb[0]/3:65,selected?t.rgb[1]/3:70,selected?t.rgb[2]/3:78);}
    colorText(x,y,s,selected?t.rgb[0]:238,selected?t.rgb[1]:242,selected?t.rgb[2]:247);
}
static void circle(float cx,float cy,float radius,int r,int g,int b,int a,bool outline=false) {
    glColor4ub(r,g,b,a);glBegin(outline?GL_LINE_LOOP:GL_TRIANGLE_FAN);if(!outline)glVertex2f(cx,cy);
    int segments=HudResolution::segments(radius);for(int i=0;i<=segments;++i){float angle=i*6.2831853f/segments;glVertex2f(cx+std::cos(angle)*radius,cy+std::sin(angle)*radius);}glEnd();
}
struct Overview {std::string map;HGLRC context=nullptr;GLuint texture=0;float zoom=1,x=0,y=0;bool rotated=false,generated=false,isometric=false,mode3D=false;float minX=0,maxX=0,minY=0,maxY=0;float u0=0,v0=0,u1=1,v1=1;ULONGLONG attempted=0;};
static Overview overview;
#include "radar_bsp.h"
static bool overviewData(const std::vector<byte> &image,std::vector<byte> &rgba,int &w,int &h) {
    if(image.size()<54)return false;BITMAPFILEHEADER file;BITMAPINFOHEADER info;
    memcpy(&file,image.data(),sizeof(file));memcpy(&info,image.data()+sizeof(file),sizeof(info));
    if(file.bfType!=0x4D42 || info.biSize<40 || info.biSize>image.size()-14 || info.biWidth<1 || info.biWidth>4096 || info.biHeight==0 || info.biHeight<-4096 || info.biHeight>4096 ||
       info.biCompression!=BI_RGB || (info.biBitCount!=8&&info.biBitCount!=24&&info.biBitCount!=32))return false;
    w=info.biWidth;h=std::abs(info.biHeight);size_t stride=((static_cast<size_t>(w)*info.biBitCount+31)/32)*4;
    if(file.bfOffBits>image.size() || stride*h>image.size()-file.bfOffBits)return false;
    int colors=info.biClrUsed?info.biClrUsed:256;size_t table=14+info.biSize;
    if(info.biBitCount==8 && (colors<1||colors>256||table+colors*4>image.size()||table+colors*4>file.bfOffBits))return false;
    rgba.resize(static_cast<size_t>(w)*h*4);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){size_t source=file.bfOffBits+(info.biHeight>0?h-1-y:y)*stride+x*(info.biBitCount/8);
        if(info.biBitCount==8){int index=image[source];if(index>=colors)return false;source=table+index*4;}
        size_t dst=(static_cast<size_t>(y)*w+x)*4;int b=image[source],g=image[source+1],r=image[source+2];
        rgba[dst]=r;rgba[dst+1]=g;rgba[dst+2]=b;rgba[dst+3]=r<20&&g>220&&b<20?0:255;
    }return true;
}
// ---- Radar texture: built on a worker thread, uploaded a little per frame ------------
// Building the 2048x2048 map from the BSP (textures, props, mipmaps) used to run on the
// render thread on every map start, on every HUD_VidInit and again every 3 seconds when
// a map could not be built, freezing the game for up to seconds each time.
namespace RadarJob {
struct Image {bool ok=false,missingBsp=false,generated=false,isometric=false,rotated=false;float zoom=1,x=0,y=0,minX=0,maxX=0,minY=0,maxY=0,u0=0,v0=0,u1=1,v1=1;
    std::vector<std::vector<byte>> levels;std::vector<int> widths,heights;
    std::shared_ptr<BspRadar::Scene> scene;std::vector<int16_t> top;};   // top-down maps: parsed map and roof heights (2048x2048)
static HANDLE thread;
static std::atomic<bool> finished{false};
static std::string jobMap;static bool job3D;static int jobCap=2048;static std::vector<byte> jobBsp;
static Image result;
static std::shared_ptr<Image> image;static std::string imageMap;static bool image3D;   // finished pixels, kept across HUD_VidInit
static std::string failedMap;static bool failed3D;static int failures;static ULONGLONG failedTick;static bool engineRead;
static void build(Image &out);
static DWORD WINAPI run(void *){Preview::diskOnly=true;Image out;build(out);result=std::move(out);finished.store(true,std::memory_order_release);return 0;}
static void start(const std::string &map,bool mode3D,int cap,std::vector<byte> bsp){
    BspRadar::cancel=false;jobMap=map;job3D=mode3D;jobCap=cap;jobBsp=std::move(bsp);result=Image{};finished=false;
    thread=CreateThread(nullptr,0,run,nullptr,0,nullptr);if(thread)SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL);   // never ahead of the game
    if(!thread){failedMap=map;failed3D=mode3D;++failures;failedTick=GetTickCount64();}
}
// Plugin unload: the builder thread runs this DLL's code.
static void stop(){
    if(thread){BspRadar::cancel=true;WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}
    finished=false;BspRadar::cancel=false;result=Image{};jobBsp.clear();image.reset();
}
}
// ---- Radar inside buildings ----------------------------------------------------------
// The map texture shows the highest surface everywhere, so inside a building the radar
// showed its roof. A window of the map around the player is rasterised again on a worker
// thread, keeping only what is below head height. Every roofed area in it (a building,
// a canopy) gets its own small texture of the floor, crates and props under its roof.
// The window is kept up to date outdoors too and the buildings next to the player are
// uploaded in advance, so the inside is shown the moment the player walks in; while a
// newer window is prepared (other floor, a crate, a step) the shown one stays.
namespace RadarPatch {
static constexpr int size=1024;            // window side in map pixels
static constexpr float aboveFeet=84;       // standing height (72) plus a margin: higher is "over the head"
static constexpr float floorDepth=112;     // this far below the feet is another floor: shown dark, without its textures
static constexpr float gapUnits=96;        // an open gap this narrow between roofs belongs to the building around it
static constexpr int prefetchPixels=320;   // buildings this close (about 320 units) are uploaded ahead
// One roofed area; its texture covers window pixels x..x+w-1, y..y+h-1 (w, h powers of two).
struct Piece {uint16_t label=0;int x=0,y=0,w=0,h=0;size_t floorPixels=0;bool clipped=false;   // clipped: the area runs past the window
    std::vector<std::vector<byte>> levels;std::vector<int> widths,heights;
    std::vector<byte> solid;};   // w*h: 255 where the cut runs through a wall or solid block (the 3D radar caps these)
struct Result {bool ok=false;std::string map;float cutoff=0;int ox=0,oy=0;std::vector<Piece> pieces;std::vector<int> pieceOf;double ms=0;
    std::vector<uint16_t> area;   // per window pixel: the roofed area it belongs to (its open spots included), or 0
    std::vector<uint16_t> room;   // per window pixel: its room (connected roofed floor), or 0
    std::vector<byte> closed;     // per room: closed (a building) or open (an arch, a porch)
    std::vector<uint32_t> roomFloor,roomOpen,roomEdge;   // per room: floor pixels; edge pixels open to the outside, of all
    // edge pixels (for the log and the tests)
    // The room nearest to window pixel (x,y) within `radius`: 0 when none.
    uint16_t roomAt(int x,int y,int radius)const{
        if(room.size()!=static_cast<size_t>(size)*size)return 0;
        for(int r=0;r<=radius;++r)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){
            if(std::max(std::abs(dx),std::abs(dy))!=r)continue;int px=x+dx,py=y+dy;if(px<0||py<0||px>=size||py>=size)continue;
            uint16_t id=room[static_cast<size_t>(py)*size+px];if(id)return id;}
        return 0;}
    // In a closed building at (x,y): its room, or (under a skylight of it) the nearest one.
    bool closedAt(int x,int y)const{if(room.empty())return true;uint16_t id=roomAt(x,y,24);return id&&id<closed.size()&&closed[id];}
    // The building at window pixel (x,y); a few pixels around also count, so a gap in a
    // roof or a doorway between two roofs does not switch the radar back to the roof.
    const Piece *at(int x,int y,int radius=0)const{
        for(int r=0;r<=radius;++r)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){
            if(std::max(std::abs(dx),std::abs(dy))!=r)continue;int px=x+dx,py=y+dy;if(px<0||py<0||px>=size||py>=size)continue;
            uint16_t label=area[static_cast<size_t>(py)*size+px];if(label&&label<pieceOf.size()&&pieceOf[label]>=0)return &pieces[pieceOf[label]];}
        return nullptr;}
};
struct Request {std::shared_ptr<RadarJob::Image> image;std::string map;float x=0,y=0,cutoff=0;};
static HANDLE thread;static std::atomic<bool> finished{false};
static Request job;static Result result;static std::atomic<bool> abandon{false};   // the running window is for a height left behind
static std::shared_ptr<Result> current;    // the newest window
// Uploaded pieces; each keeps its window alive while it is used.
struct Texture {std::shared_ptr<Result> owner;const Piece *piece=nullptr;GLuint id=0;int level=0,row=0;bool done=false;ULONGLONG used=0;};
static std::vector<std::unique_ptr<Texture>> textures;static HGLRC context;
static const Texture *shown;               // drawn this frame
static bool visible;               // a building's inside is drawn this frame
static bool inBuilding;            // ...and the player stands in it right now (only then is the outside hidden)
static bool bridging;              // ...kept from an older window while the one for this height is made (cut at the head, no caps)
static ULONGLONG bridgeSince;
static float heldCutoff=-1e30f;static ULONGLONG lastStart,failedTick;
static void build(const Request &q,Result &out);
static DWORD WINAPI run(void *){Preview::diskOnly=true;Result out;build(job,out);result=std::move(out);finished.store(true,std::memory_order_release);return 0;}
static void start(Request q){job=std::move(q);result=Result{};finished=false;abandon=false;lastStart=GetTickCount64();thread=CreateThread(nullptr,0,run,nullptr,0,nullptr);if(thread)SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL);}
static void stop(){if(thread){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}finished=false;abandon=false;job=Request{};result=Result{};current.reset();textures.clear();shown=nullptr;visible=false;inBuilding=bridging=false;bridgeSince=0;}
static void freeTexture(){
    if(context==wglGetCurrentContext())for(auto &t:textures)if(t->id)glDeleteTextures(1,&t->id);
    textures.clear();shown=nullptr;visible=false;
}
}
namespace Radar3D {static void stop();static void freeTextures();}
// Plugin unload: the worker threads stop before the DLL is freed.
static void stopRadarWork(){BspRadar::cancel=true;RadarPatch::stop();Radar3D::stop();RadarJob::stop();}
static struct RadarUpload {GLuint texture=0;int level=0,row=0;} radarUpload;
static void freeOverview() {
    if(overview.context==wglGetCurrentContext()){if(overview.texture)glDeleteTextures(1,&overview.texture);if(radarUpload.texture)glDeleteTextures(1,&radarUpload.texture);}
    overview=Overview{};radarUpload=RadarUpload{};RadarPatch::freeTexture();Radar3D::freeTextures();
}
static void contentBounds(const std::vector<byte> &pixels,int width,int height,float &u0,float &v0,float &u1,float &v1){
    u0=v0=0;u1=v1=1;
    if(width<1||height<1||pixels.size()!=static_cast<size_t>(width)*height*4)return;
    int left=width,top=height,right=-1,bottom=-1;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(pixels[(static_cast<size_t>(y)*width+x)*4+3]>16){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
    if(right<left||bottom<top)return;
    u0=std::max(0,left-2)/static_cast<float>(width);u1=std::min(width,right+3)/static_cast<float>(width);
    v0=std::max(0,top-2)/static_cast<float>(height);v1=std::min(height,bottom+3)/static_cast<float>(height);
}
static void radarContentBounds(const std::vector<byte> &pixels,int width,int height){contentBounds(pixels,width,height,overview.u0,overview.v0,overview.u1,overview.v1);}
// Mipmap chain made on the CPU (legacy GL has no mipmap generation).
static void mipLevels(std::vector<byte> pixels,int width,int height,std::vector<std::vector<byte>> &levels,std::vector<int> &widths,std::vector<int> &heights){
    levels.push_back(std::move(pixels));widths.push_back(width);heights.push_back(height);
    while(width>1||height>1){
        const auto &source=levels.back();int w=std::max(1,width/2),h=std::max(1,height/2);std::vector<byte> next(static_cast<size_t>(w)*h*4);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<4;++c){int sum=0;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)sum+=source[(static_cast<size_t>(std::min(height-1,y*2+dy))*width+std::min(width-1,x*2+dx))*4+c];
            next[(static_cast<size_t>(y)*w+x)*4+c]=static_cast<byte>((sum+2)/4);
        }
        levels.push_back(std::move(next));widths.push_back(w);heights.push_back(h);width=w;height=h;
    }
}
static void uploadRadarTexture(std::vector<byte> pixels,int width,int height){
    std::vector<std::vector<byte>> levels;std::vector<int> widths,heights;mipLevels(std::move(pixels),width,height,levels,widths,heights);
    for(size_t level=0;level<levels.size();++level)glTexImage2D(GL_TEXTURE_2D,static_cast<GLint>(level),GL_RGBA,widths[level],heights[level],0,GL_RGBA,GL_UNSIGNED_BYTE,levels[level].data());
}
// Worker thread: the window around the player with everything above head height left out.
void RadarPatch::build(const Request &q,Result &out){
    LARGE_INTEGER started,ended,frequency;QueryPerformanceCounter(&started);
    const auto &img=*q.image;if(!img.scene||img.top.size()!=size_t(2048)*2048||img.levels.empty())return;
    const float spanX=img.maxX-img.minX,spanY=img.maxY-img.minY,w1=2047,h1=2047;   // the map's own pixel grid
    const float gx=(q.x-img.minX)/spanX*w1,gy=(img.maxY-q.y)/spanY*h1;
    const int ox=static_cast<int>(std::floor(gx))-size/2,oy=static_cast<int>(std::floor(gy))-size/2;
    BspRadar::Grid grid={img.minX,img.maxY,spanX,spanY,w1,h1,static_cast<float>(ox),static_cast<float>(oy),size,size};
    BspRadar::Raster r;if(!BspRadar::rasterize(*img.scene,grid,q.cutoff,true,r,&abandon))return;
    const size_t pixels=static_cast<size_t>(size)*size;
    auto covered=[&](size_t p){return r.top[p]>q.cutoff+1;};
    // Every roofed area: covered pixels connected to each other. Walls are covered too, so
    // all rooms under one roof form one area; open ground separates buildings.
    std::vector<uint16_t> labels(pixels,0);uint16_t count=0;std::vector<int> stack;
    for(size_t seed=0;seed<pixels&&count<65535;++seed){if(labels[seed]||!covered(seed))continue;if(abandon.load(std::memory_order_relaxed))return;
        labels[seed]=++count;stack.assign(1,static_cast<int>(seed));
        while(!stack.empty()){int p=stack.back();stack.pop_back();int x=p%size,y=p/size;
            const int next[4]={x>0?p-1:-1,x+1<size?p+1:-1,y>0?p-size:-1,y+1<size?p+size:-1};
            for(int n:next)if(n>=0&&!labels[n]&&covered(n)){labels[n]=count;stack.push_back(n);}}}
    // What the radar shows inside: the floor below the head under a roof, the map elsewhere.
    // A floor more than floorDepth under the feet is another storey: it is drawn dark.
    const float low=q.cutoff-aboveFeet-floorDepth;
    auto floorHere=[&](size_t p){return labels[p]&&r.z[p]>-1e29f&&r.z[p]>=low;};
    auto lowerHere=[&](size_t p){return r.z[p]>-1e29f&&r.z[p]<low;};
    auto shownZ=[&](size_t p){return floorHere(p)?r.z[p]:r.top[p];};
    auto shownOwner=[&](size_t p){return floorHere(p)?r.owners[p]:static_cast<uint16_t>(0xFFFF);};
    std::vector<byte> inside(pixels*4,0);
    for(int y=4;y<size-4;++y){if(abandon.load(std::memory_order_relaxed))return;for(int x=4;x<size-4;++x){size_t p=static_cast<size_t>(y)*size+x;if(!floorHere(p))continue;   // a clear border keeps clamped sampling transparent
        byte *dst=inside.data()+p*4;float value=r.z[p];bool boundary=false;uint16_t own=r.owners[p];
        // Same outline rule as the map: height steps, other objects, and the walls and roof edge around the room.
        for(int d:{-1,1,-size,size}){size_t n=p+d;float other=shownZ(n);uint16_t owner=shownOwner(n);
            if(other<-1e29f||std::abs(other-value)>32||(owner!=own&&(owner||own))){boundary=true;break;}}
        if(boundary){dst[0]=247;dst[1]=246;dst[2]=235;}else{const byte *c=r.surface.data()+p*3;dst[0]=c[0];dst[1]=c[1];dst[2]=c[2];}
        dst[3]=255;
    }}
    // Rooms: roofed floor, connected. Only a closed room hides the outside: one open on 40% or
    // more of its edge (an arch, a porch, the ground under a tree or along a wall) is a passage.
    std::vector<uint16_t> room(pixels,0);std::vector<byte> closed(1,1);std::vector<uint32_t> roomFloor(1,0),roomOpen(1,0),roomEdge(1,0);
    for(size_t seed=0;seed<pixels&&closed.size()<65535;++seed){if(room[seed]||!inside[seed*4+3])continue;if(abandon.load(std::memory_order_relaxed))return;
        const uint16_t id=static_cast<uint16_t>(closed.size());room[seed]=id;stack.assign(1,static_cast<int>(seed));size_t open=0,wall=0;
        while(!stack.empty()){int p=stack.back();stack.pop_back();int x=p%size,y=p/size;
            const int next[4]={x>0?p-1:-1,x+1<size?p+1:-1,y>0?p-size:-1,y+1<size?p+size:-1};
            for(int n:next){if(n<0){++wall;continue;}
                if(inside[static_cast<size_t>(n)*4+3]){if(!room[n]){room[n]=id;stack.push_back(n);}continue;}
                if(lowerHere(n))continue;                              // a stairwell down
                if(labels[n])++wall;else ++open;}}                     // a wall, or open ground
        closed.push_back(open*5<(open+wall)*2);roomOpen.push_back(static_cast<uint32_t>(open));roomEdge.push_back(static_cast<uint32_t>(open+wall));roomFloor.push_back(0);}
    for(size_t p=0;p<pixels;++p)if(room[p])++roomFloor[room[p]];
    // Open-sky spots of a building (a skylight, a gap between its roofs) belong to it: at least
    // half of their edge is the floor of a closed room. A yard walled in by walls is outside,
    // as is open ground touching the window's edge or, unless narrow, two roofed areas.
    const int gapPixels=std::max(4,static_cast<int>(gapUnits/(std::max(spanX,spanY)/w1)));
    std::vector<uint16_t> holeOf(pixels,0);{std::vector<byte> seen(pixels,0);std::vector<int> spot;struct Contact {uint16_t label;int all,roofed;};std::vector<Contact> contacts;
        for(size_t seed=0;seed<pixels;++seed){if(labels[seed]||seen[seed])continue;if(abandon.load(std::memory_order_relaxed))return;
            spot.clear();contacts.clear();stack.assign(1,static_cast<int>(seed));seen[seed]=1;bool border=false;int x0=size,y0=size,x1=-1,y1=-1;
            while(!stack.empty()){int p=stack.back();stack.pop_back();spot.push_back(p);int x=p%size,y=p/size;
                if(x==0||y==0||x==size-1||y==size-1)border=true;x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);
                const int next[4]={x>0?p-1:-1,x+1<size?p+1:-1,y>0?p-size:-1,y+1<size?p+size:-1};
                for(int n:next){if(n<0)continue;
                    if(labels[n]){Contact *c=nullptr;for(auto &k:contacts)if(k.label==labels[n]){c=&k;break;}if(!c){contacts.push_back({labels[n],0,0});c=&contacts.back();}
                        ++c->all;c->roofed+=room[n]&&closed[room[n]];continue;}
                    if(!seen[n]){seen[n]=1;stack.push_back(n);}}}
            if(border||contacts.empty())continue;
            if(contacts.size()>1&&(x1-x0>=gapPixels&&y1-y0>=gapPixels))continue;   // open ground between buildings
            int all=0,roofed=0;for(const auto &c:contacts){all+=c.all;roofed+=c.roofed;}if(roofed*2<all)continue;   // a walled yard
            uint16_t around=std::max_element(contacts.begin(),contacts.end(),[](const Contact &a,const Contact &b){return a.roofed<b.roofed;})->label;
            for(int p:spot)holeOf[p]=around;}}
    // Bounds and floor size of every area.
    struct Box {int x0=size,y0=size,x1=-1,y1=-1;size_t floor=0;};std::vector<Box> boxes(static_cast<size_t>(count)+1);
    for(int y=0;y<size;++y)for(int x=0;x<size;++x){size_t p=static_cast<size_t>(y)*size+x;uint16_t l=labels[p];if(!l)continue;auto &b=boxes[l];
        b.x0=std::min(b.x0,x);b.y0=std::min(b.y0,y);b.x1=std::max(b.x1,x);b.y1=std::max(b.y1,y);if(inside[p*4+3])++b.floor;}
    // One texture per area with a floor, nearest to the player first, at most ~8M texels.
    std::vector<uint16_t> order;for(uint16_t l=1;l<=count;++l)if(boxes[l].floor)order.push_back(l);
    auto distance=[&](uint16_t l){const auto &b=boxes[l];int dx=std::max({b.x0-size/2,0,size/2-b.x1}),dy=std::max({b.y0-size/2,0,size/2-b.y1});return dx*dx+dy*dy;};
    std::stable_sort(order.begin(),order.end(),[&](uint16_t a,uint16_t b){return distance(a)<distance(b);});
    const bool sameGrid=img.widths[0]==2048;const auto &map=img.levels[0];size_t texels=0;
    out.pieceOf.assign(static_cast<size_t>(count)+1,-1);
    for(uint16_t l:order){if(abandon.load(std::memory_order_relaxed))return;const auto &b=boxes[l];
        int x0=std::max(0,b.x0-4),y0=std::max(0,b.y0-4),x1=std::min(size-1,b.x1+4),y1=std::min(size-1,b.y1+4),w=1,h=1;
        while(w<x1-x0+1)w*=2;while(h<y1-y0+1)h*=2;if(texels+static_cast<size_t>(w)*h>(size_t(8)<<20))continue;texels+=static_cast<size_t>(w)*h;
        Piece piece;piece.label=l;piece.x=x0;piece.y=y0;piece.w=w;piece.h=h;piece.floorPixels=b.floor;piece.clipped=b.x0<=4||b.y0<=4||b.x1>=size-5||b.y1>=size-5;
        std::vector<byte> rgba(static_cast<size_t>(w)*h*4,0);piece.solid.assign(static_cast<size_t>(w)*h,0);
        for(int ty=0;ty<h;++ty)for(int tx=0;tx<w;++tx){int x=x0+tx,y=y0+ty;byte *dst=rgba.data()+(static_cast<size_t>(ty)*w+tx)*4;
            bool mine=false;
            if(x<size&&y<size){size_t p=static_cast<size_t>(y)*size+x;if(labels[p]==l||holeOf[p]==l){if(inside[p*4+3]){memcpy(dst,inside.data()+p*4,4);continue;}mine=x>=4&&y>=4&&x<size-4&&y<size-4;
                if(mine&&lowerHere(p)){dst[0]=24;dst[1]=28;dst[2]=33;dst[3]=255;continue;}   // a lower storey (through a stairwell, over a balcony)
                if(mine&&labels[p]==l)piece.solid[static_cast<size_t>(ty)*w+tx]=255;}}
            // Walls (under the roof, no floor) and enclosed open spots keep the map's colour and
            // outlines, opaque, so the building stands on its own when the outside is hidden.
            // Other texels are transparent and also keep the map's colour, so filtering does not darken edges.
            int mx=x+ox,my=y+oy;if(sameGrid&&mx>=0&&my>=0&&mx<2048&&my<2048){const byte *m=map.data()+(static_cast<size_t>(my)*2048+mx)*4;dst[0]=m[0];dst[1]=m[1];dst[2]=m[2];}
            else if(mine){dst[0]=90;dst[1]=96;dst[2]=104;}
            if(mine)dst[3]=255;
        }
        mipLevels(std::move(rgba),w,h,piece.levels,piece.widths,piece.heights);
        out.pieceOf[l]=static_cast<int>(out.pieces.size());out.pieces.push_back(std::move(piece));
    }
    for(size_t p=0;p<pixels;++p)if(!labels[p])labels[p]=holeOf[p];   // open spots join their building
    out.area=std::move(labels);out.room=std::move(room);out.closed=std::move(closed);out.roomFloor=std::move(roomFloor);out.roomOpen=std::move(roomOpen);out.roomEdge=std::move(roomEdge);out.ox=ox;out.oy=oy;out.cutoff=q.cutoff;out.map=q.map;out.ok=true;
    QueryPerformanceCounter(&ended);QueryPerformanceFrequency(&frequency);out.ms=1000.0*(ended.QuadPart-started.QuadPart)/frequency.QuadPart;
}
// Builder thread: everything that needs neither OpenGL nor the engine.
void RadarJob::build(RadarJob::Image &out){
    const std::string map=jobMap;
    std::vector<byte> bsp=jobBsp.empty()?Preview::read(("maps/"+map+".bsp").c_str()):std::move(jobBsp);out.missingBsp=bsp.empty();
    auto bytes=Preview::read(("overviews/"+map+".txt").c_str());
    std::istringstream config(std::string(bytes.begin(),bytes.end()));std::string token,picture="overviews/"+map+".bmp";
    while(config>>token) {
        if(!_stricmp(token.c_str(),"ZOOM"))config>>out.zoom;
        else if(!_stricmp(token.c_str(),"ORIGIN")){float z;config>>out.x>>out.y>>z;}
        else if(!_stricmp(token.c_str(),"ROTATED")){int rotated=0;config>>rotated;out.rotated=rotated!=0;}
        else if(!_stricmp(token.c_str(),"IMAGE")){config>>picture;picture.erase(std::remove(picture.begin(),picture.end(),'"'),picture.end());}
    }
    std::vector<byte> rgba;int w=0,h=0;bool configured=!bytes.empty() && std::isfinite(out.zoom)&&out.zoom>=.01f&&out.zoom<=100&&std::isfinite(out.x)&&std::isfinite(out.y)&&picture.rfind("overviews/",0)==0&&picture.find("..")==std::string::npos;
    bool loaded=false;
    if(job3D){
        loaded=BspRadar::generate3D(bsp,rgba,w,h,out.minX,out.maxX,out.minY,out.maxY);
        if(loaded){out.generated=true;out.isometric=true;logLine(("Radar: generated isometric textured BSP map for "+map).c_str());}
    }else{
        // The parsed map and roof heights stay with the image for the radar inside buildings.
        loaded=BspRadar::generate(bsp,rgba,w,h,out.minX,out.maxX,out.minY,out.maxY,&out.scene,&out.top);
        if(loaded){out.generated=true;out.isometric=false;logLine(("Radar: generated outlined top-down textured BSP map for "+map).c_str());}
    }
    if(BspRadar::cancel)return;
    if(!loaded)loaded=configured && overviewData(Preview::read(picture.c_str()),rgba,w,h);
    if(!loaded){
        // The top-down build was already tried above unless the isometric one failed.
        if(!job3D||!BspRadar::generate(bsp,rgba,w,h,out.minX,out.maxX,out.minY,out.maxY)){
            if(!BspRadar::cancel)logLine(("Radar: no readable overview or supported BSP for "+map).c_str());return;
        }
        out.generated=true;out.isometric=false;logLine(("Radar: generated textured top-down BSP map for "+map).c_str());
    }else if(!out.isometric&&!out.generated)logLine(("Radar: loaded overview image for "+map).c_str());
    // Only ready-made overview images need this: there black means empty space
    // and a mild sharpen helps. Maps generated from the BSP already mark empty
    // space in alpha, and dark texels (water, shadows, fabric) must stay opaque,
    // otherwise only bright specks remain and textures look unpainted.
    if(!out.isometric&&!out.generated){
        // For flat overview images, black commonly denotes empty space.
        for(size_t i=0;i+3<rgba.size();i+=4)if(std::max({rgba[i],rgba[i+1],rgba[i+2]})<32)rgba[i+3]=0;
        auto original=rgba;
        for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){size_t i=(static_cast<size_t>(y)*w+x)*4;if(!original[i+3])continue;
            for(int c=0;c<3;++c){int sum=0,n=0;for(int d:{-1,1,-w,w}){size_t j=(static_cast<size_t>(y)*w+x+d)*4;if(original[j+3]){sum+=original[j+c];++n;}}
                if(n)rgba[i+c]=static_cast<byte>(std::max(0,std::min(255,static_cast<int>(original[i+c]*1.35f-sum*0.35f/n))));}
        }
    }
    // Legacy GL implementations require power-of-two textures; overview BMPs
    // commonly have a 1024x768 canvas. Keep their normalized coordinates intact.
    int limit=std::max(64,std::min(4096,jobCap));
    int tw=1,th=1;while(tw<w&&tw*2<=limit)tw*=2;while(th<h&&th*2<=limit)th*=2;
    if(tw!=w||th!=h){std::vector<byte> scaled(static_cast<size_t>(tw)*th*4);
        for(int y=0;y<th;++y)for(int x=0;x<tw;++x){size_t src=(static_cast<size_t>(y*h/th)*w+x*w/tw)*4,dst=(static_cast<size_t>(y)*tw+x)*4;memcpy(scaled.data()+dst,rgba.data()+src,4);}
        rgba=std::move(scaled);w=tw;h=th;
    }
    contentBounds(rgba,w,h,out.u0,out.v0,out.u1,out.v1);
    mipLevels(std::move(rgba),w,h,out.levels,out.widths,out.heights);out.ok=true;
}
// Render thread: collects a finished build; a map missing from the game folders on
// disk is read once through the engine and built again in the background.
static void finishRadarJob(){
    using namespace RadarJob;
    if(!thread||!finished.load(std::memory_order_acquire))return;
    WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;finished=false;
    bool cancelled=BspRadar::cancel.exchange(false);
    auto done=std::make_shared<Image>(std::move(result));result=Image{};
    if(cancelled)return;
    if(done->ok){image=done;imageMap=jobMap;image3D=job3D;failures=0;return;}
    if(done->missingBsp&&!engineRead){
        engineRead=true;auto bsp=Preview::read(("maps/"+jobMap+".bsp").c_str());
        if(!bsp.empty()){std::string map=jobMap;start(map,job3D,jobCap,std::move(bsp));return;}
    }
    failedMap=jobMap;failed3D=job3D;++failures;failedTick=GetTickCount64();
}
// Uploads about 1 MB per frame; the radar shows the map once every mip level is in.
static void uploadRadarStep(const RadarJob::Image &img){
    if(img.levels.empty())return;
    if(!radarUpload.texture){
        glGenTextures(1,&radarUpload.texture);glBindTexture(GL_TEXTURE_2D,radarUpload.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        const char *extensions=reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
        const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION));
        int wrap=(version&&atof(version)>=1.2)||(extensions&&(strstr(extensions,"GL_EXT_texture_edge_clamp")||strstr(extensions,"GL_SGIS_texture_edge_clamp")))?0x812F:GL_CLAMP;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
        for(size_t level=0;level<img.levels.size();++level)glTexImage2D(GL_TEXTURE_2D,static_cast<GLint>(level),GL_RGBA,img.widths[level],img.heights[level],0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        GLint allocated=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&allocated);
        if(allocated!=img.widths[0]){
            glDeleteTextures(1,&radarUpload.texture);radarUpload=RadarUpload{};RadarJob::image.reset();
            RadarJob::failedMap=overview.map;RadarJob::failed3D=overview.mode3D;RadarJob::failures=3;logLine("Radar: the map texture is larger than this OpenGL driver accepts.");return;
        }
    }
    glBindTexture(GL_TEXTURE_2D,radarUpload.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    size_t budget=size_t(1)<<20;
    while(radarUpload.level<static_cast<int>(img.levels.size())&&budget){
        int w=img.widths[radarUpload.level],h=img.heights[radarUpload.level];size_t stride=static_cast<size_t>(w)*4;
        int rows=std::max(1,std::min(h-radarUpload.row,static_cast<int>(budget/stride)));
        glTexSubImage2D(GL_TEXTURE_2D,radarUpload.level,0,radarUpload.row,w,rows,GL_RGBA,GL_UNSIGNED_BYTE,img.levels[radarUpload.level].data()+radarUpload.row*stride);
        radarUpload.row+=rows;size_t used=rows*stride;budget=used>=budget?0:budget-used;
        if(radarUpload.row>=h){++radarUpload.level;radarUpload.row=0;}
    }
    if(radarUpload.level<static_cast<int>(img.levels.size()))return;
    overview.texture=radarUpload.texture;radarUpload=RadarUpload{};
    overview.generated=img.generated;overview.isometric=img.isometric;overview.rotated=img.rotated;overview.zoom=img.zoom;overview.x=img.x;overview.y=img.y;
    overview.minX=img.minX;overview.maxX=img.maxX;overview.minY=img.minY;overview.maxY=img.maxY;overview.u0=img.u0;overview.v0=img.v0;overview.u1=img.u1;overview.v1=img.v1;
}
static void loadOverview(const std::string &map) {
    using namespace RadarJob;
    finishRadarJob();
    HGLRC context=wglGetCurrentContext();
    if(map=="menu" || map.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos){if(!overview.map.empty())freeOverview();return;}
    // The top-down map (with the parsed map and roof heights) is built in both radar modes;
    // the 3D radar draws its own geometry from that parsed map.
    if(overview.map!=map||overview.context!=context){
        // A new map or video restart: earlier failures may no longer apply.
        failedMap.clear();failures=0;engineRead=false;
        freeOverview();overview.map=map;overview.context=context;overview.mode3D=false;
    }
    if(overview.texture)return;
    if(image&&imageMap==map&&!image3D){uploadRadarStep(*image);return;}
    if(thread){if(jobMap!=map||job3D)BspRadar::cancel=true;return;}
    if(failedMap==map&&!failed3D&&(failures>=3||GetTickCount64()-failedTick<30000))return;
    image.reset();   // the previous map's pixels are no longer needed
    GLint cap=2048;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&cap);BspRadar::textureCap=cap;
    start(map,false,cap,{});
}
static void uv(float x,float y,float z,float &u,float &v) {
    if(overview.isometric){constexpr float a=.70710678118f,b=.40824829046f;float s=(x-y)*a,t=-(x+y)*b+z*2*b;u=(s-overview.minX)/(overview.maxX-overview.minX);v=(t-overview.minY)/(overview.maxY-overview.minY);return;}
    if(overview.generated){u=(x-overview.minX)/(overview.maxX-overview.minX);v=(overview.maxY-y)/(overview.maxY-overview.minY);return;}
    float span=8192/overview.zoom;
    if(overview.rotated){u=.5f+(x-overview.x)/span;v=.5f-(y-overview.y)/(span*.75f);}
    else {u=.5f-(y-overview.y)/span;v=.5f-(x-overview.x)/(span*.75f);}
}
static bool contactVisible(int index,int own,ULONGLONG tick) {
    return index>=1&&index<=32&&index!=own&&own>=1&&own<=32&&team[own]>0&&team[index]>0&&
        dots[index].tick && tick>=dots[index].tick && tick-dots[index].tick<8000 && !dots[index].dead;
}
static void drawPlayerMarker(float x,float y,bool ally,float size=1.f) {
    if(ally){circle(x,y,5*size,9,20,24,255);circle(x,y,3.7f*size,77,226,245,255);circle(x,y,1.5f*size,230,255,255,255);return;}
    glLineWidth(4*size);glColor4ub(18,12,14,255);glBegin(GL_LINES);glVertex2f(x-4*size,y-4*size);glVertex2f(x+4*size,y+4*size);glVertex2f(x-4*size,y+4*size);glVertex2f(x+4*size,y-4*size);glEnd();
    glLineWidth(2*size);glColor4ub(255,56,54,255);glBegin(GL_LINES);glVertex2f(x-4*size,y-4*size);glVertex2f(x+4*size,y+4*size);glVertex2f(x-4*size,y+4*size);glVertex2f(x+4*size,y-4*size);glEnd();glLineWidth(1);
}
static void drawRadarBorder(float cx,float cy,float radius,float resolution=1.f) {
    if(!radarBorderOn||radarBorderOpacity<=0)return;
    // A triangle ring keeps the configured pixel width on drivers that only
    // support one-pixel OpenGL lines. Draw inward to preserve the radar bounds.
    float inner=std::max(0.f,radius-static_cast<float>(std::clamp(radarBorderWidth,1,10))*resolution);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(radarBorderRgb[0],radarBorderRgb[1],radarBorderRgb[2],std::clamp(radarBorderOpacity,0,100)*255/100);
    glBegin(GL_TRIANGLE_STRIP);
    int segments=HudResolution::segments(radius);for(int i=0;i<=segments;++i){float angle=i*6.2831853f/segments,c=std::cos(angle),s=std::sin(angle);
        glVertex2f(cx+c*radius,cy+s*radius);glVertex2f(cx+c*inner,cy+s*inner);}
    glEnd();
}
// Fills a star-shaped polygon (fan from point 0) grown by `expand` pixels, with a
// `fringe`-wide alpha ramp on the outside so edges stay smooth without MSAA.
static void radarSmoothPolygon(const float (*points)[2],int count,float expand,float fringe,int r,int g,int b,int a){
    float area=0;for(int i=0;i<count;++i){const float *p=points[i],*q=points[(i+1)%count];area+=p[0]*q[1]-q[0]*p[1];}
    const float side=area>0?1.f:-1.f;float inner[8][2],outer[8][2];
    for(int i=0;i<count&&i<8;++i){
        const float *prev=points[(i+count-1)%count],*cur=points[i],*next=points[(i+1)%count];
        auto normal=[&](const float *a0,const float *a1,float &nx,float &ny){float dx=a1[0]-a0[0],dy=a1[1]-a0[1],l=std::sqrt(dx*dx+dy*dy);if(l<1e-4f)l=1;nx=dy/l*side;ny=-dx/l*side;};
        float n0x,n0y,n1x,n1y;normal(prev,cur,n0x,n0y);normal(cur,next,n1x,n1y);
        float mx=(n0x+n1x)*.5f,my=(n0y+n1y)*.5f,m2=mx*mx+my*my;if(m2>1e-6f){float k=std::min(1.f/m2,4.f);mx*=k;my*=k;}
        inner[i][0]=cur[0]+mx*expand;inner[i][1]=cur[1]+my*expand;outer[i][0]=cur[0]+mx*(expand+fringe);outer[i][1]=cur[1]+my*(expand+fringe);
    }
    glShadeModel(GL_SMOOTH);glDisable(GL_TEXTURE_2D);glBegin(GL_TRIANGLES);
    for(int i=1;i+1<count;++i){glColor4ub(r,g,b,a);glVertex2fv(inner[0]);glVertex2fv(inner[i]);glVertex2fv(inner[i+1]);}
    for(int i=0;i<count;++i){int j=(i+1)%count;
        glColor4ub(r,g,b,a);glVertex2fv(inner[i]);glVertex2fv(inner[j]);glColor4ub(r,g,b,0);glVertex2fv(outer[j]);
        glColor4ub(r,g,b,a);glVertex2fv(inner[i]);glColor4ub(r,g,b,0);glVertex2fv(outer[j]);glVertex2fv(outer[i]);}
    glEnd();
}
// Own-player marker: a black notched arrow pointing along the view direction,
// with a soft drop shadow and a thin light rim so it reads on any map texture.
static void drawRadarArrow(float x,float y,float headingX,float headingY,float size){
    float sideX=-headingY,sideY=headingX;
    auto at=[&](float forward,float lateral,float out[2]){out[0]=x+(headingX*forward+sideX*lateral)*size;out[1]=y+(headingY*forward+sideY*lateral)*size;};
    float arrow[4][2];at(10.5f,0,arrow[0]);at(-7,7,arrow[1]);at(-3.2f,0,arrow[2]);at(-7,-7,arrow[3]);
    float shadow[4][2];for(int i=0;i<4;++i){shadow[i][0]=arrow[i][0]+.8f*size;shadow[i][1]=arrow[i][1]+1.4f*size;}
    radarSmoothPolygon(shadow,4,.6f*size,2.6f*size,0,0,0,110);
    radarSmoothPolygon(arrow,4,std::max(1.f,1.5f*size),1.1f,235,238,245,235);
    radarSmoothPolygon(arrow,4,0,1.1f,10,10,12,255);
}
// Static map view: fixed texture orientation and centre, independent of the
// player's origin/yaw. At 100% all non-empty content fits inside the circle.
struct RadarMapView {
    float u,v,scaleU,scaleV;
    void point(float u0,float v0,float cx,float cy,float &x,float &y)const{x=cx+(u0-u)*scaleU;y=cy+(v0-v)*scaleV;}
};
static RadarMapView centeredRadarView(float radius,float markerMargin){
    float spanU=overview.generated?overview.maxX-overview.minX:8192/std::max(.01f,overview.zoom);
    float spanV=overview.generated?overview.maxY-overview.minY:spanU*.75f;
    if(!std::isfinite(spanU)||spanU<1)spanU=1;if(!std::isfinite(spanV)||spanV<1)spanV=1;
    float diagonal=std::hypot(spanU*(overview.u1-overview.u0),spanV*(overview.v1-overview.v0));
    float zoom=std::clamp(radarCenteredZoom,100,400)/100.f;
    float k=2*std::max(1.f,radius-markerMargin)/std::max(1.f,diagonal)*zoom;
    return {(overview.u0+overview.u1)*.5f,(overview.v0+overview.v1)*.5f,spanU*k,spanV*k};
}
struct RadarMapVertex {float x,y,u,v;};
static std::vector<RadarMapVertex> centeredRadarPolygon(float cx,float cy,float radius,int segments,const RadarMapView &view){
    std::vector<RadarMapVertex> polygon;segments=std::clamp(segments,12,512);
    for(int i=0;i<segments;++i){float a=i*6.28318530718f/segments,dx=std::cos(a)*radius,dy=std::sin(a)*radius;polygon.push_back({cx+dx,cy+dy,view.u+dx/view.scaleU,view.v+dy/view.scaleV});}
    // Clip the map/circle intersection, rather than letting GL_CLAMP stretch
    // edge texels into the empty space outside the map.
    for(int plane=0;plane<4&&!polygon.empty();++plane){
        auto distance=[&](const RadarMapVertex &p){return plane==0?p.u-overview.u0:plane==1?overview.u1-p.u:plane==2?p.v-overview.v0:overview.v1-p.v;};
        std::vector<RadarMapVertex> out;auto prev=polygon.back();float prevD=distance(prev);
        for(auto cur:polygon){float curD=distance(cur);if((prevD>=0)!=(curD>=0)){
            float t=prevD/(prevD-curD);out.push_back({prev.x+(cur.x-prev.x)*t,prev.y+(cur.y-prev.y)*t,prev.u+(cur.u-prev.u)*t,prev.v+(cur.v-prev.v)*t});
        }if(curD>=0)out.push_back(cur);prev=cur;prevD=curD;}polygon=std::move(out);
    }return polygon;
}
static void radarClampMarker(float &x,float &y,float cx,float cy,float limit){
    float dx=x-cx,dy=y-cy,d=std::hypot(dx,dy);limit=std::max(0.f,limit);if(d>limit&&d>0){x=cx+dx*limit/d;y=cy+dy*limit/d;}
}
static float predictedOrigin[3];static ULONGLONG predictedTick;static bool predictedDucking,predictedOnGround=true;
static void observePredicted(const float *o,bool ducking=false,bool onGround=true){if(o&&std::isfinite(o[0])&&std::isfinite(o[1])&&std::isfinite(o[2])){for(int i=0;i<3;++i)predictedOrigin[i]=o[i];predictedTick=GetTickCount64();predictedDucking=ducking;predictedOnGround=onGround;}}
// Render thread, inside a GL2D scope (bindings are restored): uploads pieces in the order
// asked, about 2 MB per frame while the piece the player stands in is missing, 1 MB for
// buildings uploaded ahead. A mip level is created when its upload starts, and a level
// that fits the budget goes up in one call.
static void pumpRadarPieces(std::vector<RadarPatch::Texture *> &wanted,bool urgent){
    using namespace RadarPatch;size_t budget=urgent?size_t(2)<<20:size_t(512)<<10;   // urgent: the player is in it and it is not shown yet (bigger uploads made frames stutter)
    for(auto *t:wanted){if(t->done||!budget)continue;const auto &piece=*t->piece;
        if(!t->id){glGenTextures(1,&t->id);glBindTexture(GL_TEXTURE_2D,t->id);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            const char *extensions=reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION));
            int wrap=(version&&atof(version)>=1.2)||(extensions&&(strstr(extensions,"GL_EXT_texture_edge_clamp")||strstr(extensions,"GL_SGIS_texture_edge_clamp")))?0x812F:GL_CLAMP;
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
            t->level=t->row=0;}
        glBindTexture(GL_TEXTURE_2D,t->id);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        while(t->level<static_cast<int>(piece.levels.size())&&budget){
            int w=piece.widths[t->level],h=piece.heights[t->level];size_t stride=static_cast<size_t>(w)*4;
            if(!t->row&&stride*h<=budget){   // whole level at once
                glTexImage2D(GL_TEXTURE_2D,t->level,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,piece.levels[t->level].data());
                budget-=stride*h;++t->level;continue;}
            if(!t->row)glTexImage2D(GL_TEXTURE_2D,t->level,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            int rows=std::max(1,std::min(h-t->row,static_cast<int>(budget/stride)));
            glTexSubImage2D(GL_TEXTURE_2D,t->level,0,t->row,w,rows,GL_RGBA,GL_UNSIGNED_BYTE,piece.levels[t->level].data()+t->row*stride);
            t->row+=rows;size_t used=rows*stride;budget=used>=budget?0:budget-used;
            if(t->row>=h){++t->level;t->row=0;}
        }
        t->done=t->level>=static_cast<int>(piece.levels.size());
    }
}
// Render thread: keeps a window around the player, uploads the buildings next to them and
// picks the one the player stands in.
static void updateRadarPatch(const float *origin,bool fresh){
    using namespace RadarPatch;
    ULONGLONG now=GetTickCount64();
    if(thread&&finished.load(std::memory_order_acquire)){
        WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;finished=false;
        auto done=std::make_shared<Result>(std::move(result));result=Result{};
        if(done->ok){current=done;
            // The older windows' building textures, and with them their pixels (tens of MB each in
            // this 32-bit game), go at once: only the one on screen stays, bridging to the new window.
            for(size_t i=0;i<textures.size();){auto &t=textures[i];
                if(t->owner!=current&&t.get()!=shown){if(t->id&&context==wglGetCurrentContext())glDeleteTextures(1,&t->id);textures.erase(textures.begin()+i);}else ++i;}}
        else if(abandon)lastStart=0;else failedTick=now;   // an abandoned one is followed at once
    }
    if(context!=wglGetCurrentContext()){for(auto &t:textures){t->id=0;t->done=false;}context=wglGetCurrentContext();}   // ids of another context
    const Texture *before=shown;const bool wasInBuilding=inBuilding;shown=nullptr;visible=inBuilding=bridging=false;auto img=RadarJob::image;
    if(!img||!img->scene||img->top.size()!=size_t(2048)*2048||RadarJob::imageMap!=overview.map||RadarJob::image3D||!overview.generated||overview.isometric)return;
    // Head height from the feet; it is held while airborne so jumping does not rebuild.
    float feet=origin[2]-(fresh&&predictedDucking?18.f:36.f);
    if(!fresh||predictedOnGround||heldCutoff<-1e29f)heldCutoff=feet+aboveFeet;
    const float cutoff=heldCutoff,spanX=img->maxX-img->minX,spanY=img->maxY-img->minY;
    int gx=static_cast<int>(std::floor((origin[0]-img->minX)/spanX*2047)),gy=static_cast<int>(std::floor((img->maxY-origin[1])/spanY*2047));
    if(gx<0||gy<0||gx>=2048||gy>=2048)return;
    const bool usable=current&&current->map==overview.map;const int lx=usable?gx-current->ox:0,ly=usable?gy-current->oy:0;
    const Piece *herePiece=usable?current->at(lx,ly,6):nullptr;
    // A window around the player indoors and out, so a building is ready before its door;
    // a new one for another floor, a crate or step, or after walking a quarter window
    // (an eighth when the building the player is in runs past the window's edge).
    const int dx=std::abs(lx-size/2),dy=std::abs(ly-size/2),reach=herePiece&&herePiece->clipped?size/8:size/4;
    bool fits=usable&&std::fabs(current->cutoff-cutoff)<=32&&dx<=reach&&dy<=reach;
    if(thread&&std::fabs(job.cutoff-cutoff)>32)abandon=true;   // dropped or climbed a storey while it was made: start over for this height
    const bool newHeight=!usable||std::fabs(current->cutoff-cutoff)>32;   // another storey: no pause between windows
    if(!fits&&!thread&&(newHeight||now-lastStart>=150)&&now-failedTick>=500){Request q;q.image=img;q.map=overview.map;q.x=origin[0];q.y=origin[1];q.cutoff=cutoff;start(std::move(q));}
    auto entry=[&](const std::shared_ptr<Result> &owner,const Piece *piece)->Texture &{
        for(auto &t:textures)if(t->piece==piece){t->used=now;return *t;}
        textures.push_back(std::make_unique<Texture>());auto &t=*textures.back();t.owner=owner;t.piece=piece;t.used=now;return t;};
    std::vector<Texture *> wanted;Texture *here=nullptr;
    if(herePiece){here=&entry(current,herePiece);wanted.push_back(here);}
    // Buildings next to the player, nearest first, uploaded ahead.
    if(usable){std::vector<std::pair<int,const Piece *>> nearby;   // ("near" is a Windows macro)
        for(const auto &piece:current->pieces){int dx=std::max({piece.x-lx,0,lx-(piece.x+piece.w-1)}),dy=std::max({piece.y-ly,0,ly-(piece.y+piece.h-1)});
            if(dx<=prefetchPixels&&dy<=prefetchPixels&&&piece!=(here?here->piece:nullptr))nearby.push_back({dx*dx+dy*dy,&piece});}
        std::sort(nearby.begin(),nearby.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        for(size_t i=0;i<nearby.size()&&i<8;++i)wanted.push_back(&entry(current,nearby[i].second));}
    pumpRadarPieces(wanted,here&&!here->done);
    // In a building: under its roof, or in an open spot it encloses (a skylight, a courtyard);
    // the window's areas count only when it was built for the player's present height.
    const bool inside=img->top[static_cast<size_t>(gy)*2048+gx]>cutoff+1||(usable&&std::fabs(current->cutoff-cutoff)<=32&&current->at(lx,ly));
    // A building stays shown while the player is in its area of the window it came from (at
    // any height): on stairs, under a gap and while a newer window is prepared it does not
    // blink to the outside. Out of that area it is dropped in the same frame: no hold.
    auto within=[&](const Texture *t,int radius){const auto &w=*t->owner;return w.map==overview.map&&w.at(gx-w.ox,gy-w.oy,radius)==t->piece;};
    auto nearEdge=[&](const Texture *t){int wx=gx-t->owner->ox,wy=gy-t->owner->oy;return wx<8||wy<8||wx>=size-8||wy>=size-8;};
    // A window made for another height (more than a step off) only bridges the wait for a new
    // one. One made lower down needs the feet still under its cut and floor near them: climbing
    // out of a low spot or onto a roof must not show everything around as walls cut flat or the
    // room under the player. (One made higher up, going down the
    // stairs, at most shows the steps below as the storey under it.)
    const bool waiting=!fits||(here&&!here->done);
    auto usableFor=[&](const Texture *t){const auto &w=*t->owner;
        return std::fabs(w.cutoff-cutoff)<=32||(waiting&&(w.cutoff>cutoff||(feet<w.cutoff&&w.roomAt(gx-w.ox,gy-w.oy,16))));};   // (feet still under its cut: same storey)
    Texture *last=nullptr;for(auto &t:textures)if(t.get()==before){last=t.get();break;}
    if(here&&here->done&&usableFor(here)&&(inside||within(here,0)))shown=here;
    else if(last&&last->done&&usableFor(last)&&(inside?within(last,6)||nearEdge(last):within(last,0))){shown=last;last->used=now;}
    // Still under a roof in the building of the last frame, but on a level the windows at hand
    // do not cover (down or up the stairs): that building is kept until the new window is in,
    // within its bounds and for at most 1.5 s, so the outside does not blink in.
    if(!shown&&waiting&&inside&&wasInBuilding&&last&&last->done){const auto &pc=*last->piece;int wx=gx-last->owner->ox,wy=gy-last->owner->oy;
        if(!bridgeSince)bridgeSince=now;
        if(wx>=pc.x&&wy>=pc.y&&wx<pc.x+pc.w&&wy<pc.y+pc.h&&now-bridgeSince<1500){shown=last;last->used=now;bridging=true;}}
    if(!bridging)bridgeSince=0;
    visible=shown!=nullptr;
    // The outside is hidden only while the player is in a closed building (not under an arch,
    // a porch or a tree): leaving shows the map at once.
    // (A window for another height, or with the player at or past its edge, cannot tell the room
    // here: the last frame's answer stands until the new window is in.)
    inBuilding=bridging||(visible&&(inside||within(shown,0))&&
        (std::fabs(shown->owner->cutoff-cutoff)>32||nearEdge(shown)?wasInBuilding:shown->owner->closedAt(gx-shown->owner->ox,gy-shown->owner->oy)));
    // Forget pieces not used for a few seconds.
    for(size_t i=0;i<textures.size();){auto &t=textures[i];
        if(t.get()!=shown&&now-t->used>3000){if(t->id&&context==wglGetCurrentContext())glDeleteTextures(1,&t->id);textures.erase(textures.begin()+i);}else ++i;}
}
#include "radar3d.h"
static void drawRadar(const SCREENINFO &screen) {
    if(!radarOn || !oldRadar || !oldTeam || !wglGetCurrentContext())return;
    int own=localIndex();auto *player=eng->GetLocalPlayer?eng->GetLocalPlayer():nullptr;if(!own || !player)return;
    // Predicted origin from player movement (updated every client frame) keeps the
    // map smooth while running; the server-updated entity origin steps with ping.
    const bool fresh=GetTickCount64()-predictedTick<250;const float *origin=fresh?predictedOrigin:player->origin;
    GL2D state(screen.iWidth,screen.iHeight);loadOverview(mapName());
    if(overview.texture)updateRadarPatch(origin,fresh);
    const float resolution=HudResolution::factor(screen.iWidth,screen.iHeight);
    float scale=(1.f+std::max(0,std::min(100,radarSizePercent))/100.f)*resolution;
    const float radius=84*scale;
    const float cx=16*resolution+radius,cy=22*resolution+radius;const int segments=HudResolution::segments(radius);
    float angles[3]={};if(eng->GetViewAngles)eng->GetViewAngles(angles);float yaw=angles[1]*.017453293f,sn=std::sin(yaw),cs=std::cos(yaw);
    // 3D radar: the disc, then the arrow and the players projected onto it. Until its map is
    // ready (or without multitexturing) the 2D radar below is drawn.
    if(radar3D&&overview.texture&&overview.generated){Radar3D::View view;const float feet=origin[2]-(fresh&&predictedDucking?18.f:36.f);
        if(Radar3D::draw(screen,cx,cy,radius,origin,yaw,radarCentered,feet,view)){
            float px,py;if(view.project(origin[0],origin[1],feet,px,py)){float hx=0,hy=-1;
                if(radarCentered){float ax,ay;if(view.project(origin[0]+cs*48,origin[1]+sn*48,feet,ax,ay)){float l=std::hypot(ax-px,ay-py);if(l>.001f){hx=(ax-px)/l;hy=(ay-py)/l;}}}
                drawRadarArrow(px,py,hx,hy,scale*std::clamp(radarArrowPercent,30,150)/100.f);}
            ULONGLONG tick=GetTickCount64();
            for(int i=1;i<=32;++i)if(contactVisible(i,own,tick)){
                float x=dots[i].xyz[0],y=dots[i].xyz[1],dx=x-view.target[0],dy=y-view.target[1],d=std::hypot(dx,dy),limit=view.radius*.96f;
                if(d>limit&&d>0){x=view.target[0]+dx*limit/d;y=view.target[1]+dy*limit/d;}   // kept on the disc's edge, like the 2D radar
                float sx,sy;if(view.project(x,y,dots[i].xyz[2]-36,sx,sy))drawPlayerMarker(sx,sy,team[i]==team[own],scale);
            }
            return;
        }
    }
    if(!overview.texture)circle(cx,cy,radius,16,22,26,185);
    const bool centered=radarCentered&&overview.texture;auto mapView=centeredRadarView(radius,5*scale);
    if(centered)circle(cx,cy,radius,12,16,20,210);
    if(overview.texture){
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,overview.texture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4ub(255,255,255,255);
        std::vector<RadarMapVertex> fan;float u,v;
        if(centered)fan=centeredRadarPolygon(cx,cy,radius,segments,mapView);
        else if(overview.isometric){float centerU,centerV;uv(origin[0],origin[1],origin[2],centerU,centerV);float spanS=overview.maxX-overview.minX,spanT=overview.maxY-overview.minY;
            fan.push_back({cx,cy,centerU,centerV});for(int i=0;i<=segments;++i){float a=i*6.2831853f/segments;fan.push_back({cx+std::cos(a)*radius,cy+std::sin(a)*radius,centerU+std::cos(a)*range/spanS,centerV-std::sin(a)*range/spanT});}}
        else {uv(origin[0],origin[1],origin[2],u,v);fan.push_back({cx,cy,u,v});
            for(int i=0;i<=segments;++i){float a=i*6.2831853f/segments,dx=std::cos(a)*range,dy=std::sin(a)*range;
                uv(origin[0]+dx*sn-dy*cs,origin[1]-dx*cs-dy*sn,origin[2],u,v);fan.push_back({cx+std::cos(a)*radius,cy+std::sin(a)*radius,u,v});}}
        const RadarPatch::Texture *inside=overview.isometric?nullptr:RadarPatch::shown;
        const bool insideReady=inside&&inside->done&&inside->id;
        // "Only the building": inside, the outside is not drawn until the player leaves.
        if(insideReady&&radarInsideOnly&&RadarPatch::inBuilding){if(!centered){glDisable(GL_TEXTURE_2D);circle(cx,cy,radius,16,22,26,185);glEnable(GL_TEXTURE_2D);glColor4ub(255,255,255,255);}}
        else{glBegin(GL_TRIANGLE_FAN);for(const auto &p:fan){glTexCoord2f(p.u,p.v);glVertex2f(p.x,p.y);}glEnd();}
        // Inside a building: its floor under the roof, over the same circle. The piece's texels
        // are the map's own pixels from (window + piece origin), so map coordinates convert directly.
        if(insideReady){const auto &piece=*inside->piece;const float baseX=static_cast<float>(inside->owner->ox+piece.x),baseY=static_cast<float>(inside->owner->oy+piece.y);
            glBindTexture(GL_TEXTURE_2D,inside->id);
            glBegin(GL_TRIANGLE_FAN);for(const auto &p:fan){glTexCoord2f((p.u*2048-baseX)/piece.w,(p.v*2048-baseY)/piece.h);glVertex2f(p.x,p.y);}glEnd();}
        glDisable(GL_TEXTURE_2D);
    }else {
        glColor4ub(79,108,113,80);glBegin(GL_LINES);glVertex2f(cx-radius,cy);glVertex2f(cx+radius,cy);glVertex2f(cx,cy-radius);glVertex2f(cx,cy+radius);glEnd();
    }
    float playerX=cx,playerY=cy;
    float headingX=0,headingY=-1;if(overview.isometric){headingX=(cs-sn)*.70710678f;headingY=(cs+sn)*.40824829f;float length=std::hypot(headingX,headingY);if(length>0){headingX/=length;headingY/=length;}}
    if(centered){float u,v,hu,hv;uv(origin[0],origin[1],origin[2],u,v);mapView.point(u,v,cx,cy,playerX,playerY);radarClampMarker(playerX,playerY,cx,cy,radius-14*scale);
        uv(origin[0]+cs*16,origin[1]+sn*16,origin[2],hu,hv);headingX=(hu-u)*mapView.scaleU;headingY=(hv-v)*mapView.scaleV;float length=std::hypot(headingX,headingY);if(length>.0001f){headingX/=length;headingY/=length;}else{headingX=0;headingY=-1;}}
    drawRadarArrow(playerX,playerY,headingX,headingY,scale*std::clamp(radarArrowPercent,30,150)/100.f);
    ULONGLONG tick=GetTickCount64();
    for(int i=1;i<=32;++i)if(contactVisible(i,own,tick)) {
        bool ally=team[i]==team[own];
        if(centered){float u,v,x,y;uv(dots[i].xyz[0],dots[i].xyz[1],dots[i].xyz[2],u,v);mapView.point(u,v,cx,cy,x,y);radarClampMarker(x,y,cx,cy,radius-7*scale);drawPlayerMarker(x,y,ally,scale);continue;}
        if(overview.isometric){float pu,pv,du,dv;uv(origin[0],origin[1],origin[2],pu,pv);uv(dots[i].xyz[0],dots[i].xyz[1],dots[i].xyz[2],du,dv);float spanS=overview.maxX-overview.minX,spanT=overview.maxY-overview.minY;
            float x=cx+(du-pu)*radius*spanS/range,y=cy-(dv-pv)*radius*spanT/range,d=std::hypot(x-cx,y-cy);if(d>radius-7*scale&&d>0){x=cx+(x-cx)*(radius-7*scale)/d;y=cy+(y-cy)*(radius-7*scale)/d;}drawPlayerMarker(x,y,ally,scale);continue;}
        float x=dots[i].xyz[0]-origin[0],y=dots[i].xyz[1]-origin[1];
        float dx=(x*sn-y*cs)*radius/range,dy=-(x*cs+y*sn)*radius/range;
        float distance=std::sqrt(dx*dx+dy*dy);if(distance>radius-7*scale){dx*=((radius-7*scale)/distance);dy*=((radius-7*scale)/distance);}
        drawPlayerMarker(cx+dx,cy+dy,ally,scale);
    }
    drawRadarBorder(cx,cy,radius,resolution);
}
static void frame() {
    installMessages();
    if(!frequency.QuadPart){QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&lastCounter);}
    LARGE_INTEGER counter;QueryPerformanceCounter(&counter);++frameCount;
    double elapsed=static_cast<double>(counter.QuadPart-lastCounter.QuadPart)/frequency.QuadPart;
    if(elapsed>=.5){fps=frameCount/elapsed;frameCount=0;lastCounter=counter;}
    bool customRadar=radarOn&&oldRadar&&oldTeam&&wglGetCurrentContext();
    if(lastRadarMode!=static_cast<int>(customRadar)&&eng->pfnClientCmd){
        if(customRadar || lastRadarMode==1)eng->pfnClientCmd(const_cast<char *>(customRadar?"hideradar\n":"drawradar\n"));lastRadarMode=customRadar;
    }
}
static void solidRect(const SCREENINFO &screen,int x,int y,int w,int h,int r,int g,int b) {
    if(w<=0||h<=0)return;
    if(wglGetCurrentContext()){
        GL2D state(screen.iWidth,screen.iHeight);glViewport(0,0,screen.iWidth,screen.iHeight);glDisable(GL_BLEND);
        glColor4ub(r,g,b,255);glBegin(GL_QUADS);glVertex2i(x,y);glVertex2i(x+w,y);glVertex2i(x+w,y+h);glVertex2i(x,y+h);glEnd();
    }else eng->pfnFillRGBA(x,y,w,h,r,g,b,255);
}
static void watermark(const SCREENINFO &screen) {
    if(!watermarkOn)return;hud_player_info_t info={};int own=localIndex();if(own&&eng->pfnGetPlayerInfo)eng->pfnGetPlayerInfo(own,&info);
    const char *raw=info.name?info.name:eng->pfnGetCvarString?eng->pfnGetCvarString(const_cast<char *>("name")):nullptr;
    std::string nick=raw?clean(raw,48):"Player";
    auto time=now();char suffix[96];snprintf(suffix,sizeof(suffix)," | %.0f fps | %d ms | %02u:%02u",fps,std::max(0,static_cast<int>(info.ping)),time.wHour,time.wMinute);
    std::string line="BH200 | "+nick+" | "+mapName()+suffix;int maximum=screen.iWidth-188;
    if(width(line)>maximum)line="BH200 | "+trim(nick,14)+" | "+trim(mapName(),16)+suffix;
    if(width(line)>maximum)line="BH200 | "+trim(nick,9)+" | "+trim(mapName(),10)+suffix;
    int w=std::min(screen.iWidth-20,std::max(300,width(line)+24)),x=screen.iWidth-w-10;
    const auto &t=inMenu?legacyTheme():hudTheme();solidRect(screen,x,10,w,30,16,22,24);eng->pfnFillRGBA(x,38,w,2,t.rgb[0],t.rgb[1],t.rgb[2],255);
    colorText(x+10,18,line,231,236,238);
}
// CS:GO-inspired compact HUD chat, replacing native message presentation.
struct ChatDisplay { std::string prefix,nick,body;bool radio=false; };
static ChatDisplay chatDisplay(const ChatLine &entry) {
    ChatDisplay result;result.nick=trim(entry.nick,24)+":";result.body=entry.body;
    for(;;){bool found=false;for(const char *tag:{"[Team] ","[Dead] ","[Spec] ","[Spectator] ","[RADIO] "}) {
        size_t length=strlen(tag);if(result.body.compare(0,length,tag)==0){
            if(!strcmp(tag,"[Team] "))result.prefix+="(TEAM) ";
            else if(!strcmp(tag,"[Dead] "))result.prefix+="*DEAD* ";
            else if(!strcmp(tag,"[Spec] ") || !strcmp(tag,"[Spectator] "))result.prefix+="(SPEC) ";
            else result.radio=true;
            result.body.erase(0,length);found=true;break;
        }
    }if(!found)break;}return result;
}
static void chatPlayerRgb(const ChatLine &entry,int &r,int &g,int &b){
    if(entry.side==1){r=232;g=112;b=97;return;} // Terrorist: warm red.
    if(entry.side==2){r=105;g=182;b=244;return;} // Counter-Terrorist: blue.
    static const int palette[][3]={{229,195,103},{107,185,244},{113,205,133},{224,153,109},{187,151,226}};
    unsigned hash=2166136261u;for(unsigned char c:entry.nick)hash=(hash^c)*16777619u;
    const auto &color=palette[hash%5];r=color[0];g=color[1];b=color[2];
}
static void chatPrefixRgb(const std::string &prefix,int &r,int &g,int &b){
    if(prefix.find("DEAD")!=std::string::npos){r=137;g=185;b=230;}
    else if(prefix.find("SPEC")!=std::string::npos){r=190;g=155;b=224;}
    else {r=224;g=196;b=132;}
}
static void chatBodyRgb(const ChatLine &entry,int &r,int &g,int &b){
    std::string lower=entry.body;for(char &c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(entry.nick=="Server" && lower.find("banned")!=std::string::npos){r=232;g=82;b=82;}
    else if(entry.nick=="Server" && !entry.body.empty() && entry.body[0]=='$'){r=141;g=211;b=133;}
    else if(entry.nick=="Server" && entry.body.find("[HNSRU]")!=std::string::npos){r=124;g=164;b=224;}
    else {r=220;g=223;b=218;}
}
static void chatText(int x,int y,const std::string &s,int r,int g,int b){
    colorText(x+1,y+1,s,0,0,0);colorText(x+1,y,s,r,g,b);colorText(x,y,s,r,g,b);
}
static void chatHud(const SCREENINFO &screen) {
    if(!chatOn || !oldSay)return;ChatFontScope fontScope;ULONGLONG tick=GetTickCount64();
    struct Display {const ChatLine *entry;ChatDisplay text;int leading;std::vector<std::string> lines;};
    std::vector<Display> display;int rows=0;
    int boxWidth=std::min(660,screen.iWidth-32),bodyWidth=boxWidth-24;
    chatScroll=std::max(0,std::min(std::max(0,static_cast<int>(history.size())-1),chatScroll));
    auto start=history.rbegin();if(chatOpened)std::advance(start,chatScroll);
    for(auto it=start;it!=history.rend()&&display.size()<(chatOpened?12:9);++it) {
        if(!chatOpened && (!it->tick || tick<it->tick || tick-it->tick>=12000))continue;
        auto message=chatDisplay(*it);int leading=width(message.prefix)+width(message.nick)+8+(message.radio?width("[RADIO]: "):0);
        // Avoid long names squeezing the message at small resolutions.
        if(leading>bodyWidth/2){message.nick=fitPixels(message.nick,std::max(40,bodyWidth/2-width(message.prefix)-8-(message.radio?width("[RADIO]: "):0)))+":";leading=width(message.prefix)+width(message.nick)+8+(message.radio?width("[RADIO]: "):0);}
        auto lines=wrapPixels(message.body,std::max(80,bodyWidth-leading));
        if(lines.size()>3){lines.resize(3);lines.back()=fitPixels(lines.back(),bodyWidth-leading-width("..."))+"...";}
        if(rows+static_cast<int>(lines.size())>11 && !display.empty())break;
        rows+=static_cast<int>(lines.size());display.push_back({&*it,std::move(message),leading,std::move(lines)});
    }
    if(display.empty())return;
    std::reverse(display.begin(),display.end());int x=16,y=screen.iHeight-96-rows*23;
    if(chatOpened){std::string hint="Chat history | Wheel / PgUp / PgDn | "+std::to_string(chatScroll)+" messages back";chatText(x,y-27,hint,185,194,204);}
    // No chat background: draw text and its small shadow directly on the game.
    
    for(const auto &message:display) {
        int r=0,g=0,b=0;chatPlayerRgb(*message.entry,r,g,b);
        bool server=message.entry->nick=="Server";bool money=server && !message.text.body.empty() && message.text.body[0]=='$';
        std::string lower=message.text.body;for(char &c:lower)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        bool ban=server && lower.find("banned")!=std::string::npos;
        for(size_t i=0;i<message.lines.size();++i){
            if(!i && !server){
                if(message.text.prefix.empty()){
                    if(wglGetCurrentContext()){GL2D state(screen.iWidth,screen.iHeight);circle(x+5,y+7,2,r,g,b,255);}
                    else eng->pfnFillRGBA(x+3,y+5,4,4,r,g,b,255);
                }
                int pr=0,pg=0,pb=0;chatPrefixRgb(message.text.prefix,pr,pg,pb);chatText(x+12,y,message.text.prefix,pr,pg,pb);
                chatText(x+12+width(message.text.prefix),y,message.text.nick,r,g,b);
                if(message.text.radio)chatText(x+12+width(message.text.prefix)+width(message.text.nick)+8,y,"[RADIO]: ",228,207,139);
            }
            int br=0,bg=0,bb=0;chatBodyRgb(*message.entry,br,bg,bb);
            if(ban){br=232;bg=82;bb=82;}else if(money){br=141;bg=211;bb=133;}
            chatText(x+12+(server?0:message.leading),y,message.lines[i],br,bg,bb);y+=23;
        }
    }
}
static void navigateHistory(int direction,int page=6) {historyOffset=std::max(0,std::min(std::max(0,static_cast<int>(history.size())-1),historyOffset+direction*page));historySelected=detailScroll=0;historyDetails=false;}
static void changeOption(int direction) {
    if(option==0)watermarkOn=!watermarkOn;else if(option==1)chatOn=!chatOn;else if(option==2)radarOn=!radarOn;
    else if(option==3)range=std::max(300,std::min(3000,range+direction*100));else opacity=100;
}
static void resetVideo(){Speedometer::reset();MovementHud::reset();{TimeFontScope scope;freeFont();}{ChatFontScope scope;freeFont();}freeFont();chatOpened=false;chatScroll=0;freeOverview();lastRadarMode=-1;for(auto &dot:dots)dot=RadarDot{};}
}
