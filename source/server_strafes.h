#pragma once
// Only the optional strafe table consumes server HUD text. FOG is local.
namespace ServerStrafes {
static bool readableRange(const void *source,size_t bytes){return JumpStats::nativeReadable(source,bytes);}
static bool copyText(const char *source,std::string &out,size_t limit=2048){
    out.clear();if(!source)return false;
    while(out.size()<=limit){
        uintptr_t address=reinterpret_cast<uintptr_t>(source)+out.size();if(address<reinterpret_cast<uintptr_t>(source))return false;
        MEMORY_BASIC_INFORMATION info={};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        uintptr_t end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(end<=address)return false;
        size_t count=std::min(limit+1-out.size(),size_t(end-address));auto bytes=reinterpret_cast<const char*>(address);
        auto zero=static_cast<const char*>(memchr(bytes,0,count));if(zero){out.append(bytes,zero-bytes);return true;}
        out.append(bytes,count);
    }
    out.clear();return false;
}
using Lookup=client_textmessage_t *(*)(const char *);
using Director=void (*)(int,void *);
static Lookup oldLookup;static Director oldDirector;
static bool enabled(){return ready&&!unloading&&JumpStats::enabled;}
static void captureMessage(client_textmessage_t *message){
    if(!readableRange(message,sizeof(*message)))return;
    std::string text;if(!copyText(message->pMessage,text))return;
    auto meta=JumpStats::metadata(message);if(meta.owner)JumpStats::capture(text,meta);
}
static client_textmessage_t *lookup(const char *name){
    CallbackScope scope;JumpStats::restoreNativeText();auto message=oldLookup?oldLookup(name):nullptr;
    if(enabled()&&name&&message){
        std::string key;if(copyText(name,key,128)&&JumpStats::lower(key).find("netmessage")!=std::string::npos)captureMessage(message);
    }
    JumpStats::syncNativeText();return message;
}
static void token(int size,const void *data){
    if(!enabled()||!data||size<=0||size>2048||!readableRange(data,size))return;
    JumpStats::restoreNativeText();auto bytes=static_cast<const char*>(data);auto zero=static_cast<const char*>(memchr(bytes,0,size));if(!zero)return;
    std::string name(bytes,zero);const char *key=name.c_str();if(*key=='#')++key;
    auto fn=oldLookup?oldLookup:eng?eng->pfnTextMessageGet:nullptr;
    if(fn)captureMessage(fn(key));
}
static void director(int size,void *data){
    CallbackScope scope;
    if(enabled()&&size>=31&&size<=2048&&readableRange(data,size)&&static_cast<const byte*>(data)[0]==6){
        auto bytes=static_cast<const char*>(data);auto text=bytes+30;auto zero=static_cast<const char*>(memchr(text,0,size-30));
        if(zero){
            float values[6];memcpy(values,bytes+6,sizeof(values));bool finite=true;for(float value:values)finite&=std::isfinite(value);
            if(finite){
                JumpStats::Meta meta;meta.x=values[0];meta.y=values[1];meta.director=true;
                float fade=std::clamp(values[2],0.f,60.f);if(static_cast<const byte*>(data)[1]==2)fade*=zero-text;
                float seconds=fade+std::clamp(values[3],0.f,60.f)+std::clamp(values[4],0.f,60.f);
                meta.until=GetTickCount64()+ULONGLONG(std::clamp(seconds,.1f,60.f)*1000);
                JumpStats::capture(std::string(text,zero),meta);
            }
        }
    }
    if(oldDirector)oldDirector(size,data);
}
static void install(HMODULE client){
    if(eng&&eng->pfnTextMessageGet)MH_CreateHook(reinterpret_cast<void*>(eng->pfnTextMessageGet),reinterpret_cast<void*>(lookup),reinterpret_cast<void**>(&oldLookup));
    auto target=GetProcAddress(client,"HUD_DirectorMessage");if(target)MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(director),reinterpret_cast<void**>(&oldDirector));
}
}
