//
//  RawBuffer.cpp
//  Dummy
//
//  Created by Milo Brandt on 4/17/20.
//  Copyright © 2020 Milo Brandt. All rights reserved.
//

#include "RawBuffer.h"
#include <cstring>

RawBuffer::RawBuffer(std::size_t len):data(new std::uint8_t[len]),length(len){}
RawBuffer::RawBuffer(RawBuffer&& other):data(other.data),length(other.length){
    other.data = nullptr;
}
RawBuffer::RawBuffer(OwnBuffer_t, void* src, std::size_t length):data((std::uint8_t*)src), length(length){}
RawBuffer::RawBuffer(CopyBuffer_t, void const* src, std::size_t length):data(new std::uint8_t[length]), length(length){
    memcpy(data, src, length);
}
RawBuffer::RawBuffer(CopyBuffer_t, RawBuffer const& src):RawBuffer(copyBuffer, src.data, src.length){}
RawBuffer::~RawBuffer(){
    if(data) delete[] data;
}
RawBuffer::operator bool const(){ return data != nullptr; }
