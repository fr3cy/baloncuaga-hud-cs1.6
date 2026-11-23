#pragma once
// Read-only avatar access through existing Steam interfaces; never initialize Steam,
// create a pipe, dispatch its callbacks or advertise installed ASIs to other players.
// ABI: Valve Source SDK public/steam ISteamClient / Friends017 / Utils010.
namespace Profile {
static int imageHandle;
static uint64_t account;
static ULONGLONG checked;
static HGLRC textureContext;
static std::vector<byte> pixels;
static unsigned width,height;
static bool table(void *p,int entries){return p&&Hud::readable(p,sizeof(void*))&&Hud::readable(*reinterpret_cast<void ***>(p),entries*sizeof(void*));}
template<class T>static T method(void *p,int slot,HMODULE owner){
    if(!table(p,slot+1))return nullptr;void *fn=(*reinterpret_cast<void ***>(p))[slot];return executableInModule(fn,owner)?reinterpret_cast<T>(fn):nullptr;
}
static uint64_t localAccount(){
    HKEY key=nullptr;DWORD value=0,size=sizeof(value),type=0;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\Valve\\Steam\\ActiveProcess",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return 0;
    LONG result=RegQueryValueExA(key,"ActiveUser",nullptr,&type,reinterpret_cast<BYTE *>(&value),&size);RegCloseKey(key);
    return result==ERROR_SUCCESS&&type==REG_DWORD&&size==sizeof(value)&&value?76561197960265728ULL+value:0;
}
static bool readSteam(){
    HMODULE api=GetModuleHandleA("steam_api.dll"),clientModule=GetModuleHandleA("steamclient.dll");if(!api||!clientModule)return false;
    using Factory=void *(__cdecl *)(const char *,int *);using Handle=int(__cdecl *)();
    auto factory=reinterpret_cast<Factory>(GetProcAddress(clientModule,"CreateInterface"));
    auto user=reinterpret_cast<Handle>(GetProcAddress(api,"SteamAPI_GetHSteamUser"));auto pipe=reinterpret_cast<Handle>(GetProcAddress(api,"SteamAPI_GetHSteamPipe"));
    if(!factory||!user||!pipe)return false;int hUser=user(),hPipe=pipe();if(!hUser||!hPipe)return false;
    void *client=factory("SteamClient020",nullptr);if(!client)return false;
    using Friends=void *(__thiscall *)(void *,int,int,const char *);using Utils=void *(__thiscall *)(void *,int,const char *);
    auto getFriends=method<Friends>(client,8,clientModule);auto getUtils=method<Utils>(client,9,clientModule);if(!getFriends||!getUtils)return false;
    void *friends=getFriends(client,hUser,hPipe,"SteamFriends017"),*utils=getUtils(client,hPipe,"SteamUtils010");
    using Avatar=int(__thiscall *)(void *,uint64_t);using Size=bool(__thiscall *)(void *,int,unsigned *,unsigned *);using Image=bool(__thiscall *)(void *,int,byte *,int);
    auto getAvatar=method<Avatar>(friends,35,clientModule);auto getSize=method<Size>(utils,5,clientModule);auto getImage=method<Image>(utils,6,clientModule);
    uint64_t id=localAccount();if(!id||!getAvatar||!getSize||!getImage)return false;
    int handle=getAvatar(friends,id);if(handle<=0)return false;if(handle==imageHandle && id==account && !pixels.empty())return true;
    unsigned w=0,h=0;if(!getSize(utils,handle,&w,&h)||!w||!h||w>512||h>512)return false;
    std::vector<byte> data(static_cast<size_t>(w)*h*4);if(!getImage(utils,handle,data.data(),data.size()))return false;
    pixels=std::move(data);width=w;height=h;account=id;imageHandle=handle;return true;
}
static void update(){
    ULONGLONG now=GetTickCount64();bool changed=false;
    if(now-checked>3000){checked=now;int old=imageHandle;uint64_t oldAccount=account;bool ok=readSteam();changed=ok&&(old!=imageHandle||oldAccount!=account);}
    if(pixels.empty())return;
    if(textureContext!=wglGetCurrentContext()){avatar=0;textureContext=wglGetCurrentContext();}
    if(!avatar||changed){
        glPushAttrib(GL_TEXTURE_BIT);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        if(!avatar)glGenTextures(1,&avatar);glBindTexture(GL_TEXTURE_2D,avatar);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glPopClientAttrib();glPopAttrib();
    }
}
static void draw(ImVec2 p,const std::string &nick){
    auto *d=ImGui::GetWindowDrawList();if(avatar){d->AddImageRounded(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(avatar)),p,p+ImVec2(24,24),{0,0},{1,1},ImGui::GetColorU32(IM_COL32_WHITE),12);return;}
    d->AddCircleFilled(p+ImVec2(12,12),12,ImGui::GetColorU32(ImGuiCol_Scheme));std::string initial=Hud::utf8(Hud::wide(nick).substr(0,1));auto size=ImGui::CalcTextSize(initial.c_str());d->AddText(p+ImVec2(12,12)-size/2,ui::carbonMoon?ImGui::GetColorU32(ImGuiCol_CheckMark):ImGui::GetColorU32(IM_COL32_WHITE),initial.c_str());
}
}
