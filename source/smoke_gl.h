#pragma once
#include <type_traits>
// The stock sprite renderer may ignore entity.rendercolor in TransAlpha mode.
// Tint only primitives using a verified gas-puff texture, then restore native GL color.
namespace SmokeGL {
using Bind=void (APIENTRY *)(GLenum,GLuint);using Begin=void (APIENTRY *)(GLenum);using End=void (APIENTRY *)();
using Color=void (APIENTRY *)(GLfloat,GLfloat,GLfloat,GLfloat);using ByteColor=void (APIENTRY *)(GLubyte,GLubyte,GLubyte,GLubyte);
static Bind originalBind;static Begin originalBegin;static End originalEnd;static Color originalColor;static ByteColor originalByte;
using ColorVector=void (APIENTRY *)(const GLfloat *);using ByteVector=void (APIENTRY *)(const GLubyte *);using Color3=void (APIENTRY *)(GLfloat,GLfloat,GLfloat);
using ByteColor3=void (APIENTRY *)(GLubyte,GLubyte,GLubyte);
static ColorVector originalVector,originalVector3;static ByteVector originalByteVector,originalByteVector3;static Color3 originalColor3;static ByteColor3 originalByte3;
static std::vector<GLuint> textures;static HGLRC context;static ULONGLONG checked;
static std::string level;
struct ObservedSprite {model_t *model;std::string name;};
static std::vector<ObservedSprite> observed;
static void observe(model_t *model){if(!SmokeTint::model(model)||observed.size()>=128)return;for(const auto &entry:observed)if(entry.model==model)return;observed.push_back({model,model->name});checked=0;}

static thread_local bool textureCloud;

static bool (*scopeActive)();
static thread_local GLuint boundTexture;static thread_local bool active,probing,forwardingColor;static thread_local volatile float nativeColor[4];static thread_local GLint nativeEnv;
static bool matched(GLuint id){return id&&std::find(textures.begin(),textures.end(),id)!=textures.end();}
static void invalidate(){textures.clear();context=nullptr;checked=0;active=false;boundTexture=0;}
static void probe(){
    if(!ready||unloading||!Hud::smokeTintOn||!SmokeTint::originalSprite||!eng)return;
    auto current=wglGetCurrentContext();if(!current)return;if(current!=context){invalidate();context=current;}
    const char *currentLevel=eng->pfnGetLevelName?eng->pfnGetLevelName():nullptr;if(!currentLevel)currentLevel="";
    if(level!=currentLevel){invalidate();context=current;level=currentLevel;}   // compared in place: no string copy per frame
    ULONGLONG now=GetTickCount64();if(now-checked<2000)return;checked=now;textures.clear();
    // SpriteTexture also updates GoldSrc's GL_Bind cache. Keep that binding in
    // sync at this end-of-frame boundary instead of restoring it behind the cache.
    probing=true;glPushAttrib(GL_CURRENT_BIT|GL_ENABLE_BIT);
    // Use the world model actually submitted by HUD_TempEntUpdate. SPR_Load
    // belongs to the HUD sprite cache and must not substitute a different model.
    for(const auto &entry:observed){auto sprite=entry.model;if(!Hud::readable(sprite,sizeof(*sprite))||sprite->type!=mod_sprite||!SmokeTint::model(sprite)||entry.name!=sprite->name)continue;
        int frames=std::clamp(sprite->numframes,1,256);for(int frame=0;frame<frames;++frame)if(SmokeTint::originalSprite(sprite,frame)){GLint texture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);if(texture>0&&!matched(texture))textures.push_back(texture);}
    }
    glPopAttrib();GLint bound=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);boundTexture=bound;probing=false;
    static size_t loggedCount;if(!textures.empty()&&loggedCount!=textures.size()){loggedCount=textures.size();char text[128];snprintf(text,sizeof(text),"SmokeGL: identified %u cloud texture IDs.",unsigned(textures.size()));logLine(text);}
}
static void APIENTRY bind(GLenum target,GLuint value){CallbackScope scope;originalBind(target,value);if(target==GL_TEXTURE_2D)boundTexture=value;}
static void emit(){float r=nativeColor[0],g=nativeColor[1],b=nativeColor[2];SmokeTint::tint(r,g,b);bool old=forwardingColor;forwardingColor=true;originalColor(r,g,b,nativeColor[3]);forwardingColor=old;}
// The game calls glBegin thousands of times a frame. Only a likely smoke draw asks GL anything:
// inside a smoke particle scope, or when the texture last bound through glBindTexture is one of
// the smoke cloud textures (sprites bind their frame right before drawing).
static void APIENTRY begin(GLenum mode){CallbackScope scope;
    const bool scoped=scopeActive&&scopeActive();
    if(!ready||unloading||probing||!Hud::smokeTintOn||(!scoped&&!matched(boundTexture))){active=false;originalBegin(mode);return;}
    GLint actualTexture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&actualTexture);boundTexture=static_cast<GLuint>(actualTexture);active=(scoped||matched(boundTexture))&&glIsEnabled(GL_TEXTURE_2D);
    if(active){textureCloud=matched(boundTexture)&&!(scopeActive&&scopeActive());float capturedColor[4];glGetFloatv(GL_CURRENT_COLOR,capturedColor);for(int i=0;i<4;++i)nativeColor[i]=capturedColor[i];glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&nativeEnv);
        glPushAttrib(GL_TEXTURE_BIT);
        // Preserve texture intensity/alpha for RGB and RGBA sprites.
        // GL_BLEND with equal primary/constant colour erased RGB cloud masks.
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        emit();static bool logged;if(!logged){logged=true;char message[192];GLint blendSrc=0,blendDst=0;glGetIntegerv(GL_BLEND_SRC,&blendSrc);glGetIntegerv(GL_BLEND_DST,&blendDst);snprintf(message,sizeof(message),"SmokeGL v2-fix: actual texture=%u custom=%d RGB=%d,%d,%d blend=%x/%x",boundTexture,int(textureCloud),Hud::smokeRgb[0],Hud::smokeRgb[1],Hud::smokeRgb[2],blendSrc,blendDst);logLine(message);}}
    originalBegin(mode);
}
static void APIENTRY end(){CallbackScope scope;originalEnd();if(active){active=false;originalColor(nativeColor[0],nativeColor[1],nativeColor[2],nativeColor[3]);glPopAttrib();textureCloud=false;}}
static void APIENTRY color(float r,float g,float b,float a){CallbackScope scope;if(!active||forwardingColor){originalColor(r,g,b,a);return;}nativeColor[0]=r;nativeColor[1]=g;nativeColor[2]=b;nativeColor[3]=a;emit();}
static void APIENTRY byteColor(GLubyte r,GLubyte g,GLubyte b,GLubyte a){CallbackScope scope;if(!active||forwardingColor){originalByte(r,g,b,a);return;}color(r/255.f,g/255.f,b/255.f,a/255.f);}
static void APIENTRY vectorColor(const GLfloat *value){CallbackScope scope;if(!active||forwardingColor){originalVector(value);return;}color(value[0],value[1],value[2],value[3]);}
static void APIENTRY byteVector(const GLubyte *value){CallbackScope scope;if(!active||forwardingColor){originalByteVector(value);return;}byteColor(value[0],value[1],value[2],value[3]);}
static void APIENTRY color3(GLfloat r,GLfloat g,GLfloat b){CallbackScope scope;if(!active||forwardingColor){originalColor3(r,g,b);return;}color(r,g,b,1);}
static void APIENTRY vector3(const GLfloat *value){CallbackScope scope;if(!active||forwardingColor){originalVector3(value);return;}color(value[0],value[1],value[2],1);}
// 3-component byte calls read exactly three bytes and set native alpha to 1.
static void APIENTRY byteColor3(GLubyte r,GLubyte g,GLubyte b){CallbackScope scope;if(!active||forwardingColor){originalByte3(r,g,b);return;}color(r/255.f,g/255.f,b/255.f,1.f);}
static void APIENTRY byteVector3(const GLubyte *value){CallbackScope scope;if(!active||forwardingColor){originalByteVector3(value);return;}byteColor3(value[0],value[1],value[2]);}
static_assert(std::is_same<decltype(&byteColor3),ByteColor3>::value,"GL 3-byte RGB ABI");
static_assert(std::is_same<decltype(&byteVector3),ByteVector>::value,"GL 3-byte vector ABI");
static_assert(std::is_same<decltype(&bind),Bind>::value,"GL bind ABI");
static_assert(std::is_same<decltype(&begin),Begin>::value,"GL begin ABI");
static_assert(std::is_same<decltype(&end),End>::value,"GL end ABI");
static_assert(std::is_same<decltype(&color),Color>::value,"GL color ABI");
static_assert(std::is_same<decltype(&byteColor),ByteColor>::value,"GL byte ABI");
static_assert(std::is_same<decltype(&vectorColor),ColorVector>::value,"GL vector ABI");
static_assert(std::is_same<decltype(&byteVector),ByteVector>::value,"GL byte vector ABI");
static_assert(std::is_same<decltype(&color3),Color3>::value,"GL color3 ABI");
static_assert(std::is_same<decltype(&vector3),ColorVector>::value,"GL vector3 ABI");
static bool install(){
    HMODULE module=GetModuleHandleA("opengl32.dll");if(!module)return false;
    struct Hook {const char *name;void *callback;void **original;};
    Hook hooks[]={{"glBindTexture",reinterpret_cast<void *>(bind),reinterpret_cast<void **>(&originalBind)},
        {"glBegin",reinterpret_cast<void *>(begin),reinterpret_cast<void **>(&originalBegin)},
        {"glEnd",reinterpret_cast<void *>(end),reinterpret_cast<void **>(&originalEnd)},
        {"glColor4f",reinterpret_cast<void *>(color),reinterpret_cast<void **>(&originalColor)},
        {"glColor4ub",reinterpret_cast<void *>(byteColor),reinterpret_cast<void **>(&originalByte)},
        {"glColor4fv",reinterpret_cast<void *>(vectorColor),reinterpret_cast<void **>(&originalVector)},
        {"glColor4ubv",reinterpret_cast<void *>(byteVector),reinterpret_cast<void **>(&originalByteVector)},
        {"glColor3f",reinterpret_cast<void *>(color3),reinterpret_cast<void **>(&originalColor3)},
        {"glColor3fv",reinterpret_cast<void *>(vector3),reinterpret_cast<void **>(&originalVector3)},
        {"glColor3ub",reinterpret_cast<void *>(byteColor3),reinterpret_cast<void **>(&originalByte3)},
        {"glColor3ubv",reinterpret_cast<void *>(byteVector3),reinterpret_cast<void **>(&originalByteVector3)}};
    std::vector<void *> installed;for(auto &hook:hooks){void *target=reinterpret_cast<void *>(GetProcAddress(module,hook.name));if(!target||MH_CreateHook(target,hook.callback,hook.original)!=MH_OK){for(void *old:installed)MH_RemoveHook(old);return false;}installed.push_back(target);}
    logLine("SmokeGL v2-fix: scoped float and 8-bit RGB/RGBA hooks installed; CPU fallback retained.");return true;
}
}


