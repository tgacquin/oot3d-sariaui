// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/master_quest.h"
#include "oot3d/randomizer/ui.h"
#include "oot3d/randomizer/service.h"
#include "oot3d/randomizer/presets.h"
#include "oot3d/multiplayer_startup.h"
#include "oot3d/multiplayer_settings.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>

namespace OOT3D::Randomizer {
namespace {
enum Page { General, Logic, Dungeons, Shuffles, Hints, Starting, Locations, Tricks,
            Enemies, Gameplay, Audio, Multiplayer, PageCount };
constexpr std::array<const char*, PageCount> PageNames{
    "General", "Logic / Access", "Dungeons", "Shuffles", "Hints / Traps",
    "Starting Items", "Check Locations", "Tricks / Glitches", "Enemies",
    "Gameplay / Skips", "Music / Sound", "Multiplayer"};
struct Placement { Page page; std::string section; };
Placement Place(unsigned id) {
    if(id<=Fortress) return {Logic,"Area access"};
    if(id==Songs || id==Tokens) return {Shuffles,"Items and rewards"};
    if(id==Shops || id==ShopPrices) return {Shuffles,"Shops and merchants"};
    if(id>=Rewards && id<=GanonKey) return {Dungeons,"Dungeon items"};
    if(id>=Hearts && id<=MaxRupees) {
        if((id>=KokiriSword && id<=HoverBoots) || id==Strength || id==Scale || id==Wallet)
            return {Starting,"Equipment"};
        if(id==Hearts || id==Magic || id==Consumables || id==MaxRupees)
            return {Starting,"Health and supplies"};
        return {Starting,"Items"};
    }
    if(id>=ChildStealth && id<=SongReplays) return {Gameplay,"Cutscenes and skips"};
    return {Hints,"Hints and spoiler log"};
}
Placement Place(const AdvancedOption& o) {
    const auto& k=o.key; const auto& g=o.group;
    if(g=="World access and goals") {
        if(k.starts_with("Bridge")) return {Logic,"Rainbow Bridge"};
        if(k.starts_with("Triforce")) return {Logic,"Triforce Hunt"};
        if(k=="RandomGanonsTrials" || k=="GanonsTrialsCount") return {Logic,"Ganon's trials"};
        return {Logic,"Area access"};
    }
    if(g=="World and entrances") return {Logic,k=="BombchusInLogic"?"Logic":"Entrances"};
    if(g=="Logic") return {Logic,"Logic"};
    if(k=="ItemPoolValue" || k=="ProgressiveGoronSword") return {Shuffles,"Item pool"};
    if(g=="Keys and dungeon items") return {Dungeons,k=="KeyRings" || k.starts_with("Ring")?"Key rings":k.starts_with("LACS")?"Light Arrow cutscene":"Dungeon items"};
    if(g=="Additional item shuffling") return {Shuffles,k=="Scrubsanity" || k=="ShuffleMerchants"?"Shops and merchants":"Items and rewards"};
    if(g=="Hints and miscellaneous") {
        if(k=="StartingTime") return {Starting,"Health and supplies"};
        if(k.starts_with("Chest")) return {Hints,"Chest appearance"};
        if(k.starts_with("Compasses") || k.starts_with("Maps")) return {Dungeons,"Map and compass hints"};
        return {Hints,"Hints and spoiler log"};
    }
    if(g=="Item pool and traps") return {Hints,"Traps"};
    if(g=="Gameplay and traps") {
        if(k.find("Trap")!=std::string::npos || k.starts_with("Rupoor")) return {Hints,"Traps"};
        return {Gameplay,"Gameplay"};
    }
    if(g=="Additional skips") return {Gameplay,"Convenience and skips"};
    if(g=="Additional starting inventory") {
        if(k=="StartingMasterSword" || k=="StartingBiggoronSword" || k=="StartingShardOfAgony") return {Starting,"Equipment"};
        if(k=="StartingDoubleDefense" || k=="StartingSkulltulaToken") return {Starting,"Health and supplies"};
        return {Starting,"Items"};
    }
    if(g=="Starting songs") return {Starting,"Songs"};
    if(g=="Starting rewards") return {Starting,"Dungeon rewards"};
    if(g=="Starting enemy souls") return {Starting,"Enemy souls"};
    if(g=="Starting ocarina buttons") return {Starting,"Ocarina buttons"};
    if(g=="Excluded locations") {
        // Upstream uses stable area prefixes in these names. Keep each area together.
        static constexpr std::pair<std::string_view,std::string_view> areas[]{
            {"KF ","Kokiri Forest"},{"LW ","Lost Woods"},{"SFM ","Sacred Forest Meadow"},
            {"HF ","Hyrule Field"},{"LLR ","Lon Lon Ranch"},{"Market ","Market"},{"MK ","Market"},
            {"HC ","Hyrule Castle"},{"OGC ","Outside Ganon's Castle"},{"ToT ","Temple of Time"},
            {"Kak ","Kakariko Village"},{"Graveyard ","Graveyard"},{"GY ","Graveyard"},{"DMT ","Death Mountain Trail"},
            {"DMC ","Death Mountain Crater"},{"GC ","Goron City"},{"ZR ","Zora's River"},
            {"ZD ","Zora's Domain"},{"ZF ","Zora's Fountain"},{"LH ","Lake Hylia"},
            {"GV ","Gerudo Valley"},{"GF ","Gerudo Fortress"},{"Wasteland ","Haunted Wasteland"},
            {"Colossus ","Desert Colossus"},{"Deku Tree ","Deku Tree"},
            {"Dodongos Cavern ","Dodongo's Cavern"},{"Dodongo's Cavern ","Dodongo's Cavern"},{"Jabu Jabus Belly ","Jabu-Jabu's Belly"},
            {"Forest Temple ","Forest Temple"},{"Fire Temple ","Fire Temple"},
            {"Water Temple ","Water Temple"},{"Spirit Temple ","Spirit Temple"},
            {"Shadow Temple ","Shadow Temple"},{"Bottom of the Well ","Bottom of the Well"},
            {"Ice Cavern ","Ice Cavern"},{"Gerudo Training Ground ","Gerudo Training Ground"},
            {"Ganons Castle ","Ganon's Castle"},{"Ganon's Castle ","Ganon's Castle"},
            {"Song ","Songs"},{"Sheik ","Songs"},{"Queen Gohma", "Bosses"},
            {"King Dodongo", "Bosses"},{"Barinade", "Bosses"},{"Phantom Ganon", "Bosses"},
            {"Volvagia", "Bosses"},{"Morpha", "Bosses"},{"Twinrova", "Bosses"},{"Bongo Bongo", "Bosses"}};
        for(const auto& [prefix,name]:areas) if(o.label.starts_with(prefix)) return {Locations,std::string(name)};
        return {Locations,"Other locations"};
    }
    if(g=="Logic tricks" || g=="Glitches") return {Tricks,g};
    if(g=="Enemies and Skulltulas" || g=="Enemy pool") return {Enemies,g};
    if(g=="Music and sound") return {Audio,k.starts_with("ShuffleSFX")?"Sound effects":"Music and ocarina"};
    if(g=="Multiplayer") return {Multiplayer,k=="MP_Enabled" || k=="MP_SyncId"?"Connection":"Sharing"};
    return {Gameplay,g};
}
struct Setting {
    int basic=-1;
    const AdvancedOption* advanced=nullptr;
    Placement place;
    std::string key,label;
};
const std::vector<Setting>& Settings() {
    static const auto settings=[] {
        std::vector<Setting> result;
        for(unsigned i=0;i<Specs.size();++i) result.push_back({int(i),nullptr,Place(i),Specs[i].key,Specs[i].label});
        for(const auto& o:AdvancedOptions()) result.push_back({-1,&o,Place(o),o.key,o.label});
        return result;
    }();
    return settings;
}
int Value(const Config& c,const Setting& s) {
    if(s.basic>=0) return c.options[s.basic];
    const auto it=c.advanced.find(s.key);
    return it==c.advanced.end()?s.advanced->initial:it->second;
}
int Value(const Config& c,const char* key) {
    const auto it=c.advanced.find(key);
    if(it!=c.advanced.end()) return it->second;
    for(const auto& o:AdvancedOptions()) if(o.key==key) return o.initial;
    return 0;
}
std::vector<std::string> Choices(const Setting& s) {
    if(s.advanced) return s.advanced->choices;
    const auto& spec=Specs[s.basic];
    std::vector<std::string> result;
    if(spec.choices) for(auto p=spec.choices;*p;p+=std::strlen(p)+1) result.emplace_back(p);
    else for(int n=spec.low;n<=spec.high;++n) result.push_back(spec.high==1?(n?"On":"Off"):std::to_string(n));
    return result;
}
bool Enabled(const Config& c,const Setting& s) {
    if(s.basic>=0) return s.basic!=ShopPrices || c.options[Shops]>=2;
    const auto& k=s.key;const auto& g=s.advanced->group;
    const auto v=[&](const char* key){return Value(c,key);};
    constexpr std::array bridge_counts{"BridgeStoneCount","BridgeMedallionCount","BridgeRewardCount",
        "BridgeDungeonCount","BridgeTokenCount","BridgeHeartCount"};
    constexpr std::array lacs_counts{"LACSMedallionCount","LACSStoneCount","LACSRewardCount",
        "LACSDungeonCount","LACSTokenCount","LACSHeartCount"};
    for(unsigned i=0;i<bridge_counts.size();++i) if(k==bridge_counts[i]) return v("Bridge")==int(i)+2;
    for(unsigned i=0;i<lacs_counts.size();++i) if(k==lacs_counts[i]) return c.options[GanonKey]==int(i)+7;
    if(g=="World and entrances" && k!="ShuffleEntrances" && k!="BombchusInLogic") {
        if(!v("ShuffleEntrances")) return false;
        if(k=="MixDungeons") return v("MixedEntrancePools") && v("ShuffleDungeonEntrances");
        if(k=="MixOverworld") return v("MixedEntrancePools") && v("ShuffleOverworldEntrances");
        if(k=="MixInteriors") return v("MixedEntrancePools") && v("ShuffleInteriorEntrances");
        if(k=="MixGrottos") return v("MixedEntrancePools") && v("ShuffleGrottoEntrances");
    }
    if(k=="GanonsTrialsCount") return !v("RandomGanonsTrials");
    if(k=="TriforcePiecesTotal" || k=="TriforcePiecesRequired") return v("TriforceHunt")!=0;
    if(k.starts_with("Ring")) return v("KeyRings")==2;
    if(g=="Enemy pool") return v("Enemizer")!=0;
    if(g=="Starting enemy souls") return v("ShuffleEnemySouls")!=0;
    if(g=="Starting ocarina buttons") return v("ShuffleOcarinaButtons")!=0;
    if(k=="GsLocGuaranteeNew") return v("RandomGsLocations")!=0;
    if(k=="ShuffleBGM" || k=="ShuffleMelodies") return v("ShuffleMusic")!=0;
    if(k.starts_with("ShuffleSFX") && k!="ShuffleSFX") return v("ShuffleSFX")!=0;
    if(g=="Multiplayer" && k!="MP_Enabled") return v("MP_Enabled")!=0;
    return true;
}
void Help(const Setting& s,int index) {
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
    ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28);
    ImGui::TextUnformatted(s.label.c_str());
    if(s.advanced) {
        const auto& o=*s.advanced;
        const auto& desc=index>=0 && std::size_t(index)<o.descriptions.size()?o.descriptions[index]:o.description;
        if(!desc.empty()) {ImGui::Separator();ImGui::TextUnformatted(desc.c_str());}
    }
    ImGui::PopTextWrapPos();ImGui::EndTooltip();
}
// Native item IDs index 42x42 cells in icon_item_menu00.ctxb.
int Icon(const Setting& s,int value) {
    if(s.basic>=0) {
        switch(s.basic) {
        case Ocarina:return value==2?8:7; case Hookshot:return value==2?11:10;
        case BombBag:return 77+std::max(value-1,0);case Bow:return 3;case Slingshot:return 6;
        case Bottles:return 20;case KokiriSword:return 59;case DekuShield:return 62;
        case HylianShield:return 63;case MirrorShield:return 64;case GoronTunic:return 66;
        case ZoraTunic:return 67;case IronBoots:return 69;case HoverBoots:return 70;
        case Boomerang:return 14;case Lens:return 15;case Hammer:return 17;case DinsFire:return 5;
        case FaroresWind:return 13;case NayrusLove:return 19;
        case Strength:return 80+std::max(value-1,0);case Scale:return 83+std::max(value-1,0);
        case Wallet:return value>=2?87:86;default:return -1;
        }
    }
    if(s.key=="StartingBiggoronSword") return value==1?85:61;
    static constexpr std::pair<std::string_view,int> mapping[]{
        {"StartingStickCapacity",0},{"StartingNutCapacity",1},{"StartingBombchus",9},
        {"StartingFireArrows",4},{"StartingIceArrows",12},{"StartingLightArrows",18},
        {"StartingMagicBean",16},{"StartingRutoBottle",27},{"StartingChildTrade",39},
        {"StartingMasterSword",60},{"StartingBiggoronSword",61},{"StartingShardOfAgony",111},
        {"StartingZeldasLullaby",96},{"StartingEponasSong",97},{"StartingSariasSong",98},
        {"StartingSunsSong",99},{"StartingSongOfTime",100},{"StartingSongOfStorms",101},
        {"StartingMinuetOfForest",90},{"StartingBoleroOfFire",91},{"StartingSerenadeOfWater",92},
        {"StartingRequiemOfSpirit",93},{"StartingNocturneOfShadow",94},{"StartingPreludeOfLight",95},
        {"StartingKokiriEmerald",108},{"StartingGoronRuby",109},{"StartingZoraSapphire",110},
        {"StartingLightMedallion",107},{"StartingForestMedallion",102},{"StartingFireMedallion",103},
        {"StartingWaterMedallion",104},{"StartingSpiritMedallion",105},{"StartingShadowMedallion",106}};
    if(s.key.starts_with("StartingBottle")) {
        constexpr int bottles[]{20,20,21,22,23,24,25,26,28,29,30,31,32};
        return bottles[std::clamp(value,0,12)];
    }
    for(const auto& [key,id]:mapping) if(s.key==key) return id;
    return -1;
}
void Set(Config& c,const Setting& s,int value) {
    if(s.basic>=0) c.options[s.basic]=value;else c.advanced[s.key]=value;
}
bool Control(Config& c,const Setting& s,std::uint64_t atlas,bool tile) {
    ImGui::PushID(s.key.c_str());ImGui::BeginDisabled(!Enabled(c,s));
    int value=Value(c,s),old=value;
    const int low=s.basic>=0?Specs[s.basic].low:0;
    const auto choices=Choices(s);
    int index=std::clamp(value-low,0,int(choices.size())-1);
    const bool toggle=choices.size()==2 && ((choices[0]=="Off" && choices[1]=="On") ||
        (choices[0]=="Include" && choices[1]=="Exclude"));
    if(tile) {
        const float f=ImGui::GetFontSize(),width=ImGui::GetContentRegionAvail().x;
        const bool on=value>0 || s.key=="StartingStickCapacity" || s.key=="StartingNutCapacity";
        ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(on?ImGuiCol_Header:ImGuiCol_FrameBg));
        const auto start=ImGui::GetCursorScreenPos();
        if(ImGui::Button("##item",{width,4.1f*f})) {
            if(toggle) value=!value;else ImGui::OpenPopup("Choose starting item");
        }
        ImGui::PopStyleColor();Help(s,index);
        if(atlas) {
            const int id=Icon(s,value);const float x=(id%12)*42.f,y=(id/12)*42.f;
            const float side=3.3f*f;
            ImGui::GetWindowDrawList()->AddImage(atlas,{start.x+(width-side)*.5f,start.y+.4f*f},
                {start.x+(width+side)*.5f,start.y+3.7f*f},{(x+.5f)/512,(y+.5f)/512},
                {(x+41.5f)/512,(y+41.5f)/512},ImGui::GetColorU32(on?ImVec4{1,1,1,1}:ImVec4{.5f,.5f,.5f,.65f}));
        }
        if(ImGui::BeginPopup("Choose starting item")) {
            ImGui::TextUnformatted(s.label.c_str());ImGui::Separator();
            for(unsigned i=0;i<choices.size();++i) if(ImGui::Selectable(choices[i].c_str(),index==int(i))) value=int(i)+low;
            ImGui::EndPopup();
        }
        ImGui::TextWrapped("%s",s.label.c_str());
        if(!toggle) ImGui::TextWrapped("%s",choices[std::clamp(value-low,0,int(choices.size())-1)].c_str());
    } else if(toggle) {
        // Separate the box from its wrapped label so long names never run into another column.
        bool checked=value!=0;
        if(ImGui::Checkbox("##enabled",&checked)) value=checked;
        Help(s,index);ImGui::SameLine();
        ImGui::TextWrapped("%s",s.label.c_str());Help(s,index);
    } else {
        ImGui::TextWrapped("%s",s.label.c_str());Help(s,index);
        ImGui::SetNextItemWidth(-1);
        if(s.basic>=0 && !Specs[s.basic].choices) {
            ImGui::SliderInt("##value",&value,Specs[s.basic].low,Specs[s.basic].high,"%d",ImGuiSliderFlags_AlwaysClamp);
        } else {
            int first=0;
            const auto parsed=std::from_chars(choices.front().data(),choices.front().data()+choices.front().size(),first);
            bool numbers=parsed.ec==std::errc{} && parsed.ptr==choices.front().data()+choices.front().size();
            for(unsigned i=0;numbers && i<choices.size();++i) numbers=choices[i]==std::to_string(first+int(i));
            if(numbers) {
                int number=first+index;
                if(ImGui::SliderInt("##value",&number,first,first+int(choices.size())-1,"%d",ImGuiSliderFlags_AlwaysClamp))
                    value=number-first+low;
            } else if(ImGui::BeginCombo("##value",choices[index].c_str())) {
            for(unsigned i=0;i<choices.size();++i) {
                if(ImGui::Selectable(choices[i].c_str(),index==int(i))) value=int(i)+low;
                if(index==int(i)) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
            }
        }
        Help(s,index);
    }
    ImGui::EndDisabled();ImGui::PopID();
    if(value!=old) {Set(c,s,value);return true;}
    return false;
}
}
static bool DrawPresets(Config& config) {
    static std::string selected="Beginner",message,delete_name;
    static bool error=false;
    static std::array<char,97> name{};
    bool changed=false;
    const auto entries=Presets::List();
    auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.name==selected;});
    if(it==entries.end()){selected=entries.front().name;it=entries.begin();}
    ImGui::SeparatorText("Presets");
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##Preset",selected.c_str())) {
        ImGui::TextDisabled("Built-in");
        bool custom_heading=false;
        for(const auto& entry:entries) {
            if(!entry.built_in && !custom_heading) {ImGui::Separator();ImGui::TextDisabled("Custom");custom_heading=true;}
            ImGui::PushID(entry.name.c_str());
            if(ImGui::Selectable((entry.name+"###entry").c_str(),entry.name==selected)) {
                selected=entry.name;delete_name.clear();message.clear();
            }
            if(entry.name==selected)ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.name==selected;});
    ImGui::TextWrapped("%s",it->description.c_str());
    ImGui::BeginDisabled(busy.load());
    if(ImGui::Button("Apply preset")) {
        config=Presets::Apply(*it,config);changed=true;message="Preset applied to new-file settings.";error=false;
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Applies randomizer rules for new files. Seed text, Difficulty settings and multiplayer choices stay as set.");
    if(ImGui::TreeNode("Manage custom presets")) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Preset name","Name for current settings",name.data(),name.size());
        if(ImGui::Button("Save new preset")) {
            message=Presets::SaveNew(name.data(),config);error=!message.empty();
            if(!error) {
                selected=QString::fromUtf8(name.data()).simplified().toStdString();
                message="Preset added.";name.fill(0);delete_name.clear();
            }
        }
        ImGui::BeginDisabled(it->built_in);
        if(ImGui::Button("Update selected")) {
            message=Presets::Update(it->name,config);error=!message.empty();
            if(!error)message="Preset updated.";
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Replace the selected custom preset with your current randomizer settings.");
        ImGui::SameLine();
        if(ImGui::Button("Delete selected"))delete_name=it->name;
        ImGui::EndDisabled();
        if(!delete_name.empty()) {
            ImGui::TextWrapped("Delete %s?",delete_name.c_str());
            if(ImGui::Button("Delete preset")) {
                message=Presets::Remove(delete_name);error=!message.empty();
                if(!error){message="Preset deleted.";selected="Beginner";}
                delete_name.clear();
            }
            ImGui::SameLine();if(ImGui::Button("Keep preset"))delete_name.clear();
        }
        ImGui::TreePop();
    }
    ImGui::PushTextWrapPos();
    if(!message.empty())ImGui::TextColored(error?ImVec4{1,.35f,.35f,1}:ImVec4{.6f,.9f,.6f,1},"%s",message.c_str());
    const auto storage_error=Presets::StorageError();
    if(!storage_error.empty())ImGui::TextColored({1,.35f,.35f,1},"%s",storage_error.c_str());
    ImGui::PopTextWrapPos();
    return changed;
}

void DrawSettings(std::uint64_t icons) {
    auto config=GetConfig();bool changed=false;
    static int page=General;
    static int previous_page=-1;
    static ImGuiTextFilter filter;
    static bool only_changed=false;
    const float f=ImGui::GetFontSize();
    const auto matches=[&](const Setting& s) {
        const int initial=s.basic>=0?Specs[s.basic].initial:s.advanced->initial;
        if(only_changed && Value(config,s)==initial) return false;
        if(!filter.IsActive() && !only_changed && !Enabled(config,s) &&
           ((s.key.starts_with("Bridge") && s.key.ends_with("Count")) || s.key.starts_with("LACS") ||
            s.key.starts_with("TriforcePieces"))) return false;
        const auto text=s.label+" "+s.place.section+" "+PageNames[s.place.page]+" "+s.key;
        return filter.PassFilter(text.c_str());
    };
    changed|=ImGui::Checkbox("Enable randomizer for new files",&config.enabled);
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(page==Multiplayer?
        "Seed and sharing settings apply to new files.":
        "Changes apply to new files. Existing files keep their seed and rules.");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    bool search_changed=ImGui::InputTextWithHint("##Search randomizer","Search all randomizer settings...",filter.InputBuf,IM_ARRAYSIZE(filter.InputBuf));
    if(search_changed) filter.Build();
    search_changed|=ImGui::Checkbox("Only changed settings",&only_changed);
    const bool searching=filter.IsActive() || only_changed;
    std::array<unsigned,PageCount> counts{};
    for(const auto& s:Settings()) if(matches(s)) ++counts[s.place.page];
    if(searching && !counts[page]) {
        for(unsigned i=1;i<PageCount;++i) if(counts[i]) {page=int(i);break;}
    }
    ImGui::Separator();
    const bool sidebar=ImGui::GetContentRegionAvail().x>=32*f;
    const auto remaining=ImGui::GetContentRegionAvail();
    const float body_height=std::max(remaining.y,4*f);
    if(sidebar) {
        if(ImGui::BeginChild("Randomizer categories",{11.8f*f,body_height},ImGuiChildFlags_Borders)) {
            for(unsigned i=0;i<PageCount;++i) {
                if(searching && !counts[i]) continue;
                const auto name=std::string(PageNames[i])+(searching?" ("+std::to_string(counts[i])+")":"");
                if(ImGui::Selectable(name.c_str(),page==int(i),0,{0,1.6f*f})) page=int(i);
            }
        }
        ImGui::EndChild();ImGui::SameLine();
    } else {
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##Category",PageNames[page])) {
            for(unsigned i=0;i<PageCount;++i) {
                if(searching && !counts[i]) continue;
                if(ImGui::Selectable(PageNames[i],page==int(i))) page=int(i);
            }
            ImGui::EndCombo();
        }
    }
    const bool reveal_matches=searching && (search_changed || page!=previous_page);
    previous_page=page;
    ImGui::PushID(page);
    if(ImGui::BeginChild("Randomizer content",{0,sidebar?body_height:0},ImGuiChildFlags_None)) {
        ImGui::SeparatorText(PageNames[page]);
        if(page==General && !searching) {
            ImGui::TextWrapped("Choose your settings, then create a file normally. Selecting Original or Master Quest sets the dungeon layout for that entire file.");
            changed|=DrawPresets(config);
            ImGui::Spacing();ImGui::SeparatorText("Seed");
            std::array<char,129> seed{};
            std::memcpy(seed.data(),config.seed.data(),std::min(config.seed.size(),seed.size()-1));
            ImGui::SetNextItemWidth(-1);
            if(ImGui::InputTextWithHint("##Seed","Blank = random seed for each new file",seed.data(),seed.size())) {config.seed=seed.data();changed=true;}
            ImGui::TextWrapped("Use the same seed and settings to generate the same randomizer. Glitchless logic is the default.");
            if(const auto playing=CurrentConfig()) {
                ImGui::Spacing();ImGui::SeparatorText("Current file");
                ImGui::TextWrapped("Seed: %s",playing->seed.c_str());
                if(ImGui::TreeNode("View saved rules (read only)")) {
                    for(const auto& s:Settings()) {
                        const auto choices=Choices(s);const int low=s.basic>=0?Specs[s.basic].low:0;
                        const auto index=std::clamp(Value(*playing,s)-low,0,int(choices.size())-1);
                        ImGui::TextWrapped("%s: %s",s.label.c_str(),choices[index].c_str());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::Spacing();ImGui::SeparatorText("Difficulty");
            ImGui::TextWrapped("Set damage, healing and prices in the Difficulty tab before creating a file. Randomized files retain those settings.");
            ImGui::Spacing();ImGui::SeparatorText("Defaults");
            if(ImGui::Button("Reset randomizer settings...")) ImGui::OpenPopup("Reset randomizer settings?");
            if(ImGui::BeginPopup("Reset randomizer settings?")) {
                ImGui::TextUnformatted("Reset settings for future files? Existing saves are unaffected.");
                if(ImGui::Button("Reset")) {config=NewFileConfig();changed=true;ImGui::CloseCurrentPopup();}
                ImGui::SameLine();if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::Spacing();ImGui::TextDisabled("Based on gamestabled/OoT3D_Randomizer.");
        } else {
            if(page==Starting) ImGui::TextWrapped("Click an item to toggle it or choose its upgrade. Starting items are accounted for in the shuffled pool.");
            if(page==Locations) ImGui::TextWrapped("Check a location to exclude it from progression placement. Only locations available with your chosen shuffles can be excluded.");
            if(page==Tricks) ImGui::TextWrapped("Enable only tricks you can perform. No Logic does not guarantee a beatable seed.");
            if(page==Enemies) ImGui::TextWrapped("Master Quest logic supports boss-only souls, but not Enemizer or all-enemy souls.");
            if(page==Dungeons) ImGui::TextWrapped("Original or Master Quest is chosen when creating the file. Dungeon layouts are never mixed.");
            if(page==Multiplayer) {
                OOT3D::MultiplayerSettings::Draw();
                ImGui::Spacing();ImGui::SeparatorText("New-file sharing rules");
            }
            std::vector<std::string> sections;
            for(const auto& s:Settings()) if(s.place.page==page && matches(s) &&
                std::find(sections.begin(),sections.end(),s.place.section)==sections.end()) sections.push_back(s.place.section);
            if(page==Starting) {
                const std::array<std::string_view,7> order{"Equipment","Items","Songs","Dungeon rewards",
                    "Health and supplies","Ocarina buttons","Enemy souls"};
                std::stable_sort(sections.begin(),sections.end(),[&](const auto& a,const auto& b) {
                    return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);
                });
            }
            if(page==Locations) std::sort(sections.begin(),sections.end());
            if(sections.empty()) ImGui::TextWrapped("No matching settings. Clear the search or turn off Only changed settings.");
            // Keep coherent groups in columns on wide windows; stack them on smaller ones.
            const int columns=(page==Starting || page==Locations || page==Tricks)?1:
                std::clamp(int(ImGui::GetContentRegionAvail().x/(20*f)),1,3);
            std::vector<unsigned> destinations;
            std::array<unsigned,3> loads{};
            for(const auto& section:sections) {
                const unsigned destination=std::min_element(loads.begin(),loads.begin()+columns)-loads.begin();
                destinations.push_back(destination);loads[destination]+=3;
                for(const auto& s:Settings()) if(s.place.page==page && s.place.section==section && matches(s))
                    loads[destination]+=Choices(s).size()==2?1:2;
            }
            if(ImGui::BeginTable("Setting groups",columns,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_PadOuterX)) {
                for(int column=0;column<columns;++column) {
                    ImGui::TableNextColumn();
                    for(unsigned n=0;n<sections.size();++n) {
                        if(destinations[n]!=unsigned(column)) continue;
                        const auto& section=sections[n];ImGui::PushID(section.c_str());
                        const bool collapse=page==Locations || page==Tricks || section=="Enemy souls" || section=="Enemy pool";
                        bool open=true;
                        if(collapse) {
                            // Reveal new search results once; do not undo the
                            // user's collapse click on the following frame.
                            if(reveal_matches) ImGui::SetNextItemOpen(true,ImGuiCond_Always);
                            open=ImGui::CollapsingHeader(section.c_str());
                        } else {ImGui::Spacing();ImGui::SeparatorText(section.c_str());}
                        if(open) {
                            std::vector<const Setting*> tiles,fields;
                            for(const auto& s:Settings()) if(s.place.page==page && s.place.section==section && matches(s)) {
                                if(page==Starting && icons && Icon(s,Value(config,s))>=0) tiles.push_back(&s);else fields.push_back(&s);
                            }
                            if(!tiles.empty()) {
                                if(section=="Equipment" || section=="Items")
                                    std::stable_sort(tiles.begin(),tiles.end(),[&](const auto* a,const auto* b) {
                                        return Icon(*a,0)<Icon(*b,0);
                                    });
                                const int across=std::clamp(int(ImGui::GetContentRegionAvail().x/(6.5f*f)),1,10);
                                if(ImGui::BeginTable("Starting item grid",across,ImGuiTableFlags_SizingStretchSame)) {
                                    for(const auto* s:tiles) {ImGui::TableNextColumn();changed|=Control(config,*s,icons,true);}
                                    ImGui::EndTable();
                                }
                            }
                            for(const auto* s:fields) {changed|=Control(config,*s,icons,false);ImGui::Spacing();}
                            ImGui::Spacing();
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();ImGui::PopID();
    if(changed) SetConfig(config);
}
}
