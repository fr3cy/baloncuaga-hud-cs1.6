#pragma once
#include <windows.h>
#include <GL/gl.h>

namespace UiSurface {
struct Size {int width,height;};
static bool valid(Size size){return size.width>0&&size.height>0&&size.width<=32768&&size.height<=32768;}
// The GL viewport describes the drawable used by this pass. The desktop client
// can be larger when GoldSrc stretches 1280x1024 into a 1920x1080 window.
// Never use the desktop window size to enlarge glViewport beyond that drawable.
static Size choose(Size native,Size client,Size viewport){
    if(valid(viewport))return viewport;
    if(valid(native))return native;
    if(valid(client))return client;
    return valid(native)?native:Size{0,0};
}
static Size current(int nativeWidth,int nativeHeight){
    RECT rect={};Size client={0,0},viewport={0,0};
    if(wglGetCurrentContext()){
        GLint v[4]={};glGetIntegerv(GL_VIEWPORT,v);viewport={v[2],v[3]};
        if(valid(viewport))return viewport;
        if(valid({nativeWidth,nativeHeight}))return {nativeWidth,nativeHeight};
        HWND window=WindowFromDC(wglGetCurrentDC());DWORD pid=0;
        if(window)GetWindowThreadProcessId(window,&pid);
        if(pid==GetCurrentProcessId()&&GetClientRect(window,&rect))client={rect.right-rect.left,rect.bottom-rect.top};
    }
    return choose({nativeWidth,nativeHeight},client,viewport);
}
static float mouseCoordinate(int value,int nativeExtent,int surfaceExtent){
    return nativeExtent>0?float(value)*surfaceExtent/nativeExtent:float(value);
}
}
