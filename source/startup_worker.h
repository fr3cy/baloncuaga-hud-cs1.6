#pragma once
#include <windows.h>

// Only scheduling happens under the loader lock. Windows starts the new thread
// after DLL attachment notifications have completed. InitializeASI and DllMain
// share this gate, so loaders which call both still start exactly one worker.
namespace StartupWorker {
static volatile LONG scheduled;
static HANDLE published;
static DWORD (WINAPI *routine)(void *);
static DWORD failure;
#if defined(__GNUC__)
__attribute__((force_align_arg_pointer))
#endif
static DWORD WINAPI dispatch(void *) {
    WaitForSingleObject(published,INFINITE);
    return routine(nullptr);
}
static bool schedule(DWORD (WINAPI *entry)(void *),HANDLE &thread) {
    if(InterlockedCompareExchange(&scheduled,1,0))return true;
    published=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!published){failure=GetLastError();InterlockedExchange(&scheduled,0);return false;}
    routine=entry;
    HANDLE created=CreateThread(nullptr,0,dispatch,nullptr,0,nullptr);
    if(!created){failure=GetLastError();CloseHandle(published);published=nullptr;InterlockedExchange(&scheduled,0);return false;}
    thread=created; // publish before dispatch can enable any game callback
    SetEvent(published);
    return true;
}
static void join(HANDLE &thread) {
    if(thread){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}
    if(published){CloseHandle(published);published=nullptr;}
}
}
