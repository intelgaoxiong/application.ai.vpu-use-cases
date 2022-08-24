# Copyright (C) 2018-2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

file(GLOB SRC_FILES ./*.cpp)
file(GLOB H_FILES ./*.h)

add_demo(NAME midas_demo
    SOURCES ${SRC_FILES}
    HEADERS ${H_FILES}
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/thirdparty/hvaframework ${PROJECT_SOURCE_DIR}/source/common
    DEPENDENCIES nodes monitors models pipelines ade hva)
