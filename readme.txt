executing format:
./main <num_cpu> <filename> <sched_algo> <workload_f1> < workload_f2>
sort -n -k1 cpu*.log io*.log > merged.log
python3 metrics.py

task format:
<task_id> <rt_priority> <nice> <policy> <device_id> <duration> <device_id_id> <duration> ...

// rt_priority: 0-99
// nice: -20-+19
// policy: 0 for SCHED_OTHER, 1 for SCHED_FIFO, 2 for SCHED_RR

log format:
<timestamp_us> <thread_type> <thread_id> <task_id> <event> [<extra_info>(duration)]


executing instructions:
0.
    make
1. 
    sudo time ./main 2 tasks/task5.txt n 10 10
2. 
    make merge
3.
    python3 metrics.py 







