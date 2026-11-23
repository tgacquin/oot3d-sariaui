// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include "oot3d/ocarina.h"
#include "oot3d/boss_challenge.h"
#include "oot3d/pause_prompt_assets.h"
#include "oot3d/title_prompt_assets.h"
#include "oot3d/dungeon_location_assets.h"
#include "oot3d/boss_title_assets.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
using namespace OOT3D;
namespace P=PausePrompts;
namespace F=FileSelect;
std::vector<std::uint8_t> Read(const std::string& p) {
    std::ifstream f(p,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
struct Memory {
    std::map<unsigned,unsigned> words;
    std::optional<unsigned> Read(unsigned at) {const auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};}
    void Float(unsigned at,float x) {words[at]=std::bit_cast<unsigned>(x);}
};
template<class G> void Safe(const G& g) {
    for(unsigned q=0;q<g.count;++q)for(unsigned v=0;v<4;++v) {
        const auto i=q*12+v*3;
        assert(g.positions[i]>=P::SafeLeft()-.001f && g.positions[i]<=400-P::SafeLeft()+.001f);
        assert(g.positions[i+1]>=1.999f && g.positions[i+1]<=238.001f);
    }
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string root=argv[1];
    const auto font=Read(root+"/message/us/ltn16.qbf"),icons=Read(root+"/message/parts.ctxb");
    const auto status=FileStatusText::MakeAssets(font);assert(status);
    FileStatusText::assets.store(std::make_shared<const FileStatusText::Assets>(*status));
    static_assert(P::TextWidth*P::TextHeight==128*1024); // No larger prompt texture allocation.
    std::array<std::shared_ptr<const P::Assets>,3> images;
    std::array<std::shared_ptr<DungeonLocation::Assets>,3> locations;
    for(unsigned language:{1u,2u,5u}) {
        std::cerr<<"Checking language "<<language<<std::endl;
        UILanguage::active=language;const auto index=UILanguage::Index(language);
        const auto image=P::MakeAssets(icons,font,{},language);assert(image);
        images[index]=std::make_shared<const P::Assets>(*image);
        const auto title=TitlePrompt::MakeAssets(font,language);assert(title && title->length);
        for(unsigned row=0;row<P::Text.size();++row) {
            const auto label=P::LocalizedText[index][row];
            assert(image->lengths[row]==label.size());
            assert(image->widths[row]<=P::TextWidth);
            for(unsigned i=0;i<label.size();++i) {
                assert(image->glyphs[row][i].code==label[i]);
                assert(status->Find(label[i])->code==label[i]); // No '?' fallback for accents.
            }
        }
        for(bool wide:{false,true}) {
            Widescreen::enabled=wide;Widescreen::supported_layout=true;
            // An old off-edge X must respond to the first drag back into view.
            F::Layout edited;edited.prompts[1]={400,220,.74f};
            const auto edge=F::Prompts(edited,*image,1,true);
            edited.prompts[1].x=F::VisiblePromptX(400,.74f)-1;
            const auto inset=F::Prompts(edited,*image,1,true);
            assert(std::abs(edge.icons.positions[0]-inset.icons.positions[0]-1)<.001f);
            assert(std::abs(edge.text.positions[0]-inset.text.positions[0]-1)<.001f);
            for(float x:{375.f,376.f,377.f}) {
                edited.prompts[1].x=x;
                const auto prompt=F::Prompts(edited,*image,1,true);
                assert(std::abs(prompt.icons.positions[0]-x)<.001f);
                for(unsigned q=0;q<prompt.text.count;++q)assert(prompt.text.positions[q*12+3]<x);
            }
            for(float scale:{.5f,1.f,2.f,4.f})for(float offset:{-350.f,0.f,350.f}) {
                F::Layout file;file.prompts[0]={offset,220,scale};file.prompts[1]={offset,210,scale};
                const auto g=F::Prompts(file,*image,.4f);Safe(g.icons);Safe(g.text);
                assert(g.text.count==image->lengths[P::FileSelectText]+image->lengths[P::FileBackText]);
                assert(g.text.colors[3]==.4f && file.prompts[0].x==offset);
                for(auto page:{P::Page::Items,P::Page::Gear,P::Page::Map}) {
                    P::Layout l;l.scale=scale;l.lower.pose.x=offset;
                    const auto p=P::MakeGeometry(l,*image,page);Safe(p.icons);Safe(p.text);
                    unsigned expected=0;
                    if(page==P::Page::Map)for(unsigned row:{P::MapMove,P::MapReturn,P::MapToItems})expected+=image->lengths[row];
                    else {
                        for(unsigned row:{P::Choose,P::Return,P::EquipI,P::EquipII,P::ChangeTab})expected+=image->lengths[row];
                        expected+=5; // ZL, ZR, slash.
                        if(page==P::Page::Items)expected+=image->lengths[P::ItemsToMap];
                    }
                    assert(p.text.count==expected); // No silent capacity truncation.
                }
                for(auto label:{UILanguage::Join(),UILanguage::Host(false),UILanguage::Host(true),UILanguage::Randomizer(true)}) {
                    const auto p=F::FitLabel({offset,220,scale},label);
                    assert(p.x>=P::SafeLeft()-.001f);
                    assert(p.x+(21+FileStatusText::Measure(*status,label))*p.scale<=400-P::SafeLeft()+.001f);
                }
            }
        }
        // Edge prompts: the text ends four native pixels before its button.
        // Related labels share the button column, including different languages.
        const auto ordered=[&](const P::Pair& pair,unsigned first_row,unsigned second_row,bool right) {
            unsigned begin=0;
            for(unsigned i=0;i<2;++i) {
                const unsigned count=image->lengths[i?second_row:first_row];
                float left=INFINITY,end=-INFINITY;
                for(unsigned q=begin;q<begin+count;++q)for(unsigned v=0;v<12;v+=3) {
                    left=std::min(left,pair.text.positions[q*12+v]);end=std::max(end,pair.text.positions[q*12+v]);
                }
                const float icon_left=pair.icons.positions[i*12],icon_right=pair.icons.positions[i*12+3];
                assert(right?end<icon_left:left>icon_right);
                begin+=count;
            }
            assert(std::abs(pair.icons.positions[0]-pair.icons.positions[12])<.001f);
        };
        Widescreen::enabled=true;
        for(bool right:{false,true})for(float scale:{.5f,1.f,2.f}) {
            const float x=right?300.f:20.f;
            F::Layout f;f.prompts={{{x,200,scale},{x,220,scale}}};
            ordered(F::Prompts(f,*image,1),P::FileSelectText,P::FileBackText,right);
            Ocarina::Layout o;o[Ocarina::SongsPrompt]={x,200,scale};o[Ocarina::BackPrompt]={x,220,scale};
            ordered(Ocarina::Prompts(o,*image,true,1),P::SongsText,P::FileBackText,right);
            ordered(Ocarina::Prompts(o,*image,false,1),P::SongsText,P::CancelText,right);
            SaveMenu::Layout save;save.prompts[0]={x-28,-20,scale};save.prompts[1]={x-300,0,scale};
            ordered(P::SavePrompts(*image,save,1),P::FileSelectText,P::FileBackText,right);
            ordered(P::SavePrompts(*image,save,1,true),P::OkayText,P::CancelText,right);
            const auto death=P::SavePrompts(*image,save,1,false,true);
            assert(death.icons.positions[0]==P::SavePrompts(*image,save,1).icons.positions[12]);
            P::Layout map;map.scale=scale;map.prompts[P::MapMove].group.x=x-48;
            map.prompts[P::MapReturn].group={x-280,20,1};
            ordered(P::MakeGeometry(map,*image,P::Page::Map,0,true,false),P::MapMove,P::MapReturn,right);
            BossChallenge::Layout boss;boss[BossChallenge::SelectPrompt]={x,200,scale};
            boss[BossChallenge::BackPrompt]={x,220,scale};
            ordered(BossChallenge::ButtonPrompts(boss,*image),P::FileSelectText,P::FileBackText,right);
            for(unsigned i=0;i<2;++i) {
                const auto label=F::NamePromptLabel(i),column=F::WidestPrompt({F::NamePromptLabel(0),F::NamePromptLabel(1)});
                const auto e=F::StatusPromptEdge(*status,{x,200,scale},label,column);
                assert(right?e.text+(FileStatusText::Measure(*status,label)+.75f)*scale<e.button:e.button+16*scale<e.text);
            }
        }
        std::cerr<<"Loading localized artwork"<<std::endl;
        auto location=std::make_shared<DungeonLocation::Assets>();
        const auto dir=UILanguage::Menu(language);
        assert(DungeonLocation::LoadPaper(*location,Read(root+"/menu/"+std::string(dir.begin(),dir.end())+"/hud_menu_title00.ctxb")));
        for(unsigned i=0;i<DungeonLocation::Archives.size();++i) {
            const auto name=DungeonLocation::Archives[i];
            assert(DungeonLocation::LoadTitle(*location,i,Read(root+"/scene/"+std::string(name.begin(),name.end())),language));
        }
        assert(location->available==4095);locations[index]=location;
        auto bosses=std::make_unique<BossTitle::Pixels>();
        for(unsigned i=0;i<BossTitle::Archives.size();++i) {
            const auto name=BossTitle::Archives[i];
            assert(BossTitle::LoadTitle(*bosses,i,Read(root+"/actor/"+std::string(name.begin(),name.end())),language));
        }
    }
    assert(images[0]->text!=images[1]->text && images[1]->text!=images[2]->text);
    assert(locations[0]->paper!=locations[1]->paper && locations[1]->paper!=locations[2]->paper);
    assert(locations[0]->titles!=locations[1]->titles && locations[1]->titles!=locations[2]->titles);
    // Actual native model/descriptor layout, two text layers, transformed bounds.
    Memory m;constexpr unsigned owner=0x18000000,model=0x18001000,mesh=0x18002000,desc=0x18003000,positions=0x18004000;
    m.words[owner+0x10]=m.words[owner+0x14]=model;m.words[model+4]=mesh;m.words[mesh]=desc;
    m.words[desc]=positions;m.words[desc+12]=4;
    const std::array<float,12> matrix{1,0,0,5,0,1,0,3,0,0,1,0};
    const std::array<float,12> vertices{40,16,0,280,16,0,40,32,0,280,32,0};
    for(unsigned i=0;i<12;++i){m.Float(model+0xc+4*i,matrix[i]);m.Float(positions+4*i,vertices[i]);}
    const auto before=m.words;const auto b=F::MessageBounds(m,owner);
    assert(b.left==45 && b.right==285 && b.top==19 && b.bottom==35 && m.words==before);
    for(bool wide:{false,true})for(float width:{180.f,240.f,600.f})for(float offset:{-300.f,0.f,300.f}) {
        Widescreen::enabled=wide;
        const auto fit=F::FitMessage(105+offset,295+offset,200+offset-width/2,200+offset+width/2);
        assert(fit.left>=P::SafeLeft()-.001f && fit.right<=400-P::SafeLeft()+.001f);
        assert(200+offset+fit.dx-width*fit.scale/2>=fit.left+11.99f);
        assert(200+offset+fit.dx+width*fit.scale/2<=fit.right-11.99f);
    }
    // Exercise the actual draw hook: widen the center strips, keep decorative
    // cap widths/UVs, leave the native model and saved editor layout untouched.
    Widescreen::enabled=false;
    auto hook=std::make_unique<F::Hook>();hook->prompt_slot=6;
    m.words[0x55b548+6*4]=owner;
    for(unsigned row=0;row<2;++row) {
        hook->atlas_geometry.Add(62,10+row*12,16,12,0,0,.1f,1);
        hook->atlas_geometry.Add(78,10+row*12,164,12,.1f,0,.9f,1);
        hook->atlas_geometry.Add(242,10+row*12,16,12,.9f,0,1,1);
    }
    hook->bounds[F::Prompt]=F::Bounds(hook->atlas_geometry,0,5);
    const auto old_uvs=hook->atlas_geometry.uvs;
    const auto original_memory=m.words;
    hook->FitPromptBox(m);
    const auto fitted=F::Bounds(hook->atlas_geometry,0,5);
    assert(fitted.right-fitted.left==264 && hook->fitted_prompt_text);
    assert(hook->atlas_geometry.positions[3]-hook->atlas_geometry.positions[0]==16);
    assert(hook->atlas_geometry.positions[27]-hook->atlas_geometry.positions[24]==16);
    assert(hook->atlas_geometry.uvs==old_uvs && m.words==original_memory);
    assert(hook->frame_layout.parts[F::Prompt].scale==1);
    m.words[desc+12]=0xffffffff;assert(!F::MessageBounds(m,owner).Valid());
    std::cout<<"EN/FR/ES labels, accents, capacities, viewport fitting, native message bounds, all 12 dungeon titles and 8 boss titles passed\n";
}
