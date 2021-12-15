#include "./include/timer.hpp"
#include <stdio.h>

void Timer::rewind()
{
    running = true;
    start = std::chrono::system_clock::now();
}

bool Timer::check()
{
    if (!running)
    {
        return true;
    }
    if (alert){
        alert = false;
        return false;
    }
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count() < upper;
}

void Timer::stop()
{
    running = false;
}