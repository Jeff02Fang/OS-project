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

Scheduler_O1::PriorityQueue::PriorityQueue()
    : pq(140), len(0)
    {}

void Scheduler_O1::PriorityQueue::insert(Task* task){
    int priority = (task->policy) ? task->rt_priority : 120 + task->nice;
    pq[priority].push(task);
    bitmap.set(priority, true);
    this->len += 1;
}

Task* Scheduler_O1::PriorityQueue::get(){
    for (int prio = 0; prio < 140; ++prio){
        if (bitmap.test(prio)){
            Task *task = pq[prio].front();
            pq[prio].pop();
            if (pq[prio].empty())
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
}

Scheduler_O1::Runqueue::Runqueue() {}

int Scheduler_O1::Runqueue::size() const {
    return active_pq.size() + expired_pq.size();
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
    // maintain system workload
    if (cpu_rq[cpu_id].size() < workload_factor1){
        read_next_n_tasks(workload_factor2, cpu_id, logger);
    }

    Task *task = cpu_rq[cpu_id].active_pq.get();
    if (task != nullptr){
        return task;
    }
    swap(cpu_rq[cpu_id].active_pq, cpu_rq[cpu_id].expired_pq);
    task = cpu_rq[cpu_id].active_pq.get();
    if (task != nullptr){
        return task;
    }
    return nullptr;
}

// return tasks to expired_pq
void Scheduler_O1::return_task(int cpu_id, Task *task){
    cpu_rq[cpu_id].expired_pq.insert(task);
    
}
// insert tasks to active_pq
void Scheduler_O1::insert_task(int cpu_id, Task *task){
    cpu_rq[cpu_id].active_pq.insert(task);
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
    sched_mutex.lock();
    if (!infile.is_open()){
        sched_mutex.unlock();
        return;
    }
    string line;
    int count = 0;

    while (count < n && getline(infile, line)){
        istringstream iss(line);
        int task_id, rt_priority, nice, policy;
        iss >> task_id >> rt_priority >> nice >> policy;

        vector<pair<int, int>> bursts;
        int device_id, duration;
        while (iss >> device_id >> duration){
            bursts.push_back(pair<int,int>(device_id, duration));
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
    sched_mutex.unlock();
}