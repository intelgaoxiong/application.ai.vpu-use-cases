//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVANODE_HPP
#define HVA_HVANODE_HPP

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <inc/api/hvaBlob.hpp>
#include <inc/util/hvaUtil.hpp>
#include <inc/api/hvaEvent.hpp>
#include <inc/api/hvaEventManager.hpp>
#include <list>
#include <atomic>

namespace hva{

enum hvaPortPolicy_t{
    HVA_BLOCKING_IF_FULL = 0,   // block the call if port is full 
    HVA_DISCARD_IF_FULL         // discard the content and return the call if port is full
};

using hvaConvertFunc = std::function<std::shared_ptr<hvaBlob_t>(std::shared_ptr<hvaBlob_t>)>;
using hvaDataQueue_t = std::list<std::shared_ptr<hvaBlob_t>>;

class hvaInPort_t;
class hvaOutPort_t;
class hvaNode_t;

// hvaInPort_t and hvaOutPort_t are structs that connects each pair of successive nodes. Besides
//  hvaInPort_t additionally holds a buffer queue where all input buffers are stored
class hvaInPort_t{
friend class hvaPipeline_t;
friend class hvaNode_t;
friend class hvaBatchingConfig_t;
public:
    hvaInPort_t(hvaNode_t* parentNode, hvaOutPort_t* prevPort, size_t queueSize = 1024);

    hvaInPort_t(hvaNode_t* parentNode);

    hvaStatus_t tryPush(std::shared_ptr<hvaBlob_t> data);

    /**
    * @brief push a blob to this port's buffer queue
    * 
    * @param data blob to be pushed
    * @param timeout timeout before this call returns. Set to 0 to make it blocking forever
    * @return status code
    * 
    */
    hvaStatus_t push(std::shared_ptr<hvaBlob_t> data, ms timeout = ms(0));

    void clear();

    void setPortQueuePolicy(hvaPortPolicy_t policy);

    void transitStateTo(hvaState_t state);

private:
    void setPrevPort(hvaOutPort_t* prevPort);

    std::vector<std::shared_ptr<std::mutex>> m_vecMutex;
    std::vector<std::shared_ptr<std::condition_variable>> m_vecCvEmpty;
    std::vector<std::shared_ptr<std::condition_variable>> m_vecCvFull;
    const size_t m_queueSize; // todo: impl a setQueueSize and make queuesize non-const
    hvaPortPolicy_t m_policy;
    hvaNode_t* m_parentNode;
    hvaOutPort_t* m_prevPort;
    std::vector<hvaDataQueue_t> m_vecInQueue;
    hvaState_t m_state;
};

class hvaPipeline_t;

// hvaInPort_t and hvaOutPort_t are structs that connects each pair of successive nodes. Besides
//  hvaOutPort_t holds a function pointer used for user-defined blob conversion function in case
//  the blob transmitted between two successive nodes does not match
class hvaOutPort_t{
friend class hvaPipeline_t;
public:
    hvaOutPort_t(hvaNode_t* parentNode, hvaInPort_t* nextPort);

    hvaOutPort_t(hvaNode_t* parentNode);

    hvaInPort_t* getNextPort();

    /**
    * @brief call the holded blob conversion function
    * 
    * @param data the blob to be converted 
    * @return a shared pointer to the blob after conversion
    * 
    */
    std::shared_ptr<hvaBlob_t> convert(std::shared_ptr<hvaBlob_t> data) const;

    /**
    * @brief check if the holded conversion function is valid
    * 
    * @param void
    * @return true if valid
    * 
    */
    bool isConvertValid() const;

private:
    void setNextPort(hvaInPort_t* nextPort);

    void setConvertFunc(hvaConvertFunc func);

    hvaNode_t* m_parentNode;
    hvaInPort_t* m_nextPort;
    hvaConvertFunc m_convertFunc;
};

class hvaBatchingConfig_t{
public:
    enum BatchingPolicy : unsigned{
        BatchingIgnoringStream = 0x1,   // batching algorithm ignoring the blob's stream id. default

        BatchingWithStream = 0x2,       // if set to BatchingWithStream, node workers only fetches the
                                        //  blobs corrosponding to its assigned batch index. Note that
                                        //  according to user definition, a single batch index may refer 
                                        //  to multiple stream id
        Reserved = 0x4
    };

    hvaBatchingConfig_t();

    unsigned batchingPolicy;
    std::size_t batchSize;
    std::size_t streamNum;
    std::size_t threadNumPerBatch;

    /**
    * @brief function pointer to batching algorithm
    *           batching algorithm could be default batching or provided by user
    *           batching algorithm should control mutex and cv
    * 
    * @param batchIdx batch index
    * @param vPortIdx vector of port index
    * @param pNode pointer to node
    * @return output blob vector(when batching fail, vector is empty)
    * 
    */
    std::function<std::vector<std::shared_ptr<hvaBlob_t> > (std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode)> batchingAlgo = nullptr;

    static std::vector<std::shared_ptr<hvaBlob_t> > defaultBatching(std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode);

    static std::vector<std::shared_ptr<hvaBlob_t> > streamBatching(std::size_t batchIdx, std::vector<std::size_t> vPortIdx, hvaNode_t* pNode);
};

class hvaNodeWorker_t;

// hvaNode_t, different from hvaNodeWorker_t, is the struct that stores a pipeline node's topological
//  information, e.g. nodes it connects to, as well as some common utilities that will be shared across
//  the node workers it spawned. A node may spawn multiple copies of node workers it associates to
class hvaNode_t{
friend class hvaPipeline_t;
friend class hvaBatchingConfig_t;
friend class hvaInPort_t;
public:
    
    /**
    * @brief constructor of a node
    * 
    * @param inPortNum number of in ports this node should have. Note that those ports can leave as 
    *           not-connected
    * @param outPortNum number of out ports this node should have. Note that those ports can leave as 
    *           not-connected
    * @param totalThreadNum number of workers it should spawn
    * @return void
    * 
    */
    hvaNode_t(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum);

    virtual ~hvaNode_t();

    /**
    * @brief Constructs and returns a nodeworker instance. Users are expected to 
    *           implement this function while extending node class. 
    * 
    * @param void
    * @return share pointer to the node worker constructed
    * 
    */
    virtual std::shared_ptr<hvaNodeWorker_t> createNodeWorker() const = 0;

    virtual void configBatch(const hvaBatchingConfig_t& config);
    virtual void configBatch(hvaBatchingConfig_t&& config);

    /**
    * @brief configure the time interval between each successive call of process. This defaults 
    *           to zero if not called and thus will free-wheeling or blocked on getBatchedInput
    * 
    * @param interval time interval in ms
    * @return void 
    * 
    */
    void configLoopingInterval(ms interval);

    /**
    * @brief get inputs stored in node's in port. For active nodes this call is set to blocking while
    *           for passive nodes this would set to non-blocking
    * 
    * @param batchIdx the batch index that node would like fetch. This usually sets to the argument passed
    *           from process() call
    * @return vector of shared pointer to blobs fetched. If no requested blobs found in port, this will 
    *           return an empty vector
    * 
    */
    virtual std::vector<std::shared_ptr<hvaBlob_t>> getBatchedInput(std::size_t batchIdx, std::vector<std::size_t> vPortIdx);

    std::size_t getInPortNum();

    std::size_t getOutPortNum();

    void clearAllPorts();

    /**
    * @brief send output to the next node connected at port index
    * 
    * @param data blob to be sent
    * @param portId the port index blob would be sent to
    * @param timeout the maximal timeout before this function returns. Set to zero to make it blocking
    * @return status code
    * 
    */
    hvaStatus_t sendOutput(std::shared_ptr<hvaBlob_t> data, std::size_t portId, ms timeout);

    std::size_t getTotalThreadNum();

    const hvaBatchingConfig_t* getBatchingConfigPtr();

    const hvaBatchingConfig_t& getBatchingConfig();

    ms getLoopingInterval() const;
    
    /**
    * @brief stop the getting batched input logic. Once this is stoped, every following getBatchedInput()
    *           will return an empty vector
    * 
    * @param void
    * @return void
    * 
    */
    void stopBatching();

    void turnOnBatching();

    /**
    * @brief register a callback that this node would like to listen on
    * 
    * @param event the callback associates to
    * @param callback callback function to be invoked with this event
    * @return status code
    * 
    */
    hvaStatus_t registerCallback(hvaEvent_t event, hvaEventHandlerFunc callback);

    /**
    * @brief emit an event within the node. This event will populate throughout the entire pipeline
    * 
    * @param event event to be emitted
    * @param data user-defined data to be passed to each callback function
    * @return status code
    * 
    */
    hvaStatus_t emitEvent(hvaEvent_t event, void* data);

    void setEventManager(hvaEventManager_t* evMng);

    const std::unordered_map<hvaEvent_t,hvaEventHandlerFunc>* getCallbackMap() const;

    void transitStateTo(hvaState_t state);

private:
    static std::size_t m_workerCtr;

    hvaOutPort_t& getOutputPort(std::size_t portIdx);
    hvaInPort_t& getInputPort(std::size_t portIdx);
    
    std::vector<std::unique_ptr<hvaInPort_t>> m_inPorts;
    std::vector<std::unique_ptr<hvaOutPort_t>> m_outPorts;

    const std::size_t m_inPortNum;
    const std::size_t m_outPortNum;
    hvaBatchingConfig_t m_batchingConfig;
    std::unordered_map<int, int> m_mapStream2LastFrameId;
    std::size_t m_totalThreadNum;

    ms m_loopingInterval;
    std::vector<std::shared_ptr<std::mutex>> m_vecBatchingMutex;
    std::vector<std::shared_ptr<std::condition_variable>>  m_vecBatchingCv;
    std::atomic_bool m_batchingStoped;

    hvaEventManager_t* m_pEventMng;
    std::unordered_map<hvaEvent_t,hvaEventHandlerFunc> m_callbackMap;

    hvaState_t m_state;
};

// hvaNodeWorker_t is the stuct that actually does the workload within its process() function. 
//  Node workers are spawned from nodes, where all workers spawned from the same node share every
//  thing within the node struct, but will NOT share anything within the node worker itself
class hvaNodeWorker_t{
public:
    hvaNodeWorker_t(hvaNode_t* parentNode);

    virtual ~hvaNodeWorker_t();

    /**
    * @brief the function that main workload should conduct at. This function will be invoked by framework
    *           repeatedly and in parrallel with each other node worker's process() function
    * 
    * @param batchIdx a batch index that framework will assign to each node worker. This is usually
    *           passed to getBatchedInput()
    * @return void
    * 
    */
    virtual void process(std::size_t batchIdx) = 0;

    /**
    * @brief specialization of process() where this function will be called only once before the usual
    *           process() being called
    * 
    * @param batchIdx a batch index that framework will assign to each node worker. This is usually
    *           passed to getBatchedInput()
    * @return void
    * 
    */
    virtual void processByFirstRun(std::size_t batchIdx);

    /**
    * @brief specialization of process() where this function will be called only once after all other calls
    *           of process()
    * 
    * @param batchIdx a batch index that framework will assign to each node worker. This is usually
    *           passed to getBatchedInput()
    * @return void
    * 
    */
    virtual void processByLastRun(std::size_t batchIdx);

    /**
    * @brief initialization of this node. This is called sequentially by the framework at the very beginning
    *           of pipeline begins
    * 
    * @param void
    * @return void
    * 
    */
    virtual void init();

    /**
    * @brief deinitialization of this node. This is called by framework after stop() being called
    * 
    * @param void
    * @return void
    * 
    */
    virtual void deinit();

    hvaNode_t* getParentPtr() const;

    bool isStopped() const;
protected:
    
    /**
    * @brief send output to the next node connected at port index
    * 
    * @param data blob to be sent
    * @param portId the port index blob would be sent to
    * @param timeout the maximal timeout before this function returns. Set to zero to make it blocking
    * @return status code
    * 
    */
    hvaStatus_t sendOutput(std::shared_ptr<hvaBlob_t> data, std::size_t portId, ms timeout = ms(1000));

    void breakProcessLoop();

private:
    hvaNode_t* m_parentNode;
    volatile bool m_internalStop;
};


}


#endif //#ifndef HVA_HVANODE_HPP