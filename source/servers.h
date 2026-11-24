#pragma once
// Borrow the game's existing Steam client. No new pipe, callbacks, networking
// worker or Steam initialization. History/favorite writes use Valve's API.
namespace Servers {
static constexpr uint32_t Favorite=1,History=2;
struct Entry {uint32_t app=10,ip=0;uint16_t port=0,query=0;uint32_t flags=0,last=0;};
struct Row {uint32_t ip=0;uint16_t port=0;uint32_t flags=0,last=0;std::string name;std::vector<Entry> entries;};
static uint64_t key(uint32_t ip,uint16_t port){return (uint64_t(ip)<<16)|port;}
static std::string address(uint32_t ip,uint16_t port){char s[48];snprintf(s,sizeof(s),"%u.%u.%u.%u:%u",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255,unsigned(port));return s;}
static bool parse(const char *s,uint32_t &ip,uint16_t &port){
    unsigned a,b,c,d,p;char tail;int n=sscanf(s,"%u.%u.%u.%u:%u%c",&a,&b,&c,&d,&p,&tail);
    if(n!=5||a>255||b>255||c>255||d>255||!p||p>65535)return false;
    ip=(a<<24)|(b<<16)|(c<<8)|d;port=uint16_t(p);return ip!=0;
}
using Count=int (__thiscall *)(void *);
using Read=bool (__thiscall *)(void *,int,uint32_t *,uint32_t *,uint16_t *,uint16_t *,uint32_t *,uint32_t *);
using Add=int (__thiscall *)(void *,uint32_t,uint32_t,uint16_t,uint16_t,uint32_t,uint32_t);
using Remove=bool (__thiscall *)(void *,uint32_t,uint32_t,uint16_t,uint16_t,uint32_t);
struct Api {void *object=nullptr;Count count=nullptr;Read read=nullptr;Add add=nullptr;Remove remove=nullptr;explicit operator bool()const{return object&&count&&read&&add&&remove;}};
static Api resolve(){
    HMODULE api=GetModuleHandleA("steam_api.dll"),clientModule=GetModuleHandleA("steamclient.dll");if(!api||!clientModule)return {};
    using Factory=void *(__cdecl *)(const char *,int *);using Handle=int (__cdecl *)();using Get=void *(__thiscall *)(void *,int,int,const char *);
    auto factory=reinterpret_cast<Factory>(GetProcAddress(clientModule,"CreateInterface"));auto user=reinterpret_cast<Handle>(GetProcAddress(api,"SteamAPI_GetHSteamUser"));auto pipe=reinterpret_cast<Handle>(GetProcAddress(api,"SteamAPI_GetHSteamPipe"));
    if(!factory||!user||!pipe)return {};int u=user(),p=pipe();if(!u||!p)return {};
    void *client=factory("SteamClient020",nullptr);auto get=Profile::method<Get>(client,10,clientModule);if(!get)return {};
    void *object=get(client,u,p,"SteamMatchMaking009");
    Api result{object,Profile::method<Count>(object,0,clientModule),Profile::method<Read>(object,1,clientModule),Profile::method<Add>(object,2,clientModule),Profile::method<Remove>(object,3,clientModule)};return result;
}
static std::string token(const std::string &s,size_t &at){
    while(at<s.size()){
        if(isspace(static_cast<unsigned char>(s[at]))){++at;continue;}
        if(s[at]=='/'&&at+1<s.size()&&s[at+1]=='/'){while(at<s.size()&&s[at]!='\n')++at;continue;}break;
    }
    if(at>=s.size())return {};char ch=s[at++];if(ch=='{'||ch=='}')return std::string(1,ch);
    std::string out;if(ch=='"'){while(at<s.size()){ch=s[at++];if(ch=='"')break;if(ch=='\\'&&at<s.size()&&(s[at]=='"'||s[at]=='\\'))ch=s[at++];if(out.size()<512)out+=ch;}}
    else {out+=ch;while(at<s.size()&&!isspace(static_cast<unsigned char>(s[at]))&&s[at]!='{'&&s[at]!='}')if(out.size()<512)out+=s[at++];else ++at;}return out;
}
static std::map<uint64_t,std::string> namesFromText(const std::string &text){
    std::map<uint64_t,std::string> out;if(text.size()>8*1024*1024)return out;
    struct Node {std::string name,addr;};std::vector<Node> stack(1);size_t at=0;unsigned tokens=0;
    while(at<text.size()&&++tokens<100000){auto k=token(text,at);if(k.empty())continue;
        if(k=="}"){if(stack.size()>1){auto n=stack.back();stack.pop_back();uint32_t ip;uint16_t port;if(parse(n.addr.c_str(),ip,port)&&!n.name.empty())out[key(ip,port)]=n.name;}continue;}
        if(k=="{"){if(stack.size()>=16)return {};stack.push_back({});continue;}
        auto v=token(text,at);if(v=="{"){if(stack.size()>=16)return {};stack.push_back({});continue;}
        for(char &c:k)c=char(tolower(static_cast<unsigned char>(c)));if(k=="name")stack.back().name=v;else if(k=="address")stack.back().addr=v;
    }return out;
}
static std::map<uint64_t,std::string> cachedNames(){
    HKEY registry=nullptr;wchar_t steam[32768]={};DWORD bytes=sizeof(steam),type=0;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",0,KEY_QUERY_VALUE,&registry)!=ERROR_SUCCESS)return {};
    LONG result=RegQueryValueExW(registry,L"SteamPath",nullptr,&type,reinterpret_cast<BYTE *>(steam),&bytes);RegCloseKey(registry);
    steam[32767]=0;uint64_t account=Profile::localAccount();if(result!=ERROR_SUCCESS||type!=REG_SZ||!account||account<76561197960265728ULL)return {};
    std::wstring path=std::wstring(steam)+L"/userdata/"+std::to_wstring(account-76561197960265728ULL)+L"/7/remote/serverbrowser_hist.vdf";
    std::vector<byte> data;bool exists=false;if(!EmbeddedInstaller::read(path,data,exists)||!exists)return {};
    return namesFromText(std::string(data.begin(),data.end()));
}
static std::vector<Row> merge(const std::vector<Entry> &entries,const std::map<uint64_t,std::string> &names){
    std::map<uint64_t,Row> byAddress;
    for(const auto &e:entries){if((e.app!=10&&e.app!=70&&e.app!=0)||!e.ip||!e.port||!(e.flags&3))continue;
        auto k=key(e.ip,e.port);auto &r=byAddress[k];r.ip=e.ip;r.port=e.port;r.flags|=e.flags&3;r.last=std::max(r.last,e.last);r.entries.push_back(e);
        auto n=names.find(k);if(n==names.end()&&e.query)n=names.find(key(e.ip,e.query));if(n!=names.end())r.name=ServerNames::clean(n->second);
    }
    std::vector<Row> out;for(auto &r:byAddress)out.push_back(std::move(r.second));
    std::sort(out.begin(),out.end(),[](const Row &a,const Row &b){if(bool(a.flags&Favorite)!=bool(b.flags&Favorite))return bool(a.flags&Favorite);return a.last!=b.last?a.last>b.last:key(a.ip,a.port)<key(b.ip,b.port);});return out;
}
static bool enumerate(Api api,std::vector<Entry> &out){
    if(!api)return false;int count=api.count(api.object);if(count<0||count>10000)return false;
    for(int i=0;i<count;++i){if(unloading)return false;Entry e;if(api.read(api.object,i,&e.app,&e.ip,&e.port,&e.query,&e.flags,&e.last))out.push_back(e);}return true;
}
static SRWLOCK lock=SRWLOCK_INIT;
static std::vector<Row> rows;
static std::vector<Entry> undo;
static std::map<uint64_t,std::string> liveNames;
static std::atomic<bool> busy{false};
static std::string status;
static bool requested;
static std::vector<Row> snapshot(){AcquireSRWLockShared(&lock);auto out=rows;ReleaseSRWLockShared(&lock);return out;}
static std::string message(){AcquireSRWLockShared(&lock);auto out=status;ReleaseSRWLockShared(&lock);return out;}
struct NameBatch {std::vector<Row> list;size_t next=0;unsigned epoch=0;};
static void nameStep(std::shared_ptr<NameBatch> batch){
    if(ServerNames::cancelled(batch->epoch))return;ServerNames::Winsock session;
    for(int i=0;session.ok&&i<2&&batch->next<batch->list.size()&&!ServerNames::cancelled(batch->epoch);++i){
        auto row=batch->list[batch->next++];uint16_t query=row.entries.empty()?row.port:row.entries.front().query;if(!query)query=row.port;
        auto name=ServerNames::query(row.ip,query,batch->epoch);
        if(name.empty()&&query!=row.port&&!ServerNames::cancelled(batch->epoch))name=ServerNames::query(row.ip,row.port,batch->epoch);
        if(!name.empty()&&!ServerNames::cancelled(batch->epoch)){
            AcquireSRWLockExclusive(&lock);liveNames[key(row.ip,row.port)]=name;for(auto &r:rows)if(r.ip==row.ip&&r.port==row.port)r.name=name;ReleaseSRWLockExclusive(&lock);
        }
    }
    if(ServerNames::cancelled(batch->epoch))return;
    if(session.ok&&batch->next<batch->list.size())BackgroundIo::post([batch]{nameStep(batch);});else ServerNames::pending=false;
}
static void startNames(const std::vector<Row> &list){
    auto batch=std::make_shared<NameBatch>();batch->epoch=++ServerNames::generation;
    for(const auto &r:list)if(r.name.empty()||r.name==address(r.ip,r.port))batch->list.push_back(r);
    ServerNames::pending=!batch->list.empty();if(!batch->list.empty())BackgroundIo::post([batch]{nameStep(batch);});
}
static void publish(Api api,const std::string &message){
    std::vector<Entry> data;bool ok=enumerate(api,data);auto names=ok?cachedNames():std::map<uint64_t,std::string>{};AcquireSRWLockShared(&lock);for(const auto &n:liveNames)names[n.first]=n.second;ReleaseSRWLockShared(&lock);auto list=merge(data,names);
    AcquireSRWLockExclusive(&lock);if(ok)rows=std::move(list);status=ok?message:u8"Список Steam недоступен. Запусти CS через Steam и нажми «Обновить».";ReleaseSRWLockExclusive(&lock);if(ok)startNames(snapshot());
}
static void refresh(){if(busy.exchange(true))return;++ServerNames::generation;ServerNames::pending=false;requested=true;BackgroundIo::post([]{publish(resolve(),{});busy=false;});}
static std::vector<Entry> remove(Api api,const std::vector<Row> &list,uint32_t flags){
    std::vector<Entry> removed;if(!api)return removed;
    for(const auto &r:list)for(auto e:r.entries){e.flags&=flags;if(e.flags&&api.remove(api.object,e.app,e.ip,e.port,e.query,e.flags))removed.push_back(e);}return removed;
}
static void erase(std::vector<Row> list,uint32_t flags){
    if(busy.exchange(true))return;++ServerNames::generation;ServerNames::pending=false;BackgroundIo::post([list=std::move(list),flags]{auto api=resolve();auto removed=remove(api,list,flags);AcquireSRWLockExclusive(&lock);undo=removed;ReleaseSRWLockExclusive(&lock);publish(api,removed.empty()?u8"Не удалось удалить выбранные записи.":u8"Записи удалены. Можно отменить последнее удаление.");busy=false;});
}
static void favorite(Row row,bool add){
    if(busy.exchange(true))return;++ServerNames::generation;ServerNames::pending=false;BackgroundIo::post([row=std::move(row),add]{auto api=resolve();bool ok=false;if(api){
        if(add){auto e=row.entries.empty()?Entry{}:row.entries.front();e.ip=row.ip;e.port=row.port;if(!e.query)e.query=e.port;ok=api.add(api.object,e.app,e.ip,e.port,e.query,Favorite,e.last)>=0;}
        else ok=!remove(api,{row},Favorite).empty();}
        publish(api,ok?std::string{}:std::string(u8"Не удалось изменить избранное."));busy=false;});
}
static bool canUndo(){AcquireSRWLockShared(&lock);bool value=!undo.empty();ReleaseSRWLockShared(&lock);return value;}
static void restore(){
    if(busy.exchange(true))return;++ServerNames::generation;ServerNames::pending=false;BackgroundIo::post([]{auto api=resolve();AcquireSRWLockExclusive(&lock);auto data=undo;undo.clear();ReleaseSRWLockExclusive(&lock);bool ok=bool(api);
        if(api)for(const auto &e:data)ok=(api.add(api.object,e.app,e.ip,e.port,e.query,e.flags,e.last)>=0)&&ok;
        publish(api,ok?u8"Последнее удаление отменено.":u8"Не удалось восстановить записи.");busy=false;});
}
static bool connect(uint32_t ip,uint16_t port){
    if(!ip||!port||!eng||!eng->pfnClientCmd)return false;auto command=std::string("connect ")+address(ip,port)+"\n";
    opened=false;ChatEditor::close();Mouse::sync();eng->pfnClientCmd(command.data());return true;
}
}
