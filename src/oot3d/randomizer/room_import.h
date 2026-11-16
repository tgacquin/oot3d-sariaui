// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "oot3d/randomizer/service.h"
#include "oot3d/master_quest.h"

namespace OOT3D::Randomizer {
template<class M> QByteArray ReadBytes(M&,std::uint32_t,unsigned);
template<class M> void WriteBytes(M&,std::uint32_t,const QByteArray&);
// Runs only on the emulation thread at the native file-menu dispatch. Keep the
// save buffer alive until the async writer has completed and its job is reaped.
struct RoomImport {
    enum class Stage {Idle,Queued,Waiting,Polled,Reaped};
    Stage stage=Stage::Idle;
    std::shared_ptr<const RoomJoin::Request> request;
    std::shared_ptr<const RoomFile> file;
    std::array<std::uint32_t,16> saved{};
    QByteArray previous;
    std::uint32_t job=0;
    bool success=false;
    static constexpr std::uint32_t Dispatch=0x42f9d0,Queue=0x2e613c,Poll=0x31b9c0,Reap=0x31b99c;

    template<class M> static bool Supported(M& m) {
        return m.Read(Queue)==0xe92d47f0 && m.Read(Queue+4)==0xe1a06003 &&
            m.Read(Poll)==0xe92d41f0 && m.Read(Reap)==0xe3500000 &&
            m.Read(Dispatch)==0xe5960010;
    }
    template<class M> void Fail(M& m,RoomJoin::Status status) {
        if(!previous.isEmpty()) {
            WriteBytes(m,MasterQuest::Saves+request->slot*MasterQuest::Stride,previous);
            PublishRoomFile(request->slot,{},nullptr);
        }
        if(request){request->committing=false;request->status=status;}
        *this={};
    }
    template<class R> void Call(R& r,std::uint32_t pc) {
        std::copy(r.begin(),r.end(),saved.begin());r[14]=Dispatch;r[15]=pc;
    }
    template<class M,class R> bool Frame(M& m,R& r) {
        using namespace MasterQuest;
        if(stage==Stage::Queued) {
            job=r[0];std::copy(saved.begin(),saved.end(),r.begin());
            if(!job || !TitleScreen::Pointer(job,0x118)) {Fail(m,RoomJoin::Status::SaveFailed);return false;}
            stage=Stage::Waiting;return false;
        }
        if(stage==Stage::Polled) {
            const bool done=r[0]!=0;std::copy(saved.begin(),saved.end(),r.begin());
            if(!done){stage=Stage::Waiting;return false;}
            success=(m.Read(job).value_or(0x80000000u)&0x80000000u)==0;
            stage=Stage::Reaped;Call(r,Reap);r[0]=job;return true;
        }
        if(stage==Stage::Reaped) {
            std::copy(saved.begin(),saved.end(),r.begin());
            if(!success){Fail(m,RoomJoin::Status::SaveFailed);return false;}
            m.Write(Exists+4*request->slot,1);
            PublishRoomFile(request->slot,file->native_save,&file->package);
            FileStatus::Publish(request->slot,file->package.config.seed,
                file->package.json.value("shared_game_id").toString().toStdString());
            request->saved=true;
            request->status=request->connection_lost.load()?RoomJoin::Status::SavedDisconnected:RoomJoin::Status::Connected;
            request->committing=false;
            *this={};return false;
        }
        if(stage==Stage::Waiting) {
            stage=Stage::Polled;Call(r,Poll);r[0]=job;r[1]=0;return true;
        }
        const auto next=std::atomic_load(&RoomJoin::request);
        if(!next || next->cancelled.load() || next->status!=RoomJoin::Status::ReadyToSave)return false;
        request=next;file=std::atomic_load(&next->file);
        if(next->slot>=3 || Occupied(m,next->slot)){Fail(m,RoomJoin::Status::SlotOccupied);return false;}
        if(!file || !ValidRoomSave(file->native_save) || !Supported(m) ||
           m.Read(Owner+0x10)!=3 || m.Read(0x50bb58).value_or(1)!=0) {
            Fail(m,RoomJoin::Status::SaveFailed);return false;
        }
        const auto address=Saves+next->slot*Stride;
        previous=ReadBytes(m,address,SaveSize);
        if(previous.size()!=SaveSize || !TitleScreen::Pointer(r[13]-0x100,0x100)) {
            Fail(m,RoomJoin::Status::SaveFailed);return false;
        }
        // The source is the empty slot's stable save bank, never the live player
        // SaveContext. The ordinary writer hook persists its matching sidecar.
        request->committing=true;
        auto expected=RoomJoin::Status::ReadyToSave;
        if(request->connection_lost.load() ||
           !request->status.compare_exchange_strong(expected,RoomJoin::Status::Saving)) {
            Fail(m,RoomJoin::Status::Failed);return false;
        }
        WriteBytes(m,address,file->native_save);
        stage=Stage::Queued;Call(r,Queue);
        r[13]-=0x100;
        QByteArray path;
        for(char c:std::string("data:/save0")+std::to_string(next->slot)+".bin") {
            path+=c;path+=char(0);
        }
        path+=QByteArray(2,0);WriteBytes(m,r[13],path);
        r[0]=r[13];r[1]=address;r[2]=SaveSize;r[3]=1;
        return true;
    }
};
}
