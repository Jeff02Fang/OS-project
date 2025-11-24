#ifndef TASK_HPP
#define TASK_HPP

#include <vector>
#include <utility>

struct Task {
    int task_id;
    int rt_priority;
    int nice;
    int policy;
    std::vector<std::pair<int, int>> bursts;
    int cpu_affinity;

    Task(int task_id, int rt_priority, int nice, int policy, std::vector<std::pair<int, int>> bursts, int affinity=-1);
};

#endif
