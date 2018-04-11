macro(build_opentracing)
    include(ExternalProject)
    ExternalProject_Add(opentracing-ext
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rd/opentracing-cpp
        BINARY_DIR ${CMAKE_BINARY_DIR}/3rd/opentracing-cpp
        BUILD_COMMAND $(MAKE) opentracing opentracing_mocktracer
        INSTALL_COMMAND "true")
    set(OPENTRACING_INCLUDE_DIR
        ${CMAKE_SOURCE_DIR}/3rd/opentracing-cpp/include)
    set(OPENTRACING3RD_INCLUDE_DIR
        ${CMAKE_SOURCE_DIR}/3rd/opentracing-cpp/3rd_party/include)
    set(OPENTRACINGBUILT_INCLUDE_DIR
        ${CMAKE_BINARY_DIR}/3rd/opentracing-cpp/include)
    set(MOCKTRACER_INCLUDE_DIR
        ${CMAKE_SOURCE_DIR}/3rd/opentracing-cpp/mocktracer/include)
    include_directories(
        ${OPENTRACING_INCLUDE_DIR}
        ${OPENTRACING3RD_INCLUDE_DIR}
        ${OPENTRACINGBUILT_INCLUDE_DIR}
        ${MOCKTRACER_INCLUDE_DIR})
    add_library(opentracing SHARED IMPORTED)
    set_target_properties(opentracing PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/3rd/opentracing-cpp/libopentracing.so")
    add_dependencies(opentracing opentracing-ext)
    add_custom_command(
        TARGET opentracing-ext POST_BUILD
        COMMAND cp
        ${CMAKE_BINARY_DIR}/3rd/opentracing-cpp/mocktracer/libopentracing_mocktracer.so
        ${CMAKE_BINARY_DIR}/lib/libopentracing_mocktracer.so)
endmacro()
