#ifndef DISTTRACING_JAEGERTRACERPLUGIN
#define DISTTRACING_JAEGERTRACERPLUGIN

#include "dist_tracing/TracerPlugin.h"
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
/*
 * all-in-one Jaeger:
 * navigate to http://localhost:16686 to access the Jaeger UI

docker run -d -e \
  COLLECTOR_ZIPKIN_HTTP_PORT=9411 \
  -p 5775:5775/udp \
  -p 6831:6831/udp \
  -p 6832:6832/udp \
  -p 5778:5778 \
  -p 16686:16686 \
  -p 14268:14268 \
  -p 9411:9411 \
  jaegertracing/all-in-one:latest

 * [configuration]
 * sampler
 *   type: const, probabilistic, ratelimiting, *remote
 *   param: 0.001
 *   - 0/1 (const)
 *   - [0,1] (probabilistic)
 *   - traces per second (ratelimiting)
 *   - [0,1] (remote)
 *   samplingServerURL: http://127.0.0.1:5778
 *   maxOperations: 2000
 *   samplingRefreshInterval: 60 (seconds)
 * reporter
 *   queueSize: 100 (spans)
 *   bufferFlushInterval: 10 (seconds)
 *   logSpans: false
 *   localAgentHostPort: 127.0.0.1:6831
 * headers (in HTTP header or TextMapCarrier)
 *   jaegerDebugHeader
 *   jaegerBaggageHeaderName
 *   TraceContextHeaderName
 *   traceBaggageHeaderPrefix
 * baggage_restrictions
 */
std::unique_ptr<TracerPlugin> create_jaegertracer_plugin(std::string &url) {
    std::string reporter_url = "127.0.0.1:6831";
    if(!url.empty())
        reporter_url = url;
    std::cout << "Jaeger report to: " << reporter_url << std::endl;
    std::ostringstream oss_config;
    oss_config << R"(
  {
    "service_name": "test",
    "disabled": false,
    "sampler": {
      "type": "const",
      "param": 1,
      "samplingServerURL": "",
      "maxOperations": 0,
      "samplingRefreshInterval": 60
    },
    "reporter": {
      "queueSize": 100,
      "bufferFlushInterval": 10,
      "logSpans": false,
      "localAgentHostPort": ")" << reporter_url << R"("
    },
    "headers": {
      "jaegerDebugHeader": "debug-id",
      "jaegerBaggageHeader": "baggage",
      "TraceContextHeaderName": "trace-id",
      "traceBaggageHeaderPrefix": "testctx-"
    },
    "baggage_restrictions": {
        "denyBaggageOnInitializationFailure": false,
        "hostPort": "127.0.0.1:5778",
        "refreshInterval": 60
    }
  })";
    return std::make_unique<TracerPlugin>(
            "libjaegertracing.so",
            oss_config.str());
}

#endif
