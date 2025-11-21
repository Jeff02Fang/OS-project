#include <iostream>
#include <pthread.h>
#include <queue>
#include <unistd.h>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include "Scheduler_On.hpp"
#include "Scheduler_O1.hpp"
#include "ThreadUtils.hpp"

using namespace std;

chrono::steady_clock::time_point global_start_time;

// -------------------- configuration --------------------
int workload_factor1;
int workload_factor2;

int NUM_CPU = 0;
vector<bool> cpu_state;
queue<Task*> *tasks_return_from_io = nullptr;

const int NUM_IO = 2;
const int IO_ID_OFFSET = 4;      // device ids for IO start here (4,5,...)
const int NUM_DEVICES = 8;       // total devices (cpus + possible ios)
queue<pair<int, Task*>> io_queue[NUM_IO];

mutex mutexes[NUM_DEVICES];
mutex cerr_mutex;

atomic<bool> shut_down(false);

void busy_sleep_microseconds(int duration_us);

// small safe print function to avoid interleaved cerr
void safe_cerr(const string &s){
    //pthread_mutex_lock(&cerr_mutex);
    cerr_mutex.lock();
    cerr << s << flush;
    //pthread_mutex_unlock(&cerr_mutex);
    cerr_mutex.unlock();
}

// -------------------- I/O device thread --------------------
void *IO_device(void *arg){
    int device_id = *((int*)arg);
    int io_id = device_id - IO_ID_OFFSET;
    {
        string s = "Turn on I/O_device #" + to_string(io_id) + "\n";
        safe_cerr(s);
    }
    set_realtime_and_affinity(io_id+IO_ID_OFFSET, 76-io_id);
    // init Logger
    Logger logger("io" + to_string(io_id) + ".log", global_start_time);
    // let the file be opened before running time
    logger.write("IO", io_id, -1, "INIT");
    busy_sleep_microseconds(10000);

    while (true){
        // lock this device's mutex and pop a task if present
        //pthread_mutex_lock(&mutexes[device_id]);
        mutexes[device_id].lock();
        bool has_task = !io_queue[io_id].empty();
        if (has_task){
            auto temp = io_queue[io_id].front();
            io_queue[io_id].pop();
            //pthread_mutex_unlock(&mutexes[device_id]);
            mutexes[device_id].unlock();

            int cpu_id = temp.first;
            Task *task = temp.second;
            if (!task){
                safe_cerr("IO_device: null task pointer!\n");
                continue;
            }

            if (task->bursts.empty()){
                throw runtime_error("no task duration time in IO_device");
            }

            
            auto job_type = task->bursts.front();
            int duration = job_type.second;
            //safe_cerr("I/O do tid=" + to_string(task->task_id) + " in I/O #" + to_string(io_id) + "\n");
            logger.write("IO", io_id, task->task_id, "ENTER_IO");
            if (duration <= 0) throw runtime_error("duration time error in IO_device");
            //usleep(duration);
            busy_sleep_microseconds(duration);
            task->bursts.pop();
            logger.write("IO", io_id, task->task_id, "LEAVE_IO");//, to_string(duration));

            // return to CPU queue: lock that cpu's queue mutex
            if (cpu_id < 0 || cpu_id >= NUM_CPU){
                safe_cerr("IO_device: invalid cpu_id returned: " + to_string(cpu_id) + "\n");
                continue;
            }
            //pthread_mutex_lock(&mutexes[cpu_id]);
            mutexes[cpu_id].lock();
            tasks_return_from_io[cpu_id].push(task);
            //pthread_mutex_unlock(&mutexes[cpu_id]);
            mutexes[cpu_id].unlock();
        } else {
            //pthread_mutex_unlock(&mutexes[device_id]);
            mutexes[device_id].unlock();

            // check shutdown condition: all CPUs not running?
            bool all_not_running = true;
            for (int i = 0; i < NUM_CPU; ++i){
                all_not_running &= !cpu_state[i];
            }
            if (all_not_running){
                shut_down.store(true);
                safe_cerr("Shut down I/O #" + to_string(io_id) + "\n");
                break;
            }
            //sleep(1);
            busy_sleep_microseconds(10000);
        }
    }

    pthread_exit(NULL);
    return nullptr;
}

// -------------------- CPU thread --------------------
void *processor(void *arg){
    pair<int*, Scheduler*> *cpu_param = (pair<int*, Scheduler*>*)arg;
    int cpu_id = *cpu_param->first;
    Scheduler *sched = cpu_param->second;
    safe_cerr("Turn on CPU #" + to_string(cpu_id) + "\n");

    set_realtime_and_affinity(cpu_id, 80-cpu_id);
    // init Logger
    Logger logger("cpu" + to_string(cpu_id) + ".log", global_start_time);
    logger.write("CPU", cpu_id, -1, "INIT");
    busy_sleep_microseconds(10000);

    std::chrono::microseconds total_elapsed{0};
    int count = 0;
    cpu_state[cpu_id] = true;

    //pthread_mutex_lock(&mutexes[cpu_id]);
    mutexes[cpu_id].lock();
    sched->read_next_n_tasks(workload_factor1, cpu_id, logger);
    //pthread_mutex_unlock(&mutexes[cpu_id]);
    mutexes[cpu_id].unlock();
    while (true){
        // drain returned tasks safely under lock
        //pthread_mutex_lock(&mutexes[cpu_id]);
        mutexes[cpu_id].lock();
        while (!tasks_return_from_io[cpu_id].empty()){
            Task *ret_task = tasks_return_from_io[cpu_id].front();
            tasks_return_from_io[cpu_id].pop();
            //pthread_mutex_unlock(&mutexes[cpu_id]);
            mutexes[cpu_id].unlock();

            if (!ret_task){
                safe_cerr("processor: null ret_task\n");
            } else if (!ret_task->bursts.empty()){
                sched->return_task(cpu_id, ret_task);
                logger.write("CPU", cpu_id, ret_task->task_id, "ENTER_SCHED");
            } else {
                // end of task
                logger.write("CPU", cpu_id, ret_task->task_id, "FINISH_IO");
                delete ret_task;
            }

            //pthread_mutex_lock(&mutexes[cpu_id]);
            mutexes[cpu_id].lock();
        }
        //pthread_mutex_unlock(&mutexes[cpu_id]);
        mutexes[cpu_id].unlock();

        // request a task from scheduler
        auto start = chrono::steady_clock::now();
        Task *task = sched->request_task(cpu_id, logger);
        auto finish = chrono::steady_clock::now();
        total_elapsed += std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
        count += 1;
        if (task == nullptr){

            // check IO queues and returned task queues under proper locks
            bool all_io_empty = true;
            for (int i = IO_ID_OFFSET; i < IO_ID_OFFSET + NUM_IO; ++i){
                //pthread_mutex_lock(&mutexes[i]);
                mutexes[i].lock();
                all_io_empty &= io_queue[i].empty();
                //pthread_mutex_unlock(&mutexes[i]);
                mutexes[i].unlock();
            }

            bool all_ret_empty = true;
            for (int i = 0; i < NUM_CPU; ++i){
                //pthread_mutex_lock(&mutexes[i]);
                mutexes[i].lock();
                all_ret_empty &= tasks_return_from_io[i].empty();
                //pthread_mutex_unlock(&mutexes[i]);
                mutexes[i].unlock();
            }

            // check if all IO mutexes are unlocked by trying to lock them
            bool any_io_busy = false;
            for (int i = IO_ID_OFFSET; i < IO_ID_OFFSET + NUM_IO; ++i){
                //int ret = pthread_mutex_trylock(&mutexes[i]);                
                if (mutexes[i].try_lock()){
                    //pthread_mutex_unlock(&mutexes[i]);
                    mutexes[i].unlock();
                } else {
                    any_io_busy = true; // locked by IO thread or busy
                }
            }

            if (all_io_empty && !any_io_busy && all_ret_empty){
                cpu_state[cpu_id] = false; // tell IOs this cpu is idle
            } else {
                cpu_state[cpu_id] = true;
            }

            if (shut_down.load()){
                safe_cerr("Shut down cpu #" + to_string(cpu_id) + "\n");
                break;
            }
            busy_sleep_microseconds(10000);
            //sleep(1);
            continue;
        }

        if (task->bursts.empty()){
            cerr << "no time " << task->task_id << endl;
            continue;
            //throw runtime_error("no task duration time in processor");
        }
        auto job_type = task->bursts.front();
        int device_id = job_type.first;
        int duration = job_type.second;

        bool run = false;
        // CPU work
        logger.write("CPU", cpu_id, task->task_id, "ENTER_CPU");
        if (device_id < IO_ID_OFFSET){
            //logger.write("CPU", cpu_id, task->task_id, "ENTER_CPU");

            if (duration <= 0){
                continue;
                //throw runtime_error("duration time error in processor");
            }
            busy_sleep_microseconds(duration);
            task->bursts.pop();

            if (task->bursts.empty()){
                logger.write("CPU", cpu_id, task->task_id, "FINISH_CPU", to_string(duration));
                delete task;
                continue;
            }
            run = true;
            //logger.write("CPU", cpu_id, task->task_id, "LEAVE_CPU", to_string(duration));
        }
        if (run)
            logger.write("CPU", cpu_id, task->task_id, "LEAVE_CPU", to_string(duration));
        else
            logger.write("CPU", cpu_id, task->task_id, "LEAVE_CPU");
        // look at next job
        job_type = task->bursts.front();
        device_id = job_type.first;
        duration = job_type.second;

        if (device_id >= IO_ID_OFFSET){
            int io_id = device_id - IO_ID_OFFSET;
            // enqueue to io queue under io mutex
            //pthread_mutex_lock(&mutexes[device_id]);
            mutexes[device_id].lock();
            io_queue[io_id].push({cpu_id, task});
            //pthread_mutex_unlock(&mutexes[device_id]); //
            mutexes[device_id].unlock();
        } else {
            // more cpu time - return to scheduler
            sched->return_task(cpu_id, task);
            logger.write("CPU", cpu_id, task->task_id, "ENTER_SCHED");
        }
        
        //busy_sleep_microseconds(10000);
    }
    std::cerr << "Total scheduling time for CPU #" << cpu_id << ": "<< total_elapsed.count() << endl
              << "count = " << count << endl;

    pthread_exit(NULL);
    return nullptr;
}


// argv[0] argv[1] argv[2]   argv[3]    argv[4]     argv[5]
// ./main  NUM_CPU inputfile sched_algo workload_f1 workload_f2
// -------------------- main --------------------
int main(int argc, char *argv[]){
    if (argc < 4){
        cerr << "Usage: " << argv[0] << " <num_cpu (1-4)> <inputfile> <sched_algo (n/1)\n";
        return 1;
    }

    int num_cpu = stoi(argv[1]);
    if (num_cpu < 1 || num_cpu > 4){
        cerr << "num_cpu must be 1..4\n";
        return 1;
    }
    NUM_CPU = num_cpu;

    string filename = argv[2];
    string sched_algo = argv[3];
    workload_factor1 = (argc > 4 ? stoi(argv[4]) : 10);
    workload_factor2 = (argc > 5 ? stoi(argv[5]) : 1);
    cerr << "factor1 (#tasks in ready queue before sys starts): " << workload_factor1 << endl;
    cerr << "factor2 (#tasks be added after each cpu request): " << workload_factor2 << endl;
    

    // init shared state
    cpu_state = vector<bool>(NUM_CPU, true);
    tasks_return_from_io = new queue<Task*>[NUM_CPU];

    // init mutexes
    /*
    for (int i = 0; i < NUM_DEVICES; ++i){
        if (pthread_mutex_init(&mutexes[i], nullptr) != 0){
            cerr << "Failed to init mutex " << i << "\n";
            return 1;
        }
    }*/

    // init scheduler
    Scheduler *sched;
    if (sched_algo == "On"){
        sched = new Scheduler_On(filename);
    }
    else if (sched_algo == "O1"){
        sched = new Scheduler_O1(filename, NUM_CPU);
    }
    else{
        cerr << "choose scheduler algorithm (n/1)";
        return 1;
    }


    // set time
    global_start_time = chrono::steady_clock::now();

    // create IO threads
    pthread_t io_threads[NUM_IO];
    vector<int*> io_ids;
    for (int i = 0; i < NUM_IO; ++i){
        int device_id = IO_ID_OFFSET + i;
        int *arg = new int(device_id);
        io_ids.push_back(arg);
        int ret = pthread_create(&io_threads[i], nullptr, IO_device, arg);
        if (ret != 0){
            cerr << "pthread_create error: IO\n";
            return 1;
        }
    }

    // create CPU threads
    pthread_t cpu_threads[NUM_CPU];
    vector<pair<int*, Scheduler*>*> cpu_params;
    vector<int*> cpu_ids;
    for (int i = 0; i < NUM_CPU; ++i){
        int *id = new int(i);
        cpu_ids.push_back(id);
        auto *param = new pair<int*, Scheduler*>(id, sched);
        cpu_params.push_back(param);
        int ret = pthread_create(&cpu_threads[i], nullptr, processor, param);
        if (ret != 0){
            cerr << "pthread_create error: CPU\n";
            return 1;
        }
    }

    // join IO threads
    for (int i = 0; i < NUM_IO; ++i){
        pthread_join(io_threads[i], NULL);
    }

    // join CPU threads
    for (int i = 0; i < NUM_CPU; ++i){
        pthread_join(cpu_threads[i], NULL);
    }

    // free small allocations
    for (auto p : io_ids) delete p;
    for (auto p : cpu_ids) delete p;
    for (auto p : cpu_params) delete p;

    delete[] tasks_return_from_io;
    delete sched;

    safe_cerr("Program exiting cleanly\n");
    return 0;
}

void busy_sleep_microseconds(int duration_us) {
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::microseconds(duration_us);

    while (std::chrono::steady_clock::now() < end) {
        // busy wait
    }
}


// version 10: including time stamp
// set affinity to each threads
// busy waiting timer 

// version 15: inherited from 12
// add O(n) scheduler 

// version 18: add O(1) stage1
// version 19: tasks have rt_prioriy, nice, policy
//             O(1) only supports single cpu
// version 20: 10/22 not done anything yet
// version 20: 11/6 finishes O(1)

// version 21: copy from 20, not done anything yet, 
// only needs a taskcreator, analyzer
// version 21: 11/6 adds taskGenerator, only needs analyzer

// version 23: delete unnecessary mutex, rename variables, memory leaks checked