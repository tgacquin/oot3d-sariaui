// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/map_frame_asset.h"
#include "oot3d/map_cutout.h"
#include "oot3d/world_map_assets.h"
#include "oot3d/title_hooks.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/romfs_reader.h"
#include "core/loader/loader.h"
#include "video_core/texture/etc1.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace OOT3D::MapFrame {
void Load(Core::System& system) {
    WorldMap::Load(system);
    SetTexture({});status.store(Status::Unavailable);
    std::atomic_store(&source_image,std::shared_ptr<const SourcePixels>{});
    LoadCutout();
    auto& loader=system.GetAppLoader();
    u64 program=0;
    if (loader.ReadProgramId(program)!=Loader::ResultStatus::Success || program!=TitleScreen::ProgramId) return;
    std::shared_ptr<FileSys::RomFSReader> romfs;
    if (loader.ReadRomFS(romfs)!=Loader::ResultStatus::Success || !romfs) return;
    const auto source=ReadSource(*romfs);
    if (!ValidSource(source)) {
        LOG_WARNING(Frontend,"OoT3D map frame: source missing or unsupported; leaving native map unchanged");
        return;
    }
    auto decoded=std::make_shared<SourcePixels>();
    for(unsigned y=0;y<SourceHeight;++y)for(unsigned x=0;x<SourceWidth;++x) {
        const auto at=72+((y/8)*128+x/8)*32+((y%8)/4*2+(x%8)/4)*8;
        const auto rgb=Pica::Texture::SampleETC1Subtile(LE64(source,at),x%4,y%4);
        (*decoded)[y*SourceWidth+x]=255u | (unsigned(rgb.b())<<8) |
            (unsigned(rgb.g())<<16) | (unsigned(rgb.r())<<24);
    }
    std::atomic_store(&source_image,std::shared_ptr<const SourcePixels>(decoded));
    LoadCutout();
    status.store(Status::Extracted);
    LOG_INFO(Frontend,"OoT3D map border: source ready for coordinate selection");
}
} // namespace OOT3D::MapFrame
