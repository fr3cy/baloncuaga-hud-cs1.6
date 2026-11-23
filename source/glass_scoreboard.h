#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Presentation adapter for the verified Steam Legacy CS client only. Native
// scoreboard controls continue to own every player, column, scroll and click.
// Font handles and TextImages remain owned by native VGUI; there are no DLL
// callbacks/data pointers written into a native control and no GL resources.
namespace GlassScoreboard {
static constexpr uint64_t clientFnv=UINT64_C(0x1b3b15140997c5fa);
static constexpr size_t clientBytes=1096608;
static constexpr DWORD clientTimestamp=0x5f28cde0,clientImageBytes=0x15f000;
static constexpr uintptr_t itemSchemeRva=0x8fbf0,headerSchemeRva=0x8d920;
static constexpr uintptr_t itemLayoutRva=0x8f7a0,headerLayoutRva=0x8da50;
static constexpr uintptr_t itemVtableRva=0xd255c,headerVtableRva=0xd22b4;
static constexpr uintptr_t listVtableRva=0xd1ffc,scoreboardVtableRva=0xcc2a4,textVtableRva=0xd188c;
static constexpr uintptr_t labelSetFontRva=0x7d6c0,labelGetFontRva=0x7d710;
static constexpr uintptr_t parentRva=0x76f70,schemeRva=0x779f0,managerGetterRva=0x7ede0,textSetFontRva=0x8ab00;
using SchemeFn=void (__thiscall *)(void*,void*);
using PanelFn=void (__thiscall *)(void*);
using PointerFn=void* (__thiscall *)(void*);
using HandleFn=unsigned (__thiscall *)(void*);
using FontFn=void (__thiscall *)(void*,unsigned);
using GetFontFn=unsigned (__thiscall *)(void*,const char*,bool);
static HMODULE clientModule;
static SchemeFn originalItemScheme,originalHeaderScheme;
static PanelFn originalItemLayout,originalHeaderLayout;
static std::atomic<bool> installed{false},usedFonts{false};
static std::atomic<unsigned> statusCode{0};
static thread_local bool adjusting;

static bool readable(const void *pointer,size_t bytes){
    if(!pointer||!bytes)return false;uintptr_t at=reinterpret_cast<uintptr_t>(pointer),end=at+bytes;if(end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(reinterpret_cast<const void*>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
static bool code(const void *pointer,HMODULE owner){
    MEMORY_BASIC_INFORMATION info={};return pointer&&owner&&VirtualQuery(pointer,&info,sizeof(info))&&info.AllocationBase==owner&&info.Type==MEM_IMAGE&&info.State==MEM_COMMIT&&!(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static bool vguiCode(const void *pointer){return code(pointer,GetModuleHandleW(L"vgui2.dll"));}
static uintptr_t base(){return reinterpret_cast<uintptr_t>(clientModule);}
static bool knownVtable(void *object,uintptr_t rva,size_t bytes){return readable(object,bytes)&&*reinterpret_cast<uintptr_t*>(object)==base()+rva;}
template<class T>static T field(void *object,size_t offset){T value={};memcpy(&value,static_cast<unsigned char*>(object)+offset,sizeof(value));return value;}
static bool metadata(const unsigned char *data,size_t bytes){
    if(bytes<sizeof(IMAGE_DOS_HEADER)||bytes<sizeof(IMAGE_NT_HEADERS32))return false;IMAGE_DOS_HEADER dos;memcpy(&dos,data,sizeof(dos));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0x40||size_t(dos.e_lfanew)>bytes-sizeof(IMAGE_NT_HEADERS32))return false;
    IMAGE_NT_HEADERS32 nt;memcpy(&nt,data+dos.e_lfanew,sizeof(nt));return nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&nt.FileHeader.TimeDateStamp==clientTimestamp&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&nt.OptionalHeader.SizeOfImage==clientImageBytes;
}
static uint64_t hashBytes(const unsigned char *p,size_t count,uint64_t hash=UINT64_C(14695981039346656037)){for(size_t i=0;i<count;++i)hash=(hash^p[i])*UINT64_C(1099511628211);return hash;}
static bool supportedFile(const wchar_t *path){
    FILE *file=_wfopen(path,L"rb");if(!file)return false;bool okay=false;unsigned char block[4096];
    if(!fseek(file,0,SEEK_END)&&ftell(file)==static_cast<long>(clientBytes)&&!fseek(file,0,SEEK_SET)){
        size_t count=fread(block,1,sizeof(block),file);if(count==sizeof(block)&&metadata(block,count)){uint64_t hash=hashBytes(block,count);size_t total=count;
            while((count=fread(block,1,sizeof(block),file))!=0){total+=count;hash=hashBytes(block,count,hash);}okay=!ferror(file)&&total==clientBytes&&hash==clientFnv;}}
    fclose(file);return okay;
}
static bool bytesAt(HMODULE image,uintptr_t rva,const char *bytes,size_t count){auto p=reinterpret_cast<unsigned char*>(image)+rva;return code(p,image)&&readable(p,count)&&!memcmp(p,bytes,count);}
static bool supportedImage(HMODULE image){
    auto p=reinterpret_cast<unsigned char*>(image);if(!readable(p,4096)||!metadata(p,4096))return false;
    // All targets use verified native thiscall ABIs. The four detours only
    // intercept presentation callbacks; none replace native update/input code.
    return bytesAt(image,itemSchemeRva,"\x51\x53\x56\x57\x8b\x7c\x24\x14\x57\x8b\xf1",11)
        &&bytesAt(image,headerSchemeRva,"\x51\x53\x56\x57\x8b\x7c\x24\x14\x57\x8b\xf1",11)
        &&bytesAt(image,itemLayoutRva,"\x83\xec\x28\x53\x55\x56\x8b\xf1\x57",9)
        &&bytesAt(image,headerLayoutRva,"\x83\xec\x30\x53\x55\x56\x57\x8b\xf1",9)
        &&bytesAt(image,labelSetFontRva,"\x8b\x54\x24\x04\x56\x8b\xf1\x52\x8b\x4e\x78",11)
        &&bytesAt(image,labelGetFontRva,"\x8b\x49\x78\x8b\x01\xff\xa0\x84\x00\x00\x00",11)
        &&bytesAt(image,parentRva,"\x53\x56\x57\x8b\xf1",5)
        &&bytesAt(image,schemeRva,"\x56\x8b\xf1\x8b\x46\x64",6)
        &&bytesAt(image,managerGetterRva,"\xa1",1)
        &&bytesAt(image,textSetFontRva,"\x8b\x44\x24\x04\x89\x41\x20\x8a\x41\x34\x0c\x01",12);
}
static bool active(){return installed.load(std::memory_order_acquire)&&ready&&!unloading&&!unloadRequested;}
static bool ownedLabel(void *label,bool header){
    if(!clientModule||!knownVtable(label,header?headerVtableRva:itemVtableRva,header?0xcc:0x104))return false;
    void *list=field<void*>(label,header?0xc8:0xc0);if(!knownVtable(list,listVtableRva,0x110))return false;
    // GetParent resolves VPANEL through native IPanel and its module binding;
    // no guessed pointer offsets or global scoreboard object are retained.
    void *root=reinterpret_cast<PointerFn>(base()+parentRva)(list);
    return knownVtable(root,scoreboardVtableRva,0x118)&&field<void*>(root,0x114)==list;
}
static void *nativeScheme(void *label){
    unsigned handle=reinterpret_cast<HandleFn>(base()+schemeRva)(label);if(!handle)return nullptr;
    using Getter=void* (__cdecl *)();void *manager=reinterpret_cast<Getter>(base()+managerGetterRva)();if(!readable(manager,sizeof(void*)))return nullptr;
    void **vtable=field<void**>(manager,0);if(!readable(vtable,8*sizeof(void*))||!vguiCode(vtable[7]))return nullptr;
    using Resolve=void* (__thiscall *)(void*,unsigned);return reinterpret_cast<Resolve>(vtable[7])(manager,handle);
}
static unsigned fontFor(void *label,bool header,void *scheme){
    if(!scheme)scheme=nativeScheme(label);if(!readable(scheme,sizeof(void*)))return 0;
    void **vtable=field<void**>(scheme,0);if(!readable(vtable,4*sizeof(void*))||!vguiCode(vtable[3]))return 0;
    void **panelVtable=field<void**>(label,0);if(panelVtable[21]!=reinterpret_cast<void*>(base()+0x1d8a0))return 0;
    using Proportional=bool (__thiscall *)(void*);bool proportional=reinterpret_cast<Proportional>(panelVtable[21])(label);
    return reinterpret_cast<GetFontFn>(vtable[3])(scheme,header?"GlassScoreboardHeader":"GlassScoreboardRow",proportional);
}
static bool knownText(void *text){return knownVtable(text,textVtableRva,0x38);}
static void setTextFont(void *text,unsigned font){
    if(knownText(text)&&field<unsigned>(text,0x20)!=font)reinterpret_cast<FontFn>(base()+textSetFontRva)(text,font);
}
static bool applyOne(void *label,bool header,void *scheme=nullptr){
    if(!ownedLabel(label,header))return false;
    unsigned font=fontFor(label,header,scheme);if(!font){statusCode.store(3,std::memory_order_relaxed);return false;}
    void *mainText=field<void*>(label,0x78);if(!knownText(mainText))return false;
    bool changed=field<unsigned>(mainText,0x20)!=font;
    if(changed)reinterpret_cast<FontFn>(base()+labelSetFontRva)(label,font);
    if(!header){
        // Native CItemButton owns this vector. Refresh only verified TextImages
        // from existing columns; keep its native fallback font and all data.
        int count=field<int>(label,0xf8),capacity=field<int>(label,0xf0);void **images=field<void**>(label,0xec);
        if(count>=0&&count<=64&&capacity>=count&&capacity<=256&&(count==0||readable(images,size_t(count)*sizeof(void*))))
            for(int i=0;i<count;++i)setTextFont(images[i],font);
    }
    statusCode.store(2,std::memory_order_relaxed);if(!usedFonts.exchange(true))logLine("Glass scoreboard: isolated native row and section fonts applied; native table retained.");return changed;
}
static void applyFont(void *label,bool header,void *scheme=nullptr){
    if(!active()||adjusting||!ownedLabel(label,header))return;
    adjusting=true;struct Guard{~Guard(){adjusting=false;}}guard;
    applyOne(label,header,scheme);
    // An unchanged section header can predate hook installation. Refresh the
    // current callback's verified live native headers, without storing control
    // pointers. Native SetFontSection proves array+0x78/count+0x84, stride0x24,
    // header pointer at entry+8. The original layout refreshes cached images.
    void *list=field<void*>(label,header?0xc8:0xc0);void *sections=field<void*>(list,0x78);int count=field<int>(list,0x84);
    if(count<0||count>64||(count&&!readable(sections,size_t(count)*0x24)))return;
    for(int i=0;i<count;++i){void *other=field<void*>(sections,size_t(i)*0x24+8);if(other!=label&&applyOne(other,true,scheme))originalHeaderLayout(other);}
}
#if defined(__GNUC__) && defined(__i386__)
#define BALONCUAGA_GLASS_ALIGN __attribute__((force_align_arg_pointer))
#else
#define BALONCUAGA_GLASS_ALIGN
#endif
static void __fastcall BALONCUAGA_GLASS_ALIGN itemScheme(void *label,void*,void *scheme){CallbackScope scope;originalItemScheme(label,scheme);applyFont(label,false,scheme);}
static void __fastcall BALONCUAGA_GLASS_ALIGN headerScheme(void *label,void*,void *scheme){CallbackScope scope;originalHeaderScheme(label,scheme);applyFont(label,true,scheme);}
static void __fastcall BALONCUAGA_GLASS_ALIGN itemLayout(void *label,void*){CallbackScope scope;applyFont(label,false);originalItemLayout(label);}
static void __fastcall BALONCUAGA_GLASS_ALIGN headerLayout(void *label,void*){CallbackScope scope;applyFont(label,true);originalHeaderLayout(label);}
#undef BALONCUAGA_GLASS_ALIGN
static bool available(){return installed.load(std::memory_order_acquire);}
static const char *status(){switch(statusCode.load(std::memory_order_relaxed)){case 1:return "Готово: штатная таблица со стеклянным оформлением";case 2:return "Крупный шрифт применён к штатной таблице";case 3:return "Нет шрифта GlassScoreboard в ClientScheme.res";case 4:return "Эта сборка клиента не поддерживает замену шрифта TAB";case 5:return "Штатная таблица сохранена: обработчик недоступен";default:return "Стеклянная таблица: ожидание клиента";}}
static bool install(HMODULE client){
    if(available())return true;if(!client)return false;wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(client,path,MAX_PATH);
    if(!length||length>=MAX_PATH||!supportedFile(path)||!supportedImage(client)){statusCode.store(4);logLine("Glass scoreboard: unsupported client build; native fonts retained.");return false;}
    clientModule=client;auto p=reinterpret_cast<unsigned char*>(client);
    struct Hook {uintptr_t rva;void *detour;void **original;};Hook hooks[]={{itemSchemeRva,reinterpret_cast<void*>(itemScheme),reinterpret_cast<void**>(&originalItemScheme)},{headerSchemeRva,reinterpret_cast<void*>(headerScheme),reinterpret_cast<void**>(&originalHeaderScheme)},{itemLayoutRva,reinterpret_cast<void*>(itemLayout),reinterpret_cast<void**>(&originalItemLayout)},{headerLayoutRva,reinterpret_cast<void*>(headerLayout),reinterpret_cast<void**>(&originalHeaderLayout)}};
    unsigned created=0;for(auto &hook:hooks){if(MH_CreateHook(p+hook.rva,hook.detour,hook.original)!=MH_OK){for(unsigned i=0;i<created;++i)MH_RemoveHook(p+hooks[i].rva);clientModule=nullptr;statusCode.store(5);logLine("Glass scoreboard: hook unavailable; native fonts retained.");return false;}++created;}
    installed.store(true,std::memory_order_release);statusCode.store(1);logLine("Glass scoreboard: verified native item/section presentation hooks installed.");return true;
}
static void frame(){} // Native layout callbacks refresh existing controls on TAB.
static void resetVideo(){usedFonts.store(false);if(available())statusCode.store(1);}
static void release(){installed.store(false,std::memory_order_release);usedFonts.store(false);}
} // namespace GlassScoreboard
