// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/atomic_shared_ptr.h"
#include "oot3d/message_patch.h"
#include "oot3d/randomizer/file_status.h"
#include "oot3d/randomizer/generation_progress.h"
#include "oot3d/room_join.h"
#include <array>
#include <atomic>
#include <memory>
#include <cmath>

namespace OOT3D::LoadingPrompt {
constexpr std::uint32_t MessageId=0xfff0;
constexpr std::uint32_t ErrorMessageId=0xffef;
constexpr std::uint32_t WaitingMessageId=0xffee;
constexpr std::uint32_t StartingMessageId=0xffce;
inline std::string_view StartingLabel(unsigned language=1) {
    return language==4 || language==5?"Démarrage...":
        language==6 || language==7?"Iniciando...":"Starting...";
}
inline float Opacity(std::int64_t now_us,std::int64_t period=2000000) {
    // A smooth two-second breath, always legible; only the lettering pulses.
    const double phase=double(now_us%period)/double(period);
    return .725f+.275f*float(std::cos(phase*6.283185307179586));
}
inline float PresentationOpacity(std::int64_t now_us,bool starting=false,bool generating=false) {
    if(starting)return Opacity(now_us,800000); // 2.5 times the waiting pulse rate.
    if(RoomJoin::StatusPrompt())return RoomJoin::Searching()?Opacity(now_us):1.f;
    return Opacity(now_us)*(generating?Randomizer::GenerationProgress::TransitionOpacity():1.f);
}
constexpr unsigned RoomStatusCount=static_cast<unsigned>(RoomJoin::Status::Count);
constexpr std::uint32_t IncompatibleMessageId=0xffd0,MissingMessageId=0xffcf;
static_assert(WaitingMessageId-RoomStatusCount>IncompatibleMessageId);
constexpr unsigned NoticeCount=static_cast<unsigned>(Randomizer::FileStatus::Error::Count)-3;
constexpr std::uint32_t NoticeMessageId=0xffc0;
constexpr std::uint32_t RetryMessageId=0xffa0;
constexpr unsigned RetryCount=Randomizer::GenerationProgress::AttemptLimit-1;
static_assert(NoticeMessageId-NoticeCount>RetryMessageId);
inline bool RetryMessage(std::uint32_t id) {return id<=RetryMessageId && id>RetryMessageId-RetryCount;}
inline std::uint32_t GenerationMessageId() {
    const auto attempt=Randomizer::GenerationProgress::DisplayedAttempt();
    return attempt<2?MessageId:RetryMessageId-(attempt-2);
}
inline std::string GenerationLabel(unsigned language) {
    return Randomizer::GenerationProgress::Label(Randomizer::GenerationProgress::DisplayedAttempt(),language);
}
inline bool NoticeMessage(std::uint32_t id) {return id<=NoticeMessageId && id>NoticeMessageId-NoticeCount;}
inline std::uint32_t ErrorId(Randomizer::FileStatus::Error error) {
    using Error=Randomizer::FileStatus::Error;
    return error==Error::Missing?MissingMessageId:error==Error::Incompatible?IncompatibleMessageId:
        NoticeMessageId-(static_cast<unsigned>(error)-3);
}
inline bool FileErrorMessage(std::uint32_t id) {return id==IncompatibleMessageId || id==MissingMessageId || NoticeMessage(id);}
inline bool RoomMessage(std::uint32_t id) {return id<=WaitingMessageId && id>WaitingMessageId-RoomStatusCount;}
inline std::uint32_t RoomMessageId() {return WaitingMessageId-static_cast<unsigned>(RoomJoin::CurrentStatus());}
struct Entry {
    std::uint32_t offset,signature,error_offset=0,waiting_offset=0;
    std::array<std::uint32_t,RoomStatusCount> room_offsets{};
    std::array<std::uint32_t,2> file_error_offsets{};
    std::uint32_t starting_offset=0;
    std::array<std::uint32_t,NoticeCount> notice_offsets{};
    std::array<std::uint32_t,RetryCount> retry_offsets{};
};
inline Common::AtomicSharedPtr<const Entry> entry;
inline bool Ready() {return bool(entry.load());}
// Append a private message entry without changing any native table indices.
// The ordinary prompt renderer retains its font, spacing, centering and shadow.
inline bool Prepare(std::vector<std::uint8_t>& bytes) {
    entry.store(std::shared_ptr<const Entry>{});
    using Messages::Read32;using Messages::Write32;
    if(bytes.size()<16 || Read32(bytes,0)!=0x4d51 || Read32(bytes,4)!=4)return false;
    const auto count=Read32(bytes,8);
    if(!count || count>(bytes.size()-16)/96)return false;
    std::size_t source=0;
    for(unsigned i=0;i<count;++i) {
        const auto at=16+i*96,id=Read32(bytes,at);
        if(id==MessageId || id==ErrorMessageId || id==StartingMessageId || RoomMessage(id) || FileErrorMessage(id) || RetryMessage(id))return false;
        if(id==0x900)source=at;
    }
    if(!source)return false;
    std::vector<std::uint8_t> copy(bytes.begin()+source,bytes.begin()+source+96);
    Write32(copy,0,MessageId);
    auto result=bytes;
    for(unsigned language=0;language<10;++language) {
        const auto label=Randomizer::GenerationProgress::Label(1,language);
        while(result.size()%4)result.push_back(0);
        Write32(copy,16+language*8,result.size());
        result.insert(result.end(),{0x7f,0x04,0x7f,0x1e});
        result.insert(result.end(),label.begin(),label.end());
        result.insert(result.end(),{0x7f,0x05,0x7f,0x00});
        Write32(copy,20+language*8,label.size()+8);
    }
    while(result.size()%4)result.push_back(0);
    const auto offset=std::uint32_t(result.size());
    result.insert(result.end(),copy.begin(),copy.end());
    Write32(copy,0,ErrorMessageId);
    for(unsigned language=0;language<10;++language) {
        const std::string_view label=language==4 || language==5?"MQ : désactivez enemizer et âmes normales.":
            language==6 || language==7?"MQ: desactiva enemizer y almas normales.":
            "MQ: disable enemizer and non-boss souls.";
        while(result.size()%4)result.push_back(0);
        const auto start=result.size();
        Write32(copy,16+language*8,start);
        // Native instant, centered, red text; restore white before ending.
        result.insert(result.end(),{0x7f,0x04,0x7f,0x1e,0x7f,0x1d,0x41});
        result.insert(result.end(),label.begin(),label.end());
        result.insert(result.end(),{0x7f,0x1d,0x00,0x7f,0x05,0x7f,0x00});
        Write32(copy,20+language*8,result.size()-start);
    }
    while(result.size()%4)result.push_back(0);
    const auto error_offset=std::uint32_t(result.size());
    result.insert(result.end(),copy.begin(),copy.end());
    std::array<std::uint32_t,RoomStatusCount> room_offsets{};
    for(unsigned status=0;status<RoomStatusCount;++status) {
        Write32(copy,0,WaitingMessageId-status);
        for(unsigned language=0;language<10;++language) {
            const auto label=RoomJoin::Label(static_cast<RoomJoin::Status>(status),language);
            while(result.size()%4)result.push_back(0);
            const auto start=result.size();
            Write32(copy,16+language*8,start);
            result.insert(result.end(),{0x7f,0x04,0x7f,0x1e});
            const bool red=status==static_cast<unsigned>(RoomJoin::Status::Disconnecting);
            if(red)result.insert(result.end(),{0x7f,0x1d,0x41});
            result.insert(result.end(),label.begin(),label.end());
            if(red)result.insert(result.end(),{0x7f,0x1d,0x00});
            result.insert(result.end(),{0x7f,0x05,0x7f,0x00});
            Write32(copy,20+language*8,result.size()-start);
        }
        while(result.size()%4)result.push_back(0);
        room_offsets[status]=std::uint32_t(result.size());
        result.insert(result.end(),copy.begin(),copy.end());
    }
    std::array<std::uint32_t,2> file_error_offsets{};
    for(unsigned missing=0;missing<2;++missing) {
        Write32(copy,0,missing?MissingMessageId:IncompatibleMessageId);
        for(unsigned language=0;language<10;++language) {
            const auto label=Randomizer::FileStatus::ErrorLabel(missing?
                Randomizer::FileStatus::Error::Missing:Randomizer::FileStatus::Error::Incompatible,language);
            while(result.size()%4)result.push_back(0);
            const auto start=result.size();Write32(copy,16+language*8,start);
            result.insert(result.end(),{0x7f,0x04,0x7f,0x1e,0x7f,0x1d,0x41});
            result.insert(result.end(),label.begin(),label.end());
            result.insert(result.end(),{0x7f,0x1d,0x00,0x7f,0x05,0x7f,0x00});
            Write32(copy,20+language*8,result.size()-start);
        }
        while(result.size()%4)result.push_back(0);
        file_error_offsets[missing]=std::uint32_t(result.size());
        result.insert(result.end(),copy.begin(),copy.end());
    }
    std::array<std::uint32_t,NoticeCount> notice_offsets{};
    for(unsigned i=0;i<NoticeCount;++i) {
        Write32(copy,0,NoticeMessageId-i);
        for(unsigned language=0;language<10;++language) {
            const auto label=Randomizer::FileStatus::ErrorLabel(static_cast<Randomizer::FileStatus::Error>(i+3),language);
            while(result.size()%4)result.push_back(0);
            const auto start=result.size();Write32(copy,16+language*8,start);
            result.insert(result.end(),{0x7f,0x04,0x7f,0x1e,0x7f,0x1d,0x41});
            result.insert(result.end(),label.begin(),label.end());
            result.insert(result.end(),{0x7f,0x1d,0x00,0x7f,0x05,0x7f,0x00});
            Write32(copy,20+language*8,result.size()-start);
        }
        while(result.size()%4)result.push_back(0);
        notice_offsets[i]=std::uint32_t(result.size());
        result.insert(result.end(),copy.begin(),copy.end());
    }
    Write32(copy,0,StartingMessageId);
    for(unsigned language=0;language<10;++language) {
        const auto label=StartingLabel(language);
        while(result.size()%4)result.push_back(0);
        Write32(copy,16+language*8,result.size());
        result.insert(result.end(),{0x7f,0x04,0x7f,0x1e});
        result.insert(result.end(),label.begin(),label.end());
        result.insert(result.end(),{0x7f,0x05,0x7f,0x00});
        Write32(copy,20+language*8,label.size()+8);
    }
    while(result.size()%4)result.push_back(0);
    const auto starting_offset=std::uint32_t(result.size());
    result.insert(result.end(),copy.begin(),copy.end());
    std::array<std::uint32_t,RetryCount> retry_offsets{};
    for(unsigned i=0;i<RetryCount;++i) {
        Write32(copy,0,RetryMessageId-i);
        for(unsigned language=0;language<10;++language) {
            const auto label=Randomizer::GenerationProgress::Label(i+2,language);
            while(result.size()%4)result.push_back(0);
            Write32(copy,16+language*8,result.size());
            result.insert(result.end(),{0x7f,0x04,0x7f,0x1e});
            result.insert(result.end(),label.begin(),label.end());
            result.insert(result.end(),{0x7f,0x05,0x7f,0x00});
            Write32(copy,20+language*8,label.size()+8);
        }
        while(result.size()%4)result.push_back(0);
        retry_offsets[i]=std::uint32_t(result.size());
        result.insert(result.end(),copy.begin(),copy.end());
    }
    const auto signature=std::uint32_t(result.size());
    for(auto word:{0x4c544f4fu,0x504d4f52u})
        for(unsigned i=0;i<4;++i)result.push_back(word>>(i*8));
    bytes.swap(result);
    entry.store(std::make_shared<const Entry>(Entry{offset,signature,error_offset,room_offsets[0],room_offsets,file_error_offsets,starting_offset,notice_offsets,retry_offsets}));
    return true;
}
template<class M,class R> bool Select(M& m,R& r) {
    if(r[1]!=MessageId && r[1]!=ErrorMessageId && r[1]!=StartingMessageId && !RoomMessage(r[1]) && !FileErrorMessage(r[1]) && !RetryMessage(r[1]))return false;
    const auto selected=entry.load();
    const auto base=m.Read(r[0]+4).value_or(0);
    if(!selected || !base || m.Read(base)!=0x4d51 ||
       m.Read(base+selected->signature)!=0x4c544f4f ||
       m.Read(base+selected->signature+4)!=0x504d4f52)return false;
    r[0]=base+(r[1]==StartingMessageId?selected->starting_offset:
        RetryMessage(r[1])?selected->retry_offsets[RetryMessageId-r[1]]:
        NoticeMessage(r[1])?selected->notice_offsets[NoticeMessageId-r[1]]:
        FileErrorMessage(r[1])?selected->file_error_offsets[r[1]==MissingMessageId]:
        RoomMessage(r[1])?selected->room_offsets[WaitingMessageId-r[1]]:
        r[1]==ErrorMessageId?selected->error_offset:selected->offset);r[15]=r[14];return true;
}
}
