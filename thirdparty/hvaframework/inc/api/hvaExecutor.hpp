//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAEXECUTOR_HPP
#define HVA_HVAEXECUTOR_HPP

#include <memory>
#include <vector>
#include <thread>

#include <inc/api/hvaNode.hpp>
#include <inc/util/hvaGraphMeta.hpp>
#include <inc/util/hvaUtil.hpp>

namespace hva{

// hvaExecutor_t is the actual execution unit of the entire pipeline. It is the executor which populates
//  each node's node workers and group multiple workers into a single executor. Every node workers in the same
//  executor shares the same thread while node workers in different executors may execute in parallel
class hvaExecutor_t{
public:

    hvaExecutor_t();

    hvaExecutor_t(std::size_t duplicateNum, ms loopingInterval, const hvaBatchingConfig_t* batchingConfig = nullptr);

    hvaExecutor_t(const hvaExecutor_t& ect);

    ~hvaExecutor_t();

    /**
    * @brief Add a node into the current executor, which will also populates the associated node worker from this node
    * 
    * @param node the node to be added
    * @param name the name of the node to be added
    * @return the populated node worker
    * 
    */
    hvaNodeWorker_t& addNode(const hvaNode_t& node, std::string name);

    /**
    * @brief link successive node workers in this executor
    * 
    * @param prevNodeName the node name of the node worker in front
    * @param currNodeName the node name of the node worker in back
    * @return void
    * 
    */
    void linkNode(std::string prevNodeName, std::string currNodeName);

    /**
    * @brief get the number of duplicates of this executor users set to
    * 
    * @param void
    * @return the duplicate number of this executor
    * 
    */
    std::size_t getDuplicateNum() const;

    /**
    * @brief set the batch index of this executor. This batch index is especially used in stream
    *           batching only
    * 
    * @param batchIdx the batch index to be assigned
    * @return void
    * 
    */
    void setBatchIdx(std::size_t batchIdx);

    std::size_t getThreadNumPerBatch() const;

    unsigned getBatchingPolicy() const;

    ms getLoopingInterval() const;

    /**
    * @brief internally sort the node workers contained in this executor
    * 
    * @param void
    * @return void
    * 
    */
    void generateSorted();

    /**
    * @brief initialize those node workers contained in this executor
    * 
    * @param void
    * @return void
    * 
    */
    void init();

    /**
    * @brief start those node workers contained in this executor
    * 
    * @param void
    * @return void
    * 
    */
    void start();

    /**
    * @brief stop those node workers contained in this executor. This function will invoke
    *           node workers' deinit function
    * 
    * @param void
    * @return void
    * 
    */
    void stop();

    /**
    * @brief join the thread that this executor spawns in a blocking manner
    * 
    * @param void
    * @return void
    * 
    */
    void join();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif //HVA_HVAEXECUTOR_HPP