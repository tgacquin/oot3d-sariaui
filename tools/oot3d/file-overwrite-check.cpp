// SPDX-License-Identifier: GPL-2.0-or-later
#include "oot3d/file_select.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
namespace F=OOT3D::FileSelect;
using U32=std::uint32_t;
struct Memory {
 std::map<U32,U32> words;
 std::optional<U32> Read(U32 a) {auto i=words.find(a);return i==words.end()?std::nullopt:std::optional<U32>{i->second};}
 template<class T> void Put(U32 a,const T& data){for(unsigned i=0;i<data.size();++i)words[a+4*i]=std::bit_cast<U32>(data[i]);}
};
int main(int argc,char**argv){
 assert(argc==2);Memory m;
 std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> code{std::istreambuf_iterator<char>(f),{}};
 for(unsigned i=0;i+3<code.size();i+=4)m.words[0x100000+i]=OOT3D::MapFrame::LE32(code,i);
 const U32 model=0x8100000,mesh=0x8110000,helper=0x8120000,p=0x8130000,uv=0x8140000,color=0x8150000;
 m.words[F::Owner+8]=model;m.words[F::Owner+4]=mesh;m.words[model]=0x4ebd60;m.words[model+4]=mesh;
 m.words[F::Owner+0x30]=helper;m.words[helper]=F::QuadCount;
 m.words[helper+0x10]=p;m.words[helper+0x14]=uv;m.words[helper+0x18]=color;
 F::Geometry raw;
 for(unsigned q=0;q<F::QuadCount;++q){
  const float x=std::bit_cast<float>(*m.Read(0x504ff8+q*8)),y=std::bit_cast<float>(*m.Read(0x504ff8+q*8+4));
  raw.Add(x,y,8,48,0,1,1,0);
 }
 // Normal faces are at rest, pressed variants are independently parked.
 for(unsigned q:{152u,153u,154u,159u,160u,161u})for(unsigned v=0;v<4;++v)raw.positions[q*12+v*3]+=500;
 m.Put(p,raw.positions);m.Put(uv,raw.uvs);m.Put(color,raw.colors);
 F::Layout l;l.parts[F::OverwriteCancel]={-22,9,1.3f};l.parts[F::OverwriteAccept]={31,-8,.7f};
 m.words[F::Owner+0x10]=16;F::exchange=F::Exchange::Overwrite;
 float last=1e9f;
 for(unsigned frame=1;frame<=10;++frame){
  F::exchange_frames=frame;auto g=F::Atlas(m,false,l);auto b=F::Bounds(g,148,150);
  assert(b.left<last);last=b.left;
  for(unsigned q:{152u,153u,154u,159u,160u,161u})for(unsigned v=1;v<4;++v)for(unsigned c=0;c<3;++c)assert(g.positions[q*12+v*3+c]==g.positions[q*12+c]);
 }
 auto entry=F::Atlas(m,false,l);m.words[F::Owner+0x10]=17;F::exchange=F::Exchange::None;
 auto settled=F::Atlas(m,false,l);
 for(unsigned q:{148u,149u,150u,151u,155u,156u,157u,158u})for(unsigned v=0;v<12;++v)assert(std::abs(entry.positions[q*12+v]-settled.positions[q*12+v])<.001f);
 // Selecting either copy row moves its real save slot on the shared 60 Hz
 // clock, even when the native anchor only updates every other frame.
 {
  const auto saved=m.words;
  F::Layout edited;edited.stones={13,-7,1.2f};edited.stone_spacing=.85f;
  for(unsigned i=0;i<3;++i)edited.parts[i]={float(i*7),float(i*9),.8f+.2f*i};
  for(unsigned source=0;source<3;++source)for(unsigned row=0;row<2;++row){
   m.words[F::Owner+0x14]=source;m.words[F::Owner+0x1c]=row;
   const unsigned selected=F::CopyDestinationSlot(m),first=6+37*selected;
   const float start=56.f+60*row;
   const auto native=[&](float y){
    auto card=raw;
    // File faces are generated at runtime, not stored in the ROM's static
    // pane-coordinate table (those entries are zero). Reproduce the native
    // face's two-unit inset around the scalar anchor and its attached details.
    for(unsigned q=0;q<F::QuadCount;++q)if(F::QuadPart(q)==selected)
     for(unsigned v=0;v<4;++v){
      card.positions[q*12+v*3]=14+float((q-first)%3)*96+(v%2)*96;
      card.positions[q*12+v*3+1]=y-2+(v/2)*44;
     }
    m.Put(p,card.positions);m.words[0x55b604+4*selected]=std::bit_cast<U32>(y);
   };
   native(56);m.words[F::Owner+0x10]=17;F::exchange=F::Exchange::None;
   const auto final=F::Atlas(m,false,edited);
   const float end=F::Bounds(final,first,first+2).Y();
   float previous=end+60*row*edited.stones.scale*edited.stone_spacing,last_step=1e9f;
   m.words[F::Owner+0x10]=16;F::exchange=F::Exchange::Overwrite;
   for(unsigned frame=1;frame<=10;++frame){
    native(start-(start-56)*(frame/2)/10.f);F::exchange_frames=frame;
    const auto untouched=m.words;
    const auto moving=F::Atlas(m,false,edited);
    assert(m.words==untouched);
    const float y=F::Bounds(moving,first,first+2).Y(),step=previous-y;
    if(row){assert(step>0 && step<last_step);last_step=step;}
    else assert(std::abs(y-end)<.001f);
    previous=y;
    if(frame==5)assert(std::abs(y-end)<8*edited.stones.scale*edited.stone_spacing);
    if(frame==10)for(unsigned q=0;q<F::QuadCount;++q)if(F::QuadPart(q)==selected)
     for(unsigned v=0;v<12;++v)assert(std::abs(moving.positions[q*12+v]-final.positions[q*12+v])<.001f);
   }
  }
  m.words=saved;F::exchange=F::Exchange::None;
 }
 const U32 cursor=0x8160000,cursor_model=0x8170000;
 m.words[F::Owner+0x34]=cursor;m.words[cursor+4]=cursor_model;
 auto hook=std::make_unique<F::Hook>();hook->stage=F::Hook::Stage::Native;hook->frame_layout=l;hook->bounds=F::PartBounds(raw);
 for(unsigned choice=0;choice<2;++choice){
  m.words[F::Owner+0x20]=choice;
  const unsigned part=choice?F::OverwriteAccept:F::OverwriteCancel;
  assert(F::FocusedStone(m,false)==part);
  const auto actual=hook->ModelTransform(m,cursor_model),expected=F::PartTransform(l,part,hook->bounds);
  assert(actual.x==expected.x && actual.y==expected.y && actual.sx==expected.sx);
 }
 // Native warning text follows the independently edited lower box.
 const U32 warning=0x8180000,text=0x8190000;
 m.words[0x55b548+24*4]=warning;m.words[warning+0x40]=3;m.words[warning+0x10]=text;
 hook->frame_layout.parts[F::LowerPrompt]={0,350,.8f};hook->prompt_slot=15;
 const auto warning_pose=hook->ModelTransform(m,text);
 assert(warning_pose.y==350 && warning_pose.sx==.8f);
 l.parts[F::CopyArrow]={600,20,2};m.words[F::Owner+0x10]=14;
 const auto moved=F::Atlas(m,false,l),base=F::Atlas(m,false,F::Layout{});
 const auto a=F::Bounds(moved,171,171),b=F::Bounds(base,171,171);
 assert(std::abs(a.X()-b.X()-600)<.001f && std::abs(a.Y()-b.Y()-20)<.001f);
 assert(std::abs((a.right-a.left)-2*(b.right-b.left))<.001f);
 // Keep an onscreen, fully opaque native arrow throughout the lifecycle.
 // Native cleanup can lag behind the state switch, including 32 -> 1 after
 // acknowledging success and 14 -> 15 when backing out of destination choice.
 for(unsigned state=0;state<=47;++state) {
  if(state>=12 && state<=23 && state!=15)continue;
  m.words[F::Owner+0x10]=state;const auto before=m.words;
  const auto hidden=F::Atlas(m,false,l);
  for(unsigned v=0;v<12;++v)assert(hidden.positions[171*12+v]==0);
  for(unsigned v=0;v<4;++v)assert(hidden.colors[171*16+v*4+3]==0);
  assert(m.words==before);
 }
 for(unsigned state:{24u,26u,28u,32u,1u,2u,3u,12u,13u,14u,15u,7u,8u,9u,3u,14u}) {
  m.words[F::Owner+0x10]=state;
  const auto g=F::Atlas(m,false,l);
  if(state>=12 && state<=14) {
   for(unsigned v=0;v<12;++v)assert(g.positions[171*12+v]==moved.positions[171*12+v]);
   for(unsigned v=0;v<4;++v)assert(g.colors[171*16+v*4+3]==1);
  } else {
   for(unsigned v=0;v<12;++v)assert(g.positions[171*12+v]==0);
   for(unsigned v=0;v<4;++v)assert(g.colors[171*16+v*4+3]==0);
  }
 }
 for(unsigned slot=0;slot<3;++slot)assert(F::TransitionPromptSlot(16,false,slot)==10+slot);
 assert(F::TransitionPromptSlot(18)==9 && F::TransitionPromptSlot(24)==15);
 std::cout<<"Overwrite geometry: 60 Hz entrance and selected copy-stone ease-out for all source/destination pairs, settled endpoints, independent poses, cursor, parked variants, warning and arrow passed\n";
}
