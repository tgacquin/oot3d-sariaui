// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/dpad_assets.h"
#include "oot3d/randomizer/icons.h"
#include "oot3d/romfs_file.h"
#include "oot3d/title_hooks.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/romfs_reader.h"
#include "core/loader/loader.h"
#include "video_core/texture/texture_decode.h"

namespace OOT3D::DPad {
void Load(Core::System& system) {
    Randomizer::icon_pixels.store(std::shared_ptr<const Randomizer::IconPixels>{});
    for(auto& image:textures)image.store(std::shared_ptr<const Pixels>{});
    auto& loader=system.GetAppLoader();u64 program=0;
    if(loader.ReadProgramId(program)!=Loader::ResultStatus::Success ||
       program!=TitleScreen::ProgramId)return;
    std::shared_ptr<FileSys::RomFSReader> romfs;
    if(loader.ReadRomFS(romfs)!=Loader::ResultStatus::Success || !romfs)return;
    constexpr std::array<std::u16string_view,3> item_path{
        u"menu",u"01_US_ENGLISH",u"icon_item_menu00.ctxb"};
    constexpr std::array<std::u16string_view,3> menu_path{
        u"menu",u"01_US_ENGLISH",u"menu_top_parts00.ctxb"};
    constexpr std::array<std::u16string_view,3> song_path{
        u"menu",u"01_US_ENGLISH",u"menu_okarina_parts00.ctxb"};
    auto items=ReadRomFSFile(*romfs,item_path),menu=ReadRomFSFile(*romfs,menu_path);
    if(!ValidAtlas(items,512,512,0x675b,262144) ||
       !ValidAtlas(menu,512,256,0x80336752,262144)) {
        LOG_WARNING(Frontend,"OoT3D D-pad: unsupported or missing native icon atlas");
        return;
    }
    Pica::Texture::TextureInfo item_info{},menu_info{};
    item_info.width=512;item_info.height=512;
    item_info.format=Pica::TexturingRegs::TextureFormat::ETC1A4;item_info.SetDefaultStride();
    menu_info.width=512;menu_info.height=256;
    menu_info.format=Pica::TexturingRegs::TextureFormat::RGBA4;menu_info.SetDefaultStride();
    const auto sample=[&](bool from_menu,unsigned x,unsigned y) {
        const auto& info=from_menu?menu_info:item_info;
        const auto& source=from_menu?menu:items;
        const auto c=Pica::Texture::LookupTexture(source.data()+72,x,info.height-1-y,info);
        return (u32(c.r())<<24)|(u32(c.g())<<16)|(u32(c.b())<<8)|c.a();
    };
    auto icons = std::make_shared<Randomizer::IconPixels>();
    for (unsigned id = 0; id < 144; ++id) {
        const unsigned left = (id % 12) * 42, top = (id / 12) * 42;
        for (unsigned y = 0; y < 42; ++y)
            for (unsigned x = 0; x < 42; ++x)
                (*icons)[(top + y) * 512 + left + x] = sample(false, left + x, 511 - top - y);
    }
    // OoT3D leaves the N64 song-note cells empty. Use its actual song stones,
    // preserving their 64:40 aspect ratio inside the corresponding UI cells.
    const auto songs=ReadRomFSFile(*romfs,song_path);
    const bool have_songs=ValidAtlas(songs,512,256,0x675b,131072);
    auto song_info=item_info;song_info.height=256;
    constexpr std::array<std::array<unsigned,2>,12> song_cells{{
        {1,2},{1,3},{0,3},{2,1},{2,2},{2,3}, // Warp songs.
        {0,0},{0,1},{0,2},{1,0},{1,1},{2,0}}};
    for(unsigned i=0;i<song_cells.size();++i) {
        const unsigned id=90+i,left=(id%12)*42,top=(id/12)*42;
        for(unsigned y=0;y<26;++y)for(unsigned x=0;x<42;++x) {
            auto pixel=sample(false,336+x,511-y);
            if(have_songs) {
                const auto c=Pica::Texture::LookupTexture(songs.data()+72,
                    song_cells[i][0]*64+x*64/42,song_cells[i][1]*40+y*40/26,song_info);
                pixel=(u32(c.r())<<24)|(u32(c.g())<<16)|(u32(c.b())<<8)|c.a();
            }
            (*icons)[(top+8+y)*512+left+x]=pixel;
        }
    }
    Randomizer::icon_pixels.store(std::shared_ptr<const Randomizer::IconPixels>(icons));
    for(unsigned i=0;i<TextureCount;++i)
        textures[i].store(std::shared_ptr<const Pixels>(
            std::make_shared<Pixels>(MakeTexture(i==1,sample))));
}
}
