#include "Carrier.h"
#include <opentracing/tracer.h>

using namespace std;
using namespace opentracing;

class TextMapCarrier : public TextMapReader, public TextMapWriter {
 public:
  TextMapCarrier(unordered_map<string, string>& text_map)
      : text_map_(text_map) {}

  expected<void> Set(string_view key, string_view value) const override {
    text_map_[key] = value;
    return {};
  }

  expected<void> ForeachKey(
      function<expected<void>(string_view key, string_view value)> f)
      const override {
    for (const auto& key_value : text_map_) {
      auto result = f(key_value.first, key_value.second);
      if (!result) return result;
    }
    return {};
  }

 private:
  unordered_map<string, string>& text_map_;
};

void Inject_map(const SpanContext& sc,
                unordered_map<string, string> &trace_meta) {
    TextMapCarrier carrier(trace_meta);
    Tracer::Global()->Inject(sc, carrier);
}

unique_ptr<SpanContext> Extract_map(unordered_map<string, string> &trace_meta) {
    TextMapCarrier carrier(trace_meta);
    auto ret = *Tracer::Global()->Extract(carrier);
    return ret;
}
