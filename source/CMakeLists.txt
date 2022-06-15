add_subdirectory(common)
add_subdirectory(dataPathTestWithMeta)

# add_demo(NAME <target name>
#     SOURCES <source files>
#     [HEADERS <header files>]
#     [INCLUDE_DIRECTORIES <include dir>]
#     [OPENCV_VERSION_REQUIRED <X.Y.Z>]
#     [DEPENDENCIES <dependencies>])
macro(add_demo)
    set(oneValueArgs NAME OPENCV_VERSION_REQUIRED)
    set(multiValueArgs SOURCES HEADERS DEPENDENCIES INCLUDE_DIRECTORIES)
    cmake_parse_arguments(OMZ_DEMO "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    if(OMZ_DEMO_OPENCV_VERSION_REQUIRED AND OpenCV_VERSION VERSION_LESS OMZ_DEMO_OPENCV_VERSION_REQUIRED)
        message(WARNING "${OMZ_DEMO_NAME} is disabled; required OpenCV version ${OMZ_DEMO_OPENCV_VERSION_REQUIRED}, provided ${OpenCV_VERSION}")
        return()
    endif()

    # Create named folders for the sources within the .vcproj
    # Empty name lists them directly under the .vcproj
    source_group("src" FILES ${OMZ_DEMO_SOURCES})
    if(OMZ_DEMO_HEADERS)
        source_group("include" FILES ${OMZ_DEMO_HEADERS})
    endif()

    # Create executable file from sources
    add_executable(${OMZ_DEMO_NAME} ${OMZ_DEMO_SOURCES} ${OMZ_DEMO_HEADERS})

    if(WIN32)
        set_target_properties(${OMZ_DEMO_NAME} PROPERTIES COMPILE_PDB_NAME ${OMZ_DEMO_NAME})
    endif()

    if(OMZ_DEMO_INCLUDE_DIRECTORIES)
        target_include_directories(${OMZ_DEMO_NAME} PRIVATE ${OMZ_DEMO_INCLUDE_DIRECTORIES})
    endif()

    target_link_libraries(${OMZ_DEMO_NAME} PRIVATE ${OpenCV_LIBRARIES} openvino::runtime
                                                    ${OMZ_DEMO_DEPENDENCIES} utils gflags)

    if(UNIX)
        target_link_libraries(${OMZ_DEMO_NAME} PRIVATE pthread)
    endif()
endmacro()

add_subdirectory(object_detection_demo)