// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/remote_names_geometry.h"
#include <cassert>
#include <iostream>
using namespace OOT3D;
int main() {
    namespace N=RemoteNames;using namespace Interpolation;
    const auto near=[](double a,double b){return std::abs(a-b)<.00001;};
    MultiplayerStartup::SetSession(1,"Local");
    CameraSample a;a.valid=a.projections_valid=true;a.owner=1;a.epoch=1;a.time=100000;
    a.view=Math::Identity();
    Matrix p{};p[0][1]=1;p[1][0]=-1;p[2][2]=1;p[2][3]=1;p[3][2]=-1;a.projections[0]=p;
    N::Capture(2,{"Link (1)",{0,0,-100}},a);
    auto b=a;b.time+=33333;
    N::Capture(2,{"Link (1)",{2,0,-100}},b);
    auto names=N::World(b,.5f,Math::Identity());assert(names.anchors.size()==1 && near(names.anchors[0].position[0],1));
    names=N::World(b,1,Math::Identity());assert(names.anchors.size()==1 && near(names.anchors[0].position[0],2));
    auto stale=b;stale.time+=33333;assert(N::World(stale,1,Math::Identity()).anchors.empty());
    MultiplayerStartup::SetSession(0);assert(N::World(b,1,Math::Identity()).anchors.empty());
    MultiplayerStartup::SetSession(1,"Local");N::Reset();assert(N::World(b,1,Math::Identity()).anchors.empty());
    auto font=N::MakeFont();assert(font.width && font.height && !font.pixels.empty());
    assert(font.pixels.size()<=4*1024*1024 && font.Find('A') && font.Find(0xe9));
    assert(N::Characters("Link \xc3\xa9").back()==0xe9);
    assert(N::Characters("\xff").front()=='?');
    N::Scene scene;scene.camera=b;scene.projection=p;scene.inverse_view=Math::Identity();
    N::Layout layout;layout.shadow=false;layout.backdrop=false;
    N::WorldLabels labels{{{"Link",{0,0,-100}}},Math::Identity()};
    const auto close=N::Geometry(font,labels,scene,layout);assert(close.size()==24);
    // Landscape presentation swaps the rotated framebuffer axes: displayed X
    // increases with clip Y. Both glyph order and glyph UVs must run rightward.
    const auto screen_x=[](const auto& v){return v.position[1]/v.position[3];};
    assert(screen_x(close[2])>screen_x(close[0]) && close[2].uv[0]>close[0].uv[0]);
    assert(screen_x(close[6])>screen_x(close[0]));
    // Regular and mirrored projections must both keep glyphs and letter order
    // readable, including a clip-axis flip. The anchor still follows the world.
    for(double projection_sign:{-1.,1.})for(float clip_sign:{-1.f,1.f}) {
        auto mirrored=scene;mirrored.projection[1][0]=projection_sign;mirrored.clip_y=clip_sign;
        auto offset_labels=labels;offset_labels.anchors[0].position[0]=12;
        const auto text=N::Geometry(font,offset_labels,mirrored,layout);
        assert(text.size()==close.size());
        for(unsigned glyph=0;glyph<text.size();glyph+=6) {
            assert(screen_x(text[glyph+2])>screen_x(text[glyph]));
            assert(text[glyph+2].uv[0]>text[glyph].uv[0]);
            if(glyph)assert(screen_x(text[glyph])>screen_x(text[glyph-6]));
        }
        const auto centered=N::Geometry(font,labels,mirrored,layout);
        for(unsigned i=0;i<text.size();++i)
            assert(near(screen_x(text[i])-screen_x(centered[i]),projection_sign*clip_sign*12/100));
    }
    const auto height=[](const auto& g){return std::abs(g[0].position[0]/g[0].position[3]-g[1].position[0]/g[1].position[3]);};
    labels.anchors[0].position[2]=-200;
    const auto far=N::Geometry(font,labels,scene,layout);assert(near(height(close),2*height(far)));
    labels.anchors[0].position[2]=10;assert(N::Geometry(font,labels,scene,layout).empty());
    labels.anchors[0].position[2]=-1300;assert(N::Geometry(font,labels,scene,layout).empty());
    labels.anchors[0].position={10,0,-100};scene.inverse_view[0][3]=10;labels.billboard=scene.inverse_view;
    const auto moved=N::Geometry(font,labels,scene,layout);
    for(unsigned i=0;i<close.size();++i)for(unsigned c=0;c<4;++c)assert(near(close[i].position[c],moved[i].position[c]));
    // A 90-degree camera turn keeps a camera-facing label at the same apparent
    // size and position when its world anchor is rotated with the camera.
    scene.inverse_view=Math::Identity();scene.inverse_view[0][0]=scene.inverse_view[2][2]=0;
    scene.inverse_view[0][2]=1;scene.inverse_view[2][0]=-1;
    labels.billboard=scene.inverse_view;labels.anchors[0].position={-100,0,0};
    const auto rotated=N::Geometry(font,labels,scene,layout);
    for(unsigned i=0;i<close.size();++i)for(unsigned c=0;c<4;++c)assert(near(close[i].position[c],rotated[i].position[c]));
    layout.shadow=true;assert(N::Geometry(font,labels,scene,layout).size()==close.size()*10);
    layout.shadow=false;layout.backdrop=true;
    const auto backed=N::Geometry(font,labels,scene,layout);
    assert(backed.size()==close.size()+8*32*6);
    float max_alpha=0;bool transparent_edge=false;
    for(unsigned i=0;i<backed.size()-close.size();++i) {
        const auto& v=backed[i];assert(v.color[0]==0 && v.color[1]==0 && v.color[2]==0);
        max_alpha=std::max(max_alpha,v.color[3]);transparent_edge|=v.color[3]==0;
        const unsigned x=unsigned(v.uv[0]*font.width),y=unsigned(v.uv[1]*font.height);
        assert(font.pixels[4*(y*font.width+x)+3]==255);
    }
    assert(near(max_alpha,layout.backdrop_opacity) && transparent_edge);
    for(unsigned i=0;i<close.size();++i) {
        const auto& text=backed[backed.size()-close.size()+i];
        assert(text.position==rotated[i].position && text.uv==rotated[i].uv);
    }
    layout.backdrop_opacity=0;assert(N::Geometry(font,labels,scene,layout).size()==close.size());
    layout.enabled=false;assert(N::Geometry(font,labels,scene,layout).empty());
    // Camera cuts never blend an old remote position into the new scene.
    N::Reset();N::Capture(2,{"Remote",{0,0,-100}},a);b.scene=a.scene+1;
    N::Capture(2,{"Remote",{20,0,-100}},b);
    assert(N::World(b,.5f,Math::Identity()).anchors.front().position[0]==20);
    std::cout<<"Remote names: HD atlas "<<font.width<<"x"<<font.height<<", world projection, distance scaling, camera turns, interpolation, cuts, shadow, stale frames and disconnect passed\n";
}
