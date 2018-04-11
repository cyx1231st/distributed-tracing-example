# Distributed tracing example
This repo is based on OpenTracing with the following features:
* A demo to simulate writes across Ceph cluster (From clients to OSDs with replications).
* The usage of pure OpenTracing API and illustration of OpenTracing trace model.
* Illustration of trace features:
  - "All or nothing" tracing: there will be no traces if SpanContext is not propagated at request start.
  - Trace metadata propagation across queues/threads/processes.
  - OpenTracing logging and tagging.
  - Trace sampling.
  - Browse tracings with Jaeger.
* Tracer plugins are switchable (JaegerTracer and MockTracer)

# Steps

## Build the source code
```bash
./build.sh
```

## Prepare Jaeger environment
* All-in-one Jaeger environment:
```bash
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
```
* Web UI will available at 127.0.0.1:16686 immediately
* Tracer clients (i.e. this demo) should report to 127.0.0.1:6831

## Run the demo
```bash
cd build
LD_LIBRARY_PATH=./lib ./bin/run <options>
```
options:
* help: -h
* jaeger-reporter url: -url=<url>, default to 127.0.0.1:6831
* switch from jaeger-tracer to mock-tracer plugin: -local
* the simplest demo: -e
* Ceph simulator demo settings
  - -osd=<num_osds>, default to 10
  - -client=<num_clients>, default to 1
  - -msg=<num_msgs_per_client>, default to 1
  - -replica=<num_replicas>, default to 3

## Browse tracings
* Access 127.0.0.1:16686 after running the demo.
* Or, if "-local" option is used, the tracings will be available in `mock_traces.log`
