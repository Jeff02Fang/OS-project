#ifndef SCHEDULER_O1_HPP
#define SCHEDULER_O1_HPP

#include "Scheduler.hpp"
#include <vector>
#include <queue>
#include <string>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <utility>
#include <bitset>
#include <mutex>
using namespace std;


class Scheduler_O1 : public Scheduler{
    
    ifstream infile;
    mutex sched_mutex;
    struct PriorityQueue{
    private:
        vector<queue<Task*>> pq;
        int len;
    public:
        PriorityQueue();
        void insert(Task* task);
        Task* get();
        bool empty() const;
        int size() const ;
    private:
        bitset<140> bitmap;
    };
    
    struct Runqueue{
        PriorityQueue active_pq, expired_pq;
        Runqueue();
        int size() const;
    };
    Runqueue *cpu_rq;
    
public:
    Scheduler_O1(std::string filename, int NUM_CPU);
    Task* request_task(int cpu_id, Logger &logger) override;
    void return_task(int cpu_id, Task *task) override;
    void read_next_n_tasks(int n, int cpu_id, Logger &logger) override;
    void insert_task(int cpu_id, Task *task);
    ~Scheduler_O1();
private:
    bool open_task_file(const string &filename);
};

#endif