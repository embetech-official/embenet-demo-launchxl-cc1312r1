cmake_minimum_required(VERSION 3.21)


project(embenet_node_port_cc1312 VERSION 1.0.0 LANGUAGES C)

option(EMBENET_NODE_PORT_CC1312_BUILD_TESTS "Forces building test cases" OFF)

include(cmake/simplelink_sdk_utils.cmake)
add_subdirectory(src)

if (PROJECT_IS_TOP_LEVEL OR EMBENET_NODE_PORT_CC1312_BUILD_TESTS)
  add_subdirectory(tests)
endif ()
