//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAPIPELINE_HPP
#define HVA_HVAPIPELINE_HPP

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <inc/util/hvaUtil.hpp>
#include <inc/util/hvaGraphMeta.hpp>
#include <inc/api/hvaNode.hpp>
#include <inc/api/hvaBlob.hpp>
#include <inc/api/hvaExecutor.hpp>
#include <inc/api/hvaEvent.hpp>
#include <inc/api/hvaEventManager.hpp>
#include <signal.h>
#include <inc/api/hvaLogger.hpp>

namespace hva{

class hvaNode_t;

//using hvaSystemSignalHandler = std::function<void(hvaPipeline_t*, int, siginfo_t *, void *)>;

// hvaPipeline_t is main data struct that holds a graph of pipeline to execute. To start the pipeline, users should
//  add nodes/sources, link nodes, prepare and start in sequence.
class hvaPipeline_t{
public:    

    hvaPipeline_t();

    ~hvaPipeline_t();

    /**
    * @brief Add a node into the pipeline. For those nodes that has no precedents, users should use setSource instead 
    * 
    * @param node the node to be added
    * @param name distinguishable name associated with this node. Note that this name must be unique within this pipeline
    * @return reference to the node just added
    * 
    */
    hvaNode_t& addNode(std::shared_ptr<hvaNode_t> node, std::string name);

    /**
    * @brief add a source node into the pipeline
    * 
    * @param node the node to be added as sources
    * @param name distinguishable name associated with this node. Note that this name must be unique within this pipeline
    * @return reference to the node just added
    * 
    */
    hvaNode_t& setSource(std::shared_ptr<hvaNode_t> srcNode, std::string name);

    /**
    * @brief link two sucessive nodes
    * 
    * @param prevNode the node to be linked in front
    * @param prevNodePortIdx the port index on prevNode to be linked
    * @param currNode the node to be linked in back
    * @param currNodePortIdx the port index on currNode to be linked
    * @param func optional converter function in case the data format transferred between two successive nodes do not match 
    * @return void
    * 
    */
    void linkNode(std::string prevNode, std::size_t prevNodePortIdx, std::string currNode, std::size_t currNodePortIdx, hvaConvertFunc func = {});

    /**
    * @brief prepare the pipeline. This function will invoke every node's init function
    * 
    * @param void
    * @return void
    * 
    */
    void prepare();

    /**
    * @brief start the pipeline. non-blocking
    * 
    * @param void
    * @return void
    * 
    */
    void start();

    /**
    * @brief stop the pipeline in a blocking manner. This function returns when this pipeline actually stops after
    *           invoking all nodes' deinit function
    * 
    * @param void
    * @return void
    * 
    */
    void stop();

    /**
    * @brief send a blob from outside of pipeline to a specific node within the pipeline
    * 
    * @param data blob to be sent
    * @param nodeName name of the node that this data will be sent to
    * @param portId index of the port at node which data will be sent to
    * @param timeout timeout before return if the port is full. Set to 0 to make this call blocking forever
    * @return status code
    * 
    */
    hvaStatus_t sendToPort(std::shared_ptr<hvaBlob_t> data, std::string nodeName, std::size_t portId, ms timeout = ms(1000));

    /**
    * @brief register an event to this pipeline
    * 
    * @param event event to be registered
    * @return status code
    * 
    */
    hvaStatus_t registerEvent(hvaEvent_t event);

    /**
    * @brief register a callback associated with an event that is perviously being registered
    * 
    * @param event the event that callback associates to
    * @param callback the callback function to be registered
    * @return status code
    * 
    */
    hvaStatus_t registerCallback(hvaEvent_t event, hvaEventHandlerFunc callback);

    /**
    * @brief emit an event and trigger all its associated callbacks within the current context and thread
    * 
    * @param event event to be triggered
    * @param data user-defined data that passes to every callback function
    * @return status code
    * 
    */
    hvaStatus_t emitEvent(hvaEvent_t event, void* data);

    /**
    * @brief blocks and wait until the event is triggered
    * 
    * @param event event to be waited
    * @return status code
    * 
    */
    hvaStatus_t waitForEvent(hvaEvent_t event);

    /**
    * @brief register a Linux system signal handler within the pipeline
    * 
    * @param signum the standard Linux system signal
    * @param sigHandler the signal handler function
    * @return status code
    * 
    */
    //hvaStatus_t registerSystemSignalHandler(int signum, hvaSystemSignalHandler sigHandler);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif //#ifndef HVA_HVAPIPELINE_HPP