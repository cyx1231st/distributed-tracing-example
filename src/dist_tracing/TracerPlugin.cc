#include "TracerPlugin.h"
#include <iostream>
using namespace std;

TracerPlugin::TracerPlugin(string name, string conf) : library_name(name), config(conf) {
    string error_msg;
    auto library_handle_maybe =
        opentracing::DynamicallyLoadTracingLibrary(
            name.c_str(), error_msg);
    if(!library_handle_maybe){
        cout << "Failed to load library " << name << ": ";
        if(!error_msg.empty())
            cout << error_msg << endl;
        else
            cout << library_handle_maybe.error().message() << endl;
        plugin_enabled = false;
        return;
    }
    library_handle.reset(new opentracing::DynamicTracingLibraryHandle(
                std::move(*library_handle_maybe)));

    error_msg.clear();
    auto tracer_maybe = library_handle->tracer_factory().MakeTracer(conf.c_str(), error_msg);
    if(!tracer_maybe){
        cout << "Failed to construct tracer from library " << name << ": ";
        if(!error_msg.empty())
            cout << error_msg << endl;
        else
            cout << tracer_maybe.error().message() << endl;
        plugin_enabled = false;
        return;
    }
    auto tracer = *tracer_maybe;
    cout << "Tracer loaded: mocktracer " << tracer << endl << endl;
    opentracing::Tracer::InitGlobal(move(tracer));
    plugin_enabled = true;
}

TracerPlugin::~TracerPlugin() {
    auto tracer = opentracing::Tracer::InitGlobal(nullptr);
    if (tracer != nullptr) {
        tracer->Close();
        tracer.reset();
    }
    if (library_handle != nullptr)
        library_handle.reset();
}
