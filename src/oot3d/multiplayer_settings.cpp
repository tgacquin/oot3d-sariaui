// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/multiplayer_settings.h"
#include "oot3d/multiplayer_startup.h"
#include "oot3d/randomizer/activity.h"
#include "oot3d/randomizer/service.h"
#include "oot3d/remote_names_layout.h"
#include <array>
#include <cstring>
#include <imgui.h>

namespace OOT3D::MultiplayerSettings {
void DrawNameSettings() {
    ImGui::PushID("Multiplayer names");
    auto p=RemoteNames::GetLayout();bool changed=false;
    changed|=ImGui::Checkbox("Show names",&p.enabled);
    changed|=ImGui::SliderFloat("X offset",&p.x,-300,300,"%.1f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Y offset",&p.y,-200,200,"%.1f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Scale",&p.scale,.5f,3,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Opacity",&p.opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::Checkbox("Shadow",&p.shadow);
    changed|=ImGui::SliderFloat("Shadow strength",&p.shadow_strength,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Shadow softness",&p.shadow_softness,0,6,"%.1f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Shadow X",&p.shadow_x,-12,12,"%.1f",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Shadow Y",&p.shadow_y,-12,12,"%.1f",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::TreeNode("Soft backdrop")) {
        changed|=ImGui::Checkbox("Enabled",&p.backdrop);
        changed|=ImGui::SliderFloat("Opacity",&p.backdrop_opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Width",&p.backdrop_width,.25f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Height",&p.backdrop_height,.25f,4,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Softness",&p.backdrop_softness,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("X offset",&p.backdrop_x,-20,20,"%.1f",ImGuiSliderFlags_AlwaysClamp);
        changed|=ImGui::SliderFloat("Y offset",&p.backdrop_y,-20,20,"%.1f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TreePop();
    }
    ImGui::TextDisabled("Positive Y moves names down. Names shrink with distance and hide behind walls.");
    if(ImGui::Button("Reset names")){p={};changed=true;}
    if(changed)RemoteNames::SetLayout(p);
    ImGui::PopID();
}

void Draw() {
    const auto [saved,state]=Get();
    ImGui::Text("%s",state.role==1?"Hosting":state.role==2?"Connected":state.busy?"Working...":"Not connected");
    if(state.role) {
        ImGui::Text("%s | %u player%s",state.nickname.c_str(),state.players,state.players==1?"":"s");
        if(state.port)ImGui::Text("Room port: %u",state.port);
    }
    ImGui::TextWrapped("Host or join a randomizer game from file select. Your file name is your multiplayer name.");
    ImGui::BeginDisabled(!state.can_disconnect);
    if(ImGui::Button(state.role==1?"Stop hosting":"Disconnect"))disconnect_requested=true;
    ImGui::EndDisabled();
    if(state.busy)ImGui::TextWrapped("Use Back on file select to cancel joining. Settings unlock after leaving the room.");
    if(state.role && MultiplayerStartup::phase==MultiplayerStartup::Phase::Failed) {
        ImGui::TextWrapped("Game synchronization failed.");
        if(ImGui::Button("Retry synchronization"))Randomizer::multiplayer_command=1;
    }
    ImGui::Separator();
    bool color_enabled=Randomizer::player_color_enabled.load();
    if(ImGui::Checkbox("Use player tunic color",&color_enabled)) {
        Randomizer::player_color_enabled=color_enabled;Randomizer::dirty=true;
    }
    const auto packed=Randomizer::player_color.load();
    float rgb[]{float((packed>>16)&255)/255,float((packed>>8)&255)/255,float(packed&255)/255};
    if(ImGui::ColorEdit3("Tunic color",rgb,ImGuiColorEditFlags_NoOptions)) {
        const auto byte=[](float v){return std::uint32_t(std::clamp(v,0.f,1.f)*255+.5f);};
        Randomizer::player_color=(byte(rgb[0])<<16)|(byte(rgb[1])<<8)|byte(rgb[2]);
        Randomizer::player_color_enabled=true;Randomizer::dirty=true;
    }
    ImGui::TextWrapped("Other players see this color too. Tunic abilities stay the same.");
    bool log=Randomizer::Activity::enabled.load();
    if(ImGui::Checkbox("Show activity log",&log)) {
        Randomizer::Activity::enabled=log;Randomizer::Activity::settings_dirty=true;
    }
    auto names=RemoteNames::GetLayout();
    if(ImGui::Checkbox("Show multiplayer names",&names.enabled))RemoteNames::SetLayout(names);
    if(ImGui::CollapsingHeader("Advanced connection settings")) {
        static Config draft,last;
        static bool initialized=false,edited=false;
        if(!initialized || (!edited && saved!=last)) {draft=saved;last=saved;initialized=true;}
        const auto field=[&](const char* label,std::string& value) {
            std::array<char,201> text{};
            std::memcpy(text.data(),value.data(),std::min(value.size(),text.size()-1));
            if(ImGui::InputText(label,text.data(),text.size())) {value=text.data();edited=true;}
        };
        ImGui::BeginDisabled(state.role || state.busy);
        field("Room name (optional)",draft.room_name);
        edited|=ImGui::SliderInt("Maximum players",&draft.max_players,2,16);
        edited|=ImGui::InputInt("Room port",&draft.room_port,0);
        ImGui::TextWrapped("The default room port is 24872. If busy, hosting tries the next available port.");
        edited|=ImGui::Checkbox("Search local network",&draft.search_lan);
        field("Host IPv4 address (optional)",draft.host_address);
        ImGui::TextWrapped("Leave the address empty for local play. For Tailscale, enter the host's Tailscale IPv4 address. Join still checks the room ID and file.");
        edited|=ImGui::InputInt("Discovery port",&draft.discovery_port,0);
        ImGui::TextWrapped("Use the same discovery port on both computers (default 24873). Local-machine discovery stays available.");
        if(ImGui::Button("Apply connection settings")) {Edit(draft);edited=false;last=draft;}
        ImGui::SameLine();
        if(ImGui::Button("Revert")) {draft=saved;last=saved;edited=false;}
        ImGui::EndDisabled();
        if(!state.error.empty())ImGui::TextWrapped("%s",state.error.c_str());
    }
}
}
