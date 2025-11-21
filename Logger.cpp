#include "Logger.hpp"


Logger::Logger(const std::string& filename,
               std::chrono::steady_clock::time_point t0)
            : file(filename, std::ios::out | std::ios::trunc), start_time(t0) {}

void Logger::write(const std::string& thread_type, int thread_id,
                   int task_id, const std::string& event,
                   const std::string& extra) {
            
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto us = duration_cast<microseconds>(now - start_time).count();
    file << us << " " << thread_type << " " << thread_id
            << " " << task_id << " " << event;
    if (!extra.empty()) file << " " << extra;
    file << "\n";
}

