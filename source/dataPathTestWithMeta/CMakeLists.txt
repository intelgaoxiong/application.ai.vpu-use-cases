link_directories("${PROJECT_SOURCE_DIR}/thirdparty/hvaframework/lib")

add_executable(dataPathTestWithMeta dataPathTestWithMeta.cpp)

target_include_directories(dataPathTestWithMeta PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
                                                       "${PROJECT_SOURCE_DIR}/thirdparty/hvaframework")

target_link_libraries(dataPathTestWithMeta ade hva)