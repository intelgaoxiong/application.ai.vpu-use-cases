//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAEVENTMANAGER_HPP
#define HVA_HVAEVENTMANAGER_HPP

#include <inc/api/hvaEvent.hpp> 
#include <inc/util/hvaUtil.hpp>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <unordered_set>

namespace hva{

// hvaEventManager manages all those hvaEvent_t associated in a pipeline. Note that each pipeline will
//  have its own manager
class hvaEventManager_t{
public:
    hvaEventManager_t();

    ~hvaEventManager_t();

    /**
    * @brief register a hvaEvent_t to this pipeline. Non-registered event will incur an error upon raising
    * 
    * @param event the event to be registered
    * @return status code 
    * 
    */
    hvaStatus_t registerEvent(hvaEvent_t event);

    /**
    * @brief register the callback function associated with an event. This event must be registered in this
    *           pipeline before registering a callback
    * 
    * @param event the event that callback is associated to
    * @param callback the callback function
    * @return status code
    * 
    */
    hvaStatus_t registerCallback(hvaEvent_t event, hvaEventHandlerFunc callback);

    /**
    * @brief emit an event. This event must be registered in this pipeline. This will trigger all registered 
    *           callbacks on this event in current context and current thread
    * 
    * @param event the event to raise
    * @param data the user-defined data associated with this event which will be passed to the defined event
    *           handler
    * @return status code
    * 
    */
    hvaStatus_t emitEvent(hvaEvent_t event, void* data);

    /**
    * @brief wait and block here until an event is raised
    * 
    * @param event the event to be waited
    * @return status code
    * 
    */
    hvaStatus_t waitForEvent(hvaEvent_t event);
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif //#ifndef HVA_HVAEVENTMANAGER_HPP