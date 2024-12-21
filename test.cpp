#include "include/slog.h"
#include <iostream>
#include <string>
#include <vector>
#include <ctime>

void benchmark(){
    const char* const str = "benchmark";
    auto begin = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 100000; i++){
        LOG_INFO << "Logging-" << i << "-double-" << -99.876 << "-uint64-" << (uint64_t)i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    long int avg = duration.count() / 100000;
    printf("Avg: %ld\n", avg);
    printf("Total: %ld\n", duration.count());
}

template<typename F>
void create_thread(F&& f, int cnt){
    std::vector<std::thread>threads;
    for(int i = 0; i < cnt; i++){
        threads.emplace_back(f);
    }
    for(int i = 0; i < cnt; i++){
        threads[i].join();
    }
}

int main(){
    slog::init("/tmp/log/", "log", 8);
    for(auto threads:{1,2,3}){
        create_thread(benchmark, threads);
    }

    LOG_INFO << "HELLO";
    int a=1;
    int b=2;
    CHECK_EQ_F(1,2);

    return 0;
}