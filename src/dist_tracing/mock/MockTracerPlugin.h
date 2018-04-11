#ifndef DISTTRACING_MOCKTRACERPLUGIN
#define DISTTRACING_MOCKTRACERPLUGIN

#include "dist_tracing/TracerPlugin.h"
#include <memory>

std::unique_ptr<TracerPlugin> create_mocktracer_plugin() {
    return std::make_unique<TracerPlugin>(
            "libopentracing_mocktracer.so",
            R"({"output_file": "mock_traces.log"})");
}

#endif
