// SPDX-License-Identifier: GPL-2.0-or-later
// Optional presentation extension; never changes inventory or the seed payload.
#include <stddef.h>
#include <string.h>
#include "actors/link_puppet.h"
#include "item_table.h"
#include "item_override.h"
#include "custom_models.h"
#include "objects.h"
#include "3ds/services/uds.h"

#define PEERS 16
#define MODELS 3
#define ROWS 277
#define VANILLA 0x8000
#define MAGIC 0x31574952u // RIW1, little endian, optional ghost trailer.
typedef struct {
    u32 magic;
    u16 appearance, actual, mesh;
    u8 mask, reserved;
    nn_math_MTX34 matrix[2]; // Relative to the sender's world position.
    float frame[2];
} RewardWire;
_Static_assert(sizeof(RewardWire)==116,"reward wire ABI");
typedef struct {RewardWire wire;u32 tick;} Remote;
typedef struct {
    Actor* actor;
    u16 appearance,actual,mesh;
    ObjectContext objects; // Private, reclaimable archive; no shared slot growth.
    SkeletonAnimationModel* models[2];
} Display;
extern ItemRow rItemTable[];
extern DrawItemTableEntry rActiveDrawItem;
extern ItemRow* rActiveItemRow;
extern u32* mBuffer;
extern size_t mBufSize;
extern u32 hostPacketBytes;
extern u32 rGameplayFrames;
extern LinkGhost ghosts[PEERS]; // Pinned runtime symbol; actors retain pointers here.
extern void Original_Matrix(SkeletonAnimationModel*);
extern Result Original_Send(u16,u8,u8,const void*,size_t);
extern void Original_Receive(u16);
extern void Original_Draw(Actor*,GlobalContext*);
extern void Original_Destroy(Actor*,GlobalContext*);
extern void Original_Shutdown(void);
extern void SkeletonAnimationModel_Draw(SkeletonAnimationModel*,s32);
extern void SkeletonAnimationModel_SetMesh(SkeletonAnimationModel*,s32);
extern void SkeletonAnimationModel_MatrixCopy(SkeletonAnimationModel*,nn_math_MTX34*);
static RewardWire local;
static u32 local_tick[2];
static Remote peers[PEERS];
static Display displays[MODELS];
// Optional cosmetics must yield to the game's fixed graphics heap. Inspect the
// same bounded free list used by the host diagnostics; never enlarge the pool.
__attribute__((noinline)) int Rewards_Headroom(unsigned archive_size) {
    volatile u32* heap=(volatile u32*)0x5b4f7c;
    if(heap[0]!=0x45585048)return 0;
    u32 start=heap[6],end=heap[7],block=heap[9],previous=0,total=0,largest=0,count=0;
    if(start<0x14000000 || end<=start || end>0x1c000000 || archive_size>0x400000)return 0;
    while(block) {
        if(block<start || block>end-16 || (block&3) || ++count>16384)return 0;
        volatile u32* b=(volatile u32*)block;
        u32 size=b[1];
        if(b[0]!=0x4652 || b[2]!=previous || size>end-block-16 || size>end-start-total)return 0;
        total+=size;if(size>largest)largest=size;
        previous=block;block=b[3];
    }
    if(heap[10]!=previous)return 0;
    u32 estimate=archive_size*2;
    if(estimate<0x40000)estimate=0x40000;
    return largest>=estimate && total>=estimate+0x100000;
}

static int EqualDraw(const DrawItemTableEntry* a,const DrawItemTableEntry* b) {
    return memcmp(a,b,sizeof(*a))==0;
}
static DrawItemTableEntry RowDraw(const ItemRow* row) {
    return (DrawItemTableEntry){row->objectId,row->objectModelIdx,row->cmabIndex,
                               row->objectModelIdx2,row->cmabIndex2};
}
static int Descriptor(const RewardWire* w,DrawItemTableEntry* draw,s8* special) {
    if(w->appearance&VANILLA) {
        const unsigned index=w->appearance&~VANILLA;
        if(index>=140 || w->actual || w->mesh!=index)return 0;
        *draw=gDrawItemTable[index];*special=-1;
    } else {
        if(!w->appearance || w->appearance>=ROWS || !w->actual || w->actual>=ROWS)return 0;
        *draw=RowDraw(&rItemTable[w->appearance]);
        *special=rItemTable[w->actual].special;
        if(w->mesh!=(Object_IsRupeeObject(draw->objectId)?(u16)(u8)*special:0))return 0;
    }
    return draw->objectId>0 && draw->objectModelIdx!=255;
}
static int FiniteBound(float value,float limit) {return value>=-limit && value<=limit;}
int Rewards_Valid(const RewardWire* w) {
    DrawItemTableEntry d;s8 special;
    if(w->magic!=MAGIC || w->reserved || (w->mask&~3))return 0;
    if(!w->mask)return 1;
    if(!Descriptor(w,&d,&special) || ((w->mask&2) && d.objectModelIdx2==255))return 0;
    for(unsigned i=0;i<2;++i)if(w->mask&(1<<i)) {
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
            if(!FiniteBound(w->matrix[i].data[row][col],col==3?2000.f:100.f))return 0;
        if(!FiniteBound(w->frame[i],100000.f))return 0;
    }
    return 1;
}
void Rewards_Matrix(SkeletonAnimationModel* model) {
    Original_Matrix(model); // Capture AFTER the game's custom model transform.
    if(!gGlobalContext || !PLAYER || PauseContext_GetState()!=0)return;
    unsigned index=model==PLAYER->giModels[0]?0:model==PLAYER->giModels[1]?1:2;
    if(index==2)return;
    RewardWire next={.magic=MAGIC};
    if(rActiveItemRow && PLAYER->itemActionParam<0x2b) {
        const unsigned address=(unsigned)rActiveItemRow,base=(unsigned)rItemTable;
        if(address<base || address>=base+ROWS*sizeof(ItemRow) || (address-base)%sizeof(ItemRow))return;
        next.actual=(address-base)/sizeof(ItemRow);
        for(unsigned i=1;i<ROWS;++i) {
            DrawItemTableEntry d=RowDraw(&rItemTable[i]);
            if(EqualDraw(&d,&rActiveDrawItem)) {next.appearance=i;break;}
        }
        next.mesh=Object_IsRupeeObject(rActiveDrawItem.objectId)?(u8)rActiveItemRow->special:0;
    } else {
        int id=PLAYER->giDrawIdPlusOne;
        if(id<0)id=-id;
        if(id<1 || id>140)return;
        next.appearance=VANILLA|(id-1);next.mesh=id-1;
    }
    if(next.appearance!=local.appearance || next.actual!=local.actual || next.mesh!=local.mesh)
        local=next;
    local.magic=MAGIC;local.mask|=1<<index;
    local.matrix[index]=model->mtx;
    local.matrix[index].data[0][3]-=PLAYER->actor.world.pos.x;
    local.matrix[index].data[1][3]-=PLAYER->actor.world.pos.y;
    local.matrix[index].data[2][3]-=PLAYER->actor.world.pos.z;
    local.frame[index]=model->matAnim?model->matAnim->curFrame:0;
    local_tick[index]=rGameplayFrames;
}
Result Rewards_Send(u16 node,u8 channel,u8 flags,const void* buffer,size_t size) {
    // The original 40-byte pose and optional tunic color remain unchanged.
    if(buffer==mBuffer && size==48 && mBuffer && mBuffer[0]==1 &&
       mBufSize>=size+sizeof(RewardWire)) {
        RewardWire w=local;w.magic=MAGIC;
        for(unsigned i=0;i<2;++i)
            if(rGameplayFrames-local_tick[i]>1 || !gGlobalContext || !PLAYER || !PLAYER->giModels[i])
                w.mask&=~(1<<i);
        if(!Rewards_Valid(&w))w.mask=0;
        memcpy((char*)mBuffer+size,&w,sizeof(w));size+=sizeof(w);
    }
    return Original_Send(node,channel,flags,buffer,size);
}
void Rewards_Receive(u16 node) {
    Original_Receive(node);
    if(!node || node>PEERS)return;
    Remote* p=&peers[node-1];p->wire.mask=0;
    if(hostPacketBytes!=48+sizeof(RewardWire))return;
    RewardWire w;memcpy(&w,(char*)mBuffer+48,sizeof(w));
    if(!Rewards_Valid(&w))return;
    p->wire=w;p->tick=rGameplayFrames;
}
static void Clear(Display* d) {
    for(unsigned i=0;i<2;++i)if(d->models[i]) {
        d->models[i]->vtbl->destroy(d->models[i]);d->models[i]=NULL;
    }
    // Object_Spawn finishes its read synchronously. Destroy models BEFORE ZAR.
    if(d->objects.numEntries)Object_Clear(gGlobalContext,&d->objects);
    memset(d,0,sizeof(*d));
}
static SkeletonAnimationModel* Create(Actor* actor,ZARInfo* zar,unsigned model) {
    void* cmb=ZAR_GetCMBByIndex(zar,model);
    if(!cmb)return NULL;
    // Same model factory as native 31488c/36a924, using this puppet's owner.
    u32* factory=*(u32**)(0x5be5b8+0x17c);
    if(!factory)return NULL;
    u32 previous=factory[2];factory[2]=(u32)actor->unk_178;
    typedef SkeletonAnimationModel* (*CreateFn)(void*,void*,int);
    SkeletonAnimationModel* result=((CreateFn)(*(u32**)factory)[2])(factory,cmb,1);
    factory[2]=previous;
    return result;
}
static int Init(Display* d,Actor* actor,const RewardWire* w) {
    DrawItemTableEntry draw;s8 special;
    if(!Descriptor(w,&draw,&special))return 0;
    if(!Rewards_Headroom(0))return 0;
    d->actor=actor;d->appearance=w->appearance;d->actual=w->actual;d->mesh=w->mesh;
    if(Object_Spawn(&d->objects,draw.objectId)!=0 || !d->objects.slots[0].buf) {Clear(d);return 0;}
    ObjectEntry* object=&d->objects.slots[0];
    if(!Rewards_Headroom(object->size)) {Clear(d);return 0;}
    if(!(w->appearance&VANILLA))CustomModels_EditItemCMB(object->buf,draw.objectId,special);
    for(unsigned i=0;i<2;++i) {
        unsigned model=i?draw.objectModelIdx2:draw.objectModelIdx;
        unsigned anim=i?draw.cmabIndex2:draw.cmabIndex;
        if(model==255)continue;
        SkeletonAnimationModel* sam=d->models[i]=Create(actor,&object->zarInfo,model);
        if(!sam) {Clear(d);return 0;}
        if(anim!=255) {
            void* cmab=ZAR_GetCMABByIndex(&object->zarInfo,anim);
            if(!cmab || !sam->matAnim) {Clear(d);return 0;}
            MatAnim_Init(sam->matAnim,cmab);sam->matAnim->animMode=1;
        }
        SkeletonAnimationModel_SetMesh(sam,w->mesh);
        if(!i && !(w->appearance&VANILLA))CustomModels_ApplyItemCMAB(sam,draw.objectId,special);
        if(sam->matAnim)sam->matAnim->animSpeed=0;
    }
    return 1;
}
void Rewards_Draw(Actor* actor,GlobalContext* global) {
    EnLinkPuppet* puppet=(EnLinkPuppet*)actor;
    LinkGhost* ghost=puppet->ghostPtr;
    // Shops/dialogue can defer actor updates. Do not draw a disconnected peer
    // while waiting for the ordinary update to kill and destroy its puppet.
    if(!ghost || !ghost->inUse || !ghost->isInGame) {
        for(unsigned i=0;i<MODELS;++i)if(displays[i].actor==actor)Clear(&displays[i]);
        return;
    }
    Original_Draw(actor,global);
    if(!ghost || !ghost->networkID || ghost->networkID>PEERS)return;
    Remote* p=&peers[ghost->networkID-1];
    Display* d=NULL;
    for(unsigned i=0;i<MODELS;++i)if(displays[i].actor==actor)d=&displays[i];
    unsigned count=0;
    for(Actor* a=global->actorCtx.actorList[EnLinkPuppet_InitVars.type].first;a && a!=actor;a=a->next)
        if(a->id==EnLinkPuppet_InitVars.id)++count;
    if(!ghost->inUse || !ghost->isInGame || ghost->ghostData.currentScene!=global->sceneNum ||
       ghost->ghostData.age!=gSaveContext.linkAge || count>=MODELS ||
       !p->wire.mask || rGameplayFrames-p->tick>15) {if(d)Clear(d);return;}
    if(d && (d->appearance!=p->wire.appearance || d->actual!=p->wire.actual || d->mesh!=p->wire.mesh)) {Clear(d);d=NULL;}
    if(!d) {
        for(unsigned i=0;i<MODELS;++i)if(!displays[i].actor) {d=&displays[i];break;}
        if(!d || !Init(d,actor,&p->wire))return;
    }
    for(unsigned i=0;i<2;++i)if(d->models[i] && (p->wire.mask&(1<<i))) {
        SkeletonAnimationModel* model=d->models[i];
        nn_math_MTX34 matrix=p->wire.matrix[i];
        matrix.data[0][3]+=ghost->ghostData.position.x;
        matrix.data[1][3]+=ghost->ghostData.position.y;
        matrix.data[2][3]+=ghost->ghostData.position.z;
        SkeletonAnimationModel_MatrixCopy(model,&matrix);
        if(model->matAnim)model->matAnim->curFrame=p->wire.frame[i];
        model->unk_AC=1;SkeletonAnimationModel_Draw(model,0);
    }
}
void Rewards_Destroy(Actor* actor,GlobalContext* global) {
    for(unsigned i=0;i<MODELS;++i)if(displays[i].actor==actor)Clear(&displays[i]);
    Original_Destroy(actor,global);
}
void Rewards_Shutdown(void) {
    // The networking tick (including its stale-peer timeout) stops on shutdown.
    // Retire every peer now so existing actors die and cannot respawn in-scene.
    for(unsigned i=0;i<PEERS;++i) {
        ghosts[i].inUse=false;
        ghosts[i].isInGame=false;
    }
    for(unsigned i=0;i<MODELS;++i)Clear(&displays[i]);
    memset(peers,0,sizeof(peers));memset(&local,0,sizeof(local));
    Original_Shutdown();
}
