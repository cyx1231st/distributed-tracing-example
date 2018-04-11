cmake_minimum_required(VERSION 2.8.12)

project(tracing_test CXX C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules)

#lib opentracing-cpp
include(BuildOpenTracing)
build_opentracing()

#lib jaegertracing
include(BuildJaegerClient)
build_jaegerclient()

include_directories(
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src)
