#include "Scheduler_On.hpp"
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

mutex sched_req_mutex, sched_ret_mutex;
extern mutex sched_mutex;

Scheduler_On::Scheduler_On(string filename){
    if (!open_task_file(filename)){
        throw runtime_error("infile error");
    }
}

Task* Scheduler_On::request_task(int cpu_id, Logger &logger){
    sched_req_mutex.lock();
    if ((int)ready_queue.size() < workload_factor1) 
        read_next_n_tasks(workload_factor2, cpu_id, logger);


    if (ready_queue.empty()){
        sched_req_mutex.unlock();
        return nullptr;
    }
    pair<int, Task*> best_choice(-2000, nullptr);
    // O(n) time to select next task
    for (Task *task: ready_queue){    
        int good_val = goodness(cpu_id, task);
        if (good_val > best_choice.first){
            best_choice.first = good_val;
            best_choice.second = task;
        }
    }
    ready_queue.remove(best_choice.second);

    sched_req_mutex.unlock();
    return best_choice.second;
}

void Scheduler_On::return_task(int cpu_id, Task *task){
    cpu_id += 1; // prevent warning
    sched_ret_mutex.lock();
    ready_queue.push_back(task);
    sched_ret_mutex.unlock();
}

int Scheduler_On::goodness(const int cpu_id, const Task *task) const {
    int task_cpu_id = task->bursts.front().first;
    int remaining_time = task->bursts.front().second;

    if (task->policy){ // policy == 1 || == 2
        // low priority tasks (high priority value) runs first
        return 1000 + task->rt_priority; 
    }

    int weight = remaining_time;
    if (task_cpu_id == cpu_id){
        weight += 1;
    }
    weight += 20 - task->nice;
    return weight;
}


bool Scheduler_On::open_task_file(const string &filename){
    infile.open(filename);
    if (!infile){
        cerr << "Cannot open file: " << filename << endl;
        return false;
    }
    return true;
}

void Scheduler_On::read_next_n_tasks(int n, int cpu_id, Logger &logger){
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

        queue<pair<int, int>> bursts;
        int device_id, duration;
        while (iss >> device_id >> duration){
            bursts.push(make_pair(device_id, duration));
            //cerr << task_id << " " << duration << endl;
        }

        Task *task = new Task(task_id, rt_priority, nice, policy, bursts);
        logger.write("SCHED", cpu_id, task->task_id, "ENTER_SCHED");
        return_task(-1, task);
        count += 1;
    }
    
    if (infile.eof()){
        infile.close();
    }
    sched_mutex.unlock();
}