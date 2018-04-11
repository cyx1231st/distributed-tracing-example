add_subdirectory(mock)
add_subdirectory(jaeger)

set(disttracing_srcs
    TracerPlugin.cc
    Carrier.cc)
add_library(disttracing STATIC ${disttracing_srcs})
target_link_libraries(disttracing opentracing)
