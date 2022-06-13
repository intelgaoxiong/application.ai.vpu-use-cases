//
//Copyright (C) 2020 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#ifndef HVA_HVAEVENT_HPP
#define HVA_HVAEVENT_HPP

#include <cstdint>
#include <functional>

namespace hva{

typedef uint64_t hvaEvent_t;

#define hvaEvent_Null 0x0ull

/**
* @brief function prototype for event handler
* 
* @param void* the user-defined data which will be passed to each callback function
* @return boolean on success status
* 
*/
using hvaEventHandlerFunc = std::function<bool(void*)>;

}

#endif //#ifndef HVA_HVAEVENT_HPP
