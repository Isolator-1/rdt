#ifndef TIMER_HPP
#define TIMER_HPP
#include <chrono>
class Timer{
private:
    std::chrono::system_clock::time_point start;
    bool running;
    double upper;
public:
    Timer(double _upper): upper(_upper), running(false){}
    void rewind();
    bool check();
    void stop();
};

#endif