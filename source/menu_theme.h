#pragma once
// Keep legacy settings compatible while Carbon 3XL has an independent layout.
namespace MenuTheme {
static constexpr int Aurora=2;
static int selected=Aurora;
// Aurora palettes: primary accent and secondary (gradient) colour.
enum { AccentCount=8 };
static int accent=0;
static const char *accentNames[]={"Aurora","Crimson","Mint","Violet","Sunset","Ocean","Mono","Toxic"};
static const unsigned char accents[AccentCount][2][3]={
    {{124,108,255},{56,201,255}},{{255,59,92},{255,140,66}},{{52,211,153},{45,212,191}},{{168,85,247},{217,70,239}},
    {{251,146,60},{244,63,94}},{{56,189,248},{59,130,246}},{{229,231,235},{148,155,170}},{{163,230,53},{34,197,94}}};
// Appearance options (Settings -> Appearance).
static bool customColors=false,gradient=true,frames=true;
static int custom[2][3]={{91,141,255},{168,85,247}};
static int windowOpacity=98;            // Aurora window backing, 60..100 %
static int auroraScale=100;             // Aurora menu size, 70..150 %
// Both templates share the same pages, input and persisted user colours.
enum { TemplateOrbit, TemplateAurora, TemplateCarbonMoon, TemplateCount };
static int menuTemplate=TemplateCarbonMoon;
static bool carbonStyle=false,rimGlow=true;
static int carbonStrength=35;
static bool isCarbon(){return menuTemplate==TemplateCarbonMoon;}
static bool animations=true,outerEffects=true,particles=true,reduceMotion=false;
static bool nativeBackgroundAnimation=true;
static int motionSpeed=100;
enum { LogoShine, LogoGradient, LogoPlain, LogoCount };
static int logoStyle=LogoShine;
static int menuKey=VK_INSERT;           // key that opens/closes the menu
static ImU32 accentColor(int which,float alpha=1.f,int index=-1){
    which=which?1:0;
    if(index<0&&carbonStyle){int c=which?158:238;return IM_COL32(c,c,c,static_cast<int>(std::clamp(alpha,0.f,1.f)*255));}
    if(index<0&&!gradient)which=0;
    const int a=static_cast<int>(std::clamp(alpha,0.f,1.f)*255);
    if(index<0&&customColors)return IM_COL32(custom[which][0],custom[which][1],custom[which][2],a);
    const auto &c=accents[std::clamp(index<0?accent:index,0,AccentCount-1)][which];
    return IM_COL32(c[0],c[1],c[2],a);
}
// Supported menu keys and their GoldSrc engine key numbers (keydefs.h).
struct MenuKey {int vk,engine;const char *name;};
static const MenuKey menuKeys[]={{VK_INSERT,147,"Insert"},{VK_DELETE,148,"Delete"},{VK_HOME,151,"Home"},{VK_END,152,"End"},
    {VK_PRIOR,150,"Page Up"},{VK_NEXT,149,"Page Down"},{VK_F1,135,"F1"},{VK_F2,136,"F2"},{VK_F3,137,"F3"},{VK_F4,138,"F4"},{VK_F5,139,"F5"},
    {VK_F6,140,"F6"},{VK_F7,141,"F7"},{VK_F8,142,"F8"},{VK_F9,143,"F9"},{VK_F10,144,"F10"},{VK_F12,146,"F12"}};
static const MenuKey &currentMenuKey(){for(const auto &k:menuKeys)if(k.vk==menuKey)return k;return menuKeys[0];}
static int menuEngineKey(){return currentMenuKey().engine;}
static int clampInt(const char *key,int fallback,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath)),lo,hi);}
static void load(){
    selected=Aurora;
    if(GetPrivateProfileIntA("interface","menu_theme",-1,iniPath)!=Aurora)WritePrivateProfileStringA("interface","menu_theme","2",iniPath);
    WritePrivateProfileStringA("interface","weave_menu","1",iniPath);accent=clampInt("aurora_accent",0,0,AccentCount-1);
    auroraScale=clampInt("aurora_scale",100,70,150);windowOpacity=clampInt("aurora_window_opacity",98,60,100);
    customColors=clampInt("aurora_custom_colors",0,0,1)!=0;gradient=clampInt("aurora_gradient",1,0,1)!=0;frames=clampInt("aurora_frames",1,0,1)!=0;
    logoStyle=clampInt("aurora_logo",LogoShine,0,LogoCount-1);
    menuTemplate=clampInt("menu_template",TemplateCarbonMoon,0,TemplateCount-1);
    if(clampInt("carbon_moon_revision",0,0,1)<1){
        menuTemplate=TemplateCarbonMoon;WritePrivateProfileStringA("interface","menu_template","2",iniPath);
        WritePrivateProfileStringA("interface","carbon_moon_revision","1",iniPath);
    }
    carbonStrength=clampInt("carbon_strength",35,0,100);rimGlow=clampInt("lunar_glow",1,0,1)!=0;
    animations=clampInt("menu_animations",1,0,1)!=0;outerEffects=clampInt("menu_outer_effects",1,0,1)!=0;
    particles=clampInt("menu_particles",1,0,1)!=0;reduceMotion=clampInt("menu_reduce_motion",0,0,1)!=0;
    motionSpeed=clampInt("menu_motion_speed",100,60,160);
    nativeBackgroundAnimation=clampInt("native_background_animation",1,0,1)!=0;
    for(int i=0;i<2;++i)for(int c=0;c<3;++c){char key[40];snprintf(key,sizeof(key),"aurora_custom_%d_%d",i,c);custom[i][c]=clampInt(key,custom[i][c],0,255);}
    int vk=clampInt("menu_key",VK_INSERT,0,255);menuKey=VK_INSERT;for(const auto &k:menuKeys)if(k.vk==vk)menuKey=vk;
}
static void save(){
    auto put=[](const char *key,int value){char text[12];snprintf(text,sizeof(text),"%d",value);WritePrivateProfileStringA("interface",key,text,iniPath);};
    selected=Aurora;put("menu_theme",Aurora);put("aurora_accent",accent);put("aurora_scale",auroraScale);put("aurora_window_opacity",windowOpacity);
    put("aurora_custom_colors",customColors);put("aurora_gradient",gradient);put("aurora_frames",frames);put("aurora_logo",logoStyle);put("menu_key",menuKey);
    put("menu_template",menuTemplate);put("menu_animations",animations);put("menu_outer_effects",outerEffects);
    put("menu_particles",particles);put("menu_reduce_motion",reduceMotion);put("menu_motion_speed",motionSpeed);
    put("native_background_animation",nativeBackgroundAnimation);
    put("carbon_moon_revision",1);put("carbon_strength",carbonStrength);put("lunar_glow",rimGlow);
    for(int i=0;i<2;++i)for(int c=0;c<3;++c){char key[40];snprintf(key,sizeof(key),"aurora_custom_%d_%d",i,c);put(key,custom[i][c]);}
}
static ImVec2 size(){return isCarbon()?ImVec2(1000,640):ImVec2(920,540);}
static void apply(){
    selected=Aurora;carbonStyle=isCarbon();ui::carbonMoon=carbonStyle;ui::aurora=!carbonStyle;
    if(carbonStyle){
        auto &s=ImGui::GetStyle();auto *c=s.Colors;
        c[ImGuiCol_Scheme]=ImColor(238,238,238);c[ImGuiCol_Text]=ImColor(236,236,236);c[ImGuiCol_TextDisabled]=ImColor(160,160,160);
        c[ImGuiCol_WindowBg]=ImColor(10,10,10);c[ImGuiCol_ChildBg]=ImColor(17,17,17);c[ImGuiCol_PopupBg]=ImColor(22,22,22);
        c[ImGuiCol_Border]=ImColor(50,50,50);c[ImGuiCol_BorderShadow]=ImColor(32,32,32);
        c[ImGuiCol_FrameBg]=ImColor(25,25,25);c[ImGuiCol_FrameBgHovered]=ImColor(39,39,39);c[ImGuiCol_FrameBgActive]=ImColor(49,49,49);
        c[ImGuiCol_Button]=ImColor(29,29,29);c[ImGuiCol_ButtonHovered]=ImColor(47,47,47);c[ImGuiCol_ButtonActive]=ImColor(65,65,65);
        c[ImGuiCol_Header]=ImColor(23,23,23);c[ImGuiCol_HeaderHovered]=ImColor(43,43,43);c[ImGuiCol_HeaderActive]=ImColor(61,61,61);
        c[ImGuiCol_CheckMark]=ImColor(15,15,15);c[ImGuiCol_SliderGrab]=ImColor(223,223,223);c[ImGuiCol_SliderGrabActive]=ImColor(255,255,255);
        c[ImGuiCol_NavHighlight]=ImColor(205,205,205);c[ImGuiCol_Separator]=ImColor(40,40,40);
        c[ImGuiCol_SeparatorHovered]=c[ImGuiCol_SeparatorActive]=ImColor(155,155,155);
        c[ImGuiCol_ScrollbarBg]=ImColor(0,0,0,0);c[ImGuiCol_ScrollbarGrab]=ImColor(72,72,72);
        c[ImGuiCol_ScrollbarGrabHovered]=ImColor(147,147,147);c[ImGuiCol_ScrollbarGrabActive]=ImColor(196,196,196);
        c[ImGuiCol_TextSelectedBg]=ImColor(180,180,180,78);
        s.WindowRounding=16;s.ChildRounding=10;s.PopupRounding=10;s.FrameRounding=5;s.GrabRounding=4;s.ScrollbarSize=4;s.ScrollbarRounding=4;
        return;
    }
    {
        auto &s=ImGui::GetStyle();auto *c=s.Colors;ui::aurora_glow=accentColor(1);
        c[ImGuiCol_Scheme]=ImColor(accentColor(0));c[ImGuiCol_Text]=ImColor(233,234,242);c[ImGuiCol_TextDisabled]=ImColor(154,157,179);
        c[ImGuiCol_WindowBg]=ImColor(15,16,24);c[ImGuiCol_ChildBg]=ImColor(18,19,27);c[ImGuiCol_PopupBg]=ImColor(24,26,36);
        c[ImGuiCol_Border]=ImColor(44,46,62);c[ImGuiCol_BorderShadow]=ImColor(34,36,48);
        c[ImGuiCol_FrameBg]=ImColor(11,12,18);c[ImGuiCol_FrameBgHovered]=ImColor(32,34,47);c[ImGuiCol_FrameBgActive]=ImColor(40,42,60);
        c[ImGuiCol_Button]=ImColor(24,26,36);c[ImGuiCol_ButtonHovered]=ImColor(34,36,52);c[ImGuiCol_ButtonActive]=ImColor(accentColor(0));
        c[ImGuiCol_Header]=ImColor(22,24,34);c[ImGuiCol_HeaderHovered]=ImColor(accentColor(0,.28f));c[ImGuiCol_HeaderActive]=ImColor(accentColor(0,.55f));
        c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=c[ImGuiCol_NavHighlight]=ImColor(accentColor(0));c[ImGuiCol_SliderGrabActive]=ImColor(accentColor(1));
        c[ImGuiCol_Separator]=ImColor(34,36,48);c[ImGuiCol_SeparatorHovered]=c[ImGuiCol_SeparatorActive]=ImColor(accentColor(0));
        c[ImGuiCol_ScrollbarBg]=ImColor(0,0,0,0);c[ImGuiCol_ScrollbarGrab]=ImColor(40,42,58);c[ImGuiCol_ScrollbarGrabHovered]=ImColor(accentColor(0));c[ImGuiCol_ScrollbarGrabActive]=ImColor(accentColor(1));
        c[ImGuiCol_TextSelectedBg]=ImColor(accentColor(0,.45f));
        s.WindowRounding=14;s.ChildRounding=12;s.PopupRounding=10;s.FrameRounding=8;s.GrabRounding=8;s.ScrollbarSize=4;s.ScrollbarRounding=4;
        return;
    }
}
}
