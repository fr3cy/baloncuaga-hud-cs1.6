#pragma once
#include <sstream>
#include <array>
#include <string>
#include <cmath>
#include <cctype>

namespace JumpStats {
enum Style {Table,Mini,Lines,Contrast,SyncBars,Native,StyleCount,Classic=Table,Glass=Mini,Compact=Mini,Cards=Contrast};
static bool enabled=false;
static std::atomic<bool> presentable{false};
static int style=Mini,size=12,opacity=100,holdMs=7000;
static int summaryX=38,summaryY=62,tableX=98,tableY=18;
static int rgb[3]={125,235,225};
struct Row {std::string keys,gain,loss,frames,sync;int index=0;};
struct Summary {
    std::string type,distance,pre,maximum,fog,efficiency,sync,strafes,speed,gain,loss;
    std::vector<std::string> extra;
    bool empty()const{return type.empty()&&distance.empty()&&pre.empty()&&fog.empty();}
};
struct Snapshot {Summary live,final;std::vector<Row> rows;ULONGLONG liveTick=0,finalTick=0,rowsTick=0,until=0,rowsUntil=0;};
struct Meta {float x=0,y=0;ULONGLONG until=0;const void *owner=nullptr;bool director=false;};
struct Block {Meta meta;std::vector<std::string> lines;std::string raw;};
static Snapshot state;
static std::vector<Block> blocks;
static SRWLOCK lock=SRWLOCK_INIT;
static bool hooksAvailable;
static thread_local volatile bool insideNativeDraw=false;
static std::string clean(std::string s){
    for(char &c:s)if(static_cast<unsigned char>(c)<32&&c!='\n')c=' ';
    while(!s.empty()&&std::isspace(static_cast<unsigned char>(s.back())))s.pop_back();return s;
}
static std::string lower(std::string s){for(char &c:s)if(static_cast<unsigned char>(c)<128)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
static std::vector<std::string> lines(const std::string &text){
    std::vector<std::string> result;std::istringstream in(text);std::string s;while(std::getline(in,s))result.push_back(clean(s));if(!text.empty()&&text.back()==char(10))result.push_back({});return result;
}
static bool number(const std::string &s,bool integer=false){
    if(s.empty()||s.size()>28)return false;auto n=s;if(n.back()=='%')n.pop_back();
    for(char &c:n)if(c==',')c='.';char *end=nullptr;float value=strtof(n.c_str(),&end);
    return end&&end!=n.c_str()&&!*end&&std::isfinite(value)&&std::fabs(value)<=100000&&(!integer||std::floor(value)==value);
}
static bool rows(const std::vector<std::string> &text,std::vector<Row> &out){
    for(const auto &line:text){
        std::istringstream in(line);std::vector<std::string> values;std::string s;while(in>>s)values.push_back(s);
        if(values.empty()||(lower(line).find("gain")!=std::string::npos&&lower(line).find("sync")!=std::string::npos))continue;
        auto low=lower(line);auto first=low.find_first_not_of(' ');if(first!=std::string::npos)low.erase(0,first);
        if(low.rfind("jof:",0)==0||low.rfind("jof :",0)==0){auto at=low.find(':')+1;auto value=low.substr(at);auto start=value.find_first_not_of(' ');if(start!=std::string::npos&&number(value.substr(start)))continue;}
        size_t i=0;if(number(values[0],true))++i;
        if(values.size()!=i+5)return false;
        auto keys=values[i];if(keys.empty()||keys.size()>4||keys.find_first_not_of("WASDwasd")!=std::string::npos)return false;
        if(!number(values[i+1])||!number(values[i+2])||!number(values[i+3],true)||!number(values[i+4])||values[i+4].back()!='%')return false;
        auto percent=values[i+4];percent.pop_back();for(char &c:percent)if(c==',')c='.';
        if(strtof(percent.c_str(),nullptr)<0||strtof(percent.c_str(),nullptr)>100||strtof(values[i+3].c_str(),nullptr)<0)return false;
        int index=i?int(strtol(values[0].c_str(),nullptr,10)):int(out.size())+1;if(index<1||index>1000)return false;
        out.push_back({keys,values[i+1],values[i+2],values[i+3],values[i+4],index});if(out.size()>32)return false;
    }return !out.empty();
}
static std::string field(const std::string &text,const char *label,bool annotation=false){
    auto low=lower(text),tag=lower(label);size_t p=0,n=strlen(label);
    for(;;){p=low.find(tag,p);if(p==std::string::npos)return {};
        if((!p||!std::isalnum(static_cast<unsigned char>(low[p-1])))&&
           (p+n==low.size()||!std::isalnum(static_cast<unsigned char>(label[n-1]))||!std::isalnum(static_cast<unsigned char>(low[p+n]))))break;++p;}
    p+=n;
    while(p<text.size()&&(std::isspace(static_cast<unsigned char>(text[p]))||strchr(":=([",text[p])))++p;
    size_t start=p;if(p<text.size()&&(text[p]=='+'||text[p]=='-'))++p;
    while(p<text.size()&&(std::isdigit(static_cast<unsigned char>(text[p]))||text[p]=='.'||text[p]==','))++p;
    if(p<text.size()&&text[p]=='%')++p;
    auto value=text.substr(start,p-start);if(!number(value))return {};
    if(annotation&&p<text.size()&&text[p]=='('){auto end=text.find(')',p+1);
        if(end!=std::string::npos&&end-p<16&&number(text.substr(p+1,end-p-1)))value+=text.substr(p,end-p+1);}
    return value;
}
static bool summary(const std::string &text,Summary &out,bool &final){
    auto low=lower(text);
    out.distance=field(text,"distance");out.pre=field(text,"prestrafe",true);if(out.pre.empty())out.pre=field(text,"pre:",true);
    out.maximum=field(text,"maxspeed");if(out.maximum.empty())out.maximum=field(text,"max speed");
    out.strafes=field(text,"strafes");out.sync=field(text,"sync");out.efficiency=field(text,"eff");
    if(out.efficiency.empty())out.efficiency=field(text,"efficiency");
    out.fog=field(text,"fog");if(!number(out.fog,true))out.fog.clear();out.gain=field(text,"gain");out.loss=field(text,"loss");
    for(const char *name:{"up multis bj","up multisbj","multisbj","up multibhop","stand up bhop","standup bhop","standupbhop","stand bhop","longjump","long jump","highjump","high jump","countjump","count jump","mcj","dcj","scj","sbj","bhop","wj","hj","cj","lj","bj"}){
        size_t p=low.find(name);if(p==std::string::npos)continue;
        if((p&&std::isalnum(static_cast<unsigned char>(low[p-1])))||(p+strlen(name)<low.size()&&std::isalpha(static_cast<unsigned char>(low[p+strlen(name)]))))continue;
        out.type=text.substr(p,strlen(name));break;
    }
    // Common live format: "294 u/s\nsbj 1 FOG:\n256.499\n256.499".
    for(const auto &line:lines(text)){
        auto l=lower(line);if(l.find("u/s")!=std::string::npos||l.find("units/s")!=std::string::npos){
            std::istringstream in(line);std::string value;in>>value;if(number(value))out.speed=value;
        }
        {
            auto at=l.find("fog");if(at!=std::string::npos){size_t end=at;while(end&&std::isspace(static_cast<unsigned char>(line[end-1])))--end;size_t begin=end;
                while(begin&&std::isdigit(static_cast<unsigned char>(line[begin-1])))--begin;if(begin<end)out.fog=line.substr(begin,end-begin);}
        }
        std::istringstream in(line);std::string value,rest;if((in>>value)&&!(in>>rest)&&number(value))out.extra.push_back(value);
    }
    // Common HNS result: "up multisbj: 190.774 / max: 249.6 (9.6) / sbj (1): 315.6 / strafes: 1 (100%)".
    if(out.maximum.empty())out.maximum=field(text,"max");
    if(!out.type.empty()&&out.distance.empty()&&!out.strafes.empty()){
        auto at=low.find(lower(out.type));if(at!=std::string::npos){auto colon=text.find(':',at+out.type.size());
            if(colon!=std::string::npos&&text.find('\n',at)>colon){auto value=field(text,text.substr(at,colon-at+1).c_str());if(number(value))out.distance=value;}}
    }
    if(out.fog.empty()&&!out.type.empty()){auto at=low.find("sbj (");if(at==std::string::npos)at=low.find("sbj(");if(at!=std::string::npos){auto a=low.find('(',at),b=low.find(')',a);if(b!=std::string::npos&&number(low.substr(a+1,b-a-1),true))out.fog=low.substr(a+1,b-a-1);}}
    final=!out.distance.empty();
    return (final&&(!out.type.empty()||!out.pre.empty()||(!out.maximum.empty()&&!out.strafes.empty())))||(!out.type.empty()&&(!out.pre.empty()||!out.fog.empty()))||
        (!out.pre.empty()&&!out.strafes.empty()&&(!out.sync.empty()||!out.maximum.empty()));
}
static Snapshot snapshot(){AcquireSRWLockShared(&lock);auto result=state;ReleaseSRWLockShared(&lock);return result;}
struct NativeText {client_textmessage_t *message;const char *original;};
static std::vector<NativeText> hiddenText;
static SRWLOCK textLock=SRWLOCK_INIT;
static char *emptyNativeText;
static bool nativeReadable(const void *source,size_t bytes){
    uintptr_t at=reinterpret_cast<uintptr_t>(source),end=at+bytes;if(!source||end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
static bool writableText(client_textmessage_t *message){
    if(!nativeReadable(message,sizeof(*message)))return false;MEMORY_BASIC_INFORMATION info={};auto field=&message->pMessage;
    if(!VirtualQuery(field,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    return (info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))&&reinterpret_cast<uintptr_t>(field)+sizeof(*field)<=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
}
static bool nativeString(const char *text){
    if(!text)return false;size_t at=0;
    while(at<2049){MEMORY_BASIC_INFORMATION info={};auto address=reinterpret_cast<uintptr_t>(text+at);
        if(!VirtualQuery(text+at,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(end<=address)return false;
        size_t count=std::min(size_t(2049-at),size_t(end-address));if(memchr(text+at,0,count))return true;at+=count;
    }return false;
}
static void restoreNativeText(){
    AcquireSRWLockExclusive(&textLock);
    for(auto &entry:hiddenText)if(writableText(entry.message)&&entry.message->pMessage==emptyNativeText&&nativeString(entry.original))entry.message->pMessage=entry.original;
    hiddenText.clear();ReleaseSRWLockExclusive(&textLock);
}
static void syncNativeText(){
    restoreNativeText();if(!insideNativeDraw||!enabled||!hooksAvailable||!presentable.load(std::memory_order_acquire)||unloading)return;
    if(!emptyNativeText)emptyNativeText=static_cast<char *>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,1));if(!emptyNativeText)return;
    auto now=GetTickCount64();AcquireSRWLockShared(&lock);AcquireSRWLockExclusive(&textLock);
    for(const auto &block:blocks){auto message=const_cast<client_textmessage_t *>(static_cast<const client_textmessage_t *>(block.meta.owner));
        if(!message||block.meta.until<now||!writableText(message)||message->x!=block.meta.x||message->y!=block.meta.y||!nativeString(message->pMessage)||block.raw!=message->pMessage)continue;
        hiddenText.push_back({message,message->pMessage});message->pMessage=emptyNativeText;
    }
    ReleaseSRWLockExclusive(&textLock);ReleaseSRWLockShared(&lock);
}
static void reset(){restoreNativeText();AcquireSRWLockExclusive(&lock);state={};blocks.clear();ReleaseSRWLockExclusive(&lock);presentable=false;}
static bool capture(const std::string &raw,Meta meta){
    auto text=clean(raw);if(text.empty()||text.size()>2048||!std::isfinite(meta.x)||!std::isfinite(meta.y)||meta.x < -1||meta.x>1||meta.y < -1||meta.y>1)return false;
    auto textLines=lines(text);Summary parsed;std::vector<Row> parsedRows;bool final=false,isRows=rows(textLines,parsedRows);
    bool recognized=isRows||summary(text,parsed,final);ULONGLONG now=GetTickCount64();
    if(!recognized){auto low=lower(text);if(low.rfind("jof:",0)==0&&number(field(text,"jof")))recognized=true;}
    AcquireSRWLockExclusive(&lock);
    // A channel reused for an unrelated message must immediately lose its mask.
    for(auto it=blocks.begin();it!=blocks.end();){
        bool same=meta.owner?it->meta.owner==meta.owner:
            meta.director&&it->meta.director&&std::fabs(it->meta.x-meta.x)<.0001f&&std::fabs(it->meta.y-meta.y)<.0001f;
        if(same||it->meta.until<now)it=blocks.erase(it);else ++it;
    }
    if(recognized){
        if(isRows){state.rows=std::move(parsedRows);state.rowsTick=now;state.rowsUntil=meta.until;}
        else if(final){state.final=std::move(parsed);state.finalTick=now;}
        else {state.live=std::move(parsed);state.liveTick=now;}

        if((meta.owner||meta.director)&&blocks.size()<32)blocks.push_back({meta,lines(raw),raw});
    }
    state.until=0;for(const auto &b:blocks)state.until=std::max(state.until,b.meta.until);
    ReleaseSRWLockExclusive(&lock);syncNativeText();return recognized;
}
static Meta metadata(const client_textmessage_t *message){
    Meta result;if(!nativeReadable(message,sizeof(*message))||!nativeString(message->pMessage)||!std::isfinite(message->x)||!std::isfinite(message->y)||
        !std::isfinite(message->fadein)||!std::isfinite(message->holdtime)||!std::isfinite(message->fadeout))return result;
    result.owner=message;result.x=message->x;result.y=message->y;
    float fade=std::clamp(message->fadein,0.f,60.f);if(message->effect==2)fade*=std::min(size_t(2048),strlen(message->pMessage));
    float seconds=std::clamp(message->holdtime,0.f,60.f)+fade+std::clamp(message->fadeout,0.f,60.f);
    result.until=GetTickCount64()+ULONGLONG(std::clamp(seconds,0.1f,60.f)*1000);return result;
}
static void load(){
    enabled=GetPrivateProfileIntA("interface","strafe_stats",GetPrivateProfileIntA("interface","jump_stats",0,iniPath),iniPath)!=0;
    style=std::clamp(int(GetPrivateProfileIntA("interface","strafe_style",Mini,iniPath)),0,int(StyleCount)-1);
    holdMs=std::clamp(int(GetPrivateProfileIntA("interface","strafe_hold_ms",7000,iniPath)),1000,12000);
    size=std::clamp(int(GetPrivateProfileIntA("interface","strafe_size",12,iniPath)),10,18);
    summaryX=std::clamp(int(GetPrivateProfileIntA("interface","jump_stats_x",38,iniPath)),0,100);
    summaryY=std::clamp(int(GetPrivateProfileIntA("interface","jump_stats_y",62,iniPath)),0,100);
    tableX=std::clamp(int(GetPrivateProfileIntA("interface","strafe_right",98,iniPath)),0,100);
    tableY=std::clamp(int(GetPrivateProfileIntA("interface","strafe_y",18,iniPath)),0,100);
    opacity=std::clamp(int(GetPrivateProfileIntA("interface","strafe_opacity",100,iniPath)),0,100);
    for(int i=0;i<3;++i){char key[48];snprintf(key,sizeof(key),"jump_stats_rgb_%d",i);rgb[i]=std::clamp(int(GetPrivateProfileIntA("interface",key,i==0?125:i==1?235:225,iniPath)),0,255);}
}
static void save(){
    const char *keys[]={"strafe_stats","strafe_style","strafe_size","jump_stats_x","jump_stats_y","strafe_right","strafe_y"};
    int values[]={enabled,style,size,summaryX,summaryY,tableX,tableY};char value[24];
    for(int i=0;i<7;++i){snprintf(value,sizeof(value),"%d",values[i]);WritePrivateProfileStringA("interface",keys[i],value,iniPath);}
    snprintf(value,sizeof(value),"%d",holdMs);WritePrivateProfileStringA("interface","strafe_hold_ms",value,iniPath);
    snprintf(value,sizeof(value),"%d",opacity);WritePrivateProfileStringA("interface","strafe_opacity",value,iniPath);
    for(int i=0;i<3;++i){char key[48];snprintf(key,sizeof(key),"jump_stats_rgb_%d",i);snprintf(value,sizeof(value),"%d",rgb[i]);WritePrivateProfileStringA("interface",key,value,iniPath);}
}
struct Rect {int x1,y1,x2,y2;};
static thread_local volatile bool nativeDrawing=false;
static thread_local volatile int nativeCount=0;
static thread_local volatile Rect nativeRects[32]={};
using Character=int (*)(int,int,int,int,int,int);
using Character2=int (*)(int,int,int,unsigned int);
using CharacterAdd=int (*)(int,int,int,int,int,int,unsigned int);
static Character oldCharacter;
static Character2 oldCharacter2;
static CharacterAdd oldCharacterAdd;
static bool masked(int x,int y){
    if(!enabled||!nativeDrawing||!presentable.load(std::memory_order_acquire))return false;
    for(int i=0;i<nativeCount;++i)if(x>=nativeRects[i].x1&&x<=nativeRects[i].x2&&y>=nativeRects[i].y1&&y<=nativeRects[i].y2)return true;return false;
}
static int character(int x,int y,int ch,int r,int g,int b){CallbackScope scope;if(masked(x,y))x=y=-16384;return oldCharacter(x,y,ch,r,g,b);}
static int character2(int x,int y,int ch,unsigned int font){CallbackScope scope;if(masked(x,y))x=y=-16384;return oldCharacter2(x,y,ch,font);}
static int characterAdd(int x,int y,int ch,int r,int g,int b,unsigned int font){CallbackScope scope;if(masked(x,y))x=y=-16384;return oldCharacterAdd(x,y,ch,r,g,b,font);}
static void install(){
    bool a=eng->pfnDrawCharacter&&MH_CreateHook(reinterpret_cast<void *>(eng->pfnDrawCharacter),reinterpret_cast<void *>(character),reinterpret_cast<void **>(&oldCharacter))==MH_OK;
    bool b=eng->pfnVGUI2DrawCharacter&&MH_CreateHook(reinterpret_cast<void *>(eng->pfnVGUI2DrawCharacter),reinterpret_cast<void *>(character2),reinterpret_cast<void **>(&oldCharacter2))==MH_OK;
    bool c=eng->pfnVGUI2DrawCharacterAdd&&MH_CreateHook(reinterpret_cast<void *>(eng->pfnVGUI2DrawCharacterAdd),reinterpret_cast<void *>(characterAdd),reinterpret_cast<void **>(&oldCharacterAdd))==MH_OK;
    hooksAvailable=(a||b||c)&&(!eng->pfnDrawCharacter||a)&&(!eng->pfnVGUI2DrawCharacter||b)&&(!eng->pfnVGUI2DrawCharacterAdd||c);char line[100];snprintf(line,sizeof(line),"Jump stats: native glyph capture legacy=%d VGUI=%d additive=%d.",int(a),int(b),int(c));logLine(line);
}
struct NativeScope {
    bool previous,previousInside;int previousCount;
    NativeScope():previous(nativeDrawing),previousInside(insideNativeDraw),previousCount(nativeCount){
        insideNativeDraw=true;
        syncNativeText();nativeDrawing=false;nativeCount=0;if(!enabled||!hooksAvailable||!presentable.load(std::memory_order_acquire)||!eng||!eng->pfnGetScreenInfo)return;
        SCREENINFO screen={};screen.iSize=sizeof(screen);if(!eng->pfnGetScreenInfo(&screen)||screen.iWidth<=0||screen.iHeight<=0)return;
        ULONGLONG now=GetTickCount64();AcquireSRWLockShared(&lock);
        for(const auto &block:blocks){
            if(block.meta.until<now||nativeCount>=32)continue;
            int width=0;for(const auto &line:block.lines){int w=0;for(unsigned char ch:line)w+=std::max(1,int(screen.charWidths[ch]));width=std::max(width,w);}
            int height=std::max(1,screen.iCharHeight)*int(block.lines.size());
            int x=block.meta.x==-1?(screen.iWidth-width)/2:block.meta.x<0?int((1+block.meta.x)*screen.iWidth-width):int(block.meta.x*screen.iWidth);
            int y=block.meta.y==-1?(screen.iHeight-height)/2:block.meta.y<0?int((1+block.meta.y)*screen.iHeight-height):int(block.meta.y*screen.iHeight);
            x=std::clamp(x,0,std::max(0,screen.iWidth-width));y=std::clamp(y,0,std::max(0,screen.iHeight-height));
            int n=nativeCount++;nativeRects[n].x1=x-3;nativeRects[n].y1=y-3;nativeRects[n].x2=x+width+3;nativeRects[n].y2=y+height+3;
        }
        ReleaseSRWLockShared(&lock);nativeDrawing=true;
    }
    ~NativeScope(){restoreNativeText();insideNativeDraw=previousInside;if(previousInside)syncNativeText();nativeDrawing=previous;nativeCount=previousCount;}
};
static bool visible(ULONGLONG now){
    if(!enabled||!hooksAvailable)return false;auto value=snapshot();auto last=value.rowsTick;
    return enabled&&hooksAvailable&&last&&now>=last&&(now-last<ULONGLONG(holdMs)||now<value.rowsUntil)&&!value.rows.empty();
}
}
