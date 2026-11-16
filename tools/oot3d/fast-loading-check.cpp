// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/file_sys/ivfc_archive.h"
#include "core/file_sys/layered_fs.h"
#include "oot3d/fast_loading.h"

// Entirely synthetic read-only RomFS; never opens a game, config or save.
class MemoryRomFS : public FileSys::RomFSReader {
public:
    std::vector<u8> bytes;
    MemoryRomFS() {
        bytes.resize(0x110);
        const auto word=[&](unsigned at,u32 value){std::memcpy(bytes.data()+at,&value,4);};
        word(0,0x28);word(0xc,0x30);word(0x10,0x18);
        word(0x1c,0x50);word(0x20,0x28);word(0x24,0x100);
        word(0x34,~0u);word(0x38,~0u);word(0x3c,0);word(0x40,~0u);
        word(0x54,~0u);word(0x60,16);word(0x68,~0u);word(0x6c,8);
        const char16_t name[]=u"test";std::memcpy(bytes.data()+0x70,name,8);
        for(unsigned i=0;i<16;++i)bytes[0x100+i]=u8(0xa0+i);
    }
    std::size_t GetSize() const override {return bytes.size();}
    std::size_t ReadFile(std::size_t offset,std::size_t length,u8* buffer) override {
        assert(offset+length<=bytes.size());
        std::memcpy(buffer,bytes.data()+offset,length);return length;
    }
    bool AllowsCachedReads() const override {return false;}
    bool CacheReady(std::size_t,std::size_t) override {return false;}
private:
    template<class Archive> void serialize(Archive& ar,const unsigned int) {
        ar&boost::serialization::base_object<FileSys::RomFSReader>(*this);
        ar&bytes;
    }
    friend class boost::serialization::access;
};

int main() {
    using namespace FileSys;
    using namespace OOT3D;
    Common::Log::DisableLoggingInTests();
    auto source=std::make_shared<MemoryRomFS>();
    RomFSDelayGenerator native;
    constexpr std::size_t large=1024*1024;
    assert(native.GetReadDelayNs(large)==99148922);
    for(u64 title:{FastLoading::ProgramId,u64(0),u64(0x0004000000125500),u64(0x000400DB00010302)}) {
        auto reader=std::make_shared<LayeredFS>(source,"","",false,title);
        std::unique_ptr<FileBackend> file=std::make_unique<IVFCFile>(reader,std::make_unique<RomFSDelayGenerator>());
        std::array<u8,16> expected{};for(unsigned i=0;i<16;++i)expected[i]=u8(0xa0+i);
        for(bool on:{false,true,false,true}) {
            QOL::fast_loading=on;
            for(std::size_t length:{std::size_t(0),std::size_t(4),std::size_t(4096),large,large*16}) {
                const auto stock=native.GetReadDelayNs(length);
                const auto want=on && title==FastLoading::ProgramId?std::min(stock,FastLoading::ReadDelayCapNs):stock;
                assert(file->GetReadDelayNs(length)==want);
                assert(file->GetOpenDelayNs()==native.GetOpenDelayNs());
            }
            // Routing changes timing only: identical real bytes on the same open handle.
            std::array<u8,16> data{};
            auto result=file->Read(file->GetSize()-16,16,data.data());
            assert(result.Succeeded() && *result==16 && data==expected);
        }
        // In-memory state roundtrip retains the owning title and live toggle behavior.
        std::stringstream state;
        {oarchive out(state);out.register_type<MemoryRomFS>();out<<file;}
        file.reset();
        {iarchive in(state);in.register_type<MemoryRomFS>();in>>file;}
        QOL::fast_loading=true;
        assert(file->GetReadDelayNs(large)==(title==FastLoading::ProgramId?FastLoading::ReadDelayCapNs:native.GetReadDelayNs(large)));
        QOL::fast_loading=false;
        assert(file->GetReadDelayNs(large)==native.GetReadDelayNs(large));
    }
    QOL::fast_loading=true;
    // Direct/unknown readers and in-memory system assets retain stock timing.
    IVFCFile direct(source,std::make_unique<RomFSDelayGenerator>());
    IVFCFileInMemory system(source->bytes,0,source->bytes.size(),std::make_unique<RomFSDelayGenerator>());
    assert(direct.GetReadDelayNs(large)==native.GetReadDelayNs(large));
    assert(system.GetReadDelayNs(large)==native.GetReadDelayNs(large));
    DefaultDelayGenerator other_files;ExeFSDelayGenerator exefs;
    assert(other_files.GetReadDelayNs(large)==native.GetReadDelayNs(large));
    assert(exefs.GetReadDelayNs(large)==native.GetReadDelayNs(large));
    assert(FastLoading::ReadDelay(FastLoading::ProgramId,0)==0);
    std::cout<<"Fast loading: actual RomFS routing, toggle on/off, title isolation, byte preservation, stock file timing and in-memory serialization passed\n";
}
