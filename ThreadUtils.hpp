#ifndef THREAD_UTILS_HPP
#define THREAD_UTILS_HPP

#include <pthread.h>

// Returns true if successfully pinned and set to SCHED_FIFO
bool set_realtime_and_affinity(int cpu_id, int priority = 80);

#endif // THREAD_UTILS_HPP
