#ifndef SCHEDULER_ON_HPP
#define SCHEDULER_ON_HPP

#include "Scheduler.hpp"
#include <queue>
#include <list>
#include <string>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <mutex>

class Scheduler_On : public Scheduler{
    std::list<Task*> ready_queue;
    int seed;
    std::ifstream infile;
    std::recursive_mutex rq_mutex;
public:
    Scheduler_On(std::string filename);
    ~Scheduler_On();
    Task* request_task(int cpu_id, Logger &logger) override;
    void return_task(int cpu_id, Task *task) override;
    void read_next_n_tasks(int n, int cpu_id, Logger &logger) override;
private:
    int goodness(const int cpu_id, const Task *task) const ;
    bool open_task_file(const std::string &filename);
};

#endif