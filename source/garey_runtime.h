#pragma once
// The original plugin (with debug path metadata removed) stays a separate Windows-loaded module. No manual PE
// mapping, executable-code modifications, borrowed HUD callbacks or forced unloads.
namespace GareyRuntime {
enum State {NotStarted,Loaded,Existing,Conflict,Failed};
static State state=NotStarted;
static DWORD error;
static HMODULE module;
static ULONGLONG started;
static constexpr size_t payloadBytes=1396736;
static constexpr uint64_t payloadHash=UINT64_C(0x63df6bccf68ab85f);
using Loader=HMODULE (WINAPI *)(LPCWSTR,HANDLE,DWORD);
static bool payload(HMODULE owner,std::vector<byte> &out){
    auto resource=FindResourceW(owner,MAKEINTRESOURCEW(6000),MAKEINTRESOURCEW(10));
    if(!resource){SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);return false;}
    auto loaded=LoadResource(owner,resource);
    if(!loaded||!EmbeddedSkyboxes::unpack(static_cast<const byte *>(LockResource(loaded)),SizeofResource(owner,resource),out)||out.size()!=payloadBytes||EmbeddedInstaller::hash(out)!=payloadHash){SetLastError(ERROR_INVALID_DATA);return false;}
    return true;
}
static bool prepare(HMODULE owner,const std::wstring &root,std::wstring &path){
    std::vector<byte> expected,existing;bool exists=false;if(!payload(owner,expected))return false;
    const std::wstring relative=L".baloncuaga-runtime/csfix-a8546e972abb/cs_patches.asi";
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
        if(name.find(L"cs_patches")==std::wstring::npos&&name.find(L"csfix")==std::wstring::npos)continue;
        std::vector<byte> bytes;bool exists=false;
        if(EmbeddedInstaller::read(entry.szExePath,bytes,exists)&&exists&&bytes.size()==payloadBytes&&(EmbeddedInstaller::hash(bytes)==payloadHash||EmbeddedInstaller::hash(bytes)==UINT64_C(0xc47f0ebd51507fa9))){found=entry.hModule;break;}
        // Do not stack an unknown CS Fix build over another copy.
        state=Conflict;error=ERROR_ALREADY_EXISTS;
    }while(Module32NextW(snapshot,&entry));CloseHandle(snapshot);return found;
}
static bool start(HMODULE owner,const std::wstring &root,Loader load=LoadLibraryExW){
    started=GetTickCount64();
    if(Garey::variable("fps_visual")){state=Existing;logLine("CS Fix: existing fps_visual provider retained.");return true;}
    if((module=existingModule())){state=Existing;logLine("CS Fix: original module already loaded; duplicate skipped.");return true;}
    if(state==Conflict){logLine("CS Fix: a different CS Fix module is already loaded; duplicate skipped.");return false;}
    std::wstring path;if(!prepare(owner,root,path)){state=Failed;error=GetLastError();}
    else {module=load(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(module){state=Loaded;logLine("CS Fix: embedded original loaded through the Windows loader; awaiting fps_visual.");return true;}
        state=Failed;error=GetLastError();}
    char line[128];snprintf(line,sizeof(line),"CS Fix: automatic load failed, Windows error %lu; HUD continues.",static_cast<unsigned long>(error));logLine(line);return false;
}
static void startForGame(){
    wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768){state=Failed;error=ERROR_BAD_PATHNAME;return;}
    auto slash=wcsrchr(exe,L'\\');if(!slash){state=Failed;error=ERROR_BAD_PATHNAME;return;}*slash=0;start(selfModule,exe);
}
}
