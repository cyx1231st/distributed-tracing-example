#ifndef DISTTRACING_TRACERPLUGIN
#define DISTTRACING_TRACERPLUGIN

#include <string>
#include <memory>

#include <opentracing/dynamic_load.h>

class TracerPlugin{
  public:
    explicit TracerPlugin(std::string name, std::string conf);
    virtual ~TracerPlugin();
    bool is_enabled() const { return plugin_enabled; }
  private:
    const std::string library_name;
    const std::string config;
    std::unique_ptr<const opentracing::DynamicTracingLibraryHandle>
        library_handle;
    bool plugin_enabled = false;
};

#endif
