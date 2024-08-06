#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <algorithm>
#include <thread>
#include <unordered_map>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <glog/logging.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>

#include "rpc/mapreduce/Master.h"
#include "mapreduce/mapreduce.h"

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
            TaskState status;       // 任务状态: 空闲、进行中、已完成
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
            int32_t AssignId() override;
            void AssignTask(TaskResponse &_return) override;
            void CommitTask(const TaskResult &result) override;
            void SetExitServer(std::function<void(void)> exit_server); // 设置退出服务器函数，赋值给 exit_server_

        private:
            int TryGetIdleTask();                  // 尝试获取空闲任务id
            void SetTaskToProcessing(int task_id); // 添加任务到进行中队列

            void DetectTimeoutTask();
            void RemoveFiles(const vector<string> &files);
            void SwitchToReducePhase();
            void SwitchToCompletedPhase();

            void CommitMapTask(const TaskResult &result);
            void CommitReduceTask(const TaskResult &result);

            vector<Task> map_tasks_;
            vector<Task> reduce_tasks_;
            queue<int> idle_; // 是否空闲
            mutex lock_;
            unordered_map<int, TaskProcessStatus> processing_;
            MasterStatus status_;
            uint32_t completed_cnt_; // 完成的任务数量
            std::atomic<int> worker_id_;
            std::function<void(void)> exit_server_; // 退出函数
        };

        MasterHandler::MasterHandler(vector<string> files, int reduce_number)
            : map_tasks_(vector<Task>(files.size())),
              reduce_tasks_(vector<Task>(reduce_number)),
              status_(MasterStatus::MAP_PHASE),
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
        int32_t MasterHandler::AssignId()
        {
            LOG(INFO) << "Receive assignId request, assign worker id: " << worker_id_.load();
            return worker_id_.fetch_add(1); // 返回旧值，但 worker_id_ 初始化时为 1
        }

        // 为 worker 分配任务
        void MasterHandler::AssignTask(TaskResponse &_return)
        {
            std::lock_guard<mutex> lock(lock_); // 锁定 lock_; 使用 lock_guard 对象，自动释放锁
            int task_id = TryGetIdleTask();     // 获取空闲任务
            if (task_id != -1)
            {
                SetTaskToProcessing(task_id); // 设置任务为进行中
            }
            switch (status_)
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
                LOG(ERROR) << "Unknown MasterStatus: " << static_cast<int>(status_);
                break;
            }
            LOG(INFO) << "Assign task response: " << [&_return]() -> std::string
            {
                std::ostringstream oss;
                _return.printTo(oss);
                return std::move(oss).str();
            }();
        }

        // worker 提交任务结果
        void MasterHandler::CommitTask(const TaskResult &result)
        {
            std::lock_guard<mutex> lock(lock_);
            LOG(INFO) << "Receive commitTask request: " << [&result]() -> std::string
            {
                std::ostringstream oss;
                result.printTo(oss);
                return std::move(oss).str();
            }();

            // 检查状态是否已经改变/完成、任务是否超时
            if (status_ == MasterStatus::COMPLETED_PHASE)
            {
                LOG(WARNING) << "Master has completed all tasks, ignore.";
                RemoveFiles(result.result_location);
                return;
            }
            if ((status_ == MasterStatus::MAP_PHASE && result.type != ResponseType::MAP_TASK) ||
                (status_ == MasterStatus::REDUCE_PHASE && result.type != ResponseType::REDUCE_TASK))
            {
                LOG(WARNING) << "Task type mismatch, ignore.";
                RemoveFiles(result.result_location);
                return;
            }
            auto &tasks = (status_ == MasterStatus::MAP_PHASE) ? map_tasks_ : reduce_tasks_;
            if (tasks[result.id].status == TaskState::COMPLETED)
            {
                LOG(WARNING) << "Task " << result.id << " has been completed, ignore.";
                RemoveFiles(result.result_location);
                return;
            }

            // 更新任务状态
            tasks[result.id].status = TaskState::COMPLETED;
            processing_.erase(result.id);

            switch (status_)
            {
            case MasterStatus::MAP_PHASE:
                CommitMapTask(result);
                completed_cnt_++;
                LOG(INFO) << fmt::format("Map task {} completed, total: {}/{}", result.id, completed_cnt_, map_tasks_.size());
                if (completed_cnt_ == map_tasks_.size())
                {
                    SwitchToReducePhase();
                }
                break;
            case MasterStatus::REDUCE_PHASE:
                CommitReduceTask(result);
                completed_cnt_++;
                LOG(INFO) << fmt::format("Reduce task {} completed, total: {}/{}", result.id, completed_cnt_, reduce_tasks_.size());
                if (completed_cnt_ == reduce_tasks_.size())
                {
                    SwitchToCompletedPhase();
                }
                break;
            default:
                LOG(ERROR) << "Unknown MasterStatus: " << static_cast<int>(status_);
                break;
            }
        }

        void MasterHandler::DetectTimeoutTask()
        {
            while (true)
            {
                std::this_thread::sleep_for(TIME_OUT / 2);
                std::lock_guard<mutex> lock(lock_);
                auto now = std::chrono::steady_clock::now();
                for (auto it = processing_.begin(); it != processing_.end(); it++)
                {
                    if (now - it->second.start_time > TIME_OUT)
                    {
                        LOG(WARNING) << fmt::format("Task {} timeout, reset to idle", it->first);
                        auto &tasks = (status_ == MasterStatus::MAP_PHASE) ? map_tasks_ : reduce_tasks_;
                        tasks[it->first].status = TaskState::IDLE;
                        idle_.push(it->first);
                        it = processing_.erase(it);
                    }
                }
            }
        }

        int MasterHandler::TryGetIdleTask()
        {
            LOG(INFO) << "Try to get idle task...(idle size: " << idle_.size() << ")";
            auto &tasks = (status_ == MasterStatus::MAP_PHASE) ? map_tasks_ : reduce_tasks_;

            int task_id = -1;
            while (!idle_.empty())
            {
                task_id = idle_.front();
                idle_.pop();
                if (tasks[task_id].status == TaskState::IDLE)
                {
                    // 找到空闲任务，即未被 worker 处理的任务
                    break;
                }
                else
                {
                    LOG(INFO) << "Task " << task_id << " is not idle, try next...";
                    continue;
                }
            }
            LOG(INFO) << fmt::format("Get {}-th {} idle task (total number: {})",
                                     task_id,
                                     (status_ == MasterStatus::MAP_PHASE) ? "map" : "reduce",
                                     tasks.size());
            return task_id;
        }

        void MasterHandler::SetTaskToProcessing(int task_id)
        {
            auto &tasks = (status_ == MasterStatus::MAP_PHASE) ? map_tasks_ : reduce_tasks_;
            tasks[task_id].status = TaskState::IN_PROGRESS;
            processing_.emplace(std::piecewise_construct,
                                std::forward_as_tuple(task_id),
                                std::forward_as_tuple(task_id, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()));
            LOG(INFO) << fmt::format("Set {}-th {} task (total number: {}) to processing",
                                     task_id,
                                     (status_ == MasterStatus::MAP_PHASE) ? "map" : "reduce",
                                     tasks.size());
        }

        void MasterHandler::RemoveFiles(const vector<string> &files)
        {
            for (const auto &file : files)
            {
                LOG(INFO) << "Remove file: " << file;
                std::remove(file.c_str());
            }
        }

        void MasterHandler::SwitchToReducePhase()
        {
            LOG(INFO) << "All map tasks completed, start reduce phase.";
            status_ = MasterStatus::REDUCE_PHASE;
            completed_cnt_ = 0;
            for (int i = 0; i < reduce_tasks_.size(); ++i)
            {
                reduce_tasks_[i] = Task{i, TaskState::IDLE, {}, {}};
                reduce_tasks_[i].params.reserve(map_tasks_.size());
                for (const auto &task : map_tasks_)
                {
                    reduce_tasks_[i].params.emplace_back(task.results[i]);
                }
                idle_.push(i);
            }
            LOG(INFO) << "Switch to reduce phase, number of idle tasks: " << reduce_tasks_.size();
        }

        void MasterHandler::SwitchToCompletedPhase()
        {
            LOG(INFO) << "All reduce tasks completed, start completed phase.";
            status_ = MasterStatus::COMPLETED_PHASE;
            for (Task &task : map_tasks_)
            {
                RemoveFiles(task.results);
            }
            std::thread exit_server_thread(exit_server_);
            exit_server_thread.detach();
        }

        void MasterHandler::CommitMapTask(const TaskResult &result)
        {
            map_tasks_[result.id].results = [&result]() -> vector<string>
            {
                LOG(INFO) << fmt::format("Commit map task {} result: {}", result.id, fmt::join(result.result_location, ", "));
                vector<string> tmp_result(result.result_location.size());
                for (int i = 0; i < result.result_location.size(); ++i)
                {
                    string new_name = fmt::format("mr.mid{}.rid{}", result.id, i);
                    std::rename(result.result_location[i].c_str(), new_name.c_str());
                    tmp_result.emplace_back(std::move(new_name));
                }
                LOG(INFO) << fmt::format("Commit map task {} result: {}", result.id, fmt::join(tmp_result, ", "));
                return st::move(tmp_result);
            }();
        }

        void MasterHandler::CommitReduceTask(const TaskResult &result)
        {
            if (result.result_location.size() != 1)
            {
                LOG(ERROR) << "Reduce task result location size is not 1: " << result.result_location.size();
                return;
            }
            string new_name = fmt::format("mr.out{}", result.id);
            std::rename(result.result_location[0].c_str(), new_name.c_str());
            reduce_task_.[result.id].results.emplace_back(std::move(new_name));
            LOG(INFO) << fmt::format("Commit reduce task {} result: {}", result.id, new_name);
        }

        void MasterHandler::SetExitServer(std::function<void(void)> exit_server)
        {
            exitServer_ = exit_server;
        }
    }
}

int main(int argc, char **argv)
{
    using namespace distributedsystem::mapreduce;
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
    std::for_each(files.begin(), files.end(), [&argv, &pos](string &file) {
        file = argv[++pos];                
        LOG(INFO) << pos << "-th input file: " << file; });

    LOG(INFO) << files.size() << " files waiting to be processed.";
    /** 1. 设置服务器端口 */
    int port = 9090;
    /** 2. 创建处理器 */
    /** 2.1 CalculatorHandler 是自己实现的类,包含了服务定义中各个方法的具体实现 */
    /** 2.2 CalculatorProcessor 是 thrift 自动生成的类，它知道如何将接收到的请求分发到正确的处理方法 */
    int reduce_number = 8;
    std::shared_ptr<MasterHandler> handler(new MasterHandler(files, reduce_number));    // MasterHandler 需要自己实现
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