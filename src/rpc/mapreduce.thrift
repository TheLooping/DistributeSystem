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
     * For map tasks, resultNum should be set to the number of reduce tasks;
     * For reduce tasks, resultNum should always be 1
     */
    4: i32 resultNum
}

struct TaskResult {
    1: i32 id,
    2: ResponseType type,    
    3: list<string> result_location
}

/* 
 * 服务定义规定如下: 
 * 1 可以使用逗号或分号标识结束
 * 2 参数可以是基本类型或者结构体，但参数只能是const，不可以作为返回值
 * 3 返回值可以是基本类型或者结构体或void
 */
/* RPC server对外提供的服务，会由 thrift 编译器生成对应语言（如cpp）的代码 */
service Master {
    /* 为 worker 分配一个 ID，返回值是整数，即ID */
    i32 AssignId(),

    /* 为 worker 分配任务（map/reduce） */    
    TaskResponse AssignTask(),    

    /* 接收 worker 执行结果 */
    void CommitTask(1: TaskResult result),
}
