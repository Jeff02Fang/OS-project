#include "Task.hpp"

Task::Task(int task_id, int rt_priority, int nice, int policy, std::queue<std::pair<int, int>> bursts, int affinity) 
    : task_id(task_id), rt_priority(rt_priority), nice(nice), policy(policy)
    , bursts(bursts), cpu_affinity(affinity) {}

