cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(VpuUseCases)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

# Dependency HVA framework binary.
#if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hvaframework/lib/libhva.so")
#  message(FATAL_ERROR "Please build HVA framework and copy to 'gsthvasample/3rdparty/hvaframework/', please refer 'gsthvasample/3rdparty/hvaframework/README.md' to build hav framework.")
#endif()

find_package(OpenCV REQUIRED COMPONENTS core highgui videoio imgproc imgcodecs gapi)
find_package(OpenVINO REQUIRED COMPONENTS Runtime)
find_package(Open3D REQUIRED)

add_subdirectory(thirdparty/gflags)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/hvaframework/include/hva"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/hvaframework/include/hva/inc/api"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/hvaframework/include/hva/inc/util"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/hvaframework/include/ade"
  )
include_directories("${Open3D_ROOT}/include" "${Open3D_ROOT}/include/open3d/3rdparty")

link_directories("${Open3D_ROOT}/bin")
link_libraries(Open3D::Open3D)

add_subdirectory(source)