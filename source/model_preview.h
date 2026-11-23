#pragma once
#include <vector>
#include <string>
#include <limits>
#include <GL/gl.h>
#include "gl_names.h"

// Small, independent GoldSrc studio viewer. It renders the real mesh and skin
// in the first idle pose; it does not touch the game's studio renderer/state.
namespace Preview {
struct Point { float x,y,z; };
struct Vertex { Point p; float u,v; };
struct Skin { int width=0,height=0,flags=0; bool hands=false; GLuint handle=0; std::vector<byte> pixels; };
struct Mesh { int skin=-1; bool hands=false; std::vector<Vertex> triangles; };
struct Model { std::vector<Skin> skins; std::vector<Mesh> meshes; Point center={},weaponCenter={}; float radius=1,weaponRadius=1; bool hasWeapon=false; };
struct Matrix { float m[3][4]; };
static Model cached;
static std::string cachedPath;
static std::string error;
static HGLRC context;
static bool spin=true;
static bool showHands;
static float yaw=25,pitch=15;

template<class T> static const T *at(const std::vector<byte> &data, int64_t offset, int64_t count=1) {
    if (offset<0 || count<0 || offset%alignof(T) ||
        static_cast<uint64_t>(offset)>data.size() ||
        static_cast<uint64_t>(count)>(data.size()-static_cast<size_t>(offset))/sizeof(T)) return nullptr;
    return reinterpret_cast<const T *>(data.data()+offset);
}
static bool finite(Point p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
static bool handName(const char *raw,size_t size) {
    std::string name(raw,strnlen(raw,size));
    for(char &c:name)if(c>='A'&&c<='Z')c+=32;
    return name.find("hand")!=std::string::npos || name.find("glove")!=std::string::npos ||
        name.find("finger")!=std::string::npos || name.find("arm")!=std::string::npos ||
        name.find("sleeve")!=std::string::npos || name.find("view_skin")!=std::string::npos;
}
static Point transform(const Matrix &m, Point p) {
    return {m.m[0][0]*p.x+m.m[0][1]*p.y+m.m[0][2]*p.z+m.m[0][3],
            m.m[1][0]*p.x+m.m[1][1]*p.y+m.m[1][2]*p.z+m.m[1][3],
            m.m[2][0]*p.x+m.m[2][1]*p.y+m.m[2][2]*p.z+m.m[2][3]};
}
static Matrix concatenate(const Matrix &a, const Matrix &b) {
    Matrix c={};
    for(int i=0;i<3;++i) for(int j=0;j<4;++j) {
        for(int k=0;k<3;++k) c.m[i][j]+=a.m[i][k]*b.m[k][j];
        if(j==3)c.m[i][j]+=a.m[i][3];
    }
    return c;
}
static Matrix boneMatrix(float *v) {
    float sx=std::sin(v[3]),cx=std::cos(v[3]),sy=std::sin(v[4]),cy=std::cos(v[4]),sz=std::sin(v[5]),cz=std::cos(v[5]);
    // GoldSrc AngleQuaternion/QuaternionMatrix convention: Rz * Ry * Rx.
    return {{{cy*cz,sx*sy*cz-cx*sz,cx*sy*cz+sx*sz,v[0]},
             {cy*sz,sx*sy*sz+cx*cz,cx*sy*sz-sx*cz,v[1]},
             {-sy,sx*cy,cx*cy,v[2]}}};
}
static bool textures(const std::vector<byte> &data, Model &out,bool originalColors=false) {
    const studiohdr_t *header=at<studiohdr_t>(data,0);
    if(!header || header->numtextures<0 || header->numtextures>256) return false;
    const mstudiotexture_t *entries=at<mstudiotexture_t>(data,header->textureindex,header->numtextures);
    if(!entries && header->numtextures) return false;
    size_t totalPixels=0;
    for(int i=0;i<header->numtextures;++i) {
        const auto &t=entries[i];
        if(t.width<1 || t.height<1 || t.width>4096 || t.height>4096) return false;
        size_t count=static_cast<size_t>(t.width)*t.height;
        totalPixels+=count; if(totalPixels>16*1024*1024) return false;
        const byte *indices=at<byte>(data,t.index,count+768); if(!indices)return false;
        const byte *palette=indices+count;
        Skin skin;skin.width=t.width;skin.height=t.height;skin.flags=t.flags;skin.hands=handName(t.name,sizeof(t.name));skin.pixels.resize(count*4);
        for(size_t p=0;p<count;++p) {
            int index=indices[p];
            // A neutral preview exposure compensates for GoldSrc's texture gamma.
            for(int c=0;c<3;++c)skin.pixels[p*4+c]=originalColors?palette[index*3+c]:static_cast<byte>(255*std::pow(palette[index*3+c]/255.f,0.5f));
            skin.pixels[p*4+3]=(t.flags&STUDIO_NF_MASKED)&&index==255?0:255;
        }
        out.skins.push_back(std::move(skin));
    }
    return true;
}
static bool parse(const std::vector<byte> &data,const std::vector<byte> &textureData,Model &out,bool originalColors=false) {
    out=Model{};
    const studiohdr_t *h=at<studiohdr_t>(data,0);
    if(!h || h->id!=0x54534449 || h->version!=10 || h->length<static_cast<int>(sizeof(*h)) ||
       static_cast<size_t>(h->length)>data.size() || h->numbones<1 || h->numbones>128 ||
       h->numbodyparts<1 || h->numbodyparts>32 || h->numseq<1 || h->numseq>2048) return false;
    const auto *bones=at<mstudiobone_t>(data,h->boneindex,h->numbones);
    const auto *parts=at<mstudiobodyparts_t>(data,h->bodypartindex,h->numbodyparts);
    const auto *sequences=at<mstudioseqdesc_t>(data,h->seqindex,h->numseq);
    if(!bones || !parts || !sequences) return false;
    const mstudioseqdesc_t *seq=nullptr;
    for(int i=0;i<h->numseq;++i) if(sequences[i].seqgroup==0) {
        if(!seq) seq=&sequences[i];
        if(!_strnicmp(sequences[i].label,"idle",4)) { seq=&sequences[i];break; }
    }
    int64_t animBase=0;
    const mstudioanim_t *animation=nullptr;
    if(seq && h->numseqgroups>0 && h->numseqgroups<=32) {
        const auto *groups=at<mstudioseqgroup_t>(data,h->seqgroupindex,h->numseqgroups);
        if(groups) { animBase=static_cast<int64_t>(groups[0].unused2)+seq->animindex;
            animation=at<mstudioanim_t>(data,animBase,h->numbones); }
    }
    Matrix matrices[128];
    for(int i=0;i<h->numbones;++i) {
        if(bones[i].parent < -1 || bones[i].parent>=i) return false;
        float values[6];memcpy(values,bones[i].value,sizeof(values));
        for(int c=0;c<6;++c) {
            if(animation && animation[i].offset[c]) {
                auto *v=at<mstudioanimvalue_t>(data,animBase+i*sizeof(mstudioanim_t)+animation[i].offset[c],2);
                if(!v || !v[0].num.valid || !v[0].num.total || v[0].num.valid>v[0].num.total) return false;
                values[c]+=v[1].value*bones[i].scale[c];
            }
            if(!std::isfinite(values[c]) || std::fabs(values[c])>1e6) return false;
        }
        Matrix local=boneMatrix(values);
        matrices[i]=bones[i].parent<0?local:concatenate(matrices[bones[i].parent],local);
    }
    const auto &td=textureData.empty()?data:textureData;
    const studiohdr_t *th=at<studiohdr_t>(td,0);
    if(!th || th->id!=0x54534449 || th->version!=10 || !textures(td,out,originalColors))return false;
    const short *skinrefs=nullptr;
    if(th->numskinref>0 && th->numskinref<=4096 && th->numskinfamilies>0)
        skinrefs=at<short>(td,th->skinindex,th->numskinref);
    Point low={1e20f,1e20f,1e20f}, high={-1e20f,-1e20f,-1e20f};
    size_t budget=0;
    for(int p=0;p<h->numbodyparts;++p) {
        // Show bodygroup zero, matching the default first-person model body.
        const mstudiomodel_t *m=at<mstudiomodel_t>(data,parts[p].modelindex);
        if(parts[p].nummodels<1 || !m || m->numverts<0 || m->numverts>65536 || m->nummesh<0 || m->nummesh>256)return false;
        const Point *vertices=at<Point>(data,m->vertindex,m->numverts);
        const byte *boneIds=at<byte>(data,m->vertinfoindex,m->numverts);
        const auto *meshes=at<mstudiomesh_t>(data,m->meshindex,m->nummesh);
        if(!vertices || !boneIds || !meshes)return false;
        std::vector<Point> transformed(m->numverts);
        for(int v=0;v<m->numverts;++v) {
            if(boneIds[v]>=h->numbones || !finite(vertices[v]))return false;
            Point point=transform(matrices[boneIds[v]],vertices[v]);
            if(!finite(point) || std::max({std::fabs(point.x),std::fabs(point.y),std::fabs(point.z)})>1e6)return false;
            transformed[v]=point;
        }
        for(int n=0;n<m->nummesh;++n) {
            Mesh mesh; const auto &src=meshes[n];
            int texture=src.skinref;
            if(skinrefs && src.skinref>=0 && src.skinref<th->numskinref) texture=skinrefs[src.skinref];
            if(texture>=0 && texture<static_cast<int>(out.skins.size())) mesh.skin=texture;
            mesh.hands=handName(parts[p].name,sizeof(parts[p].name)) || (mesh.skin>=0 && out.skins[mesh.skin].hands);
            int64_t cursor=src.triindex;
            for(int command=0;command<65536;++command) {
                const short *countPtr=at<short>(data,cursor);if(!countPtr)return false;
                int signedCount=*countPtr;cursor+=2;
                if(!signedCount)break;
                int count=std::abs(signedCount);if(count<3 || count>32767)return false;
                const short *commands=at<short>(data,cursor,static_cast<int64_t>(count)*4);if(!commands)return false;
                cursor+=static_cast<int64_t>(count)*8;
                std::vector<Vertex> strip;strip.reserve(count);
                for(int k=0;k<count;++k) {
                    int index=commands[k*4];if(index<0 || index>=m->numverts)return false;
                    float tw=mesh.skin<0?1.f:out.skins[mesh.skin].width, ht=mesh.skin<0?1.f:out.skins[mesh.skin].height;
                    strip.push_back({transformed[index],commands[k*4+2]/tw,commands[k*4+3]/ht});
                }
                for(int k=2;k<count;++k) {
                    if(++budget>500000)return false;
                    int indices[3]={signedCount<0?0:k-2,k-1,k};
                    if(signedCount>0 && (k&1))std::swap(indices[0],indices[1]);
                    for(int index:indices) {
                        Vertex vertex=strip[index];mesh.triangles.push_back(vertex);
                        low.x=std::min(low.x,vertex.p.x);low.y=std::min(low.y,vertex.p.y);low.z=std::min(low.z,vertex.p.z);
                        high.x=std::max(high.x,vertex.p.x);high.y=std::max(high.y,vertex.p.y);high.z=std::max(high.z,vertex.p.z);
                    }
                }
                if(command==65535)return false;
            }
            if(!mesh.triangles.empty())out.meshes.push_back(std::move(mesh));
        }
    }
    if(out.meshes.empty())return false;
    out.center={(low.x+high.x)/2,(low.y+high.y)/2,(low.z+high.z)/2};
    float dx=high.x-low.x,dy=high.y-low.y,dz=high.z-low.z;
    out.radius=std::sqrt(dx*dx+dy*dy+dz*dz)/2;
    low={1e20f,1e20f,1e20f};high={-1e20f,-1e20f,-1e20f};
    for(const auto &mesh:out.meshes)if(!mesh.hands)for(const auto &vertex:mesh.triangles) {
        const auto &p=vertex.p;out.hasWeapon=true;
        low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
        high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);
    }
    if(out.hasWeapon) {
        out.weaponCenter={(low.x+high.x)/2,(low.y+high.y)/2,(low.z+high.z)/2};
        dx=high.x-low.x;dy=high.y-low.y;dz=high.z-low.z;out.weaponRadius=std::sqrt(dx*dx+dy*dy+dz*dz)/2;
        if(out.weaponRadius<0.001f)out.hasWeapon=false;
    }
    return std::isfinite(out.radius)&&out.radius>0.001f;
}
// Set on worker threads (radar builder): the engine file system is not thread-safe,
// so only the game folders on disk are searched there.
static thread_local bool diskOnly;
static std::vector<byte> readFile(const char *full) {
    std::vector<byte> result;FILE *f=fopen(full,"rb");
    if(f) {
        fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
        if(size>0 && size<=64*1024*1024) {result.resize(size);if(fread(result.data(),1,size,f)!=static_cast<size_t>(size))result.clear();}
        fclose(f);
    }
    return result;
}
static std::vector<byte> read(const char *path) {
    char full[MAX_PATH];snprintf(full,sizeof(full),"%s\\cstrike\\%s",rootPath,path);
    std::vector<byte> result=readFile(full);
    if(result.empty() && diskOnly) {
        for(const char *folder:{"cstrike_downloads","cstrike_addon","valve","valve_downloads"}){
            snprintf(full,sizeof(full),"%s\\%s\\%s",rootPath,folder,path);result=readFile(full);if(!result.empty())break;}
        return result;
    }
    if(result.empty() && eng->COM_LoadFile && eng->COM_FreeFile) {
        int size=0;byte *data=eng->COM_LoadFile(const_cast<char *>(path),5,&size);
        if(data) {if(size>0 && size<=64*1024*1024) result.assign(data,data+size);eng->COM_FreeFile(data);}
    }
    return result;
}
static void release() {
    if(wglGetCurrentContext() && wglGetCurrentContext()==context)
        for(auto &skin:cached.skins)if(skin.handle)glDeleteTextures(1,&skin.handle);
    cached=Model{};cachedPath.clear();error.clear();context=nullptr;
}
static void invalidateContext() {
    // The old context may already have been destroyed during a video/map reset.
    cached=Model{};cachedPath.clear();error.clear();context=nullptr;
}
static bool load(const char *path) {
    if(cachedPath==path && context==wglGetCurrentContext())return !cached.meshes.empty();
    release();cachedPath=path;context=wglGetCurrentContext();
    auto data=read(path);auto *header=at<studiohdr_t>(data,0);
    std::vector<byte> companion;
    if(header && !header->numtextures) {
        std::string name=path;if(name.size()>=4) name.insert(name.size()-4,"T");companion=read(name.c_str());
    }
    static constexpr const char outfitPrefix[]="models/baloncuaga_outfits/";
    bool originalColors=!_strnicmp(path,outfitPrefix,sizeof(outfitPrefix)-1);
    if(!parse(data,companion,cached,originalColors)) { cached=Model{};error="Cannot preview this MDL.";return false; }
    for(auto &skin:cached.skins) {
        glGenTextures(1,&skin.handle);glBindTexture(GL_TEXTURE_2D,skin.handle);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,skin.width,skin.height,0,GL_RGBA,GL_UNSIGNED_BYTE,skin.pixels.data());
    }
    return true;
}
static bool draw(int x,int y,int w,int h,int sw,int sh,const char *path,float time,float opacity=1.f) {
    opacity=std::isfinite(opacity)?std::max(0.f,std::min(opacity,1.f)):0.f;if(opacity<=.001f)return true;
    if(!wglGetCurrentContext()) {error="Preview requires OpenGL.";return false;}
    GLint viewport[4],mode;glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_MATRIX_MODE,&mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    using ActiveTexture=void(APIENTRY *)(GLenum);
    auto active=reinterpret_cast<ActiveTexture>(wglGetProcAddress("glActiveTextureARB"));
    GLint activeUnit=0x84C0;
    if(reinterpret_cast<uintptr_t>(active)>4 && reinterpret_cast<uintptr_t>(active)!=static_cast<uintptr_t>(-1)) {
        glGetIntegerv(0x84E0,&activeUnit);
        GLint units=1;glGetIntegerv(0x84E2,&units);
        for(int unit=1;unit<std::min(units,16);++unit){active(0x84C0+unit);glDisable(GL_TEXTURE_2D);}
        active(0x84C0);
    } else active=nullptr;
    bool loaded=load(path);
    if(loaded) {
        int px=viewport[0]+x*viewport[2]/sw,py=viewport[1]+(sh-y-h)*viewport[3]/sh;
        int pw=std::max(1,w*viewport[2]/sw),ph=std::max(1,h*viewport[3]/sh);
        glViewport(px,py,pw,ph);glEnable(GL_SCISSOR_TEST);glScissor(px,py,pw,ph);
        glDepthMask(GL_TRUE);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
        if(opacity<.999f){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}else glDisable(GL_BLEND);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
        bool weaponOnly=!showHands&&cached.hasWeapon;
        double r=(weaponOnly?cached.weaponRadius:cached.radius)*1.16,aspect=static_cast<double>(pw)/ph;
        if(aspect<1)r/=aspect;
        glOrtho(-r*aspect,r*aspect,-r,r,-r*4,r*4);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glRotatef(-70+pitch,1,0,0);glRotatef(yaw+(spin?std::fmod(time*24.f,360.f):0),0,0,1);
        Point center=weaponOnly?cached.weaponCenter:cached.center;
        glTranslatef(-center.x,-center.y,-center.z);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        for(const auto &mesh:cached.meshes) {
            if(weaponOnly && mesh.hands)continue;
            glColor4f(1,1,1,opacity);
            if(mesh.skin>=0) {
                const auto &skin=cached.skins[mesh.skin];glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,skin.handle);
                if(skin.flags&STUDIO_NF_MASKED){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f*opacity);}else glDisable(GL_ALPHA_TEST);
            } else {glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glColor4f(0.7f,0.8f,0.9f,opacity);}
            glBegin(GL_TRIANGLES);
            for(const auto &v:mesh.triangles){glTexCoord2f(v.u,v.v);glVertex3f(v.p.x,v.p.y,v.p.z);}glEnd();
        }
        glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
    }
    glPopClientAttrib();glPopAttrib();if(active)active(activeUnit);glMatrixMode(mode);
    return loaded;
}
}
