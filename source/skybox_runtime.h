#pragma once
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <utility>
#include <new>
#include <stdexcept>
#include "embedded_skyboxes.h"

// Client-only replacement in the native sky draw pass. Original geometry,
// visibility, native sky names, server variables and engine texture IDs remain
// untouched. High plugin texture names never pass through GoldSrc's 16-bit bind.
namespace Skybox {
struct Image {int width=0,height=0;std::vector<unsigned char> rgba;};
static constexpr const char *ids[]={"","sunny","night","sunset","overcast","aurora","space"};
static constexpr const char *faces[]={"rt","bk","lf","ft","up","dn"};
static constexpr uintptr_t drawRva=0x507ae,bindRva=0x3c0a0,skyIdsRva=0x643460,cacheRva=0x142240,orderRva=0x148960;
static constexpr uint64_t hwFnv=UINT64_C(0xcd9e843a0b68179e);static constexpr size_t hwBytes=1641376;
static constexpr DWORD hwTimestamp=0x5f3d8dcc,hwImageBytes=0x1259000;
using DrawFn=void (*)();using BindFn=void (*)(int);
static DrawFn originalDraw;static BindFn originalBind;static HMODULE engineModule;
static std::atomic<bool> hooked{false};static HGLRC ownerContext;
static std::array<GLuint,6> textures={};static std::array<Image,6> queuedImages;
static int activeIndex,lastGoodIndex,pendingIndex=-1;static bool queuedDecoded;
static const char *message=u8"Стандартное небо карты.";
static thread_local unsigned skyDepth;
static thread_local bool skyChanged;

static bool readable(const void *p,size_t n){
    if(!p||!n)return false;uintptr_t at=reinterpret_cast<uintptr_t>(p),end=at+n;if(end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION m={};if(!VirtualQuery(reinterpret_cast<const void *>(at),&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;if(next<=at)return false;at=next;}return true;
}
static bool executable(const void *p,HMODULE owner){MEMORY_BASIC_INFORMATION m={};return p&&owner&&VirtualQuery(p,&m,sizeof(m))&&m.AllocationBase==owner&&m.Type==MEM_IMAGE&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
static bool metadata(const unsigned char *p,size_t bytes){
    if(bytes<sizeof(IMAGE_DOS_HEADER)||bytes<sizeof(IMAGE_NT_HEADERS32))return false;IMAGE_DOS_HEADER dos;memcpy(&dos,p,sizeof(dos));if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0x40||size_t(dos.e_lfanew)>bytes-sizeof(IMAGE_NT_HEADERS32))return false;
    IMAGE_NT_HEADERS32 nt;memcpy(&nt,p+dos.e_lfanew,sizeof(nt));return nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&nt.FileHeader.TimeDateStamp==hwTimestamp&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&nt.OptionalHeader.SizeOfImage==hwImageBytes;
}
static uint64_t hashBytes(const unsigned char *p,size_t count,uint64_t hash=UINT64_C(14695981039346656037)){for(size_t i=0;i<count;++i)hash=(hash^p[i])*UINT64_C(1099511628211);return hash;}
static bool supportedFile(const wchar_t *path){
    FILE *file=_wfopen(path,L"rb");if(!file)return false;unsigned char block[4096];bool okay=false;
    if(!fseek(file,0,SEEK_END)&&ftell(file)==static_cast<long>(hwBytes)&&!fseek(file,0,SEEK_SET)){
        size_t count=fread(block,1,sizeof(block),file);if(count==sizeof(block)&&metadata(block,count)){uint64_t hash=hashBytes(block,count);size_t total=count;while((count=fread(block,1,sizeof(block),file))!=0){total+=count;hash=hashBytes(block,count,hash);}okay=!ferror(file)&&total==hwBytes&&hash==hwFnv;}}
    fclose(file);return okay;
}
static uintptr_t operand(const unsigned char *p){uintptr_t value=0;memcpy(&value,p,sizeof(value));return value;}
static bool supportedImage(HMODULE module){
    auto b=reinterpret_cast<const unsigned char *>(module);if(!readable(b,4096)||!metadata(b,4096)||!executable(b+drawRva,module)||!executable(b+bindRva,module))return false;
    const unsigned char drawHead[]={0x55,0x8b,0xec,0x83,0xec,0x1c},bindHead[]={0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0x0d};
    if(!readable(b+drawRva,6)||!readable(b+bindRva,24)||memcmp(b+drawRva,drawHead,6)||memcmp(b+bindRva,bindHead,8)||operand(b+bindRva+8)!=reinterpret_cast<uintptr_t>(b)+cacheRva)return false;
    const unsigned char *fetch=b+0x508f0;if(!readable(fetch,20)||memcmp(fetch,"\x8b\x04\xb5",3)||memcmp(fetch+7,"\x8b\x0c\x85",3)||fetch[14]!=0x51||fetch[15]!=0xe8)return false;
    if(operand(fetch+3)!=reinterpret_cast<uintptr_t>(b)+orderRva||operand(fetch+10)!=reinterpret_cast<uintptr_t>(b)+skyIdsRva)return false;
    int32_t relative=0;memcpy(&relative,fetch+16,4);if(fetch+20+relative!=b+bindRva)return false;
    const int order[]={0,2,1,3,4,5};return readable(b+orderRva,sizeof(order))&&!memcmp(b+orderRva,order,sizeof(order))&&readable(b+skyIdsRva,24)&&readable(b+cacheRva,4);
}
static bool relativePath(const char *path){
    if(!path||!*path||strstr(path,"..")||strchr(path,':')||*path=='/'||*path=='\\')return false;size_t count=0;for(;path[count];++count)if(count>=240||static_cast<unsigned char>(path[count])<32)return false;return true;
}
static std::wstring gameFile(const char *relative){
    if(!relativePath(relative))return {};wchar_t path[MAX_PATH];DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!n||n>=MAX_PATH)return {};std::wstring full(path,n);size_t slash=full.find_last_of(L"/\\");if(slash==std::wstring::npos)return {};full.resize(slash+1);
    for(size_t i=0;relative[i];++i){unsigned char c=static_cast<unsigned char>(relative[i]);if(c>=128)return {};full+=c=='/'?L'\\':wchar_t(c);}return full;
}
// Top-first RGBA matches the native TGA loader's texture convention. Upload it
// directly: no additional vertical flip for sky faces or ImGui preview images.
static bool decodeTga(const unsigned char *bytes,size_t size,Image &out){
    try {
    if(!bytes||size<18||bytes[1]!=0||(bytes[2]!=2&&bytes[2]!=10)||(bytes[16]!=24&&bytes[16]!=32)||(bytes[17]&0xc0))return false;
    unsigned w=bytes[12]|(unsigned(bytes[13])<<8),h=bytes[14]|(unsigned(bytes[15])<<8);if(!w||!h||w>2048||h>2048)return false;
    size_t at=18+bytes[0],count=size_t(w)*h;if(at>size)return false;unsigned channels=bytes[16]/8;
    if(bytes[2]==2&&count>(size-at)/channels)return false;
    if(bytes[2]==10&&((count+127)/128)*(channels+1)>size-at)return false;
    Image image;image.width=int(w);image.height=int(h);image.rgba.resize(count*4);size_t pixel=0;
    auto emit=[&](const unsigned char *value){unsigned rawY=unsigned(pixel/w),rawX=unsigned(pixel%w),y=bytes[17]&0x20?rawY:h-1-rawY,x=bytes[17]&0x10?w-1-rawX:rawX;auto *dst=image.rgba.data()+(size_t(y)*w+x)*4;dst[0]=value[2];dst[1]=value[1];dst[2]=value[0];dst[3]=channels==4?value[3]:255;++pixel;};
    if(bytes[2]==2){if(count>(size-at)/channels)return false;while(pixel<count){emit(bytes+at);at+=channels;}}
    else while(pixel<count){if(at>=size)return false;unsigned packet=bytes[at++],length=(packet&127)+1;if(length>count-pixel)return false;
        if(packet&128){if(channels>size-at)return false;const auto *value=bytes+at;at+=channels;for(unsigned i=0;i<length;++i)emit(value);}
        else{if(size_t(length)*channels>size-at)return false;for(unsigned i=0;i<length;++i){emit(bytes+at);at+=channels;}}}
    out=std::move(image);return true;
    }catch(const std::bad_alloc &){return false;}catch(const std::length_error &){return false;}
}
static bool readTga(const char *relative,Image &image){
    try {
    std::vector<unsigned char> bytes;
    return EmbeddedSkyboxes::read(selfModule,relative,bytes)&&decodeTga(bytes.data(),bytes.size(),image);
    }catch(const std::bad_alloc &){return false;}catch(const std::length_error &){return false;}
}
static bool readSet(int index,std::array<Image,6> &images){
    try {
    if(index<=0||index>=7)return false;for(int i=0;i<6;++i){std::string path=std::string("baloncuaga_skyboxes/")+ids[index]+"/"+faces[i]+".tga";
        if(!readTga(path.c_str(),images[i])){message=u8"Повреждён встроенный набор неба. Прежнее небо сохранено.";return false;}
        int w=images[i].width,h=images[i].height;if(w!=h||w<64||(w&(w-1))){message=u8"Грани неба должны быть квадратными: 64–2048, степень двойки.";return false;}
        if(i&&w!=images[0].width){message=u8"У шести граней разный размер. Прежнее небо сохранено.";return false;}}
    return true;
    }catch(const std::bad_alloc &){message=u8"Недостаточно памяти. Прежнее небо сохранено.";return false;}catch(const std::length_error &){message=u8"Слишком большой набор текстур. Прежнее небо сохранено.";return false;}
}
static int cachedNativeTexture(){int id=-1;if(engineModule){auto p=reinterpret_cast<const unsigned char *>(engineModule)+cacheRva;if(readable(p,4))memcpy(&id,p,4);}return id;}
static int nativeFace(int argument){
    if(!engineModule||argument<=0)return -1;auto p=reinterpret_cast<const unsigned char *>(engineModule)+skyIdsRva;if(!readable(p,24))return -1;int names[6];memcpy(names,p,sizeof(names));
    for(int i=0;i<6;++i){if(names[i]<=0||!(names[i]&65535))return -1;for(int j=0;j<i;++j)if((names[i]&65535)==(names[j]&65535))return -1;}for(int i=0;i<6;++i)if(names[i]==argument)return i;return -1;
}
static void restoreDriverBinding(){int native=cachedNativeTexture();glBindTexture(GL_TEXTURE_2D,native>=0&&native<=65535?GLuint(native):0);}
static bool available(){return hooked.load(std::memory_order_acquire);}
static int selected(){return activeIndex;}
static const char *status(){return message;}
static void save(){char value[16];snprintf(value,sizeof(value),"%d",lastGoodIndex);WritePrivateProfileStringA("interface","skybox_preset",value,iniPath);}
static void discardQueued(){for(auto &image:queuedImages)image=Image{};queuedDecoded=false;}
static void dropTextures(){
    if(ownerContext&&wglGetCurrentContext()==ownerContext){GLint bound=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);bool ours=false;for(GLuint id:textures)if(id&&id==GLuint(bound))ours=true;if(ours)restoreDriverBinding();for(GLuint id:textures)if(id)glDeleteTextures(1,&id);}
    textures.fill(0);ownerContext=nullptr;activeIndex=0;
}
static void release(){dropTextures();discardQueued();pendingIndex=-1;message=u8"Стандартное небо карты.";}
static void resetVideo(){int wanted=pendingIndex>=0?pendingIndex:(activeIndex?activeIndex:lastGoodIndex);dropTextures();discardQueued();pendingIndex=wanted;message=u8"Небо будет восстановлено после обновления видео.";}
static void load(){int index=GetPrivateProfileIntA("interface","skybox_preset",0,iniPath);lastGoodIndex=index>=0&&index<7?index:0;pendingIndex=lastGoodIndex;discardQueued();message=lastGoodIndex?u8"Ожидание загрузки выбранного неба.":u8"Стандартное небо карты.";}
static bool request(int index){
    if(index<0||index>=7)return false;if(index&&!available()){message=u8"Этот движок не поддерживает безопасную замену неба.";return false;}
    if(!index){discardQueued();pendingIndex=0;lastGoodIndex=0;save();message=u8"Возвращается стандартное небо карты.";return true;}
    std::array<Image,6> images;if(!readSet(index,images)){discardQueued();pendingIndex=-1;return false;}queuedImages=std::move(images);queuedDecoded=true;pendingIndex=index;message=u8"Небо проверено; ожидается загрузка текстур.";return true;
}
static bool edgeClampSupported(){const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION));int major=0,minor=0;if(version)sscanf(version,"%d.%d",&major,&minor);return major>1||(major==1&&minor>=2);}
static void frame(){
    if(!ready||unloading||unloadRequested)return;
    HGLRC current=wglGetCurrentContext();if(!current)return;
    if(ownerContext&&ownerContext!=current){textures.fill(0);ownerContext=nullptr;activeIndex=0;discardQueued();if(pendingIndex<0)pendingIndex=lastGoodIndex;}
    if(pendingIndex<0||skyDepth)return;int wanted=pendingIndex;
    if(!wanted){dropTextures();discardQueued();pendingIndex=-1;lastGoodIndex=0;save();message=u8"Стандартное небо карты.";return;}
    if(!available()){message=u8"Этот движок не поддерживает безопасную замену неба.";pendingIndex=-1;discardQueued();return;}
    if(!queuedDecoded&&!readSet(wanted,queuedImages)){pendingIndex=-1;discardQueued();return;}
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);if(maximum<queuedImages[0].width){pendingIndex=-1;discardQueued();message=u8"Видеокарта не поддерживает размер граней. Прежнее небо сохранено.";return;}
    std::array<GLuint,6> fresh={};glPushAttrib(GL_TEXTURE_BIT);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glGenTextures(6,fresh.data());GLint wrap=edgeClampSupported()?0x812f:GL_CLAMP;bool okay=true;
    for(int i=0;i<6;++i){if(!fresh[i]){okay=false;break;}glBindTexture(GL_TEXTURE_2D,fresh[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,queuedImages[i].width,queuedImages[i].height,0,GL_RGBA,GL_UNSIGNED_BYTE,queuedImages[i].rgba.data());GLint storedWidth=0,storedHeight=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&storedWidth);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&storedHeight);if(storedWidth!=queuedImages[i].width||storedHeight!=queuedImages[i].height){okay=false;break;}}
    glPopClientAttrib();glPopAttrib();if(!okay){for(GLuint id:fresh)if(id)glDeleteTextures(1,&id);pendingIndex=-1;discardQueued();message=u8"Ошибка загрузки текстур; прежнее небо сохранено.";return;}
    dropTextures();textures=fresh;ownerContext=current;activeIndex=lastGoodIndex=wanted;pendingIndex=-1;discardQueued();save();message=u8"Небо применено. Геометрия и освещение карты сохранены.";
}
#if defined(__GNUC__)
#define SKYBOX_ALIGN __attribute__((force_align_arg_pointer))
#else
#define SKYBOX_ALIGN
#endif
SKYBOX_ALIGN static void bind(int argument){
    CallbackScope scope;originalBind(argument);if(!skyDepth||!ready||unloading||unloadRequested||!activeIndex||!ownerContext||wglGetCurrentContext()!=ownerContext)return;
    int face=nativeFace(argument);if(face<0||cachedNativeTexture()!=(argument&65535)||!textures[face]||!glIsTexture(textures[face]))return;
    glBindTexture(GL_TEXTURE_2D,textures[face]);skyChanged=true;
}
SKYBOX_ALIGN static void draw(){
    CallbackScope scope;HGLRC context=wglGetCurrentContext();bool previous=skyChanged;skyChanged=false;++skyDepth;
    struct Scope {HGLRC context;bool previous;~Scope(){bool changed=skyChanged;--skyDepth;if(changed&&context&&wglGetCurrentContext()==context)restoreDriverBinding();skyChanged=previous||(skyDepth&&changed);}} guard{context,previous};originalDraw();
}
#undef SKYBOX_ALIGN
static bool install(HMODULE hw){
    if(available())return true;if(!hw)return false;wchar_t path[MAX_PATH];DWORD n=GetModuleFileNameW(hw,path,MAX_PATH);
    if(!n||n>=MAX_PATH||!supportedFile(path)||!supportedImage(hw)){message=u8"Этот движок не поддерживает безопасную замену неба.";logLine("Skybox: unsupported engine fingerprint; native sky retained.");return false;}
    auto b=reinterpret_cast<unsigned char *>(hw);if(MH_CreateHook(b+bindRva,reinterpret_cast<void *>(bind),reinterpret_cast<void **>(&originalBind))!=MH_OK){message=u8"Не удалось подключить безопасную замену неба.";return false;}
    if(MH_CreateHook(b+drawRva,reinterpret_cast<void *>(draw),reinterpret_cast<void **>(&originalDraw))!=MH_OK){MH_RemoveHook(b+bindRva);originalBind=nullptr;message=u8"Не удалось подключить безопасную замену неба.";return false;}
    engineModule=hw;hooked.store(true,std::memory_order_release);message=u8"Стандартное небо карты.";logLine("Skybox: verified native sky-pass and bind hooks installed; engine sky state untouched.");return true;
}
} // namespace Skybox
