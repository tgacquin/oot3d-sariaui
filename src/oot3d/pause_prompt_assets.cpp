// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_prompt_assets.h"
#include "oot3d/hd_font.h"
#include "oot3d/hd_rupee_font.h"
#include "oot3d/hd_boss_timer_font.h"
#include "oot3d/graphics_memory_diagnostics.h"
#include "oot3d/romfs_file.h"
#include "oot3d/title_hooks.h"
#include "oot3d/title_prompt_assets.h"
#include "oot3d/file_status_assets.h"
#include "oot3d/file_background_assets.h"
#include "oot3d/file_particles.h"
#include "oot3d/shop_hint_assets.h"
#include "oot3d/boss_title_assets.h"
#include "oot3d/dungeon_location_assets.h"
#include "oot3d/quest_choice_assets.h"
#include "oot3d/loading_icon_assets.h"
#include "oot3d/room_player_icon.h"
#include "video_core/texture/etc1.h"
#include "common/logging/log.h"
#include "common/cityhash.h"
#include "core/core.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/file_sys/romfs_reader.h"
#include "core/loader/loader.h"

namespace OOT3D::PausePrompts {
void Load(Core::System& system) {
    UILanguage::active=static_cast<unsigned>(Service::CFG::GetModule(system)->GetSystemLanguage());
    GraphicsMemory::live_diagnostics.Reset();
    HDFont::Reset();
    std::atomic_store(&HDFont::rupee_atlas,std::shared_ptr<const HDFont::RupeeAtlas>{});
    for(auto& atlas:HDFont::boss_timer_atlases)
        std::atomic_store(&atlas,std::shared_ptr<const HDFont::RupeeAtlas>{});
    std::atomic_store(&DungeonLocation::assets,std::shared_ptr<const DungeonLocation::Assets>{});
    std::atomic_store(&RoomPlayerIcon::pixels,std::shared_ptr<const RoomPlayerIcon::Pixels>{});
    std::atomic_store(&BossTitle::pixels,std::shared_ptr<const BossTitle::Pixels>{});
    std::atomic_store(&ShopHint::pixels,std::shared_ptr<const ShopHint::Pixels>{});
    std::atomic_store(&FileParticles::pixels,std::shared_ptr<const FileParticles::Pixels>{});
    std::atomic_store(&LoadingIcon::pixels,std::shared_ptr<const LoadingIcon::Pixels>{});
    LoadingIcon::ObserveTop({});
    std::atomic_store(&assets,std::shared_ptr<const Assets>{});
    std::atomic_store(&TitlePrompt::assets,std::shared_ptr<const TitlePrompt::Assets>{});
    std::atomic_store(&FileStatusText::assets,std::shared_ptr<const FileStatusText::Assets>{});
    std::atomic_store(&FileBackground::pixels,std::shared_ptr<const FileBackground::Pixels>{});
    std::atomic_store(&QuestChoice::pixels,std::shared_ptr<const QuestChoice::Pixels>{});
    auto& loader=system.GetAppLoader();u64 program=0;
    if(loader.ReadProgramId(program)!=Loader::ResultStatus::Success ||
       program!=TitleScreen::ProgramId)return;
    std::shared_ptr<FileSys::RomFSReader> romfs;
    if(loader.ReadRomFS(romfs)!=Loader::ResultStatus::Success || !romfs)return;
    constexpr std::array<std::u16string_view,2> icon_path{u"message",u"parts.ctxb"};
    constexpr std::array<std::u16string_view,3> map_icon_path{u"menu",u"01_US_ENGLISH",u"menu_map_parts00.ctxb"};
    std::atomic_store(&RoomPlayerIcon::pixels,RoomPlayerIcon::Extract(ReadRomFSFile(*romfs,map_icon_path),
        [](u64 value,unsigned x,unsigned y){return Pica::Texture::SampleETC1Subtile(value,x,y);}));
    constexpr std::array<std::u16string_view,2> loading_path{u"misc",u"throbber.ctxb"};
    constexpr std::array<std::u16string_view,2> particle_path{u"actor",u"zelda_keep.zar"};
    auto titles=std::make_shared<BossTitle::Pixels>();
    bool titles_loaded=true;
    for(unsigned i=0;i<BossTitle::Archives.size();++i) {
        const std::array<std::u16string_view,2> path{u"actor",BossTitle::Archives[i]};
        if(!BossTitle::LoadTitle(*titles,i,ReadRomFSFile(*romfs,path),UILanguage::active.load()))titles_loaded=false;
    }
    if(titles_loaded)std::atomic_store(&BossTitle::pixels,std::shared_ptr<const BossTitle::Pixels>(titles));
    else LOG_WARNING(Frontend,"OoT3D boss intro titles unavailable");
    auto locations=std::make_shared<DungeonLocation::Assets>();
    const std::array<std::u16string_view,3> paper_path{u"menu",UILanguage::Menu(UILanguage::active.load()),u"hud_menu_title00.ctxb"};
    if(DungeonLocation::LoadPaper(*locations,ReadRomFSFile(*romfs,paper_path))) {
        for(unsigned i=0;i<DungeonLocation::Archives.size();++i) {
            const std::array<std::u16string_view,2> path{u"scene",DungeonLocation::Archives[i]};
            if(!DungeonLocation::LoadTitle(*locations,i,ReadRomFSFile(*romfs,path),UILanguage::active.load()))
                LOG_WARNING(Frontend,"OoT3D dungeon-entry title unavailable: {}",i);
        }
        std::atomic_store(&DungeonLocation::assets,std::shared_ptr<const DungeonLocation::Assets>(locations));
    }
    const auto keep=ReadRomFSFile(*romfs,particle_path);
    if(const auto hint=ShopHint::MakePixels(keep))
        std::atomic_store(&ShopHint::pixels,std::make_shared<const ShopHint::Pixels>(*hint));
    if(const auto particle=FileParticles::MakePixels(keep))
        std::atomic_store(&FileParticles::pixels,std::make_shared<const FileParticles::Pixels>(*particle));
    else
        LOG_WARNING(Frontend,"OoT3D file particles: native floating-particle texture unavailable");
    if(const auto icon=LoadingIcon::MakePixels(ReadRomFSFile(*romfs,loading_path)))
        std::atomic_store(&LoadingIcon::pixels,std::make_shared<const LoadingIcon::Pixels>(*icon));
    constexpr std::array<std::u16string_view,3> font_path{u"message",u"us",u"ltn16.qbf"};
    constexpr std::array<std::u16string_view,3> cursor_path{u"menu",u"01_US_ENGLISH",u"menu_cursor00.ctxb"};
    constexpr std::array<std::u16string_view,3> background_path{u"menu",u"01_US_ENGLISH",u"common_bg01.ctxb"};
    const std::array<std::u16string_view,4> quest_path{u"misc",u"us",UILanguage::active==2?u"french":UILanguage::active==5?u"spanish":u"english",u"ura.ctxb"};
    if(const auto quest=QuestChoice::MakePixels(ReadRomFSFile(*romfs,quest_path)))
        std::atomic_store(&QuestChoice::pixels,std::shared_ptr<const QuestChoice::Pixels>(
            std::make_shared<QuestChoice::Pixels>(*quest)));
    if(const auto background=FileBackground::MakePixels(ReadRomFSFile(*romfs,background_path)))
        std::atomic_store(&FileBackground::pixels,std::shared_ptr<const FileBackground::Pixels>(
            std::make_shared<FileBackground::Pixels>(*background)));
    const auto font=ReadRomFSFile(*romfs,font_path);
    if (HDFont::LoadArtwork(font))
        LOG_INFO(Frontend, "OoT3D HD font: {} redrawn glyphs ready", HDFont::ArtworkGlyphCount());
    else
        LOG_WARNING(Frontend, "OoT3D HD font: unsupported font; preserving native lettering");
    constexpr std::array<std::u16string_view,2> system_font_path{u"message",u"sys8.qbf"};
    if(!HDFont::LoadSystemArtwork(ReadRomFSFile(*romfs,system_font_path)))
        LOG_WARNING(Frontend,"OoT3D HD date/time font: unsupported system font; preserving native lettering");
    constexpr std::array<std::u16string_view,3> number_path{u"menu",u"01_US_ENGLISH",u"num_all00.ctxb"};
    const auto numbers=ReadRomFSFile(*romfs,number_path);
    constexpr std::array<std::u16string_view,3> challenge_languages{u"english",u"french",u"spanish"};
    for(unsigned language=0;language<challenge_languages.size();++language) {
        const std::array<std::u16string_view,4> path{u"misc",u"us",challenge_languages[language],u"challenge00.ctxb"};
        const auto bytes=ReadRomFSFile(*romfs,path);
        if(auto atlas=HDFont::DecodeBossTimerAtlas(bytes,
            [](u64 value,unsigned x,unsigned y){return Pica::Texture::SampleETC1Subtile(value,x,y);})) {
            atlas->hash=Common::CityHash64(reinterpret_cast<const char*>(bytes.data()+72),bytes.size()-72);
            std::atomic_store(&HDFont::boss_timer_atlases[language],
                std::shared_ptr<const HDFont::RupeeAtlas>(std::move(atlas)));
        }
    }
    auto rupees=std::make_shared<HDFont::RupeeAtlas>();
    rupees->native=numbers;
    rupees->rgba=HDFont::MakeRupeeAtlas(numbers);
    if(!rupees->rgba.empty()) {
        rupees->hash=Common::CityHash64(reinterpret_cast<const char*>(numbers.data()+72),numbers.size()-72);
        std::atomic_store(&HDFont::rupee_atlas,std::shared_ptr<const HDFont::RupeeAtlas>(std::move(rupees)));
    }
    const auto register_text = [&](unsigned width, unsigned height,
                                   std::span<const std::uint32_t> native,
                                   const std::vector<HDFont::TextGlyph>& glyphs) {
        auto rgba = HDFont::MakeTextAtlas(font, width, height, glyphs);
        if (rgba.empty()) {
            LOG_WARNING(Frontend, "OoT3D HD text atlas generation failed: {}x{}", width, height);
            return;
        }
        const auto hash = Common::CityHash64(reinterpret_cast<const char*>(native.data()),
                                             native.size_bytes());
        auto thin_rgba=HDFont::MakeTextAtlas(font,width,height,glyphs,true);
        if(thin_rgba.empty() || !HDFont::RegisterTextAtlas(width, height, hash, std::move(rgba),std::move(thin_rgba)))
            LOG_WARNING(Frontend, "OoT3D HD text atlas registration failed: {}x{}", width, height);
    };
    if(const auto status=FileStatusText::MakeAssets(font)) {
        std::vector<HDFont::TextGlyph> glyphs;
        for (const auto& glyph : status->glyphs)
            glyphs.push_back({glyph.code, glyph.x + 1, glyph.y + 2});
        register_text(FileStatusText::Width, FileStatusText::Height, status->pixels, glyphs);
        std::atomic_store(&FileStatusText::assets,std::make_shared<const FileStatusText::Assets>(*status));
    }
    if(const auto title=TitlePrompt::MakeAssets(font,UILanguage::active.load())) {
        std::vector<HDFont::TextGlyph> glyphs;
        for (unsigned i = 0; i < title->length; ++i)
            glyphs.push_back({unsigned(TitlePrompt::Labels[UILanguage::Index(UILanguage::active.load())][i]), title->glyphs[i].x, 2});
        register_text(TitlePrompt::Width, TitlePrompt::Height, title->pixels, glyphs);
        std::atomic_store(&TitlePrompt::assets,std::shared_ptr<const TitlePrompt::Assets>(
            std::make_shared<TitlePrompt::Assets>(*title)));
    }
    const auto image=MakeAssets(ReadRomFSFile(*romfs,icon_path),font,
                                ReadRomFSFile(*romfs,cursor_path),UILanguage::active.load());
    if(!image) {
        LOG_WARNING(Frontend,"OoT3D pause prompts: unsupported or missing button atlas/font");
        return;
    }
    std::vector<HDFont::TextGlyph> glyphs;
    for (unsigned row = 0; row < Text.size(); ++row)
        for (unsigned i = 0; i < image->lengths[row]; ++i)
            glyphs.push_back({image->glyphs[row][i].code, image->glyphs[row][i].x, row * TextRowHeight + 2});
    register_text(TextWidth, TextHeight, image->text, glyphs);
    std::atomic_store(&assets,std::shared_ptr<const Assets>(std::make_shared<Assets>(*image)));
}
}
