#pragma once
// A2S_INFO runs on the existing I/O worker. No Steam callback objects or extra
// threads survive F11. Names remain in RAM; the game's server files are not edited.
namespace ServerNames {
static std::atomic<unsigned> generation{0};
static std::atomic<bool> pending{false};
static std::string clean(std::string name){
    for(char &c:name)if(static_cast<unsigned char>(c)<32)c=' ';
    name=Hud::utf8(Hud::wide(name));while(!name.empty()&&name.back()==' ')name.pop_back();
    auto at=name.find_first_not_of(' ');return at==std::string::npos?std::string{}:name.substr(at);
}
static bool cstring(const byte *data,size_t bytes,size_t &at,std::string &out){
    if(at>=bytes)return false;auto end=static_cast<const byte *>(memchr(data+at,0,bytes-at));if(!end||size_t(end-data)-at>255)return false;
    out.assign(reinterpret_cast<const char *>(data+at),end-(data+at));at=size_t(end-data)+1;return true;
}
static std::string decode(const byte *data,size_t bytes){
    if(bytes<7||memcmp(data,"\xff\xff\xff\xff",4))return {};size_t at=5;std::string name,ignored;
    if(data[4]==0x49){++at;if(!cstring(data,bytes,at,name))return {};}
    else if(data[4]==0x6d){if(!cstring(data,bytes,at,ignored)||!cstring(data,bytes,at,name))return {};}
    else return {};return clean(name);
}
static bool cancelled(unsigned epoch){return unloading||generation.load()!=epoch;}
static std::string query(uint32_t ip,uint16_t port,unsigned epoch){
    if(!ip||!port||cancelled(epoch))return {};
    SOCKET socketHandle=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socketHandle==INVALID_SOCKET)return {};
    struct Close {SOCKET value;~Close(){closesocket(value);}} close{socketHandle};
    u_long nonblocking=1;if(ioctlsocket(socketHandle,FIONBIO,&nonblocking))return {};
    sockaddr_in target={};target.sin_family=AF_INET;target.sin_addr.s_addr=htonl(ip);target.sin_port=htons(port);
    const char request[]="\xff\xff\xff\xffTSource Engine Query";
    std::vector<byte> packet(request,request+sizeof(request));
    for(int attempt=0;attempt<2&&!cancelled(epoch);++attempt){
        if(sendto(socketHandle,reinterpret_cast<const char *>(packet.data()),int(packet.size()),0,reinterpret_cast<sockaddr *>(&target),sizeof(target))!=int(packet.size()))return {};
        ULONGLONG deadline=GetTickCount64()+240;bool challenge=false;
        while(GetTickCount64()<deadline&&!cancelled(epoch)){
            fd_set readers;FD_ZERO(&readers);FD_SET(socketHandle,&readers);timeval wait={0,20000};
            if(select(0,&readers,nullptr,nullptr,&wait)<=0)continue;
            byte response[4096];sockaddr_in peer={};int peerBytes=sizeof(peer);
            int bytes=recvfrom(socketHandle,reinterpret_cast<char *>(response),sizeof(response),0,reinterpret_cast<sockaddr *>(&peer),&peerBytes);
            if(bytes<5||peer.sin_addr.s_addr!=target.sin_addr.s_addr||peer.sin_port!=target.sin_port||memcmp(response,"\xff\xff\xff\xff",4))continue;
            if(response[4]==0x41&&bytes>=9&&attempt==0){packet.resize(sizeof(request));packet.insert(packet.end(),response+5,response+9);challenge=true;break;}
            auto name=decode(response,size_t(bytes));if(!name.empty())return name;
        }if(!challenge)break;
    }return {};
}
struct Winsock {bool ok=false;Winsock(){WSADATA data={};ok=WSAStartup(MAKEWORD(2,2),&data)==0;}~Winsock(){if(ok)WSACleanup();}};
}
