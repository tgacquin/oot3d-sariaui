// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/world_map_assets.h"
#include "oot3d/world_map.h"
#include "oot3d/romfs_file.h"
#include "oot3d/title_hooks.h"
#include "core/core.h"
#include "core/file_sys/romfs_reader.h"
#include "core/loader/loader.h"
#include "common/logging/log.h"
#include "video_core/texture/etc1.h"

namespace OOT3D::WorldMap {
namespace {
std::mutex warm_mutex;
Warmer warmer;
std::future<std::shared_ptr<const DungeonParchment::Warm>> dungeon_job;
std::shared_ptr<const WarmSource> warm_source;
}
void Warmup() {
    std::scoped_lock lock{warm_mutex};
    const auto layout=PauseMenu::GetLayout();
    warmer.Poll(warm_source,layout,GetOutline());
    if(!warm_source || !DungeonParchment::Enabled(layout) || !PauseMenu::Valid(layout[PauseMenu::DungeonMap]))return;
    // Quad zero of the native dungeon atlas is the 208x176 paper.
    const DungeonParchment::Recipe recipe(layout,{56,32,264,208});
    const auto matches=[&](const std::shared_ptr<const DungeonParchment::Warm>& w) {
        return w && w->atlas==warm_source->dungeon_atlas && w->key==recipe.key;
    };
    if(dungeon_job.valid()) {
        if(dungeon_job.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        const auto result=dungeon_job.get();
        if(matches(result))DungeonParchment::warm.store(result);
    }
    if(matches(DungeonParchment::warm.load()))return;
    dungeon_job=std::async(std::launch::async,[source=warm_source,recipe] {
        return std::make_shared<const DungeonParchment::Warm>(DungeonParchment::Warm{
            recipe.key,source->dungeon_atlas,recipe.Render(source->paper)});
    });
}
void Load(Core::System& system) {
    {
        std::scoped_lock lock{warm_mutex};warm_source.reset();
        DungeonParchment::warm.store(std::shared_ptr<const DungeonParchment::Warm>{});
        warm_artwork.store(std::shared_ptr<const WarmPair>{});
    }
    paper_image.store(std::shared_ptr<const PaperImage>{});
    source_image.store(std::shared_ptr<const MapFrame::SourcePixels>{});
    LoadOutline();
    auto& loader=system.GetAppLoader();u64 program=0;
    if(loader.ReadProgramId(program)!=Loader::ResultStatus::Success || program!=TitleScreen::ProgramId)return;
    std::shared_ptr<FileSys::RomFSReader> romfs;
    if(loader.ReadRomFS(romfs)!=Loader::ResultStatus::Success || !romfs)return;
    constexpr std::array<std::u16string_view,3> path{u"menu",u"01_US_ENGLISH",u"menu_map_parts00.ctxb"};
    const auto dungeon=ReadRomFSFile(*romfs,path);
    const auto paper=ExtractPaper(dungeon,[](std::uint64_t bits,unsigned x,unsigned y) {
        const auto c=Pica::Texture::SampleETC1Subtile(bits,x,y);return std::array{c.r(),c.g(),c.b()};
    });
    paper_image.store(std::shared_ptr<const PaperImage>(paper));
    if(!paper)LOG_WARNING(Frontend,"OoT3D overworld parchments: unsupported or missing dungeon atlas");
    constexpr std::array<std::u16string_view,3> map_path{u"menu",u"01_US_ENGLISH",u"menu_map_parts01.ctxb"};
    const auto atlas=ReadRomFSFile(*romfs,map_path);
    if(!paper || atlas.size()!=131144 || MapFrame::LE32(atlas,0)!=0x62787463 ||
       MapFrame::LE32(atlas,20)!=72 || MapFrame::LE32(atlas,44)!=(512|(256u<<16)) ||
       MapFrame::LE32(atlas,48)!=0x675b)return;
    auto source=std::make_shared<WarmSource>();source->paper_image=paper;
    auto dungeon_atlas=std::make_shared<std::vector<U32>>(131072/4);
    for(unsigned i=0;i<dungeon_atlas->size();++i)(*dungeon_atlas)[i]=MapFrame::LE32(dungeon,72+4*i);
    source->dungeon_atlas=std::move(dungeon_atlas);
    source->paper.Set(paper->colors,PaperWidth,PaperHeight);
    auto compressed=std::make_shared<std::vector<U32>>(131072/4);
    for(unsigned i=0;i<compressed->size();++i)(*compressed)[i]=MapFrame::LE32(atlas,72+4*i);
    source->compressed=compressed;source->map.resize(MapWidth*MapHeight);
    for(unsigned y=0;y<MapHeight;++y)for(unsigned x=0;x<MapWidth;++x) {
        const unsigned at=72+((y/8)*64+x/8)*64+((y%8)/4*2+(x%8)/4)*16;
        const auto rgb=Pica::Texture::SampleETC1Subtile(MapFrame::LE64(atlas,at+8),x%4,y%4);
        const auto alpha=((MapFrame::LE64(atlas,at)>>(4*(4*(x%4)+y%4)))&15)*17;
        source->map[y*MapWidth+x]={float(rgb.r()),float(rgb.g()),float(rgb.b()),float(alpha)};
    }
    {
        std::scoped_lock lock{warm_mutex};warm_source=std::move(source);
    }
    Warmup();
}
}
