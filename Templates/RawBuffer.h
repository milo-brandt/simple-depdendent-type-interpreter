//
//  RawBuffer.hpp
//  Dummy
//
//  Created by Milo Brandt on 4/17/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#ifndef RawBuffer_h
#define RawBuffer_h

#include <cstdint>
#include <utility>

struct CopyBuffer_t{};
constexpr CopyBuffer_t copyBuffer;
struct OwnBuffer_t{};
constexpr OwnBuffer_t ownBuffer;

struct RawBuffer{
    std::uint8_t* data = nullptr;
    std::size_t length;
    RawBuffer() = default;
    RawBuffer(RawBuffer const&) = delete;
    RawBuffer(RawBuffer&&);
    explicit RawBuffer(std::size_t);
    RawBuffer& operator=(RawBuffer const&) = delete;
    RawBuffer& operator=(RawBuffer&&);
    RawBuffer(CopyBuffer_t, void const* src, std::size_t length);
    RawBuffer(CopyBuffer_t, RawBuffer const&);
    RawBuffer(OwnBuffer_t, void* src, std::size_t length);
    operator bool const();
    ~RawBuffer();
};

#endif /* RawBuffer_hpp */
