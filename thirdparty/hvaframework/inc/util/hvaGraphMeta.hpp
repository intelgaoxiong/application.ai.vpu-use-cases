//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAGRAPHMETA_HPP
#define HVA_HVAGRAPHMETA_HPP

#include <memory>
#include <string>

namespace hva{

class hvaNodeWorker_t;
class hvaNode_t;

struct NodeWorkerMeta{
    std::shared_ptr<hvaNodeWorker_t> ptr;
    static std::string name(){ return "NodeWorkerMeta";};
};

struct NodeMeta{
    std::shared_ptr<hvaNode_t> ptr;
    static std::string name(){ return "NodeMeta";};
};

struct NatureMeta{
    bool isActive;
    static std::string name(){ return "NatureMeta";};
};

struct NodeNameMeta{
    std::string nodeName;
    static std::string name(){ return "NodeNameMeta";};
};

struct IsCompiledMeta{
    bool isCompiled;
    static std::string name(){ return "IsCompiledMeta";};
};

}

#endif //HVA_HVAGRAPHMETA_HPP