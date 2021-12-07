/**
 * @file test_timer.cpp
 * @author zqs
 * @brief 测试计时器
 * @version 0.1
 * @date 2021-12-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <iostream>
#include <chrono>
#include <thread>
#include "../include/timer.hpp"

int main ()
{
    Timer t(5);
    t.rewind();
    int i = 0;
    while(true){
        if(!t.check()){
            break;
        }
        printf("%d\n", i++);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}