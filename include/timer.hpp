/**
 * @file timer.hpp
 * @author zqs
 * @brief 简单计时器
 * @version 0.1
 * @date 2021-12-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef TIMER_HPP
#define TIMER_HPP
#include <chrono>
class Timer{
private:
    std::chrono::system_clock::time_point start;
    bool running; // 定时器是否启动
    bool alert;   // 定时器是否提前结束
    double upper;
    std::chrono::system_clock::time_point finish;
public:
    /**
     * @brief Construct a new Timer object
     * 
     * @param _upper 时间上限，单位ms
     */
    Timer(double _upper): upper(_upper), running(false), alert(false){}

    /**
     * @brief 重置并开始
     * 
     */
    void rewind();
    /**
     * @brief 检查是否未到超时时间
     * 
     * @return true 定时器未启动或者未超时
     * @return false 超时或者提前结束
     */
    bool check();

    /**
     * @brief 暂停
     * 
     */
    void stop();

    /**
     * @brief 继续
     * 
     */
    void conti() {running = true;}

    /**
     * @brief 提前到时
     * 
     */
    void stop_earlier() {alert = true;}

};

#endif