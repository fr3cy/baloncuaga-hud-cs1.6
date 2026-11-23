#pragma once
struct OutfitAsset {const char *native,*file;int side,bones;uint64_t hash;};
static const OutfitAsset assets[]={
{"urban","models/baloncuaga_outfits/ct.mdl",2,53,UINT64_C(0x40e72911d055fb59)},
{"gign","models/baloncuaga_outfits/gign.mdl",2,53,UINT64_C(0x9d149dd52390f565)},
{"gsg9","models/baloncuaga_outfits/gsg9.mdl",2,53,UINT64_C(0x56100800d84d30fd)},
{"sas","models/baloncuaga_outfits/sas.mdl",2,55,UINT64_C(0xa3aa30148b7f58d5)},
{"vip","models/baloncuaga_outfits/vip.mdl",2,53,UINT64_C(0xd8be58c7edfd659b)},
{"terror","models/baloncuaga_outfits/tt.mdl",1,53,UINT64_C(0x27250bd3be2f7101)},
{"leet","models/baloncuaga_outfits/leet.mdl",1,55,UINT64_C(0xf5fec4eecbef4e7d)},
{"arctic","models/baloncuaga_outfits/arctic.mdl",1,55,UINT64_C(0x52a4b29965422153)},
{"guerilla","models/baloncuaga_outfits/guerilla.mdl",1,53,UINT64_C(0xca6534f8d4557f67)}
};
static constexpr int assetCount=sizeof(assets)/sizeof(assets[0]);
