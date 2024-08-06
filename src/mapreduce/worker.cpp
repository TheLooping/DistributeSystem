#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <dlfcn.h>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include "rpc/mapreduce/Master.h"
#include "mapreduce/mapreduce.h"

namespace distributedsystem
{
    namespace mapreduce
    {
        using namespace apache::thrift;
        using namespace apache::thrift::protocol;
        using namespace apache::thrift::transport;

        using std::string;
        using std::unordered_map;
        using std::vector;

        constexpr auto TIME_OUT = std::chrono::seconds(5);

        class Worker
        {
            public:
            Worker(std::shared_ptr<TProtocol> protocol, MapFunc mapf, ReduceFunc reducef)
                : master_(MasterClient(protocol))
                , mapf_(mapf)
                , reducef_(reducef)
            {
            }


            private:
                MasterClient master_;
                MapFunc mapf_;
                ReduceFunc reducef_;
                int id_;

        };

    }
}

#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

int main(int argc, char **argv)
{   
    using namespace distributedsystem::mapreduce;
    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "Worker start.";

    std::shared_ptr<TTransport> socket(new TSocket("localhost", 8888));
    std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    google::FlushLogFiles(google::INFO);

    try {
        transport->open();

    }
    

}