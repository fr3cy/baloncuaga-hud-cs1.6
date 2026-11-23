#pragma once
#include <string>
#include <fstream>
// Disk validator: no engine call is allowed until every referenced block passes.
namespace Mdl {
using Bytes=std::vector<byte>;
static std::string error;
template<class T> static const T *at(const Bytes &b,int64_t off,int64_t n=1){
    if(off<0||n<0||off%alignof(T)||uint64_t(off)>b.size()||uint64_t(n)>(b.size()-size_t(off))/sizeof(T))return nullptr;
    return reinterpret_cast<const T *>(b.data()+off);
}
static bool fail(const char *s){error=s;return false;}
static Bytes read(const std::string &p){Bytes b;FILE *f=fopen(p.c_str(),"rb");if(!f)return b;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n>0&&n<=64*1024*1024){b.resize(n);if(fread(b.data(),1,n,f)!=size_t(n))b.clear();}fclose(f);return b;}
static bool header(const Bytes &b){auto h=at<studiohdr_t>(b,0);return h&&h->id==0x54534449&&h->version==10&&h->length>=int(sizeof(*h))&&size_t(h->length)==b.size();}
static bool textures(const Bytes &b){
    if(!header(b))return fail(u8"Неверный заголовок файла текстур.");auto h=at<studiohdr_t>(b,0);
    if(h->numtextures<1||h->numtextures>100||h->numskinref<1||h->numskinref>100||h->numskinfamilies<1||h->numskinfamilies>256)return fail(u8"Некорректная таблица текстур.");
    auto t=at<mstudiotexture_t>(b,h->textureindex,h->numtextures);auto skins=at<short>(b,h->skinindex,int64_t(h->numskinref)*h->numskinfamilies);
    if(!t||!skins)return fail(u8"Обрезана таблица текстур.");
    for(int i=0;i<h->numtextures;++i)if(t[i].width<1||t[i].height<1||t[i].width>2048||t[i].height>2048||int64_t(t[i].width)*t[i].height>640*480||!at<byte>(b,t[i].index,int64_t(t[i].width)*t[i].height+768))return fail(int64_t(t[i].width)*t[i].height>640*480?u8"Текстура слишком большая для Steam Legacy. Уменьши её до 512x512.":u8"Повреждены пиксели или палитра текстуры.");
    for(int i=0;i<h->numskinref*h->numskinfamilies;++i)if(skins[i]<0||skins[i]>=h->numtextures)return fail(u8"Неверный индекс скина.");return true;
}
struct Asset {Bytes main,texture;std::vector<Bytes> groups;std::vector<int> sequenceMap;uint64_t hash=1469598103934665603ULL;};
static bool validate(const std::string &path,Asset &out,const Bytes *stock=nullptr,bool playerModel=false){
    error.clear();out=Asset{};out.main=read(path);
    if(!header(out.main))return fail(u8"Нужен целый GoldSrc MDL версии 10 (IDST).");auto h=at<studiohdr_t>(out.main,0);
    // GoldSrc uploads pixels first, then caches only texturedataindex bytes.
    // Bound every player CPU reference against that retained prefix.
    Bytes cpu;
    if(playerModel){
        if(h->numtextures<1||h->texturedataindex<int(sizeof(*h))||size_t(h->texturedataindex)>out.main.size())return fail(u8"Неверная граница данных модели игрока.");
        cpu.assign(out.main.begin(),out.main.begin()+h->texturedataindex);
    }
    const auto &b=playerModel?cpu:out.main;
    if(h->numbones<1||h->numbones>128||h->numseq<1||h->numseq>2048||h->numseqgroups<1||h->numseqgroups>32||h->numbodyparts<1||h->numbodyparts>32||h->numbonecontrollers<0||h->numbonecontrollers>8||h->numhitboxes<0||h->numhitboxes>128||h->numattachments<0||h->numattachments>4||h->numtransitions<0||h->numtransitions>256)return fail(u8"Недопустимое число элементов MDL.");
    auto bones=at<mstudiobone_t>(b,h->boneindex,h->numbones);auto seq=at<mstudioseqdesc_t>(b,h->seqindex,h->numseq);auto groups=at<mstudioseqgroup_t>(b,h->seqgroupindex,h->numseqgroups);auto parts=at<mstudiobodyparts_t>(b,h->bodypartindex,h->numbodyparts);
    if(!bones||!seq||!groups||!parts||!at<mstudiobonecontroller_t>(b,h->bonecontrollerindex,h->numbonecontrollers)||!at<mstudiobbox_t>(b,h->hitboxindex,h->numhitboxes)||!at<mstudioattachment_t>(b,h->attachmentindex,h->numattachments)||!at<byte>(b,h->transitionindex,int64_t(h->numtransitions)*h->numtransitions))return fail(u8"Повреждены таблицы MDL.");
    for(int i=0;i<h->numbones;++i){if(bones[i].parent<-1||bones[i].parent>=i)return fail(u8"Повреждена иерархия костей.");for(int c=0;c<6;++c)if(!std::isfinite(bones[i].value[c])||!std::isfinite(bones[i].scale[c])||bones[i].bonecontroller[c]<-1||bones[i].bonecontroller[c]>=h->numbonecontrollers)return fail(u8"Повреждены параметры костей.");}
    auto controllers=at<mstudiobonecontroller_t>(b,h->bonecontrollerindex,h->numbonecontrollers);for(int i=0;i<h->numbonecontrollers;++i)if(controllers[i].bone<0||controllers[i].bone>=h->numbones||controllers[i].index<0||controllers[i].index>4||!std::isfinite(controllers[i].start)||!std::isfinite(controllers[i].end))return fail(u8"Поврежден контроллер кости.");
    auto boxes=at<mstudiobbox_t>(b,h->hitboxindex,h->numhitboxes);for(int i=0;i<h->numhitboxes;++i)if(boxes[i].bone<0||boxes[i].bone>=h->numbones)return fail(u8"Поврежден хитбокс MDL.");
    auto attachments=at<mstudioattachment_t>(b,h->attachmentindex,h->numattachments);for(int i=0;i<h->numattachments;++i)if(attachments[i].bone<0||attachments[i].bone>=h->numbones)return fail(u8"Повреждена точка прикрепления MDL.");
    out.groups.resize(h->numseqgroups);
    for(int i=1;i<h->numseqgroups;++i){char suffix[16];snprintf(suffix,sizeof(suffix),"%02d.mdl",i);auto companion=path.substr(0,path.size()-4)+suffix;out.groups[i]=read(companion);auto gh=at<studioseqhdr_t>(out.groups[i],0);if(!gh||gh->id!=0x51534449||gh->version!=10||gh->length!=int(out.groups[i].size()))return fail(u8"Нет целого файла анимаций: имя01.mdl, имя02.mdl…");}
    // CS player aim sequences use native 3x3 blends. Weapon validation stays at four.
    const int blendLimit=playerModel&&(h->numbones==53||h->numbones==55)&&h->numseq==111&&h->numseqgroups==1?9:4;
    for(int s=0;s<h->numseq;++s){const auto &q=seq[s];
        if(!memchr(q.label,0,32)||!std::isfinite(q.fps)||q.fps<=0||q.numframes<1||q.numframes>100000||q.seqgroup<0||q.seqgroup>=h->numseqgroups||q.numblends<1||q.numblends>blendLimit||q.numevents<0||q.numevents>1024||q.numpivots<0||q.numpivots>256||q.nextseq<0||q.nextseq>=h->numseq||q.motionbone<0||q.motionbone>=h->numbones)return fail(u8"Некорректная анимация MDL.");
        auto events=at<mstudioevent_t>(b,q.eventindex,q.numevents);if(!events||!at<mstudiopivot_t>(b,q.pivotindex,q.numpivots))return fail(u8"Повреждены события анимации.");
        for(int e=0;e<q.numevents;++e)if(!memchr(events[e].options,0,64)||events[e].frame<0||events[e].frame>=q.numframes)return fail(u8"Некорректное событие MDL.");
        const auto &a=q.seqgroup?out.groups[q.seqgroup]:b;int64_t base=q.animindex+(q.seqgroup?0:groups[0].unused2);auto anim=at<mstudioanim_t>(a,base,int64_t(h->numbones)*q.numblends);if(!anim)return fail(u8"Обрезаны анимации MDL.");
        for(int k=0;k<h->numbones*q.numblends;++k)for(int c=0;c<6;++c)if(anim[k].offset[c]){int64_t p=base+k*sizeof(mstudioanim_t)+anim[k].offset[c];int frames=0;while(frames<q.numframes){auto v=at<mstudioanimvalue_t>(a,p);if(!v||v->num.total==0||v->num.valid==0||v->num.valid>v->num.total||!at<mstudioanimvalue_t>(a,p,1+v->num.valid))return fail(u8"Поврежден поток кадров MDL.");frames+=v->num.total;p+=(1+v->num.valid)*sizeof(*v);}}
    }
    if(!h->numtextures){out.texture=read(path.substr(0,path.size()-4)+"T.mdl");if(!textures(out.texture))return fail(u8"Нет совместимого файла текстур имяT.mdl.");}else if(!textures(out.main))return false;
    auto th=at<studiohdr_t>(out.texture.empty()?out.main:out.texture,0);
    if(playerModel){
        auto t=at<mstudiotexture_t>(b,h->textureindex,h->numtextures);
        if(!t||!at<short>(b,h->skinindex,int64_t(h->numskinref)*h->numskinfamilies))return fail(u8"Таблицы модели за границей кэша GoldSrc.");
        for(int i=0;i<h->numtextures;++i)if(t[i].index<h->texturedataindex)return fail(u8"Текстуры модели пересекают данные геометрии.");
    }
    for(int p=0;p<h->numbodyparts;++p){if(parts[p].nummodels<1||parts[p].nummodels>256||parts[p].base<1)return fail(u8"Некорректная группа мешей.");auto models=at<mstudiomodel_t>(b,parts[p].modelindex,parts[p].nummodels);if(!models)return fail(u8"Обрезана группа мешей.");
        for(int m=0;m<parts[p].nummodels;++m){const auto &v=models[m];if(v.numverts<0||v.numverts>2048||v.numnorms<0||v.numnorms>2048||v.nummesh<0||v.nummesh>256)return fail(u8"Меш превышает ограничения GoldSrc.");auto verts=at<float>(b,v.vertindex,int64_t(v.numverts)*3);auto norm=at<float>(b,v.normindex,int64_t(v.numnorms)*3);auto vb=at<byte>(b,v.vertinfoindex,v.numverts);auto nb=at<byte>(b,v.norminfoindex,v.numnorms);auto mesh=at<mstudiomesh_t>(b,v.meshindex,v.nummesh);if(!verts||!norm||!vb||!nb||!mesh)return fail(u8"Обрезаны вершины или нормали.");
            for(int i=0;i<v.numverts;++i)if(vb[i]>=h->numbones||!std::isfinite(verts[3*i])||!std::isfinite(verts[3*i+1])||!std::isfinite(verts[3*i+2]))return fail(u8"Неверная вершина MDL.");
            for(int i=0;i<v.numnorms;++i)if(nb[i]>=h->numbones||!std::isfinite(norm[3*i])||!std::isfinite(norm[3*i+1])||!std::isfinite(norm[3*i+2]))return fail(u8"Неверная нормаль MDL.");
            int normalCursor=0;
            for(int i=0;i<v.nummesh;++i){const int normalStart=normalCursor;if(playerModel){if(mesh[i].numnorms<0||mesh[i].numnorms>v.numnorms-normalCursor)return fail(u8"Неверные данные освещения MDL.");normalCursor+=mesh[i].numnorms;}if(mesh[i].skinref<0||mesh[i].skinref>=th->numskinref)return fail(u8"Меш ссылается на отсутствующую текстуру.");int64_t pos=mesh[i].triindex;int budget=0;for(;;){auto cmd=at<short>(b,pos);if(!cmd||++budget>100000)return fail(u8"Повреждены треугольники MDL.");pos+=2;if(!*cmd)break;int n=std::abs(int(*cmd));auto tr=at<short>(b,pos,int64_t(n)*4);if(n<3||!tr)return fail(u8"Обрезаны треугольники MDL.");for(int j=0;j<n;++j)if(tr[j*4]<0||tr[j*4]>=v.numverts||tr[j*4+1]<0||tr[j*4+1]>=v.numnorms||(playerModel&&(tr[j*4+1]<normalStart||tr[j*4+1]>=normalCursor)))return fail(u8"Индекс треугольника вне меша.");pos+=n*8;}}
            if(playerModel&&normalCursor!=v.numnorms)return fail(u8"Неполные данные освещения MDL.");
        }
    }
    if(stock){auto sh=at<studiohdr_t>(*stock,0);auto ss=sh?at<mstudioseqdesc_t>(*stock,sh->seqindex,sh->numseq):nullptr;if(!ss)return fail(u8"Не удалось прочесть стандартные анимации.");for(int i=0;i<sh->numseq;++i){int match=-1;if(i<h->numseq&&!_stricmp(ss[i].label,seq[i].label))match=i;else for(int j=0;j<h->numseq;++j)if(!_stricmp(ss[i].label,seq[j].label)){match=j;break;}if(match<0)return fail(u8"Нет обязательной анимации оружия; модель несовместима.");out.sequenceMap.push_back(match);}}
    for(const auto *data:{&out.main,&out.texture})for(byte x:*data){out.hash^=x;out.hash*=1099511628211ULL;}for(auto &g:out.groups)for(byte x:g){out.hash^=x;out.hash*=1099511628211ULL;}return true;
}
static bool write(const std::string &p,const Bytes &b){FILE *f=fopen(p.c_str(),"wb");if(!f)return false;bool ok=fwrite(b.data(),1,b.size(),f)==b.size();ok=fclose(f)==0&&ok;return ok;}
static bool stage(Asset &a,std::string &relative){
    char name[32];snprintf(name,sizeof(name),"%016llx",static_cast<unsigned long long>(a.hash));relative=std::string("models/customizer/cache/")+name+".mdl";std::string dir=std::string(rootPath)+"/cstrike/models/customizer/cache";CreateDirectoryA(dir.c_str(),nullptr);std::string full=std::string(rootPath)+"/cstrike/"+relative;auto h=at<studiohdr_t>(a.main,0);
    for(size_t i=1;i<a.groups.size();++i){char suffix[16];snprintf(suffix,sizeof(suffix),"%02u.mdl",unsigned(i));std::string r=relative.substr(0,relative.size()-4)+suffix;if(r.size()>=64)return fail(u8"Слишком длинный путь анимации.");auto group=reinterpret_cast<mstudioseqgroup_t *>(a.main.data()+h->seqgroupindex)+i;snprintf(group->name,sizeof(group->name),"%s",r.c_str());if(!write(std::string(rootPath)+"/cstrike/"+r,a.groups[i]))return fail(u8"Не удалось записать кэш анимаций.");}
    if(!a.texture.empty()&&!write(full.substr(0,full.size()-4)+"T.mdl",a.texture))return fail(u8"Не удалось записать кэш текстур.");if(!write(full,a.main))return fail(u8"Не удалось записать кэш модели.");return true;
}
}
