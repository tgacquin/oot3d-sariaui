// SPDX-License-Identifier: GPL-2.0-or-later
// Exercises actual ImGui popup hit-testing through the frontend pointer gate.
// No renderer, game, live settings or save files are opened.
#include "mm3d/overlay_pointer.h"
#include <cassert>
#include <iostream>

int main() {
    for(float scale:{1.f,1.5f,2.f})for(bool choose_quit:{false,true}) {
        ImGui::CreateContext();
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.DisplaySize={1280*scale,720*scale};io.DeltaTime=1.f/60;
        io.FontGlobalScale=scale;ImGui::GetStyle().ScaleAllSizes(scale);
        unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        MM3D::OverlayPointer capture;bool quit=false;
        ImVec2 quit_button{},cancel_button{};
        const auto frame=[&](bool open=false) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({20*scale,20*scale});
            ImGui::SetNextWindowSize({220*scale,600*scale});
            ImGui::Begin("Dekui settings (F1)");
            capture.panel={ImGui::GetWindowPos(),ImGui::GetWindowSize()};
            if(open)ImGui::OpenPopup("Quit Dekui?");
            ImGui::SetNextWindowPos({640*scale,360*scale},ImGuiCond_Always,{.5f,.5f});
            if(ImGui::BeginPopupModal("Quit Dekui?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Save your progress in the game before quitting.");
                if(ImGui::Button("Quit")){quit=true;ImGui::CloseCurrentPopup();}
                auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
                quit_button={(a.x+b.x)/2,(a.y+b.y)/2};
                ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
                a=ImGui::GetItemRectMin();b=ImGui::GetItemRectMax();
                cancel_button={(a.x+b.x)/2,(a.y+b.y)/2};
                ImGui::EndPopup();
            }
            ImGui::End();ImGui::Render();
            capture.popup_open=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
        };
        frame(true);frame();frame();assert(capture.popup_open);
        const auto target=choose_quit?quit_button:cancel_button;
        assert(!capture.panel.Contains(target.x,target.y));
        const auto click=[&](bool popup_routing) {
            for(bool down:{true,false}) {
                if(!popup_routing)capture.popup_open=false; // Reproduce the old panel-only gate.
                const bool consumed=capture.Consume(target.x,target.y,0,down,true);
                assert(consumed==popup_routing);
                io.AddMousePosEvent(target.x,target.y);
                if(consumed)io.AddMouseButtonEvent(0,down);
                frame();
            }
            frame();
        };
        click(false);assert(capture.popup_open && !quit);
        click(true);assert(!capture.popup_open && quit==choose_quit);
        // No modal means gameplay clicks remain outside the overlay. Preserve
        // drag ownership when a pointer moves across the panel boundary.
        assert(!capture.Consume(1000*scale,600*scale,0,true,true));
        assert(!capture.Consume(30*scale,30*scale,0,false,true));
        assert(capture.Consume(30*scale,30*scale,0,true,true));
        assert(capture.Consume(1000*scale,600*scale,0,false,true));
        capture.popup_open=true;
        assert(!capture.Consume(target.x,target.y,0,true,false));
        assert(!capture.Consume(target.x,target.y,0,false,false));
        ImGui::DestroyContext();
    }
    std::cout<<"Real ImGui Quit/Cancel outside settings panel: old input loss reproduced; modal routing, scales, hidden overlay and drag ownership passed\n";
}
