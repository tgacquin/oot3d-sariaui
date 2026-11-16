// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/pause_color_settings.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <map>
using namespace OOT3D::PauseMenu;
struct Memory {
    std::map<U32,U32> words;
    std::optional<U32> Read(U32 at) {
        auto i=words.find(at);return i==words.end()?std::nullopt:std::optional{i->second};
    }
};
int main() {
    QTemporaryDir dir;assert(dir.isValid());
    const QString path=dir.filePath("colors.ini");
    Layout legacy;
    legacy.header_overlay={{.56f,.67f,.26f},1};
    legacy.slots_overlay={{.59f,.76f,.48f},.8f};
    legacy.header_y=61;legacy[Map]={20,30,.9f};
    ColorScheme first{legacy.header_overlay,legacy.header_overlay,legacy.header_overlay,legacy.slots_overlay};
    Layout current=legacy;
    {
        QSettings settings(path,QSettings::IniFormat);
        settings.setValue("Unrelated/value",42);
        LoadColorLayouts(settings,current);
        assert(current.color_layouts.active==0 && current.color_layouts.working==first);
        assert(current.color_layouts.saved[0]==first && current.color_layouts.saved[1]==first);
        assert(current.header_y==61 && current[Map].x==20);
        auto& colors=current.color_layouts;
        colors.Select(1);
        for(unsigned i=0;i<4;++i)colors.working[i]={{.1f*i,.2f*i,.3f*i},.25f*i};
        assert(colors.Unsaved() && colors.saved[0]==first && colors.saved[1]==first);
        // An unrelated layout autosave must not turn a preview into a saved preset.
        SaveColorLayouts(settings,current);settings.sync();
        auto reload=legacy;LoadColorLayouts(settings,reload);
        assert(reload.color_layouts.active==1 && reload.color_layouts.working==first);
        colors.Save();assert(!colors.Unsaved());
        const auto second=colors.working;
        colors.Select(0);assert(colors.working==first);
        colors.Select(1);assert(colors.working==second);
        colors.working[0].opacity=1;assert(colors.Unsaved());
        colors.Select(0);colors.Select(1);assert(colors.working==second);
        colors.Select(5);assert(colors.active==1);
        SaveColorLayouts(settings,current);settings.sync();
        assert(settings.value("Unrelated/value").toInt()==42);
    }
    {
        QSettings settings(path,QSettings::IniFormat);
        auto reload=legacy;LoadColorLayouts(settings,reload);
        assert(reload.color_layouts.saved==current.color_layouts.saved);
        assert(reload.color_layouts.active==1 && reload.color_layouts.working==current.color_layouts.saved[1]);
        Memory m;
        assert(BannerColor(m,reload)==reload.color_layouts.working[0]);
        m.words[0x5066f8+0x34]=2;
        assert(BannerColor(m,reload)==reload.color_layouts.working[1]);
        m.words[0x5066f8+0x34]=0;m.words[0x50446c+0x18]=2;
        assert(BannerColor(m,reload)==reload.color_layouts.working[2]);
        assert(Color(reload,PauseColor::SlotBackground)==reload.color_layouts.working[3]);
        reload.color_layouts.Select(0);SaveColorLayouts(settings,reload);settings.sync();
        LoadColorLayouts(settings,reload);assert(reload.color_layouts.working==first);
        settings.setValue("PauseScreen/color_layouts/active",-42);
        settings.setValue("PauseScreen/color_layouts/layout_1/map_banner/red","invalid");
        settings.setValue("PauseScreen/color_layouts/layout_1/map_banner/opacity",4);
        LoadColorLayouts(settings,reload);
        assert(reload.color_layouts.active==0);
        assert(reload.color_layouts.working[0].color[0]==legacy.header_overlay.color[0]);
        assert(reload.color_layouts.working[0].opacity==1);
    }
    std::cout<<"Color layouts: legacy migration, all four page colors, save/switch/reload, unsaved preview isolation, unrelated settings and invalid input passed\n";
}
