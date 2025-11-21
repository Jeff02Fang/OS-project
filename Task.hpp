#ifndef TASK_HPP
#define TASK_HPP

#include <queue>
#include <utility>

struct Task {
    int task_id;
    int rt_priority;
    int nice;
    int policy;
    std::queue<std::pair<int, int>> bursts;
    int cpu_affinity;

    Task(int task_id, int rt_priority, int nice, int policy, std::queue<std::pair<int, int>> bursts, int affinity=-1);
};

#endif
