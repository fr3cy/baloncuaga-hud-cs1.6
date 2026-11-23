#pragma once
// Included inside Weave. Combat metrics are design units; buildDenseFonts
// rasterizes these faces at the current HUD framebuffer scale.
// System font files are read at runtime and are not redistributed in the ASI.
namespace CombatFonts {
static ImFont *faces[6]={};
static const char *family="Embedded bold";
static bool nativeFace(const ImFont *face){if(!face)return false;for(auto *entry:faces)if(face==entry)return true;return false;}
static ImFont *largeFor(float width){return faces[width<1000?3:width<1600?4:5];}
struct FontSource {std::string path;std::vector<unsigned char> bytes;};
static std::vector<FontSource> sources;
static FontSource *source(const char *path){
    for(auto &entry:sources)if(entry.path==path)return &entry;
    FILE *file=fopen(path,"rb");if(!file)return nullptr;
    bool measured=fseek(file,0,SEEK_END)==0;long length=measured?ftell(file):-1;
    if(length<128||length>32*1024*1024||fseek(file,0,SEEK_SET)!=0){fclose(file);return nullptr;}
    FontSource entry;entry.path=path;entry.bytes.resize(static_cast<size_t>(length));
    bool read=fread(entry.bytes.data(),1,entry.bytes.size(),file)==entry.bytes.size();fclose(file);if(!read)return nullptr;
    FT_Library library=nullptr;if(FT_Init_FreeType(&library))return nullptr;
    FT_Face face=nullptr;bool valid=FT_New_Memory_Face(library,entry.bytes.data(),static_cast<FT_Long>(entry.bytes.size()),0,&face)==0;
    if(valid){valid=FT_Get_Char_Index(face,'0')&&FT_Get_Char_Index(face,0x0410)&&FT_Get_Char_Index(face,0x0451);FT_Done_Face(face);}
    FT_Done_FreeType(library);if(!valid)return nullptr;sources.push_back(std::move(entry));return &sources.back();
}
static bool validFont(const char *path){return source(path)!=nullptr;}
static ImFont *systemFace(ImFontAtlas *fonts,const char *path,float size,const ImFontConfig &base,const ImWchar *ranges){
    auto *data=source(path);if(!data)return nullptr;ImFontConfig config=base;
    if(!config.Name[0]){const char *name=strrchr(path,'/');snprintf(config.Name,sizeof(config.Name),"%s, %.0fpx",name?name+1:path,size);}
    // This ImGui version copies memory when OwnedByAtlas=false at AddFont.
    // Register the shared bytes without a copy, then leave ownership with this
    // cache. reset() runs only after DestroyContext has discarded font configs.
    config.FontDataOwnedByAtlas=true;
    ImFont *face=fonts->AddFontFromMemoryTTF(data->bytes.data(),static_cast<int>(data->bytes.size()),size,&config,ranges);
    if(face)fonts->ConfigData.back().FontDataOwnedByAtlas=false;return face;
}
static void add(ImFontAtlas *fonts,const char *windowsFolder){
    char fontPath[MAX_PATH]={};family="Embedded bold";
    const char *files[]={"ariblk.ttf","seguibl.ttf","arialbd.ttf","segoeuib.ttf","seguisb.ttf"};
    const char *names[]={"Arial Black","Segoe UI Black","Arial Bold","Segoe UI Bold","Segoe UI Semibold"};
    bool systemFont=false;
    for(int i=0;i<5;++i){snprintf(fontPath,sizeof(fontPath),"%s/Fonts/%s",windowsFolder,files[i]);
        if(GetFileAttributesA(fontPath)!=INVALID_FILE_ATTRIBUTES&&validFont(fontPath)){family=names[i];systemFont=true;break;}}
    // The value face draws numeric/ASCII HUD values, not player names. Its
    // smaller glyph range leaves room for Cyrillic labels on old 1024px GL.
    static const ImWchar labels[]={0x20,0xFF,0x400,0x45F,0x2013,0x2014,0x2026,0x2026,0};
    static const ImWchar values[]={0x20,0x7F,0x2013,0x2014,0};
    static const ImWchar digits[]={0x20,0x20,0x2D,0x2D,0x30,0x39,0x3F,0x3F,0};
    // Three large native faces contain only health/armour digits. Their actual
    // FreeType's real-dimension request gives Arial Black ink heights near
    //41/50/55px at these sizes, without scaling an existing bitmap.
    static const float sizes[]={14,18,40,78,94,104};
    for(int i=0;i<6;++i){
        ImFontConfig config;config.PixelSnapH=true;config.FontBuilderFlags=0;
        snprintf(config.Name,sizeof(config.Name),"Combat / %s / %.0fpx",family,sizes[i]);
        const ImWchar *range=i>2?digits:i==2?values:labels;
        faces[i]=systemFont?systemFace(fonts,fontPath,sizes[i],config,range):nullptr;
        if(!faces[i]){config.FontDataOwnedByAtlas=false;faces[i]=fonts->AddFontFromMemoryTTF(font_bold_bin,sizeof(font_bold_bin),sizes[i],&config,range);}
    }
    combatSmall=faces[0];combatLabel=faces[1];combatValue=faces[2];
}
static void reset(){for(auto &face:faces)face=nullptr;family="Embedded bold";sources.clear();}
} // namespace CombatFonts
