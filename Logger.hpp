#pragma once
#include <fstream>
#include <chrono>
#include <string>

class Logger {
    std::ofstream file;
    std::chrono::steady_clock::time_point start_time;

public:
    Logger(const std::string& filename,
           std::chrono::steady_clock::time_point t0);
//        : file(filename, std::ios::out | std::ios::trunc), start_time(t0) {}

    void write(const std::string& thread_type, int thread_id,
               int task_id, const std::string& event,
               const std::string& extra = "");
};
