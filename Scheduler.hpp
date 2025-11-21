#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "Task.hpp"
#include "Logger.hpp"

class Scheduler {
public:
    Scheduler() {}
    virtual Task* request_task(int cpu_id, Logger &logger) = 0;
    virtual void return_task(int cpu_id, Task *task) = 0;
    virtual void read_next_n_tasks(int n, int cpu_id, Logger &logger) = 0;
    virtual ~Scheduler() {}
};

#endif
