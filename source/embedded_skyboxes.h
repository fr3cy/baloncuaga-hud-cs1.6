#pragma once
#include <windows.h>
#include <ft2build.h>
#include FT_GZIP_H
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <new>
#include <stdexcept>

// The existing statically linked FreeType includes a checksum-validating zlib
// decoder. Resources stay compressed in the module; no files are extracted.
namespace EmbeddedSkyboxes {
static int resourceId(const char *path){
    if(!path)return 0;
    static const char *skies[]={"sunny","night","sunset","overcast","aurora","space"};
    static const char *faces[]={"rt","bk","lf","ft","up","dn","preview"};
    char expected[128];
    for(int sky=0;sky<6;++sky)for(int face=0;face<7;++face){
        snprintf(expected,sizeof(expected),"baloncuaga_skyboxes/%s/%s.tga",skies[sky],faces[face]);
        if(!strcmp(path,expected))return 4100+sky*7+face;
    }
    return 0;
}
static void *allocate(FT_Memory,long size){return size>0?std::malloc(size_t(size)):nullptr;}
static void release(FT_Memory,void *block){std::free(block);}
static void *resize(FT_Memory,long,long size,void *block){return size>0?std::realloc(block,size_t(size)):nullptr;}
static bool unpack(const unsigned char *bytes,size_t size,std::vector<unsigned char> &out){
    if(!bytes||size<10||size>32u*1024*1024||memcmp(bytes,"BSK1",4))return false;
    const unsigned long length=unsigned(bytes[4])|(unsigned(bytes[5])<<8)|(unsigned(bytes[6])<<16)|(unsigned(bytes[7])<<24);
    if(length<18||length>32u*1024*1024)return false;
    try {
        std::vector<unsigned char> decoded(length);
        FT_MemoryRec_ memory={nullptr,allocate,release,resize};FT_ULong actual=length;
        if(FT_Gzip_Uncompress(&memory,decoded.data(),&actual,bytes+8,FT_ULong(size-8))||actual!=length)return false;
        out.swap(decoded);return true;
    }catch(const std::bad_alloc &){return false;}catch(const std::length_error &){return false;}
}
static bool read(HMODULE module,const char *path,std::vector<unsigned char> &out){
    const int id=resourceId(path);if(!module||!id)return false;
    HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));if(!resource)return false;
    const DWORD size=SizeofResource(module,resource);HGLOBAL loaded=LoadResource(module,resource);
    return loaded&&unpack(static_cast<const unsigned char *>(LockResource(loaded)),size,out);
}
}
