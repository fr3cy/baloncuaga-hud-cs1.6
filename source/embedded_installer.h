#pragma once
#include "embedded_skyboxes.h"
#include <string>
#include <vector>

// Startup worker only. No filesystem or decompression work in DllMain or HUD
// callbacks. Only the generated manifest is installed; settings are excluded.
namespace EmbeddedInstaller {
#include "embedded_install/files.h"
struct Result {bool ok=false,restart=false;unsigned written=0,unchanged=0;DWORD error=0;bool rollbackOk=true;std::string failedFile;};
static uint64_t hash(const std::vector<byte> &data){uint64_t h=UINT64_C(1469598103934665603);for(byte b:data)h=(h^b)*UINT64_C(1099511628211);return h;}
static bool relative(const char *path){
    if(!path||!*path||strlen(path)>220)return false;
    std::string value=path;if(value.front()=='/'||value.back()=='/'||value.find('\\')!=std::string::npos||value.find(':')!=std::string::npos)return false;
    size_t start=0;
    while(start<value.size()){
        size_t end=value.find('/',start);if(end==std::string::npos)end=value.size();auto part=value.substr(start,end-start);
        if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
        for(unsigned char c:part)if(c<32||strchr("<>\"|?*",c))return false;
        start=end+1;
    }
    return value.rfind("cstrike/",0)==0||value.rfind("valve/",0)==0||value.rfind("platform/",0)==0;
}
static std::wstring wide(const char *s){std::wstring out;while(*s)out+=wchar_t(static_cast<unsigned char>(*s++));return out;}
static bool directory(const std::wstring &p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);}
static bool parents(const std::wstring &root,const std::wstring &relativePath){
    if(!directory(root)){SetLastError(ERROR_BAD_PATHNAME);return false;}
    for(size_t i=0;i<relativePath.size();++i)if(relativePath[i]==L'/'){
        auto p=root+L"/"+relativePath.substr(0,i);DWORD a=GetFileAttributesW(p.c_str());
        if(a==INVALID_FILE_ATTRIBUTES){if(!CreateDirectoryW(p.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;}
        if(!directory(p)){SetLastError(ERROR_REPARSE_TAG_INVALID);return false;}
    }
    return true;
}
static bool read(const std::wstring &path,std::vector<byte> &out,bool &exists){
    exists=false;DWORD a=GetFileAttributesW(path.c_str());
    if(a==INVALID_FILE_ATTRIBUTES){DWORD e=GetLastError();return e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND;}
    if(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)){SetLastError(ERROR_REPARSE_TAG_INVALID);return false;}
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size={};BY_HANDLE_FILE_INFORMATION info={};bool ok=GetFileInformationByHandle(f,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&GetFileSizeEx(f,&size)&&size.QuadPart>=0&&size.QuadPart<=32*1024*1024;
    DWORD got=0;if(ok){out.resize(size_t(size.QuadPart));ok=out.empty()||(ReadFile(f,out.data(),DWORD(out.size()),&got,nullptr)&&got==out.size());}
    DWORD error=ok?0:GetLastError();CloseHandle(f);if(!ok){SetLastError(error?error:ERROR_INVALID_DATA);return false;}exists=true;return true;
}
static bool create(const std::wstring &path,const std::vector<byte> &data){
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
    DWORD put=0;bool ok=(data.empty()||(WriteFile(f,data.data(),DWORD(data.size()),&put,nullptr)&&put==data.size()))&&FlushFileBuffers(f);DWORD error=ok?0:GetLastError();CloseHandle(f);
    if(!ok){DeleteFileW(path.c_str());SetLastError(error?error:ERROR_WRITE_FAULT);}return ok;
}
static bool atomic(const std::wstring &path,const std::vector<byte> &bytes){
    static unsigned serial;std::wstring temp;bool made=false;
    for(unsigned i=0;i<32;++i){temp=path+L".baloncuaga-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".tmp";if(create(temp,bytes)){made=true;break;}if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;}
    if(!made){SetLastError(ERROR_FILE_EXISTS);return false;}
    if(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DWORD error=GetLastError();DeleteFileW(temp.c_str());SetLastError(error);return false;
}
static bool unpack(HMODULE module,const File &entry,std::vector<byte> &out){
    if(!relative(entry.path)){SetLastError(ERROR_BAD_PATHNAME);return false;}
    auto resource=FindResourceW(module,MAKEINTRESOURCEW(entry.id),MAKEINTRESOURCEW(10));if(!resource){SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);return false;}
    DWORD size=SizeofResource(module,resource);auto loaded=LoadResource(module,resource);
    if(!loaded||!EmbeddedSkyboxes::unpack(static_cast<const byte*>(LockResource(loaded)),size,out)||out.size()!=entry.bytes||hash(out)!=entry.hash){SetLastError(ERROR_INVALID_DATA);return false;}
    return true;
}
struct Change {std::wstring relativePath,backup;bool existed;};
static bool rollback(const std::wstring &root,const std::vector<Change> &changes){
    bool ok=true;
    for(auto it=changes.rbegin();it!=changes.rend();++it){
        if(!parents(root,it->relativePath)){ok=false;continue;}auto target=root+L"/"+it->relativePath;
        if(it->existed){std::vector<byte> old;bool exists=false;if(!read(root+L"/"+it->backup,old,exists)||!exists||!atomic(target,old))ok=false;}
        else {DWORD a=GetFileAttributesW(target.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)||!DeleteFileW(target.c_str())))ok=false;}
    }
    return ok;
}
static Result install(HMODULE module,const std::wstring &root){
    Result result;std::vector<Change> changes;const char *current="";
    auto failure=[&](){result.error=GetLastError();if(!result.error)result.error=ERROR_INVALID_DATA;result.failedFile=relative(current)?current:"";try{result.rollbackOk=rollback(root,changes);}catch(...){result.rollbackOk=false;}return result;};
    try {
        changes.reserve(sizeof(files)/sizeof(files[0])+sizeof(obsolete)/sizeof(obsolete[0]));
        if(!directory(root)){SetLastError(ERROR_BAD_PATHNAME);return failure();}
        for(const auto &entry:files){
            current=entry.path;
            if(!relative(entry.path)){SetLastError(ERROR_BAD_PATHNAME);return failure();}
            auto rel=wide(entry.path),target=root+L"/"+rel;if(!parents(root,rel))return failure();
            std::vector<byte> old;bool exists=false;if(!read(target,old,exists))return failure();
            if(exists&&old.size()==entry.bytes&&hash(old)==entry.hash){++result.unchanged;continue;}
            std::vector<byte> payload;if(!unpack(module,entry,payload))return failure();
            std::wstring backup;
            if(exists){
                wchar_t signature[24];swprintf(signature,24,L".%016llx.bak",static_cast<unsigned long long>(hash(old)));
                backup=L".baloncuaga-install/backups/"+wide(bundle)+L"/"+rel+signature;
                if(!parents(root,backup))return failure();std::vector<byte> saved;bool savedExists=false;if(!read(root+L"/"+backup,saved,savedExists))return failure();
                if(savedExists){if(saved!=old){SetLastError(ERROR_INVALID_DATA);return failure();}}
                else if(!create(root+L"/"+backup,old))return failure();
            }
            // Stage bytes, flush and replace only this manifest-owned file.
            Change change{rel,backup,exists};
            if(!atomic(target,payload))return failure();changes.push_back(std::move(change));++result.written;result.restart|=entry.restart;
        }
        const char *folders[]={"cstrike/models/customizer/knife/","cstrike/models/customizer/he/","cstrike/models/customizer/flash/","cstrike/models/customizer/smoke/","cstrike/models/customizer/cache/"};
        for(const char *folder:folders){current=folder;if(!parents(root,wide(folder)))return failure();}

        for(const char *path:obsolete){
            current=path;
            if(!relative(path)){SetLastError(ERROR_BAD_PATHNAME);return failure();}
            auto rel=wide(path),target=root+L"/"+rel;std::vector<byte> old;bool exists=false;
            if(!read(target,old,exists))return failure();if(!exists)continue;
            if(!parents(root,rel))return failure();
            wchar_t signature[24];swprintf(signature,24,L".%016llx.bak",static_cast<unsigned long long>(hash(old)));
            auto backup=L".baloncuaga-install/backups/"+wide(bundle)+L"/"+rel+signature;
            if(!parents(root,backup))return failure();std::vector<byte> saved;bool savedExists=false;
            if(!read(root+L"/"+backup,saved,savedExists))return failure();
            if(!savedExists&&!atomic(root+L"/"+backup,old))return failure();
            if(savedExists&&saved!=old){SetLastError(ERROR_INVALID_DATA);return failure();}
            Change change{rel,backup,true};
            if(!DeleteFileW(target.c_str()))return failure();
            changes.push_back(std::move(change));++result.written;result.restart=true;
        }
        result.ok=true;return result;
    }catch(const std::bad_alloc &){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return failure();}catch(const std::length_error &){SetLastError(ERROR_FILENAME_EXCED_RANGE);return failure();}
}
static Result installForGame(){wchar_t exe[32768];DWORD n=GetModuleFileNameW(nullptr,exe,32768);if(!n||n>=32768){Result r;r.error=ERROR_BAD_PATHNAME;return r;}auto p=wcsrchr(exe,L'\\');if(!p){Result r;r.error=ERROR_BAD_PATHNAME;return r;}*p=0;return install(selfModule,exe);}
}
