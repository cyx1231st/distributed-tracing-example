macro(build_jaegerclient)
    include(ExternalProject)
    ExternalProject_Add(jaegerclient-ext
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rd/jaeger-client-cpp
        BINARY_DIR ${CMAKE_BINARY_DIR}/3rd/jaeger-client-cpp
        BUILD_COMMAND $(MAKE) jaegertracing
        INSTALL_COMMAND "true")
    add_custom_command(
        TARGET jaegerclient-ext POST_BUILD
        COMMAND cp
        ${CMAKE_BINARY_DIR}/3rd/jaeger-client-cpp/libjaegertracing.so
        ${CMAKE_BINARY_DIR}/lib/libjaegertracing.so)
endmacro()
