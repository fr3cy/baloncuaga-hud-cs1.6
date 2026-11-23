#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>

// Supported GameUI only: the original background paints first, decoration uses
// its native Surface, then native Repaint schedules the next background pass.
// No SwapBuffers painting, GL/ImGui state, resources, input or worker threads.
namespace NativeMenuAnimation {
static constexpr uint64_t supportedFnv=UINT64_C(0xd1c94a373e371908);
static constexpr size_t supportedFileBytes=842648;
static constexpr DWORD supportedTimestamp=0x5f28cefc,supportedImageBytes=0xe3000;
static constexpr uintptr_t paintRva=0x16e60,repaintRva=0x439c0,surfaceGetterRva=0x3f040;
static constexpr uintptr_t surfaceGlobalRva=0xca2ec,panelGlobalRva=0xc3ca8,panelVtableRva=0x92334;
using PanelFn=void (__thiscall *)(void *);
using ColorFn=void (__thiscall *)(void *,int,int,int,int);
using RectFn=void (__thiscall *)(void *,int,int,int,int);
using SizeFn=void (__thiscall *)(void *,int &,int &);
struct Surface {void *object;ColorFn color;RectFn rect,line;SizeFn size;};
static HMODULE module;
static PanelFn originalPaint,nativeRepaint;
static std::atomic<bool> installed{false},pendingArm{false};
static std::atomic<bool> loggedArm{false},loggedFrame{false},loggedMissingTiles{false};
static thread_local bool painting;

static bool readable(const void *pointer,size_t bytes){
    if(!pointer||!bytes)return false;uintptr_t at=reinterpret_cast<uintptr_t>(pointer),end=at+bytes;if(end<at)return false;
    while(at<end){MEMORY_BASIC_INFORMATION info={};if(!VirtualQuery(reinterpret_cast<const void *>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(next<=at)return false;at=next;}
    return true;
}
static bool moduleCode(const void *pointer,HMODULE owner){
    MEMORY_BASIC_INFORMATION info={};return pointer&&owner&&VirtualQuery(pointer,&info,sizeof(info))&&info.AllocationBase==owner&&info.Type==MEM_IMAGE&&info.State==MEM_COMMIT&&!(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static bool surfaceCode(const void *pointer){return moduleCode(pointer,GetModuleHandleW(L"vgui2.dll"))||moduleCode(pointer,GetModuleHandleW(L"hw.dll"));}
static bool peMetadata(const unsigned char *data,size_t bytes){
    if(bytes<sizeof(IMAGE_DOS_HEADER)||bytes<sizeof(IMAGE_NT_HEADERS32))return false;IMAGE_DOS_HEADER dos;memcpy(&dos,data,sizeof(dos));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0x40||size_t(dos.e_lfanew)>bytes-sizeof(IMAGE_NT_HEADERS32))return false;
    IMAGE_NT_HEADERS32 nt;memcpy(&nt,data+dos.e_lfanew,sizeof(nt));return nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&nt.FileHeader.TimeDateStamp==supportedTimestamp&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&nt.OptionalHeader.SizeOfImage==supportedImageBytes;
}
static uint64_t hashBytes(const unsigned char *data,size_t size,uint64_t hash=UINT64_C(14695981039346656037)){for(size_t i=0;i<size;++i)hash=(hash^data[i])*UINT64_C(1099511628211);return hash;}
static bool supportedFile(const wchar_t *path){
    FILE *file=_wfopen(path,L"rb");if(!file)return false;unsigned char block[4096];bool okay=false;
    if(fseek(file,0,SEEK_END)==0&&ftell(file)==static_cast<long>(supportedFileBytes)&&fseek(file,0,SEEK_SET)==0){
        size_t first=fread(block,1,sizeof(block),file);if(first==sizeof(block)&&peMetadata(block,first)){
            uint64_t hash=hashBytes(block,first);size_t total=first,count;while((count=fread(block,1,sizeof(block),file))!=0){hash=hashBytes(block,count,hash);total+=count;}
            okay=!ferror(file)&&total==supportedFileBytes&&hash==supportedFnv;}}
    fclose(file);return okay;
}
static bool supportedImage(HMODULE image){
    auto base=reinterpret_cast<const unsigned char *>(image);if(!readable(base,4096)||!peMetadata(base,4096))return false;
    const unsigned char paintBytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x30,0x53,0x55,0x56,0x8b,0xf1,0x8d,0x44,0x24,0x34,0x57,0x8d,0x4c,0x24,0x3c,0x50,0x51,0x8b,0xce,0x89,0x74,0x24,0x30,0xe8};
    const unsigned char repaintBytes[]={0x53,0x56,0x8b,0xf1,0x57,0xc6,0x46,0x30,0x01,0xe8,0x72,0xb6,0xff,0xff,0x8b,0xf8,0x8b,0x06,0x8b,0xce,0x8b,0x1f,0xff,0x10,0x50,0x8b,0xcf,0xff,0x93,0xb0,0x00,0x00,0x00};
    if(!moduleCode(base+paintRva,image)||!moduleCode(base+repaintRva,image)||!readable(base+paintRva,sizeof(paintBytes))||!readable(base+repaintRva,sizeof(repaintBytes))||memcmp(base+paintRva,paintBytes,sizeof(paintBytes))||memcmp(base+repaintRva,repaintBytes,sizeof(repaintBytes)))return false;
    if(!moduleCode(base+surfaceGetterRva,image)||!readable(base+surfaceGetterRva,6)||base[surfaceGetterRva]!=0xa1||base[surfaceGetterRva+5]!=0xc3)return false;
    uintptr_t address=0;memcpy(&address,base+surfaceGetterRva+1,sizeof(address));return address==reinterpret_cast<uintptr_t>(base)+surfaceGlobalRva;
}
static bool getSurface(Surface &surface){
    if(!module)return false;auto slot=reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(module)+surfaceGlobalRva);if(!readable(slot,sizeof(void *)))return false;
    void *object=*slot;if(!readable(object,sizeof(void *)))return false;void **vtable=*reinterpret_cast<void ***>(object);if(!readable(vtable,33*sizeof(void *)))return false;
    // These slots are verified against this exact native binary's call sites.
    // +0x20=int4 colour, +0x24=filled rect, +0x2c=four-int line, +0x80=size.
    if(!surfaceCode(vtable[8])||!surfaceCode(vtable[9])||!surfaceCode(vtable[11])||!surfaceCode(vtable[32]))return false;
    surface={object,reinterpret_cast<ColorFn>(vtable[8]),reinterpret_cast<RectFn>(vtable[9]),reinterpret_cast<RectFn>(vtable[11]),reinterpret_cast<SizeFn>(vtable[32])};return true;
}
static bool knownPanel(void *panel){
    if(!module||!readable(panel,0x90))return false;void *vtable=*reinterpret_cast<void **>(panel);if(vtable!=reinterpret_cast<unsigned char *>(module)+panelVtableRva)return false;
    auto bytes=static_cast<unsigned char *>(panel);int tiles=0,canvasWidth=0,canvasHeight=0;void *list=nullptr;
    memcpy(&tiles,bytes+0x80,sizeof(tiles));memcpy(&list,bytes+0x74,sizeof(list));memcpy(&canvasWidth,bytes+0x88,sizeof(canvasWidth));memcpy(&canvasHeight,bytes+0x8c,sizeof(canvasHeight));
    if(tiles<=0||tiles>128||canvasWidth<=0||canvasHeight<=0||canvasWidth>16384||canvasHeight>16384||!readable(list,size_t(tiles)*0x18))return false;
    // The verified native loader writes six-field, 0x18-byte entries. The draw
    // helper reads texture/x/y/width/height at +0/+4/+8/+c/+10 and scaled at +14.
    for(int i=0;i<tiles;++i){int fields[5];memcpy(fields,static_cast<unsigned char *>(list)+i*0x18,sizeof(fields));
        if(fields[0]<=0||fields[1]<0||fields[2]<0||fields[3]<=0||fields[4]<=0||fields[3]>canvasWidth||fields[4]>canvasHeight||fields[1]>canvasWidth-fields[3]||fields[2]>canvasHeight-fields[4])return false;}
    return true;
}
static bool enabled(){
    if(!ready||unloading||unloadRequested||!MenuTheme::nativeBackgroundAnimation||MenuTheme::reduceMotion||!eng||!eng->pfnGetLevelName)return false;
    const char *level=eng->pfnGetLevelName();return !level||!*level;
}
static void colour(const Surface &s,int grey,int alpha){s.color(s.object,grey,grey,grey,alpha);}
static bool decorate(const Surface &s,double time){
    int width=0,height=0;s.size(s.object,width,height);if(width<320||height<240||width>16384||height>16384||!std::isfinite(time))return false;
    double t=std::fmod(time,600.);if(t<0)t+=600.;
    auto fraction=[](double value){return value-std::floor(value);};
    auto pulse=[](double value){return 1.-std::abs(value*2.-1.);};
    const int left=int(width*.45),right=int(width*.97),span=right-left;
    const int unit=(std::max)(1,int((std::min)(width,height)*.0015));
    // Short motion strokes stay to the right of the native navigation. Each
    // frame starts from the real background, so the finish never leaves trails.
    for(int i=0;i<8;++i){double phase=fraction(t*.055+i*.137);
        int x1=left+int(span*phase),x0=(std::max)(left,x1-int(span*(.13+.035*(i%3))));
        int y1=int(height*(.225+.062*i)),y0=y1+int(height*.018);if(x1<=x0)continue;
        int alpha=9+int(17*pulse(phase));colour(s,232,alpha);s.line(s.object,x0,y0,x1,y1);
        if(i%2==0&&x1-x0>5){colour(s,220,alpha/3);s.line(s.object,x0,y0+unit,x1,y1+unit);}}
    // Repeated sole notches suggest tread and forward movement without placing
    // another image or a bright decorative shape over the shoe photography.
    const int notch=(std::max)(2,int(span*.012)),rise=(std::max)(1,int(height*.007));
    for(int i=0;i<12;++i){double phase=fraction(i/12.+t*.02);
        int x=left+int(span*(.03+.88*phase)),y=int(height*(.755+.018*(i%3)));
        int alpha=16+int(16*pulse(fraction(t*.11+i*.13)));colour(s,218,alpha);
        s.line(s.object,x,y,x+notch,y-rise);s.line(s.object,x+notch,y-rise,x+notch*2,y);}
    // A quiet, straight sheen moves across the product area like studio light.
    int sheen=left+int(span*(.12+.70*fraction(t*.035))),lean=int(span*.09);
    for(int i=0;i<3;++i){colour(s,242,13-i*4);s.line(s.object,sheen+i*unit,int(height*.29),sheen+lean+i*unit,int(height*.68));}
    int baseline=int(height*.875);colour(s,210,12);s.line(s.object,left+int(span*.12),baseline,right-int(span*.02),baseline);
    int marker=left+int(span*(.12+.82*fraction(t*.09))),length=(std::max)(3,int(span*.009));
    colour(s,235,26);s.rect(s.object,marker,baseline-unit,marker+length,baseline+unit*2);
    colour(s,245,105);s.rect(s.object,marker,baseline,marker+length,baseline+unit);
    // The original non-empty tile loop ends with white colour; preserve that
    // native Surface postcondition before child buttons and dialogs are painted.
    s.color(s.object,255,255,255,255);return true;
}
template<typename Prepare> static bool paintPass(void *panel,PanelFn paint,PanelFn repaint,Prepare prepare){
    paint(panel);Surface surface;double time=0;if(!prepare(surface,time)||!decorate(surface,time))return false;repaint(panel);return true;
}
#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
static void __fastcall paint(void *panel,void *){
    CallbackScope scope;if(painting){originalPaint(panel);return;}painting=true;struct Scope {~Scope(){painting=false;}} guard;
    bool drawn=paintPass(panel,originalPaint,nativeRepaint,[panel](Surface &surface,double &time){
        if(!enabled())return false;if(!knownPanel(panel)){if(!loggedMissingTiles.exchange(true))logLine("Native menu animation: native background tiles unavailable; decorative pass skipped.");return false;}
        if(!getSurface(surface))return false;
        time=double(GetTickCount64()%600000)/1000.*(std::max)(10,(std::min)(400,MenuTheme::motionSpeed))/100.;return true;});
    if(drawn&&!loggedFrame.exchange(true))logLine("Native menu animation: first native background frame painted.");
}
static bool available(){return installed.load(std::memory_order_acquire);}
// Render-thread calls only. Installation itself never invokes a VGUI method.
static void *backgroundPanel(){
    if(!module)return nullptr;auto address=reinterpret_cast<void **>(reinterpret_cast<unsigned char *>(module)+panelGlobalRva);return readable(address,sizeof(void *))&&knownPanel(*address)?*address:nullptr;
}
static void requestRepaint(){
    if(!available()||!nativeRepaint)return;if(void *panel=backgroundPanel())nativeRepaint(panel);
}
static void armFromRenderThread(){
    if(!available())return;if(!enabled()){pendingArm.store(true,std::memory_order_release);return;}
    if(pendingArm.load(std::memory_order_acquire)){if(void *panel=backgroundPanel()){nativeRepaint(panel);pendingArm.store(false,std::memory_order_release);if(!loggedArm.exchange(true))logLine("Native menu animation: initial native repaint armed.");}}
}
static bool install(HMODULE gameui){
    if(available())return true;if(!gameui)return false;wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(gameui,path,MAX_PATH);
    if(!length||length>=MAX_PATH||!supportedFile(path)||!supportedImage(gameui)){logLine("Native menu animation: unsupported GameUI build; native background retained.");return false;}
    auto base=reinterpret_cast<unsigned char *>(gameui);if(MH_CreateHook(base+paintRva,reinterpret_cast<void *>(paint),reinterpret_cast<void **>(&originalPaint))!=MH_OK){logLine("Native menu animation: background hook unavailable; native background retained.");return false;}
    module=gameui;nativeRepaint=reinterpret_cast<PanelFn>(base+repaintRva);installed.store(true,std::memory_order_release);pendingArm.store(true,std::memory_order_release);
    logLine("Native menu animation: verified native background pass; native Surface drawing and repaint, no idle overlay.");return true;
}
} // namespace NativeMenuAnimation
