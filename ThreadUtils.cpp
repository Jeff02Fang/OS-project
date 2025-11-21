#include "ThreadUtils.hpp"
#include <sched.h>
#include <iostream>
#include <cstring>

bool set_realtime_and_affinity(int id, int priority) {
    pthread_t this_thread = pthread_self();

    // 1. Pin to specific CPU
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id, &cpuset);

    int ret = pthread_setaffinity_np(this_thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        std::cerr << "Failed to set CPU affinity: " << strerror(ret) << std::endl;
        return false;
    }

    // 2. Set SCHED_FIFO real-time priority
    sched_param sch_params;
    sch_params.sched_priority = priority;

    ret = pthread_setschedparam(this_thread, SCHED_FIFO, &sch_params);
    if (ret != 0) {
        std::cerr << "Failed to set SCHED_FIFO: " << strerror(ret)
                  << " (try sudo or CAP_SYS_NICE)" << std::endl;
        return false;
    }

    // 3. Confirm settings
    int policy;
    sched_param check;
    pthread_getschedparam(this_thread, &policy, &check);
    //std::cerr << "Thread pinned to device " << id
              //<< ", policy=" << (policy == SCHED_FIFO ? "SCHED_FIFO" : "OTHER")
    //          << ", priority=" << check.sched_priority << std::endl;

    return true;
}
