// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/cityhash.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/memory.h"
#include "oot3d/hd_font.h"
#include "oot3d/master_quest.h"
#include <map>

namespace OOT3D::NativePromptDiagnostics {
// Read CPU-visible pixels without flushing/invalidation: a diagnostic must not
// accidentally repair the stale GPU resource it is trying to identify.
inline std::span<const u8> Pixels(Memory::MemorySystem& memory, HDFont::Descriptor d) {
    const auto size=HDFont::ByteSize(d);
    if(!size)return {};
    const auto* ptr=memory.GetPointer(d.address);
    const auto base=reinterpret_cast<std::uintptr_t>(memory.GetFCRAMPointer(0));
    const auto at=reinterpret_cast<std::uintptr_t>(ptr);
    if(!ptr || at<base || at-base+size>Memory::FCRAM_N3DS_SIZE)return {};
    // Reject a virtual range crossing an unrelated page mapping.
    if(memory.GetPointer(d.address+size-1)!=ptr+size-1)return {};
    return {ptr,size};
}
inline u64 Hash(std::span<const u8> pixels) {
    return Common::CityHash64(reinterpret_cast<const char*>(pixels.data()),pixels.size());
}
inline std::vector<u8> Decode(HDFont::Descriptor d,std::span<const u8> pixels) {
    if(pixels.size()!=HDFont::ByteSize(d) || pixels.empty())return {};
    std::vector<u8> out(d.width*d.height);
    for(unsigned y=0;y<d.height;++y)for(unsigned x=0;x<d.width;++x) {
        unsigned morton=0;
        for(unsigned b=0;b<3;++b)morton|=((x>>b)&1)<<(2*b)|((y>>b)&1)<<(2*b+1);
        const unsigned p=((y/8)*(d.width/8)+x/8)*64+morton;
        out[(d.height-1-y)*d.width+x]=d.format==8?pixels[p]:d.format==9?(pixels[p]&15)*17:
            ((pixels[p/2]>>((p%2)*4))&15)*17;
    }
    return out;
}
struct Trace {
    struct Upload { HDFont::Descriptor descriptor; u64 hash; u32 engine,resource; };
    std::map<u32,Upload> uploads;
    std::array<std::array<u64,10>,42> signatures{};
    std::array<u64,42> sampled_frame{};
    unsigned reports=0;
    void Uploaded(Memory::MemorySystem& memory,HDFont::Descriptor d,u32 engine,u32 resource) {
        const auto pixels=Pixels(memory,d);if(pixels.empty())return;
        if(uploads.size()>=256 && !uploads.contains(d.address))uploads.erase(uploads.begin());
        uploads.insert_or_assign(d.address,Upload{d,Hash(pixels),engine,resource});
    }
    template<class M> void Draw(M& m,Memory::MemorySystem& memory,u32 model,u64 frame) {
        if(reports>=64)return;
        // Only native message controllers, never file names or typed characters.
        for(unsigned slot=6;slot<27;++slot) {
            const u32 controller=m.Read(0x55b548+slot*4).value_or(0);
            if(!TitleScreen::Pointer(controller,0x4c))continue;
            for(unsigned layer=0;layer<2;++layer) {
                if(m.Read(controller+0x10+4*layer)!=model)continue;
                const unsigned key=(slot-6)*2+layer;
                if(sampled_frame[key] && frame-sampled_frame[key]<30)return;
                sampled_frame[key]=frame;
                const auto read=[&](u32 at){return m.Read(at).value_or(0);};
                const u32 mesh=read(model+4),desc=read(mesh),dims=read(mesh+0x15c);
                HDFont::Descriptor d{read(mesh+0x158),dims&0xffff,dims>>16,read(mesh+0x164)};
                const auto pixels=Pixels(memory,d);
                const u64 hash=pixels.empty()?0:Hash(pixels);
                const u32 uv=read(desc+4),vertices=read(desc+12);
                std::array<u32,8> coords{};
                if(TitleScreen::Pointer(uv,sizeof(coords)))
                    for(unsigned i=0;i<coords.size();++i)coords[i]=read(uv+4*i);
                const u64 uv_hash=Common::CityHash64(reinterpret_cast<const char*>(coords.data()),sizeof(coords));
                const u32 state=read(0x504fa0+0x10);
                const std::array<u64,10> sig{controller,model,d.address,dims,d.format,hash,uv_hash,state,
                    MasterQuest::PromptMessage(),vertices};
                if(sig==signatures[key])return;
                signatures[key]=sig;
                const auto uploaded=uploads.find(d.address);
                const bool known=uploaded!=uploads.end() && uploaded->second.descriptor==d;
                const u64 original=known?uploaded->second.hash:0;
                const unsigned report=++reports;
                LOG_INFO(Core,"OoT native prompt: capture={} state={} slot={} layer={} quest={} controller={:08x} model={:08x} mesh={:08x} texture={:08x} dims={}x{} format={} hash={:016x} upload_known={} upload_hash={:016x} upload_resource={:08x} controller_resource={:08x} resource_texture={:08x} vertices={} uv={:08x} first_uvs={:08x}/{:08x}/{:08x}/{:08x}/{:08x}/{:08x}/{:08x}/{:08x} sampler={:08x}/{:08x}/{:08x}/{:08x}",
                    report,state,slot,layer,MasterQuest::PromptMessage(),controller,model,mesh,d.address,d.width,d.height,d.format,hash,
                    known,original,known?uploaded->second.resource:0,read(controller+4*layer),read(read(controller+4*layer)+0x4c),
                    vertices,uv,coords[0],coords[1],coords[2],coords[3],coords[4],coords[5],coords[6],coords[7],
                    read(mesh+0x13c),read(mesh+0x140),read(mesh+0x144),read(mesh+0x148));
                const auto decoded=Decode(d,pixels);
                if(!decoded.empty()) {
                    const auto path=FileUtil::GetUserPath(FileUtil::UserPath::LogDir)+
                        "oot-native-prompt-"+std::to_string(report)+".pgm";
                    FileUtil::IOFile file(path,"wb");
                    if(file.IsOpen()) {
                        file.WriteString("P5\n"+std::to_string(d.width)+" "+std::to_string(d.height)+"\n255\n");
                        file.WriteBytes(decoded.data(),decoded.size());
                    }
                }
                return;
            }
        }
    }
};
}
