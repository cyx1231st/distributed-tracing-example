#ifndef DISTTRACING_CARRIER
#define DISTTRACING_CARRIER

#include <opentracing/span.h>
#include <string>
#include <memory>
#include <unordered_map>

void Inject_map(const opentracing::SpanContext& sc,
                std::unordered_map<std::string, std::string> &trace_meta);

std::unique_ptr<opentracing::SpanContext> Extract_map(
        std::unordered_map<std::string, std::string> &trace_meta);

#endif  // DISTTRACING_CARRIER
