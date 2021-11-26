#include "./include/timer.hpp"
#include <stdio.h>

void Timer::rewind() { 
    running = true;
    start = std::chrono::system_clock::now();
}

bool Timer::check() {
    if (!running) {
        return true;
    }
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = end - start;
    // printf("pass: %f s, upper: %d\n", diff.count(), upper);
    return diff.count() < upper;
}

void Timer::stop() {
    running = false;
}