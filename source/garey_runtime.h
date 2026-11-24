#pragma once
// Install the reduced FPS/voice component for the next normal loader pass.
// Never load a second hook provider into the running process or unload it.
namespace GareyRuntime {
enum State {NotStarted,Loaded,Existing,Conflict,Failed,RestartRequired,NotAutoloaded};
static State state=NotStarted;
static DWORD error;
static HMODULE module;
static ULONGLONG started;
static constexpr size_t payloadBytes=1398784;
static constexpr uint64_t payloadHash=UINT64_C(0xfa46b10619bceea5);
static constexpr const wchar_t *automaticName=L"cs_fix.asi";
static constexpr const wchar_t *previousAutomaticName=L"00_BALONCUAGA_cs_patches.asi";
static bool minimal;
static std::wstring modulePath;
static std::vector<std::wstring> legacy;
static bool oldPayload(const std::vector<byte> &data){
    if(data.size()==1396736){auto h=EmbeddedInstaller::hash(data);return h==UINT64_C(0x63df6bccf68ab85f)||h==UINT64_C(0xc47f0ebd51507fa9);}
    return data.size()==1397248&&EmbeddedInstaller::hash(data)==UINT64_C(0x9c1a56c7da3c94aa);
}
static DWORD progress(size_t offset){
    if(!module||!minimal)return 0;
    auto p=reinterpret_cast<const DWORD *>(reinterpret_cast<uintptr_t>(module)+offset);
    MEMORY_BASIC_INFORMATION info={};
    if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return 0;
    return *p;
}
static bool ready(){return minimal?progress(0xa88000)==2:Garey::variable("fps_visual")!=nullptr;}
static bool pending(){return minimal&&progress(0xa88000)<2;}
static std::wstring lower(std::wstring s){for(auto &c:s)c=c==L'/'?L'\\':towlower(c);return s;}
static bool finalPath(const std::wstring &path,bool folder,std::wstring &out){
    HANDLE file=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(folder?FILE_FLAG_BACKUP_SEMANTICS:0),nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION info={};wchar_t resolved[32768];
    bool ok=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
    DWORD n=ok?GetFinalPathNameByHandleW(file,resolved,32768,FILE_NAME_NORMALIZED):0;DWORD e=GetLastError();CloseHandle(file);
    if(!ok||!n||n>=32768){SetLastError(e?e:ERROR_BAD_PATHNAME);return false;}
    out=lower(std::wstring(resolved,n));while(!out.empty()&&(out.back()==L'\\'||out.back()==L'/'))out.pop_back();return true;
}
static bool within(const std::wstring &root,const std::wstring &path){
    std::wstring base,file;if(!finalPath(root,true,base)||!finalPath(path,false,file))return false;
    base+=L'\\';if(file.rfind(base,0)!=0){SetLastError(ERROR_ACCESS_DENIED);return false;}return true;
}
static void remember(const std::wstring &path){
    std::wstring key;if(!finalPath(path,false,key))key=lower(path);
    for(const auto &p:legacy){std::wstring old;if(!finalPath(p,false,old))old=lower(p);if(old==key)return;}
    legacy.push_back(path);
}
static bool payload(HMODULE owner,std::vector<byte> &out){
    auto resource=FindResourceW(owner,MAKEINTRESOURCEW(6000),MAKEINTRESOURCEW(10));
    if(!resource){SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);return false;}
    auto loaded=LoadResource(owner,resource);
    if(!loaded||!EmbeddedSkyboxes::unpack(static_cast<const byte *>(LockResource(loaded)),SizeofResource(owner,resource),out)||out.size()!=payloadBytes||EmbeddedInstaller::hash(out)!=payloadHash){SetLastError(ERROR_INVALID_DATA);return false;}
    return true;
}
static bool prepare(HMODULE owner,const std::wstring &root,std::wstring &path){
    std::vector<byte> expected,existing;bool exists=false;if(!payload(owner,expected))return false;
    const std::wstring relative=automaticName;
    if(!EmbeddedInstaller::parents(root,relative))return false;path=root+L"/"+relative;
    if(!EmbeddedInstaller::read(path,existing,exists))return false;
    if(exists&&existing!=expected){SetLastError(ERROR_CRC);return false;}
    return exists||EmbeddedInstaller::create(path,expected);
}
static HMODULE existingModule(){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());if(snapshot==INVALID_HANDLE_VALUE)return nullptr;
    MODULEENTRY32W entry={};entry.dwSize=sizeof(entry);HMODULE found=nullptr;
    if(Module32FirstW(snapshot,&entry))do {
        std::wstring name=entry.szModule;for(auto &c:name)c=towlower(c);
        const bool named=name.find(L"cs_patches")!=std::wstring::npos||name.find(L"csfix")!=std::wstring::npos||name.find(L"cs_fix")!=std::wstring::npos;
        if(!named&&entry.modBaseSize!=0xa87000&&entry.modBaseSize!=0xa88000&&entry.modBaseSize!=0xa89000)continue;
        std::vector<byte> bytes;bool exists=false;
        if(EmbeddedInstaller::read(entry.szExePath,bytes,exists)&&exists){
            if(bytes.size()==payloadBytes&&EmbeddedInstaller::hash(bytes)==payloadHash){found=entry.hModule;minimal=true;modulePath=entry.szExePath;continue;}
            if(oldPayload(bytes)){remember(entry.szExePath);continue;}
        }
        if(named){state=Conflict;error=ERROR_ALREADY_EXISTS;}
    }while(Module32NextW(snapshot,&entry));CloseHandle(snapshot);return found;
}
static bool existedAtProcessStart(const std::wstring &path){
    FILETIME created,exited,kernel,user;WIN32_FILE_ATTRIBUTE_DATA info={};
    return GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)&&GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)&&
        CompareFileTime(&info.ftCreationTime,&created)<0;
}
using Finder=HMODULE (*)();
static void diskLegacy(const std::wstring &folder){
    WIN32_FIND_DATAW entry={};auto search=FindFirstFileW((folder+L"/*.asi").c_str(),&entry);if(search==INVALID_HANDLE_VALUE)return;
    do {if(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        if(entry.nFileSizeHigh||(entry.nFileSizeLow!=1396736&&entry.nFileSizeLow!=1397248))continue;
        auto path=folder+L"/"+entry.cFileName;std::vector<byte> bytes;bool exists=false;
        if(EmbeddedInstaller::read(path,bytes,exists)&&exists&&oldPayload(bytes))remember(path);
    }while(FindNextFileW(search,&entry));FindClose(search);
}
struct Retired {std::wstring path,backup;};
static bool undo(const std::vector<Retired> &files){bool ok=true;for(auto i=files.rbegin();i!=files.rend();++i)if(!MoveFileExW(i->backup.c_str(),i->path.c_str(),MOVEFILE_WRITE_THROUGH))ok=false;return ok;}
static bool normalizeName(HMODULE owner,const std::wstring &root,bool &renamed){
    renamed=false;std::vector<std::wstring> names={root+L"/"+previousAutomaticName,root+L"/scripts/"+previousAutomaticName};
    if(!modulePath.empty()&&lower(modulePath).find(lower(previousAutomaticName))!=std::wstring::npos)names.push_back(modulePath);
    std::vector<std::wstring> paths;
    for(const auto &name:names){
        std::vector<byte> bytes;bool exists=false;if(!EmbeddedInstaller::read(name,bytes,exists))return false;if(!exists)continue;
        if(bytes.size()!=payloadBytes||EmbeddedInstaller::hash(bytes)!=payloadHash){SetLastError(ERROR_ALREADY_EXISTS);return false;}
        if(!within(root,name))return false;
        std::wstring key;if(!finalPath(name,false,key))return false;bool duplicate=false;
        for(const auto &p:paths){std::wstring other;if(finalPath(p,false,other)&&other==key)duplicate=true;}
        if(!duplicate)paths.push_back(name);
    }
    if(paths.empty())return true;
    std::vector<byte> expected,existing;bool targetExists=false;if(!payload(owner,expected))return false;
    auto target=root+L"/"+automaticName;
    if(!EmbeddedInstaller::read(target,existing,targetExists))return false;
    if(targetExists&&existing!=expected){SetLastError(ERROR_CRC);return false;}
    std::vector<Retired> changes;changes.reserve(paths.size());
    for(const auto &path:paths){
        bool moved=false;std::wstring destination;
        if(!targetExists){destination=target;moved=MoveFileExW(path.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)!=0;}
        else for(unsigned n=0;n<64;++n){
            destination=path+L".baloncuaga-v32"+(n?L"-"+std::to_wstring(n):L"")+L".disabled";
            if(MoveFileExW(path.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){moved=true;break;}
            auto e=GetLastError();if(e!=ERROR_ALREADY_EXISTS&&e!=ERROR_FILE_EXISTS)break;
        }
        if(!moved){auto e=GetLastError();undo(changes);SetLastError(e);return false;}
        changes.push_back({path,destination});targetExists=true;
    }
    renamed=true;return true;
}
static bool retire(const std::wstring &root,std::vector<Retired> &files){
    // Verify the complete set before the first rename. Files outside this game
    // installation and unknown versions are never modified.
    for(const auto &path:legacy){std::vector<byte> bytes;bool exists=false;
        if(!within(root,path)||!EmbeddedInstaller::read(path,bytes,exists))return false;
        if(!exists||!oldPayload(bytes)){SetLastError(ERROR_CRC);return false;}
    }
    for(const auto &path:legacy){bool moved=false;
        for(unsigned n=0;n<64;++n){auto backup=path+L".baloncuaga-v32"+(n?L"-"+std::to_wstring(n):L"")+L".disabled";
            if(MoveFileExW(path.c_str(),backup.c_str(),MOVEFILE_WRITE_THROUGH)){files.push_back({path,backup});moved=true;break;}
            auto e=GetLastError();if(e!=ERROR_ALREADY_EXISTS&&e!=ERROR_FILE_EXISTS)break;
        }
        if(!moved){auto e=GetLastError();undo(files);files.clear();SetLastError(e);return false;}
    }return true;
}
static bool start(HMODULE owner,const std::wstring &root,Finder find=existingModule){
    started=GetTickCount64();state=NotStarted;error=0;module=nullptr;minimal=false;modulePath.clear();legacy.clear();
    module=find();
    if(state==Conflict){logLine("CS Fix: a different CS Fix module is already loaded; duplicate skipped.");return false;}
    if(find==existingModule){diskLegacy(root);diskLegacy(root+L"/scripts");}
    std::vector<Retired> retired;
    if(!legacy.empty()){
        std::vector<byte> expected;if(!payload(owner,expected)||!retire(root,retired)){state=Failed;error=GetLastError();return false;}
    }
    bool renamed=false;if(!normalizeName(owner,root,renamed)){state=Failed;error=GetLastError();undo(retired);return false;}
    if(module||Garey::variable("fps_visual")){
        if(retired.empty()){state=Existing;return true;}
    }
    std::wstring path;if(!prepare(owner,root,path)){state=Failed;error=GetLastError();undo(retired);}
    else {state=retired.empty()&&!renamed&&existedAtProcessStart(path)?NotAutoloaded:RestartRequired;error=0;return false;}
    char line[128];snprintf(line,sizeof(line),"CS Fix: automatic load failed, Windows error %lu; HUD continues.",static_cast<unsigned long>(error));logLine(line);return false;
}
static void awaitAutoload(Finder find=existingModule,unsigned attempts=500){
    if(state!=NotAutoloaded)return;
    // The short filename can be enumerated after the HUD. Await that normal
    // loader pass before installing client hooks; never LoadLibrary it late.
    for(unsigned i=0;i<attempts&&!unloadRequested.load(std::memory_order_acquire);++i){
        if((module=find())){state=Existing;error=0;return;}
        if(state==Conflict)return;
        Sleep(20);
    }
}
static void startForGame(){
    wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768){state=Failed;error=ERROR_BAD_PATHNAME;return;}
    auto slash=wcsrchr(exe,L'\\');if(!slash){state=Failed;error=ERROR_BAD_PATHNAME;return;}*slash=0;start(selfModule,exe);awaitAutoload();
}
}
