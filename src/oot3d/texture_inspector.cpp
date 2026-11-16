// Copyright 2026 MM3D presentation contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/texture_inspector.h"
#include "oot3d/texture_picker.h"
#include "common/file_util.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <algorithm>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace OOT3D {
struct TextureInspector::Impl {
    const Vulkan::Instance& instance;
    std::mutex& submit_mutex;
    vk::Sampler sampler;
    std::vector<std::shared_ptr<Textures::Thumbnail>> thumbnails;
    std::vector<VkDescriptorSet> sets;
    std::shared_ptr<Textures::Thumbnail> detail;
    VkDescriptorSet detail_set{};
    int atlas = -1, columns = 1, rows = 1;
    float zoom = 1;
    bool atlas_open = false;
    std::vector<int> cells;
    std::string status;
    QString directory = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir)) + "oot3d-captures/";
    Impl(const Vulkan::Instance& value, std::mutex& mutex) : instance(value), submit_mutex(mutex) {
        sampler = instance.GetDevice().createSampler({.magFilter=vk::Filter::eNearest,
            .minFilter=vk::Filter::eNearest, .mipmapMode=vk::SamplerMipmapMode::eNearest,
            .addressModeU=vk::SamplerAddressMode::eClampToEdge,
            .addressModeV=vk::SamplerAddressMode::eClampToEdge,
            .addressModeW=vk::SamplerAddressMode::eClampToEdge});
    }
    void Drain() {
        std::scoped_lock lock{submit_mutex, instance.PresentQueueMutex()};
        instance.GetDevice().waitIdle();
    }
    void Release() {
        for (auto set : sets) ImGui_ImplVulkan_RemoveTexture(set);
        sets.clear(); thumbnails.clear();
        if (detail_set) ImGui_ImplVulkan_RemoveTexture(detail_set);
        detail_set = {}; detail.reset();
    }
    ~Impl() { Release(); Textures::Clear(); instance.GetDevice().destroySampler(sampler); }
    void Save() {
        QJsonArray textures;
        for (const auto& item : thumbnails) {
            if (!Textures::IsSelected(item->key)) continue;
            QJsonObject object{{"key", QString::number(qulonglong(item->key),16)},
                {"width",int(item->width)}, {"height",int(item->height)}};
            if (atlas >= 0 && thumbnails[atlas] == item) {
                QJsonArray selected;
                for (int cell : cells) selected.append(cell);
                object.insert("columns",columns); object.insert("rows",rows);
                object.insert("cells",selected);
            }
            textures.append(object);
        }
        QDir().mkpath(directory);
        QSaveFile file(directory + "texture-selection.json");
        const auto bytes = QJsonDocument(QJsonObject{{"textures",textures}}).toJson();
        const bool ok = file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
        status = ok ? "Saved texture-selection.json" : "Could not save selection";
    }
};
TextureInspector::TextureInspector(const Vulkan::Instance& instance, std::mutex& mutex)
    : impl(std::make_unique<Impl>(instance,mutex)) {}
TextureInspector::~TextureInspector() = default;
void TextureInspector::Draw() {
    auto& s = *impl;
    if (ImGui::Button("Capture textures")) Textures::Start();
    ImGui::SameLine();
    if (ImGui::Button("Restore draws")) Textures::Restore();
    ImGui::SameLine();
    if (ImGui::Button("Clear captures")) {
        Textures::Clear(); s.Drain(); s.Release(); s.atlas=-1; s.atlas_open=false; s.cells.clear();
    }
    const auto captured = Textures::Snapshot();
    while (s.thumbnails.size() < captured.size()) {
        const auto& item=captured[s.thumbnails.size()];
        s.sets.push_back(ImGui_ImplVulkan_AddTexture(s.sampler,item->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        s.thumbnails.push_back(item);
    }
    ImGui::Text("Textures: %u / %u | hidden draws: %u",unsigned(s.thumbnails.size()),Textures::Limit,Textures::hidden.load());
    ImGui::TextUnformatted(InspectionMilliseconds()<Textures::capture_until.load() ? "Capturing (C)" : "C: capture");
    ImGui::TextWrapped("Click thumbnails to select textures. Hover for a larger preview. Capture runs for five seconds of emulation; resume if paused.");
    if (ImGui::BeginChild("Texture thumbnails",{0,235*ImGui::GetIO().FontGlobalScale},ImGuiChildFlags_Borders)) {
        for (unsigned i=0;i<s.thumbnails.size();++i) {
            const auto& item=s.thumbnails[i];
            ImGui::PushID(int(i));
            if (i%3) ImGui::SameLine();
            ImGui::BeginGroup();
            float size=112*ImGui::GetIO().FontGlobalScale/std::max(item->width,item->height);
            const bool selected=Textures::IsSelected(item->key);
            if(selected) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4{.15f,.55f,.3f,1});
            if(ImGui::ImageButton("texture",(ImTextureID)s.sets[i],{item->width*size,item->height*size},
                {0,1},{1,0},{.25f,.25f,.25f,1})) Textures::ToggleSelection(item->key);
            if(selected) ImGui::PopStyleColor();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::BeginTooltip();
                size=320.f/std::max(item->width,item->height);
                ImGui::Image((ImTextureID)s.sets[i],{item->width*size,item->height*size},{0,1},{1,0});
                ImGui::Text("Texture #%u | %ux%u",i,item->width,item->height);
                ImGui::EndTooltip();
            }
            ImGui::Text("#%u %ux%u%s",i,item->width,item->height,selected?" *":"");
            if(ImGui::Button("Edit grid")) {
                if(s.atlas!=int(i)) {s.cells.clear();s.columns=s.rows=1;s.zoom=1;}
                s.atlas=int(i);s.atlas_open=true;
                if(!selected)Textures::ToggleSelection(item->key);
                Textures::RequestDetail(item->key,"");
            }
            ImGui::EndGroup(); ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::Text("Selected: %u",Textures::SelectionCount());
    ImGui::BeginDisabled(Textures::SelectionCount()==0);
    if(ImGui::Button("Hide selected textures for 5 seconds")) Textures::HideSelected();
    ImGui::SameLine();
    if(ImGui::Button("Save selection")) s.Save();
    if(ImGui::Button("Record selected draws")) {
        QDir().mkpath(s.directory);
        Textures::StartRecording((s.directory+"texture-bindings.log").toStdString(),"selected");
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",Textures::RecordingStatus().c_str());
    ImGui::TextWrapped("Temporary hiding follows the captured texture address. Recapture after changing scenes. Selection files contain metadata only.");
    ImGui::TextWrapped("%s",s.status.c_str());
    if(s.atlas_open && s.atlas>=0 && size_t(s.atlas)<s.thumbnails.size()) {
        ImGui::SetNextWindowSize({750,650},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Atlas grid editor",&s.atlas_open)) {
            const auto detail=Textures::Detail();
            if(s.detail!=detail) {
                s.Drain();
                if(s.detail_set) ImGui_ImplVulkan_RemoveTexture(s.detail_set);
                s.detail_set={}; s.detail=detail;
                if(detail)s.detail_set=ImGui_ImplVulkan_AddTexture(s.sampler,detail->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            if(!detail) ImGui::TextWrapped("Waiting for full-resolution capture. Resume while this texture is visible.");
            else {
                bool changed=ImGui::SliderInt("Columns",&s.columns,1,64,"%d",ImGuiSliderFlags_NoInput);
                changed|=ImGui::SliderInt("Rows",&s.rows,1,64,"%d",ImGuiSliderFlags_NoInput);
                if(changed)s.cells.clear();
                ImGui::SliderFloat("Zoom",&s.zoom,.25f,8.f,"%.2fx",ImGuiSliderFlags_NoInput);
                if(ImGui::Button("Clear cells")) s.cells.clear();
                ImGui::SameLine(); if(ImGui::Button("Save selection"))s.Save();
                ImGui::Text("%ux%u | %u cells selected",detail->width,detail->height,unsigned(s.cells.size()));
                if(ImGui::BeginChild("Atlas canvas",{0,0},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar)) {
                    const auto origin=ImGui::GetCursorScreenPos();
                    const ImVec2 size{detail->width*s.zoom,detail->height*s.zoom};
                    ImGui::Image((ImTextureID)s.detail_set,size,{0,1},{1,0});
                    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                        const auto mouse=ImGui::GetIO().MousePos;
                        const int col=std::clamp(int((mouse.x-origin.x)/size.x*s.columns),0,s.columns-1);
                        const int row=std::clamp(int((mouse.y-origin.y)/size.y*s.rows),0,s.rows-1);
                        const int cell=row*s.columns+col;
                        auto it=std::find(s.cells.begin(),s.cells.end(),cell);
                        if(it==s.cells.end())s.cells.push_back(cell);else s.cells.erase(it);
                    }
                    auto* draw=ImGui::GetWindowDrawList();
                    for(int cell:s.cells) {
                        ImVec2 a{origin.x+(cell%s.columns)*size.x/s.columns,origin.y+(cell/s.columns)*size.y/s.rows};
                        draw->AddRectFilled(a,{a.x+size.x/s.columns,a.y+size.y/s.rows},IM_COL32(70,220,100,90));
                    }
                    for(int c=0;c<=s.columns;++c)draw->AddLine({origin.x+c*size.x/s.columns,origin.y},{origin.x+c*size.x/s.columns,origin.y+size.y},IM_COL32(255,255,255,110));
                    for(int r=0;r<=s.rows;++r)draw->AddLine({origin.x,origin.y+r*size.y/s.rows},{origin.x+size.x,origin.y+r*size.y/s.rows},IM_COL32(255,255,255,110));
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}
}
