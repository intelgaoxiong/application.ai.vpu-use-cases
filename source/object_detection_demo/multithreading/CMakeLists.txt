# Copyright (C) 2018-2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

file(GLOB SRC_FILES ./*.cpp)
file(GLOB H_FILES ./*.h)

add_demo(NAME multi_threading_object_detection_demo
    SOURCES ${SRC_FILES}
    HEADERS ${H_FILES}
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/thirdparty/hvaframework
    DEPENDENCIES monitors models pipelines ade hva)
