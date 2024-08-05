#ifndef DISTRIBUTED_SYSTEM_MAPREDUCE_H_
#define DISTRIBUTED_SYSTEM_MAPREDUCE_H_

#include <string>
#include <vector>

namespace distributedsystem {
namespace mapreduce {
    using std::string;
    struct KeyValue{
        string key;
        string val;
    };
    using MapFunc = std::vector<KeyValue> (*)(string& key, string& value);
    using ReduceFunc = std::vector<string> (*)(string& key, std::vector<string>& value);
}
}

#endif // DISTRIBUTED_SYSTEM_MAPREDUCE_H_