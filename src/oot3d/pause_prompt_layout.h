// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include "oot3d/control_swap.h"
#include "oot3d/ui_language.h"

namespace Core { class System; }
namespace OOT3D::PausePrompts {
struct Pose {float x=0,y=0,scale=1;};
struct Prompt {Pose group{},button{};};
struct Row {Pose pose{};float spacing=8;};
struct NavigationShadow {
    bool enabled=true;
    float x=0,y=0,width=64,height=100,opacity=.45f,softness=.7f;
};
enum PromptIndex { Choose, Return, MapMove, MapReturn, EquipI, EquipII, ChangeTab, ItemsToMap, MapToItems, PromptCount };
constexpr unsigned ZLText=PromptCount, ZRText=ZLText+1, SlashText=ZRText+1;
struct Layout {
    bool enabled=true;
    Row lower{};
    float scale=1,icon_scale=1,letter_spacing=1;
    float navigation_distance=4, navigation_speed=1; // Total travel in pixels; cycles per second.
    NavigationShadow map_navigation_shadow{};
    std::array<Prompt,PromptCount> prompts{};
};
constexpr std::array Names{"choose","return","map_move","map_return","equip_i","equip_ii","change_tab","items_to_map","map_to_items"};
constexpr std::array Labels{"Items / Gear: A Choose","Items / Gear: B Return",
                            "Map: Circle Pad Move cursor","Map: B Return","Items / Gear: ZL Equip to I",
                            "Items / Gear: ZR Equip to II","Items / Gear: L / R Change tab",
                            "Items: R / arrow / Map","Map: arrow / L / Items"};
inline const char* Label(unsigned i) {
    if(ControlSwap::shoulders.load()) {
        switch(i) {
        case EquipI:return "Items / Gear: L Equip to I";
        case EquipII:return "Items / Gear: R Equip to II";
        case ChangeTab:return "Items / Gear: ZL / ZR Change tab";
        case ItemsToMap:return "Items: ZR / arrow / Map";
        case MapToItems:return "Map: arrow / ZL / Items";
        }
    }
    return Labels[i];
}
constexpr unsigned FileSelectText=SlashText+1, FileBackText=FileSelectText+1;
constexpr unsigned SongsText=FileBackText+1, CancelText=SongsText+1;
constexpr unsigned OkayText=CancelText+1;
constexpr std::array<std::string_view,17> Text{"Choose","Return","Move cursor","Return",
    "- Equip to I","- Equip to II","Change tab","Map","Items","ZL","ZR","/","Select","Back","Songs","Cancel","OK"};
inline constexpr std::array<std::array<std::u32string_view,17>,3> LocalizedText{{
    {U"Choose",U"Return",U"Move cursor",U"Return",U"- Equip to I",U"- Equip to II",
     U"Change tab",U"Map",U"Items",U"ZL",U"ZR",U"/",U"Select",U"Back",U"Songs",U"Cancel",U"OK"},
    {U"Choisir",U"Retour",U"Déplacer",U"Retour",U"- Équiper I",U"- Équiper II",
     U"Changer d'onglet",U"Carte",U"Objets",U"ZL",U"ZR",U"/",U"Choisir",U"Retour",U"Chants",U"Annuler",U"OK"},
    {U"Elegir",U"Volver",U"Mover cursor",U"Volver",U"- Asignar a I",U"- Asignar a II",
     U"Cambiar pestaña",U"Mapa",U"Objetos",U"ZL",U"ZR",U"/",U"Elegir",U"Atrás",U"Canciones",U"Cancelar",U"Aceptar"}
}};
inline std::mutex mutex;
inline Layout layout{};
inline std::atomic<bool> dirty{false};
inline Layout GetLayout() {std::scoped_lock lock{mutex};return layout;}
inline void SetLayout(const Layout& value,bool save=true) {
    std::scoped_lock lock{mutex};layout=value;if(save)dirty=true;
}
constexpr unsigned IconWidth=256,IconHeight=128,TextWidth=256,TextHeight=512,TextRowHeight=24;
struct Assets {
    struct Glyph {unsigned x=0,width=0,code=0;};
    // Keep the native RGBA4 atlas byte-for-byte. The renderer's existing hash
    // lookup then applies custom texture packs, including runtime toggles.
    std::array<std::uint32_t,IconWidth*IconHeight/2> icons{};
    std::array<std::uint32_t,TextWidth*TextHeight> text{};
    std::array<std::uint32_t,128*128/2> cursor{};
    bool cursor_ready=false;
    std::array<unsigned,Text.size()> widths{};
    std::array<std::array<Glyph,24>,Text.size()> glyphs{};
    std::array<unsigned,Text.size()> lengths{};
    unsigned native_spacing=2;
};
inline Common::AtomicSharedPtr<const Assets> assets;
inline auto GetAssets() {return assets.load();}
void Load(Core::System& system);
}
