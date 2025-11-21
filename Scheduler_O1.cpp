#include "Scheduler_O1.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <time.h>
#include <stdexcept>
#include <mutex>
using namespace std;

extern int NUM_CPU;
extern int workload_factor1;
extern int workload_factor2;

mutex mutex5;

Scheduler_O1::PriorityQueue::PriorityQueue(int min_prio, int max_prio)
    : min_prio(min_prio), max_prio(max_prio), pq(max_prio-min_prio+1), len(0)
    {}

void Scheduler_O1::PriorityQueue::insert(Task* task){
    int priority = (task->policy) ? task->rt_priority : 120 + task->nice;
    pq[priority-min_prio].push(task);
    bitmap.set(priority, true);
    this->len += 1;
}

Task* Scheduler_O1::PriorityQueue::get(){
    for (int prio = min_prio; prio < max_prio+1; ++prio){
        if (bitmap.test(prio)){
            int idx = prio - min_prio;
            Task *task = pq[idx].front();
            pq[idx].pop();
            if (pq[idx].empty())
                bitmap.set(prio, false);
            this->len -= 1;
            return task;
        }
    }
    return nullptr;
}

bool Scheduler_O1::PriorityQueue::empty() const {
    return bitmap.none();
}

int Scheduler_O1::PriorityQueue::size() const {
    return this->len;
    /*
    int result = 0;
    for (const queue<Task*> &q: pq){
        result += q.size();
    }
    return result;*/
}

Scheduler_O1::Runqueue::Runqueue()
    : realtime_pq(0, 99), active_pq(100, 139), expired_pq(100, 139)
    {}

int Scheduler_O1::Runqueue::size() const {
    return active_pq.size() + expired_pq.size() + realtime_pq.size();
}

Scheduler_O1::Scheduler_O1(string filename, int NUM_CPU){

    cpu_rq = new Scheduler_O1::Runqueue[NUM_CPU];

    if (!open_task_file(filename)){
        throw runtime_error("infile error");
    }
}

Scheduler_O1::~Scheduler_O1(){
    delete [] cpu_rq;
}

Task* Scheduler_O1::request_task(int cpu_id, Logger &logger){
    // add more tasks into system

    if (cpu_rq[cpu_id].size() < workload_factor1){
        mutex5.lock();
        read_next_n_tasks(workload_factor2, cpu_id, logger);
        mutex5.unlock();
    }


    Task *task = cpu_rq[cpu_id].realtime_pq.get();
    if (task != nullptr){
        return task;
    }
    else {
        task = cpu_rq[cpu_id].active_pq.get();
        if (task != nullptr){
            return task;
        }
        swap(cpu_rq[cpu_id].active_pq, cpu_rq[cpu_id].expired_pq);
        task = cpu_rq[cpu_id].active_pq.get();
        if (task != nullptr){
            return task;
        }
        /*if (cpu_rq[cpu_id].active_pq.bitmap.count() > 0 ||
            cpu_rq[cpu_id].expired_pq.bitmap.count() > 0 ||
            cpu_rq[cpu_id].realtime_pq.bitmap.count() > 0){
                throw runtime_error("rq > 0 but returns nullptr");
            }*/
        return nullptr;
    }
    return nullptr;
}

// return tasks to expired_pq
void Scheduler_O1::return_task(int cpu_id, Task *task){
    if (task->policy){ // It's a real-time task
        // add task to cpu's realtime_pq
        cpu_rq[cpu_id].realtime_pq.insert(task);
    }
    else{
        // add task to cpu's expired_pq
        cpu_rq[cpu_id].expired_pq.insert(task);
    }
}
// insert tasks to active_pq
void Scheduler_O1::insert_task(int cpu_id, Task *task){
    if (task->policy){ // It's a real-time task
        // add task to cpu's realtime_pq
        cpu_rq[cpu_id].realtime_pq.insert(task);
    }
    else{
        // add task to cpu's active_pq
        cpu_rq[cpu_id].active_pq.insert(task);
    }
}


bool Scheduler_O1::open_task_file(const string &filename){
    infile.open(filename);
    if (!infile){
        cerr << "Cannot open file: " << filename << endl;
        return false;
    }
    return true;
}

void Scheduler_O1::read_next_n_tasks(int n, int cpu_id, Logger &logger){
    if (!infile.is_open()){
        return;
    }
    string line;
    int count = 0;

    while (count < n && getline(infile, line)){
        istringstream iss(line);
        int task_id, rt_priority, nice, policy;
        iss >> task_id >> rt_priority >> nice >> policy;

        queue<pair<int, int>> bursts;
        int device_id, duration;
        while (iss >> device_id >> duration){
            bursts.push(pair<int,int>(device_id, duration));
        }

        Task *task = new Task(task_id, rt_priority, nice, policy, bursts);
        logger.write("SCHED", cpu_id, task->task_id, "ENTER_SCHED");
        if (task->bursts.empty()){
            throw runtime_error("no bursts @ ENTER_SCHED");
        }
        insert_task(cpu_id, task);
        count += 1;
    }
    
    if (infile.eof()){
        infile.close();
    }
}