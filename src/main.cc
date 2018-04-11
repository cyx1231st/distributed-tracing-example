#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <opentracing/tracer.h>
#include "dist_tracing/Carrier.h"
#include "dist_tracing/mock/MockTracerPlugin.h"
#include "dist_tracing/jaeger/JaegerTracerPlugin.h"

using namespace std;
using namespace opentracing;

// global settings
unsigned int num_clients = 1;
unsigned int num_osds = 10;
unsigned int num_replicas = 3;
unsigned int num_msgs = 1;

// random utilities
random_device rd;

unsigned int random_select_osd(){
    static uniform_int_distribution<unsigned int> dist(0, num_osds-1);
    return dist(rd);
}

void random_select_replicas(vector<unsigned int> &replicas, unsigned int osd_id){
    assert(num_replicas <= num_osds);
    assert(osd_id < num_osds);

    replicas.resize(num_replicas-1);
    vector<unsigned int> choices(num_osds-1);
    int j = 0;
    for(unsigned int i=0; i<num_osds; ++i){
        if(i == osd_id)
            continue;
        else{
            choices[j] = i;
            ++j;
        }
    }
    for(unsigned int i=0; i<num_replicas-1; ++i){
        uniform_int_distribution<unsigned int> dist_replica(i, num_osds-2);
        unsigned int choice = dist_replica(rd);
        swap(choices[i], choices[choice]);
    }
    for(unsigned int i=0; i<num_replicas; ++i)
        replicas[i] = choices[i];
}

void random_process(){
    static uniform_int_distribution<unsigned int> dist(10, 50);
    this_thread::sleep_for(chrono::milliseconds(dist(rd)));
}

// messaging
struct Message{
  public:
    string payload;
    unordered_map<string, string> trace_meta;
};

struct Buffer{
  public:
    void push(Message *msg, unsigned int source_id){
        unique_lock<mutex> locker(mu);
        buffer_.push(make_pair(msg, source_id));
        locker.unlock();
    }
    pair<Message*, unsigned int> pop(){
        unique_lock<mutex> locker(mu);
        if(buffer_.empty()){
            locker.unlock();
            return make_pair(nullptr, 88888);
        }else{
            auto ret = buffer_.front();
            buffer_.pop();
            locker.unlock();
            return ret;
        }
    }
  private:
    mutex mu;
    queue<pair<Message*, unsigned int>> buffer_;
};

vector<Buffer> bufs_write(num_osds);
vector<Buffer> bufs_replica(num_osds);
vector<Buffer> bufs_replica_reply(num_osds);
vector<Buffer> bufs_write_reply(num_clients);

void send_osd(unsigned int source_client, unsigned int osd_id, Message *msg,
        const SpanContext *sc_maybe = nullptr){
    shared_ptr<Span> span;
    if(sc_maybe)
        span = Tracer::Global()->StartSpan("send_osd", {ChildOf(sc_maybe)});

    random_process();
    
    if(span){
        Inject_map(span->context(), msg->trace_meta);
        ostringstream os_send;
        os_send << "client" << source_client;
        span->Log({{"SEND_FROM", os_send.str()}});
    }
    bufs_write[osd_id].push(msg, source_client);

    if(span) span->Finish();
}

void send_osd_reply(unsigned int source_osd, unsigned int client_id, Message *msg,
        const SpanContext *sc_maybe = nullptr){
    shared_ptr<Span> span;
    if(sc_maybe)
        span = Tracer::Global()->StartSpan("send_osd_reply", {ChildOf(sc_maybe)});

    random_process();
    if(span){
        Inject_map(span->context(), msg->trace_meta);
        ostringstream os_send;
        os_send << "osd" << source_osd;
        span->Log({{"SEND_FROM", os_send.str()}});
    }
    bufs_write_reply[client_id].push(msg, source_osd);

    if(span) span->Finish();
}


void send_replica(unsigned int source_osd, vector<unsigned int> &replicas, Message *msg,
        const SpanContext *sc_maybe = nullptr){
    shared_ptr<Span> span;
    if(sc_maybe)
        span = Tracer::Global()->StartSpan("send_replica", {ChildOf(sc_maybe)});

    random_process();
    if(span){
        Inject_map(span->context(), msg->trace_meta);
        ostringstream os_send;
        os_send << "osd" << source_osd;
        span->Log({{"SEND_FROM", os_send.str()}});
    }
    for(auto &i: replicas)
        bufs_replica[i].push(msg, source_osd);

    if(span) span->Finish();
}

void send_replica_reply(unsigned int source_osd, unsigned int osd_id, Message *pmsg,
        const SpanContext *sc_maybe = nullptr){
    shared_ptr<Span> span;
    if(sc_maybe)
        span = Tracer::Global()->StartSpan("send_replica_reply", {ChildOf(sc_maybe)});

    random_process();
    if(span){
        Inject_map(span->context(), pmsg->trace_meta);
        ostringstream os_send;
        os_send << "osd" << source_osd;
        span->Log({{"SEND_FROM", os_send.str()}});
    }
    bufs_replica_reply[osd_id].push(pmsg, source_osd);

    if(span) span->Finish();
}

// client/osd operations
void do_client_write(unsigned int client_id, unsigned int msg_id, Message *pmsg){
    shared_ptr<Span> span;
/*
 * switch on/off here
 */
    span = Tracer::Global()->StartSpan("do_client_write");

    random_process();
    stringstream ss;
    ss << client_id << ":" << msg_id;
    pmsg->payload = ss.str();
    if(span)
        span->SetTag("request_id", pmsg->payload);
    unsigned int osd_id = random_select_osd();
    if(span)
        span->SetTag("primary_osd", osd_id);
    stringstream sdbg;
    sdbg << "[" << pmsg->payload << "@client" << client_id << "] write start -> osd" << osd_id << "\n";
    cout << sdbg.str();
    if(span)
        send_osd(client_id, osd_id, pmsg, &span->context());
    else
        send_osd(client_id, osd_id, pmsg);
    random_process();

    if(span) span->Finish();
}

void do_client_reply(unsigned int client_id, Message *pmsg){
    auto sc_maybe = Extract_map(pmsg->trace_meta);
    shared_ptr<Span> span;
    if(sc_maybe){
        ostringstream os_rcv;
        os_rcv << "client" << client_id;
        span = Tracer::Global()->StartSpan(
                "do_client_reply",
                {ChildOf(&*sc_maybe),
                 SetTag("RECEIVED_BY", os_rcv.str())});
    }

    stringstream sdbg;
    sdbg << "[" << pmsg->payload << "@client" << client_id << "] write finish\n";
    cout << sdbg.str();
    random_process();

    if(span) span->Finish();
}

struct QueueItem {
    unsigned int reply_osd;
    Message *pmsg;
    unordered_map<string, string> tracing_meta;
};

#define LOCAL 99999
void do_osd_write(unsigned int osd_id, unsigned int source_client, Message *pmsg,
        vector<QueueItem> &pending_writes,
        unordered_map<Message *, pair<unsigned int, unsigned int>> &pending_replies){
    auto sc_maybe = Extract_map(pmsg->trace_meta);
    shared_ptr<Span> span;
    if(sc_maybe){
        ostringstream os_rcv;
        os_rcv << "osd" << osd_id;
        span = Tracer::Global()->StartSpan(
                "do_osd_write",
                {ChildOf(&*sc_maybe),
                 SetTag("RECEIVED_BY", os_rcv.str())});
    }

    random_process();
    vector<unsigned int> replicas;
    random_select_replicas(replicas, osd_id);
    ostringstream sdbg;
    sdbg << "[" << pmsg->payload << "@osd" << osd_id << "] replica start -> osd";
    for(auto i: replicas)
        sdbg << i << ",";
    sdbg << "\n";
    cout << sdbg.str();
    if(span)
        send_replica(osd_id, replicas, pmsg, &span->context());
    else
        send_replica(osd_id, replicas, pmsg);
    QueueItem qitem{LOCAL, pmsg, unordered_map<string, string>()};
    if(span) {
        Inject_map(span->context(), qitem.tracing_meta);
        span->Log({{"SEND_LOCAL", ""}});
    }
    pending_writes.push_back(move(qitem));
    pending_replies[pmsg] = make_pair(0, source_client);
    random_process();

    if(span) span->Finish();
}

void do_osd_replica(unsigned int osd_id, unsigned int source_osd, Message *pmsg,
        vector<QueueItem> &pending_writes){
    auto sc_maybe = Extract_map(pmsg->trace_meta);
    shared_ptr<Span> span;
    if(sc_maybe){
        ostringstream os_rcv;
        os_rcv << "osd" << osd_id;
        span = Tracer::Global()->StartSpan(
                "do_osd_replica",
                {ChildOf(&*sc_maybe),
                 SetTag("RECEIVED_BY", os_rcv.str())});
    }

    stringstream sdbg;
    sdbg << "[" << pmsg->payload << "@osd" << osd_id << "] replica received\n";
    cout << sdbg.str();
    QueueItem qitem{source_osd, pmsg, unordered_map<string, string>()};
    if(span) {
        Inject_map(span->context(), qitem.tracing_meta);
        ostringstream os_send;
        os_send << "osd" << source_osd;
        span->Log({{"SEND_LOCAL", ""}});
    }
    pending_writes.push_back(qitem);
    random_process();

    if(span) span->Finish();
}

void do_osd_persist(
        unsigned int osd_id,
        QueueItem &qi,
        unordered_map<Message *, pair<unsigned int, unsigned int>> &pending_replies){
    auto sc_maybe = Extract_map(qi.tracing_meta);
    shared_ptr<Span> span;
    if(sc_maybe)
        span = Tracer::Global()->StartSpan(
                "do_osd_persist",
                {ChildOf(&*sc_maybe),
                 SetTag("RECEIVED_LOCAL", "")});

    random_process();
    stringstream sdbg;
    sdbg << "[" << qi.pmsg->payload << "@osd" << osd_id << "] persist";
    if(qi.reply_osd == LOCAL){
        sdbg << "(local) ";
        auto &item = pending_replies[qi.pmsg];
        item.first += 1;
        sdbg << item.first << "/" << num_replicas << " ";
        if(item.first == num_replicas){
            sdbg << "-> client" << item.second << "\n";
            cout << sdbg.str();
            if(span)
                send_osd_reply(osd_id, item.second, qi.pmsg, &span->context());
            else
                send_osd_reply(osd_id, item.second, qi.pmsg);
            pending_replies.erase(qi.pmsg);
        }else
            sdbg << "\n";
    }else{
        sdbg << " -> " << qi.reply_osd << "\n";
        if(span)
            send_replica_reply(osd_id, qi.reply_osd, qi.pmsg, &span->context());
        else
            send_replica_reply(osd_id, qi.reply_osd, qi.pmsg);
    }
    cout << sdbg.str();
    random_process();

    if(span) span->Finish();
}

void do_osd_replica_reply(unsigned int osd_id, Message *pmsg,
        unordered_map<Message *, pair<unsigned int, unsigned int>> &pending_replies){
    auto sc_maybe = Extract_map(pmsg->trace_meta);
    shared_ptr<Span> span;
    if(sc_maybe){
        ostringstream os_rcv;
        os_rcv << "osd" << osd_id;
        span = Tracer::Global()->StartSpan(
                "do_osd_replica_reply",
                {ChildOf(&*sc_maybe),
                 SetTag("RECEIVED_BY", os_rcv.str())});
    }

    stringstream sdbg;
    sdbg << "[" << pmsg->payload << "@osd" << osd_id << "] replica replied ";
    auto &item = pending_replies[pmsg];
    item.first += 1;
    sdbg << item.first << "/" << num_replicas << " ";
    if(item.first == num_replicas){
        sdbg << "-> client" << item.second << "\n";
        cout << sdbg.str();
        if(span)
            send_osd_reply(osd_id, item.second, pmsg, &span->context());
        else
            send_osd_reply(osd_id, item.second, pmsg);
        pending_replies.erase(pmsg);
    }else{
        sdbg << "\n";
        cout << sdbg.str();
    }
    random_process();

    if(span) span->Finish();
}

// client/osd workers
unsigned int alive_clients = num_clients;

void client_worker(unsigned int client_id){
    stringstream sdbg;
    sdbg << "[sys] client" << client_id << " started\n";
    cout << sdbg.str();

    unsigned int cnt_sent = 0;
    unsigned int cnt_replied = 0;
    vector<Message> messages(num_msgs);
    Buffer *buf_write_reply = &bufs_write_reply[client_id];

    while(cnt_replied < num_msgs){
        random_process();
        //send write
        if(cnt_sent < num_msgs){
            do_client_write(client_id, cnt_sent, &messages[cnt_sent]);
            ++cnt_sent;
        }
        //process write reply
        auto item = buf_write_reply->pop();
        if(item.first){
            do_client_reply(client_id, item.first);
            ++cnt_replied;
        }
    }
    --alive_clients;
    stringstream sdbg1;
    sdbg1 << "[sys] client" << client_id << " stopped\n";
    cout << sdbg1.str();
}

void osd_worker(unsigned int osd_id){
    stringstream sdbg;
    sdbg << "[sys] osd" << osd_id << " started\n";
    cout << sdbg.str();

    Buffer *buf_write = &bufs_write[osd_id];
    Buffer *buf_replica = &bufs_replica[osd_id];
    Buffer *buf_replica_reply = &bufs_replica_reply[osd_id];
    //source_osd_id, msg
    vector<QueueItem> pending_writes;
    //msg -> (cnt_replication_finished, client_id)
    unordered_map<Message *, pair<unsigned int, unsigned int>> pending_replies;
    while(alive_clients){
        random_process();
        //process write
        auto item = buf_write->pop();
        if(item.first)
            do_osd_write(osd_id, item.second, item.first, pending_writes, pending_replies);
        //process replica
        item = buf_replica->pop();
        if(item.first)
            do_osd_replica(osd_id, item.second, item.first, pending_writes);
        //process replica reply
        item = buf_replica_reply->pop();
        if(item.first)
            do_osd_replica_reply(osd_id, item.first, pending_replies);
        //process write
        if(pending_writes.size()){
            if(pending_writes.size() > 1)
                random_shuffle(pending_writes.begin(), pending_writes.end());
            auto &witem = pending_writes.back();
            do_osd_persist(osd_id, witem, pending_replies);
            pending_writes.pop_back();
        }
    }
    stringstream sdbg1;
    sdbg1 << "[sys] osd" << osd_id << " stopped\n";
    cout << sdbg1.str();
}

// main
string usage = "<USAGE>\n"
               "help: -h\n"
               "jaeger-reporter url: -url=<url>, default to 127.0.0.1:6831\n"
               "switch from jaeger-tracer to mock-tracer plugin: -local\n"
               "the simplest demo: -e\n"
               "Ceph simulator demo settings:\n"
               "  -osd=<num_osds>, default to 10\n"
               "  -client=<num_clients>, default to 1\n"
               "  -msg=<num_msgs_per_client>, default to 1\n"
               "  -replica=<num_replicas>, default to 3\n";

int main(int argc, const char **argv){
    bool do_example = false;
	string j_reporter = "";
    bool use_mock = false;
    if(argc > 1){
        for(int i=1; i<argc; ++i){
            if(strcmp(argv[i], "-h") == 0){
                cout << usage << endl;
                return 0;
            }else if(strcmp(argv[i], "-e") == 0){
                do_example = true;
            }else if(strncmp("-osd=", argv[i], 5) == 0){
                num_osds = atoi(argv[i]+5);
            }else if(strncmp("-client=", argv[i], 8) == 0){
                num_clients = atoi(argv[i]+8);
            }else if(strncmp("-msg=", argv[i], 5) == 0){
                num_msgs = atoi(argv[i]+5);
            }else if(strncmp("-replica=", argv[i], 9) == 0){
                num_replicas = atoi(argv[i]+9);
            }else if(strncmp("-url=", argv[i], 5) == 0){
				j_reporter = string(argv[i]+5);
            }else if(strncmp("-local", argv[i], 6) == 0){
                use_mock = true;
            }else{
                cout << usage << endl;
                return 0;
            }
        }
    }

    // tracer plugin
    //
    unique_ptr<TracerPlugin> plugin;
    if(use_mock)
        plugin = create_mocktracer_plugin();
    else
        plugin = create_jaegertracer_plugin(j_reporter);
    if(!plugin->is_enabled()){
        cout << "Warn: tracer plugin not available!" << endl;
        cout << "Run NOOP-TRACER." << endl << endl;
    }else if(use_mock){
        cout << "Run MOCK-TRACER." << endl << endl;
    }else
        cout << "Run JAEGER-TRACER." << endl << endl;
    auto tracer = Tracer::Global();

    if(do_example){
        cout << "EXAMPLE:" << endl;
        cout << "  a: Tag(abc-123, xyz-true)" << endl;
        cout << "  b: <- a, Bag(baggage-coke)" << endl;
        cout << "  c: << a, Log(logging-23333)" << endl;
        cout << "  d: <- 1, << b/c, Tag(starttag-histarttag)" << endl;
        cout << "  e: <- b/d" << endl;

        auto span_a =
            tracer->StartSpan("a", {SetTag("abc", 123), SetTag("xyz", true)});
        span_a->Finish();
        auto span_b = tracer->StartSpan("b", {ChildOf(&span_a->context())});
        span_b->SetBaggageItem("baggage", "coke");
        span_b->Finish();
        auto span_c = tracer->StartSpan("c", {FollowsFrom(&span_a->context())});
        span_c->Log({{"logging", 23333}});
        auto span_d = tracer->StartSpan("d", {ChildOf(&span_a->context()),
                                              FollowsFrom(&span_b->context()),
                                              FollowsFrom(&span_c->context()),
                                              SetTag("starttag", "histarttag")});
        span_c->Finish();
        span_d->Finish();
        auto span_e = tracer->StartSpan("e", {ChildOf(&span_b->context()),
                                              ChildOf(&span_d->context())});
        span_e->Finish();
    } else {
/* rados simulation
 */
        cout << "SIMULATE:" << endl;
        cout << "  msgs per client: " << num_msgs << endl;
        cout << "  clients:         " << num_clients << endl;
        cout << "  osds:            " << num_osds << endl;
        cout << "  replicas:        " << num_replicas << endl;
        cout << ">>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        vector<thread> clients;
        for(int i=0; i<num_clients; ++i)
            clients.push_back(move(thread(client_worker, i)));
        vector<thread> osds;
        for(int i=0; i<num_osds; ++i)
            osds.push_back(move(thread(osd_worker, i)));
        for(auto &th : clients)
            th.join();
        for(auto &th : osds)
            th.join();
        cout << "<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    }

    tracer->Close();
}
