#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "weave/imgui/imgui.h"
#include "sneaker_image.h"

// One embedded RGBA shoe for the menu badge and both style previews. The GL
// object belongs to the render context; no disk IO or worker threads are used.
namespace SneakerArt {
inline float unit(float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;}
inline bool finite(ImVec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
inline ImU32 grey(int v,float a=1.f){v=std::clamp(v,0,255);return IM_COL32(v,v,v,int(unit(a)*255.f+.5f));}
static GLuint texture;static HGLRC textureContext;static bool failed;
template<class T> static T procedure(const char *name){
    auto p=wglGetProcAddress(name);auto n=reinterpret_cast<uintptr_t>(p);
    return n>3&&n!=UINTPTR_MAX?reinterpret_cast<T>(p):nullptr;
}
static bool extension(const char *name){
    const char *start=reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));if(!start)return false;
    const char *p=start;const size_t n=std::strlen(name);while((p=std::strstr(p,name))){if((p==start||p[-1]==' ')&&(p[n]==0||p[n]==' '))return true;p+=n;}return false;
}
struct UploadState {
    using Active=void (APIENTRY *)(GLenum);using Bind=void (APIENTRY *)(GLenum,GLuint);
    Active active;Bind bind;GLint oldActive=0x84c0,oldBuffer=0;
    UploadState(){
        active=procedure<Active>("glActiveTexture");if(!active)active=procedure<Active>("glActiveTextureARB");
        glPushAttrib(GL_TEXTURE_BIT);glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        if(active){glGetIntegerv(0x84e0,&oldActive);active(0x84c0);}
        const auto *v=reinterpret_cast<const char *>(glGetString(GL_VERSION));
        int major=v?std::atoi(v):0,minor=0;if(v){const char *dot=std::strchr(v,'.');if(dot)minor=std::atoi(dot+1);}
        if(major>2||(major==2&&minor>=1)||extension("GL_ARB_pixel_buffer_object")){
            bind=procedure<Bind>("glBindBuffer");if(!bind)bind=procedure<Bind>("glBindBufferARB");
            if(bind){glGetIntegerv(0x88ef,&oldBuffer);bind(0x88ec,0);}
        }else bind=nullptr;
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);glPixelStorei(GL_UNPACK_LSB_FIRST,GL_FALSE);
    }
    ~UploadState(){
        if(bind)bind(0x88ec,static_cast<GLuint>(oldBuffer));
        glPopClientAttrib();glPopAttrib();if(active)active(static_cast<GLenum>(oldActive));
    }
};
static void resetVideo(){
    if(texture&&textureContext==wglGetCurrentContext())glDeleteTextures(1,&texture);
    texture=0;textureContext=nullptr;failed=false;
}
static bool mipmaps(const Skybox::Image &image,std::vector<Skybox::Image> &levels){
    try{
        levels.clear();levels.reserve(10);
        while(true){const auto &source=levels.empty()?image:levels.back();if(source.width==1&&source.height==1)return true;
            Skybox::Image next;next.width=std::max(1,source.width/2);next.height=std::max(1,source.height/2);next.rgba.resize(size_t(next.width)*next.height*4);
            for(int y=0;y<next.height;++y)for(int x=0;x<next.width;++x){unsigned a=0,r=0,g=0,b=0;
                for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto *p=source.rgba.data()+(size_t(std::min(source.height-1,y*2+j))*source.width+std::min(source.width-1,x*2+i))*4;
                    a+=p[3];r+=unsigned(p[0])*p[3];g+=unsigned(p[1])*p[3];b+=unsigned(p[2])*p[3];}
                auto *p=next.rgba.data()+(size_t(y)*next.width+x)*4;p[0]=a?(r+a/2)/a:0;p[1]=a?(g+a/2)/a:0;p[2]=a?(b+a/2)/a:0;p[3]=(a+2)/4;
            }levels.push_back(std::move(next));
        }
    }catch(const std::bad_alloc &){return false;}catch(const std::length_error &){return false;}
}
static GLuint image(){
    HGLRC current=wglGetCurrentContext();if(!current)return 0;
    if(textureContext!=current){texture=0;failed=false;textureContext=current;}
    if(texture||failed)return texture;
    Skybox::Image decoded;if(!Skybox::decodeTga(SneakerImage::tga,sizeof(SneakerImage::tga),decoded)||decoded.width!=512||decoded.height!=256){failed=true;return 0;}
    std::vector<Skybox::Image> levels;if(!mipmaps(decoded,levels)){failed=true;return 0;}
    UploadState state;GLuint candidate=0;glGenTextures(1,&candidate);if(!candidate){failed=true;return 0;}
    glBindTexture(GL_TEXTURE_2D,candidate);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    for(size_t level=0;level<=levels.size();++level){const auto &pixels=level?levels[level-1]:decoded;
        glTexImage2D(GL_TEXTURE_2D,GLint(level),GL_RGBA,pixels.width,pixels.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());
        GLint w=0,h=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,GLint(level),GL_TEXTURE_HEIGHT,&h);
        if(w!=pixels.width||h!=pixels.height){glDeleteTextures(1,&candidate);failed=true;return 0;}
    }
    texture=candidate;return texture;
}
inline void carbon(ImDrawList *draw,ImVec2 a,ImVec2 b,float strength){
    strength=unit(strength)*unit(ImGui::GetStyle().Alpha);if(!draw||strength<=0.f||!finite(a)||!finite(b)||b.x<=a.x||b.y<=a.y)return;
    const float width=b.x-a.x,height=b.y-a.y;if(!std::isfinite(width)||!std::isfinite(height)||width>32768||height>32768)return;
    const float pitch=std::max(16.f,std::sqrt(width*height/620.f));draw->PushClipRect(a,b,true);int row=0;
    for(float y=a.y;y<b.y;y+=pitch,++row){int col=0;for(float x=a.x;x<b.x;x+=pitch,++col){
        const float x1=std::min(x+pitch-.8f,b.x),y1=std::min(y+pitch-.8f,b.y);bool vertical=((col+row)%4)<2;
        draw->AddRectFilled({x,y},{x1,y1},grey(vertical?2:30,strength*.24f));
        if(vertical){float f1=x+pitch*.23f,f2=x+pitch*.58f;if(f1<x1)draw->AddRectFilled({f1,std::min(y+.6f,y1)},{std::min(f1+1.f,x1),y1},grey(112,strength*.10f));if(f2<x1)draw->AddRectFilled({f2,std::min(y+.6f,y1)},{std::min(f2+.7f,x1),y1},grey(160,strength*.065f));}
        else{float f1=y+pitch*.23f,f2=y+pitch*.58f;if(f1<y1)draw->AddRectFilled({std::min(x+.6f,x1),f1},{x1,std::min(f1+1.f,y1)},grey(112,strength*.10f));if(f2<y1)draw->AddRectFilled({std::min(x+.6f,x1),f2},{x1,std::min(f2+.7f,y1)},grey(160,strength*.065f));}
    }}draw->PopClipRect();
}
inline void fallback(ImDrawList *d,ImVec2 c,float width,float alpha,float angle){
    auto p=[&](float x,float y){float cs=std::cos(angle),sn=std::sin(angle);return ImVec2(c.x+width*(x*cs-y*sn),c.y+width*(x*sn+y*cs));};
    const ImVec2 upper[]={p(-.87f,.05f),p(-.72f,-.31f),p(-.49f,-.27f),p(-.35f,-.05f),p(-.08f,-.17f),p(.14f,-.05f),p(.64f,.04f),p(.92f,.20f),p(.86f,.33f),p(-.81f,.31f)};
    for(int i=0;i<IM_ARRAYSIZE(upper);++i)d->AddTriangleFilled(p(0,.14f),upper[i],upper[(i+1)%IM_ARRAYSIZE(upper)],grey(165,alpha));
    d->AddPolyline(upper,IM_ARRAYSIZE(upper),grey(242,alpha),ImDrawFlags_Closed,std::max(.8f,width*.012f));
    const ImVec2 sole[]={p(-.86f,.24f),p(-.34f,.31f),p(.28f,.29f),p(.94f,.18f),p(.96f,.36f),p(.71f,.48f),p(-.79f,.46f),p(-.92f,.37f)};
    for(int i=0;i<IM_ARRAYSIZE(sole);++i)d->AddTriangleFilled(p(0,.35f),sole[i],sole[(i+1)%IM_ARRAYSIZE(sole)],grey(229,alpha));
    d->AddPolyline(sole,IM_ARRAYSIZE(sole),grey(72,alpha),ImDrawFlags_Closed,std::max(.7f,width*.013f));
    for(int i=0;i<5;++i){float x=-.33f+i*.10f;d->AddLine(p(x,-.11f),p(x+.15f,.07f),grey(35,alpha),std::max(1.f,width*.025f));}
    for(int i=0;i<8;++i){float x=-.73f+i*.21f;d->AddLine(p(x,.40f),p(x+.06f,.32f),grey(65,alpha*.85f),std::max(.7f,width*.011f));}
}
inline void shoe(ImDrawList *d,ImVec2 center,float halfWidth,float time,float alpha=1.f,bool allowUpload=true){
    alpha=unit(alpha);if(!d||alpha<=0||!finite(center)||!std::isfinite(halfWidth)||halfWidth<2.f)return;
    halfWidth=std::min(halfWidth,420.f);if(!std::isfinite(time))time=0;float t=std::fmod(time,600.f),angle=-.045f+.022f*std::sin(t*.65f);
    center.y+=std::sin(t*.8f)*std::min(2.2f,halfWidth*.024f);
    // A low contact shadow anchors the shoe without an astronomical disc/orbit.
    for(int i=3;i>0;--i)d->AddRectFilled(center+ImVec2(-halfWidth*.71f-i,halfWidth*.37f-i),center+ImVec2(halfWidth*.76f+i,halfWidth*.42f+i),grey(0,alpha*.05f),halfWidth*.12f);
    GLuint tex=allowUpload?image():(textureContext==wglGetCurrentContext()?texture:0);
    if(!tex){fallback(d,center,halfWidth,alpha,angle);return;}
    auto p=[&](float x,float y){float cs=std::cos(angle),sn=std::sin(angle);return ImVec2(center.x+x*cs-y*sn,center.y+x*sn+y*cs);};
    d->AddImageQuad(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex)),p(-halfWidth,-halfWidth*.5f),p(halfWidth,-halfWidth*.5f),p(halfWidth,halfWidth*.5f),p(-halfWidth,halfWidth*.5f),{0,0},{1,0},{1,1},{0,1},grey(255,alpha));
}

struct PerimeterPoint{ImVec2 pos,normal;};
inline float perimeterLength(ImVec2 size){return 2.f*(size.x+size.y-64.f)+100.5309649f;}
inline PerimeterPoint perimeter(ImVec2 origin,ImVec2 size,float distance){
    constexpr float pi=3.14159265358979323846f,r=16.f,arc=pi*r*.5f;
    float w=size.x-2*r,h=size.y-2*r,length=std::max(1.f,2*(w+h)+4*arc);
    float d=std::fmod(distance,length);if(d<0)d+=length;
    auto corner=[&](float x,float y,float angle,float local){angle+=local/r;ImVec2 n(std::cos(angle),std::sin(angle));return PerimeterPoint{{origin.x+x+n.x*r,origin.y+y+n.y*r},n};};
    if(d<w)return {{origin.x+r+d,origin.y},{0,-1}};d-=w;
    if(d<arc)return corner(size.x-r,r,-pi*.5f,d);d-=arc;
    if(d<h)return {{origin.x+size.x,origin.y+r+d},{1,0}};d-=h;
    if(d<arc)return corner(size.x-r,size.y-r,0,d);d-=arc;
    if(d<w)return {{origin.x+size.x-r-d,origin.y+size.y},{0,1}};d-=w;
    if(d<arc)return corner(r,size.y-r,pi*.5f,d);d-=arc;
    if(d<h)return {{origin.x,origin.y+size.y-r-d},{-1,0}};d-=h;
    return corner(r,r,pi,std::min(d,arc));
}
inline void surround(ImDrawList *draw,ImVec2 origin,ImVec2 size,float time,float progress,bool particles,bool reduceMotion){
    progress=unit(progress);if(!draw||progress<=0.f||!finite(origin)||!finite(size)||size.x<40||size.y<40||size.x>32768||size.y>32768)return;
    const float styleAlpha=unit(ImGui::GetStyle().Alpha);if(styleAlpha<=0.f)return;
    auto color=[&](int value,float alpha){return grey(value,alpha*styleAlpha);};
    if(!std::isfinite(time))time=0.f;float t=reduceMotion?0.f:std::fmod(time,600.f);
    for(int layer=7;layer>=1;--layer){float e=layer*3.f;
        draw->AddRectFilled({origin.x-e,origin.y-e+6},{origin.x+size.x+e,origin.y+size.y+e+6},color(0,(8-layer)*.008f),16+e);}
    for(int layer=3;layer>=1;--layer){float e=layer*2.f;
        draw->AddRect({origin.x-e,origin.y-e},{origin.x+size.x+e,origin.y+size.y+e},color(230,.014f*(4-layer)),16+e,0,1.2f);}
    const float length=perimeterLength(size),head=t*54.f;
    for(int k=0;k<2;++k)for(int i=0;i<18;++i){float a=head+k*length*.5f-96.f+i*(96.f/18),next=a+96.f/18;
        auto p=perimeter(origin,size,a),q=perimeter(origin,size,next);float light=(i+1)/18.f;
        draw->AddLine(p.pos,q.pos,color(245,.16f+light*.40f),1.f);}
    if(!reduceMotion&&progress<.999f){float e=(1-progress)*19.f;
        draw->AddRect({origin.x-e,origin.y-e},{origin.x+size.x+e,origin.y+size.y+e},color(220,(1-progress)*.11f),16+e,0,1.f);}
    if(!particles||reduceMotion)return;
    for(int i=0;i<14;++i){float seed=i*.61803398875f,phase=seed*6.2831853f;
        auto p=perimeter(origin,size,seed*length+t*(i%2?7.f:-5.f));float away=10+(i%4)*7+3.f*std::sin(t*.3f+phase);
        ImVec2 c(p.pos.x+p.normal.x*away,p.pos.y+p.normal.y*away);float life=.28f+.5f*(.5f+.5f*std::sin(t*.45f+phase));
        draw->AddCircleFilled(c,3.8f,color(225,life*.035f),10);draw->AddCircleFilled(c,i%4==0?1.55f:1.f,color(235,life),8);
        if(i%4==0)draw->AddLine({c.x-4,c.y+2},{c.x+4,c.y-2},color(235,life*.35f),1.f);
    }
}
} // namespace SneakerArt
