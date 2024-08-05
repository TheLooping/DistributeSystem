#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "rpc/mapreduce/Master.h"
#include "tools/ToString.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glog/logging.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

namespace distributedsystem
{
    namespace mapreduce
    {
        using namespace ::apache::thrift;
        using namespace ::apache::thrift::protocol;
        using namespace ::apache::thrift::transport;
        using namespace ::apache::thrift::server;

        using std::mutex;
        using std::queue;
        using std::string;
        using std::unordered_map;
        using std::vector;
        using time_point = std::chrono::steady_clock::time_point;

        constexpr auto TIME_OUT = std::chrono::seconds(5);

        enum class TaskType
        {
            MAP,
            REDUCE,
            WAIT,
            EXIT
        };

        struct Task
        {
            TaskType type;
            string file;
            string key;
            string value;
        };

        struct TaskProcessStatus
        {
            int id;
            time_point start_time;
            time_point last_time;
        };

        class MasterHandler : virtual public MasterIf
        {
        private:
            enum class TaskStatus
            {
                WAIT,
                PROCESSING,
                FINISHED
            };

        public:
        private:
            vector<Task> map_tasks_;
            vector<Task> reduce_tasks_;
            queue<int> idle_; // 是否空闲
            mutex lock_;
            unordered_map<int, TaskProcessStatus> processing_;
            MasterState state_;
            uint32_t completed_cnt_; // 完成的任务数量
            std::atomic<int> worker_id_;
            std::function<void(void)> exit_server_; // 退出函数
        };

    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "Usage: ./master file1 file2 ..." << std::endl;
        exit(-1);
    }
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "../logs/master";

    
    return 0;
}