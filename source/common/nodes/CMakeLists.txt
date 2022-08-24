# Copyright (C) 2018-2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

file(GLOB SOURCES ./src/*.cpp)
file(GLOB HEADERS ./include/*.h)

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj
source_group("src" FILES ${SOURCES})
source_group("include" FILES ${HEADERS})

add_library(nodes STATIC ${SOURCES} ${HEADERS})
target_include_directories(nodes PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/thirdparty/hvaframework" "${CMAKE_CURRENT_SOURCE_DIR}/../common")
target_link_libraries(nodes PRIVATE pipelines models gflags openvino::runtime utils opencv_core opencv_imgproc)