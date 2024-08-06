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

        enum class TaskState
        {
            IDLE = 0,
            IN_PROGRESS,
            COMPLETED
        };

        struct Task
        {
            int32_t id;             // 任务 id
            TaskState state;        // 任务状态: 空闲、进行中、已完成
            vector<string> params;  // 任务参数
            vector<string> results; // 任务结果
        };

        struct TaskProcessStatus
        {
            int id;
            time_point start_time; // 任务开始时间
            time_point last_time;  // 任务最后一次更新时间
        };

        // MasterIf 是 thrift 根据 mapreduce.thrift 自动生成的接口类
        // MasterHandler 继承自 MasterIf，需要实现 MasterIf 中的所有接口
        // assignId, assignTask, commitTask
        class MasterHandler : virtual public MasterIf
        {
        private:
            enum class MasterStatus
            {
                MAP_PHASE,
                REDUCE_PHASE,
                COMPLETED_PHASE
            };

        public:
            MasterHandler(vector<string> files, int reduce_number);
            ~MasterHandler() override = default;
            int32_t assignId() override;
            void assignTask(TaskResponse &_return) override;
            void commitTask(const TaskResult &result) override;

            void SetExitServer(std::function<void(void)> exit_server);

        private:
            vector<Task> map_tasks_;
            vector<Task> reduce_tasks_;
            queue<int> idle_; // 是否空闲
            mutex lock_;
            unordered_map<int, TaskProcessStatus> processing_;
            MasterStatus state_;
            uint32_t completed_cnt_; // 完成的任务数量
            std::atomic<int> worker_id_;
            std::function<void(void)> exit_server_; // 退出函数
        };

        MasterHandler::MasterHandler(vector<string> files, int reduce_number)
            : map_tasks_(vector<Task>(files.size())),
              reduce_tasks_(vector<Task>(reduce_number)),
              state_(MasterStatus::MAP_PHASE),
              completed_cnt_(0),
              worker_id_(1)
        {
            for (int i = 0; i < files.size(); ++i)
            {
                map_tasks_[i] = Task{i, TaskState::IDLE, {files[i]}, {}}; // 初始化 map 任务
                idle_.push(i);                                            // 将任务 id 放入空闲队列
            }
            std::thread detect_timeout_daemon([this]()
                                              { this->DetectTimeoutTask(); });
            detect_timeout_daemon.detach(); // 分离线程，后台运行

            LOG(INFO) << "MasterHandler init success, number of idle tasks: " << files.size();
        }

        // 为 worker 分配 ID
        int32_t MasterHandler::assignId()
        {
            LOG(INFO) << "Receive assignId request, assign worker id: " << worker_id_.load();
            return worker_id_.fetch_add(1); // 返回旧值，但 worker_id_ 初始化时为 1
        }

        // 为 worker 分配任务
        viod MasterHandler::assignTask(TaskResponse &_return)
        {
            std::lock_guard<mutex> lock(lock_); // 锁定 lock_; 使用 lock_guard 对象，自动释放锁
            int task_id = TryGetIdleTask();     // 获取空闲任务
            if (task_id != -1)
            {
                SetTask2Processing(task_id); // 设置任务为进行中
            }
            switch (state_)
            {
            case MasterStatus::MAP_PHASE:
                if (task_id != -1)
                {
                    _return.id = task_id;
                    _return.type = ResponseType::MAP_TASK;
                    _return.params = map_tasks_[task_id].params;
                    LOG(INFO) << "Assign map task: " << task_id;
                }
                else
                {
                    _return.type = ResponseType::WAIT;
                    LOG(INFO) << "No map task available, wait...";
                }
                _return.resultNum = reduce_tasks_.size();
                break;
            case MasterStatus::REDUCE_PHASE:
                if (task_id != -1)
                {
                    _return.id = task_id;
                    _return.type = ResponseType::REDUCE_TASK;
                    _return.params = reduce_tasks_[task_id].params;
                    LOG(INFO) << "Assign reduce task: " << task_id;
                }
                else
                {
                    _return.type = ResponseType::WAIT;
                    LOG(INFO) << "No reduce task available, wait...";
                }
                _return.resultNum = 0;
                break;
            case MasterStatus::COMPLETED_PHASE:
                _return.type = ResponseType::COMPLETED;
                LOG(INFO) << "All tasks completed.";
                break;
            default:
                LOG(ERROR) << "Unknown MasterStatus: " << static_cast<int>(state_);
                break;
            }
            LOG(INFO) << "Assign task response: " << [&_return]() -> std::string
            {
                std::ostringstream oss;
                _return.printTo(oss);
                return std::move(oss).str();
            }();
        }

    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: ./master file1 file2 ..." << std::endl;
        exit(-1);
    }
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "../logs/master";

    LOG(INFO) << "INPUT ARGS: " << argc;
    vector<string> files(argc - 1);
    int pos = 0;
    for_each(files.begin(), files.end(), [&argv, &pos](string &file)
             {
        file = argv[++pos];
        LOG(INFO) << i << "-th input file: " << file; });
    LOG(INFO) << file.size() << " files waiting to be processed.";
    /** 1. 设置服务器端口 */
    int port = 9090;
    /** 2. 创建处理器 */
    /** 2.1 CalculatorHandler 是自己实现的类,包含了服务定义中各个方法的具体实现 */
    /** 2.2 CalculatorProcessor 是 thrift 自动生成的类，它知道如何将接收到的请求分发到正确的处理方法 */
    std::shared_ptr<MasterHandler> handler(new MasterHandler(files));    // MasterHandler 需要自己实现
    std::shared_ptr<TProcessor> processor(new MasterProcessor(handler)); // MasterProcessor 由 thrift 自动生成
    /** 3. 创建传输层，底层的网络通信,如建立连接、发送和接收数据，TServerSocket 即指定 TCP socket  */
    std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port)); // Thrift 提供，自己指定端口
    /** 4. 创建传输工厂，为每个新的客户端连接创建传输对象 */
    std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory()); // Thrift 提供，自己选择合适的传输工厂
    /** 5. 创建协议工厂，创建序列化和反序列化数据的协议对象 */
    std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory()); // Thrift 提供，自己选择合适的协议工厂
    /** 6. 创建服务器 */
    TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory); // Thrift 提供，默认传入上面的参数
    // set exit server function
    handler->SetExitServer([&server]()
                           { server.stop(); });
    LOG(INFO) << "Start master server on port " << port;
    /** 7. 启动服务器 */
    server.serve(); // Thrift 提供，启动上述服务器

    google::FlushLogFiles(google::INFO);
    return 0;
}