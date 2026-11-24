namespace ChatScroll {
static HWND window;
using Subclass= LRESULT (CALLBACK *)(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
using SetSubclass=BOOL (WINAPI *)(HWND,Subclass,UINT_PTR,DWORD_PTR);
using RemoveSubclass=BOOL (WINAPI *)(HWND,Subclass,UINT_PTR);
using GetSubclass=BOOL (WINAPI *)(HWND,Subclass,UINT_PTR,DWORD_PTR *);
using DefSubclass=LRESULT (WINAPI *)(HWND,UINT,WPARAM,LPARAM);
static HMODULE controls;
static bool ownControls;
static SetSubclass setSubclass;
static RemoveSubclass removeSubclass;
static GetSubclass getSubclass;
static DefSubclass defSubclass;
static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
static constexpr UINT_PTR subclassId=0x42414c4f;
static bool unicode;
static void start(){ChatEditor::begin(false);}
static void close(){ChatEditor::close();}
static void scroll(int amount){Hud::chatScroll=std::clamp(Hud::chatScroll+amount,0,Hud::chatScrollLimit);}
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    CallbackScope callbackScope;
    bool wide=unicode;
    if((message==WM_KEYDOWN||message==WM_SYSKEYDOWN)&&wp==VK_F11){unloadRequested=true;return 0;}
    if(message==WM_MOUSEWHEEL)Showkeys::wheelEvent(static_cast<short>(HIWORD(wp))); // Showkeys wheel counter (observe only)
    if(Hud::chatOpened && opened && row==10){
        if(message==WM_MOUSEWHEEL){int delta=static_cast<short>(HIWORD(wp));if(delta)scroll(delta>0?2:-2);return 0;}
        if(message==WM_KEYDOWN&&(wp==VK_PRIOR||wp==VK_NEXT)){scroll(wp==VK_PRIOR?5:-5);return 0;}
        if(message==WM_KEYDOWN){ChatEditor::key(wp);return 0;}
        if(message==WM_CHAR){ChatEditor::character(wp,wide);return 0;}
        if(message==WM_UNICHAR){if(wp==0xFFFF)return TRUE;ChatEditor::character(wp,true);return 0;}
        if(message==WM_KEYUP)return 0;
    }
    if(opened && Weave::acceptsInput()){
        if(message==WM_MOUSEWHEEL || message==WM_CHAR || message==WM_KEYDOWN || message==WM_KEYUP){
            Weave::event(message,wp,wide);
            if(message==WM_KEYDOWN && wp==VK_ESCAPE){opened=false;Mouse::sync();}
            return 0;
        }
        if(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_MBUTTONDOWN||message==WM_MBUTTONUP||message==WM_MOUSEMOVE)return 0;
    }
    if(Hud::chatOpened && opened && row==10){
        if(message==WM_KEYDOWN){ChatEditor::key(wp);return 0;}
        if(message==WM_CHAR){ChatEditor::character(wp,wide);return 0;}
        if(message==WM_UNICHAR){if(wp==0xFFFF)return TRUE;ChatEditor::character(wp,true);return 0;}
        if(message==WM_KEYUP)return 0;
    }
    if(Hud::chatOpened && !opened){
        if(message==WM_MOUSEWHEEL){int delta=static_cast<short>(HIWORD(wp));if(delta)scroll((delta>0?1:-1)*std::max(1,std::abs(delta)/WHEEL_DELTA)*2);return 0;}
        if(message==WM_KEYDOWN && (wp==VK_PRIOR||wp==VK_NEXT)){
            scroll(wp==VK_PRIOR?5:-5);return 0;
        }
        if(message==WM_KEYUP && (wp==VK_PRIOR||wp==VK_NEXT))return 0;
        if(message==WM_KEYDOWN){ChatEditor::key(wp);return 0;}
        if(message==WM_CHAR){ChatEditor::character(wp,wide);return 0;}
        if(message==WM_UNICHAR){if(wp==0xFFFF)return TRUE;ChatEditor::character(wp,true);return 0;}
        if(message==WM_KEYUP)return 0;
        if(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_MOUSEMOVE)return 0;
        if(message==WM_KILLFOCUS)close();
    }
    if(message==WM_NCDESTROY){removeSubclass(hwnd,procedure,subclassId);window=nullptr;close();}
    return defSubclass(hwnd,message,wp,lp);
}

static bool uninstall(){
    if(window&&IsWindow(window)){DWORD_PTR reference=0;
        if(getSubclass&&getSubclass(window,procedure,subclassId,&reference)&&!removeSubclass(window,procedure,subclassId))return false;
        if(getSubclass&&getSubclass(window,procedure,subclassId,&reference))return false;
    }
    window=nullptr;return true;
}
static void release(){
    if(ownControls&&controls)FreeLibrary(controls);
    ownControls=false;controls=nullptr;setSubclass=nullptr;getSubclass=nullptr;removeSubclass=nullptr;defSubclass=nullptr;
}
static bool resolveControls(){
    if(!controls)return false;
    // Common Controls 5.x exposes GetWindowSubclass only by ordinal. GoldSrc
    // commonly loads this version without a version-6 activation manifest.
    auto resolve=[](const char *name,WORD ordinal){auto fn=GetProcAddress(controls,name);return fn?fn:GetProcAddress(controls,MAKEINTRESOURCEA(ordinal));};
    setSubclass=reinterpret_cast<SetSubclass>(resolve("SetWindowSubclass",410));
    getSubclass=reinterpret_cast<GetSubclass>(resolve("GetWindowSubclass",411));
    removeSubclass=reinterpret_cast<RemoveSubclass>(resolve("RemoveWindowSubclass",412));
    defSubclass=reinterpret_cast<DefSubclass>(resolve("DefSubclassProc",413));
    if(!setSubclass||!getSubclass||!removeSubclass||!defSubclass)return false;
    // Another window hook can retain Common Controls' master WNDPROC after our
    // subclass is removed. Keep that Windows module alive for the process;
    // no BALONCUAGA callbacks or resources are retained by this pin.
    HMODULE pinned=nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(defSubclass),&pinned)!=FALSE;
}
static void install(){
    if(window&&IsWindow(window))return;
    HWND candidate=GetForegroundWindow();DWORD pid=0;DWORD thread=GetWindowThreadProcessId(candidate,&pid);
    if(!candidate||pid!=GetCurrentProcessId()||thread!=GetCurrentThreadId())return;
    if(!controls){controls=LoadLibraryA("comctl32.dll");ownControls=controls!=nullptr;
        if(!controls)return;
    }
    if(!resolveControls()){static bool logged=false;if(!logged){logged=true;logLine("Chat input: Common Controls subclass API unavailable.");}return;}
    unicode=IsWindowUnicode(candidate)!=0;
    if(setSubclass(candidate,procedure,subclassId,0)){window=candidate;logLine("Chat window subclass installed; independent removal supported.");}
}
}
