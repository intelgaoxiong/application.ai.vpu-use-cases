//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVABLOB_HPP
#define HVA_HVABLOB_HPP

#include <inc/api/hvaBuf.hpp>

#include <functional>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>

namespace hva{

template<bool, typename T1, typename T2>
struct _deleterHelper{
    typedef T2 type;
};

template<typename T1, typename T2>
struct _deleterHelper<true, T1, T2>{
    typedef T1 type;
};

class hvaBufContainerHolderBase_t{
public:
    virtual ~hvaBufContainerHolderBase_t();
};

template<typename T, typename META_T = void>
class hvaBufContainerHolder_t : public hvaBufContainerHolderBase_t{
public:
    hvaBufContainerHolder_t(hvaBuf_t<T,META_T>* buf);

    hvaBufContainerHolder_t(std::shared_ptr<hvaBuf_t<T,META_T>> buf);

    hvaBufContainerHolder_t() = delete;

    std::shared_ptr<hvaBuf_t<T,META_T>> m_buf;

};

// hvaBlob_t is the data struct that being trasmitted between every nodes. A hvaNode_t can contain
//  zero or one or more hvaBuf_t. hvaBlob_t will hold a reference count of each hvaBuf_t it contains
class hvaBlob_t{
public:
    hvaBlob_t();

    ~hvaBlob_t();

    /**
    * @brief push a raw pointer of hvaBuf_t into the blob
    * 
    * @param buf the hvaBuf_t to add
    * @return void
    * 
    */
    template<typename T, typename META_T = void>
    void push(hvaBuf_t<T, META_T>* buf);

    /**
    * @brief push a shared pointer of hvaBuf_t into the blob. This blob will increment the ref count
    *           of this shared pointer
    * 
    * @param buf the shared pointer to hvaBuf_t to add
    * @return void
    * 
    */
    template<typename T, typename META_T = void>
    void push(std::shared_ptr<hvaBuf_t<T, META_T>> buf);

    /**
    * @brief get the idx hvaBuf_t in this blob in form of shared pointer
    * 
    * @param idx the index of hvaBuf_t to get in this blob
    * @return void
    * 
    */
    template<typename T, typename META_T = void>
    std::shared_ptr<hvaBuf_t<T,META_T>> get(std::size_t idx);

    /**
    * @brief emplace a new hvaBuf_t within this blob
    * 
    * @param ptr the pointer to raw data buffer
    * @param size the indicated size of the raw data buffer
    * @param metaPtr the pointer to meta struct
    * @param d the optional deleter function specified by users
    * @return void
    * 
    */
    template<typename T, typename META_T = void, typename std::enable_if<std::is_same<META_T,void>::value,int>::type = 0>
    std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d = {});

    template<typename T, typename META_T = void, typename std::enable_if<!std::is_same<META_T,void>::value,int>::type = 0>
    std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d = {});

    std::vector<hvaBufContainerHolderBase_t*> vBuf;
    std::chrono::milliseconds timestamp;
    int streamId;
    int frameId;
    int typeId;
    int ctx;

};

template<typename T, typename META_T>
hvaBufContainerHolder_t<T,META_T>::hvaBufContainerHolder_t(hvaBuf_t<T,META_T>* buf)
        :m_buf{buf}{

}

template<typename T, typename META_T>
hvaBufContainerHolder_t<T,META_T>::hvaBufContainerHolder_t(std::shared_ptr<hvaBuf_t<T,META_T>> buf)
        :m_buf{buf}{

}

template<typename T, typename META_T>
void hvaBlob_t::push(hvaBuf_t<T, META_T>* buf){
    vBuf.push_back(new hvaBufContainerHolder_t<T,META_T>{buf});
}

template<typename T, typename META_T>
void hvaBlob_t::push(std::shared_ptr<hvaBuf_t<T, META_T>> buf){
    vBuf.push_back(new hvaBufContainerHolder_t<T,META_T>{buf});
}

template <typename T, typename META_T>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::get(std::size_t idx){
    return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf[idx])->m_buf;
}

#pragma optimize("", off)

template<typename T, typename META_T, typename std::enable_if<std::is_same<META_T,void>::value,int>::type>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d){
    push(new hvaBuf_t<T,void>(ptr,size,d));
    return dynamic_cast<hvaBufContainerHolder_t<T,void>*>(vBuf.back())->m_buf;
}

template<typename T, typename META_T, typename std::enable_if<!std::is_same<META_T,void>::value,int>::type>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d){
    push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
    return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
}

#pragma optimize("", on)

}
#endif //#ifndef HVA_HVABLOB_HPP