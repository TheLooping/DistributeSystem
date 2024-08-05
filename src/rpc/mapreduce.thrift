enum ResponseType {
    WAIT = 0,
    MAP_TASK,
    REDUCE_TASK,
    COMPLETED
}

struct TaskResponse {
    1: i32 id,
    2: ResponseType type,
    3: list<string> params,
    
    /*
     * For map tasks, resultNum should be set to the number of reduce tasks, and
     * for reduce tasks, resultNum should always be 1
     */
    4: i32 resultNum
}

struct TaskResult {
    1: i32 id,
    2: ResponseType type,
    3: list<string> rs_loc 
}

/* RPC server对外提供的服务，会由 thrift 编译器生成对应语言（如cpp）的代码 */
service Master {
    /* 为 worker 分配一个 ID，返回值是整数，即ID */
    i32 assignId();
    /* 为 worker 分配任务（map/reduce） */    
    TaskResponse assignTask(),    
    /* 接收 worker 执行结果 */
    void commitTask(1: TaskResult result)
}
