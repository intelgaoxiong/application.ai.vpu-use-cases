//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAUTIL_HPP
#define HVA_HVAUTIL_HPP

#include <chrono>
#include <stdexcept>
#include <memory>
#include <string>

namespace hva{

void hvaAssertFail(const char* cond, int line, const char* file,
                      const char* func);

// #define HVA_ASSERT(cond) if(!cond){ std::cout<<"Assert Failed!"<<std::endl;}
#define HVA_ASSERT(cond) do{if(!(cond)){::hva::hvaAssertFail(#cond, __LINE__, __FILE__, \
    __func__);}} while(false);

enum hvaStatus_t{
    hvaSuccess = 0,
    hvaFailure,
    hvaPortFullDiscarded,
    hvaPortFullTimeout,
    hvaPortNullPtr,
    hvaEventRegisterFailed,
    hvaEventNotFound,
    hvaCallbackFail
};

enum hvaState_t{
    idle = 0,
    initialized,
    running,
    paused,
    stop
};

using ms = std::chrono::milliseconds;

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    std::unique_ptr<char[]> buf( new char[ size ] ); 
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
};

std::string string_format( const std::string& format);

}

#endif //HVA_HVAUTIL_HPP