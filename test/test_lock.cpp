#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <vector>

using namespace std;

condition_variable cv1;
condition_variable cv2;
mutex m1;
mutex m2;
vector<int> v1;

atomic_bool deadlock;


void fun1(){
    unique_lock<mutex> ul1(m1);
    printf("fun1 get m1\n");
    {
        unique_lock<mutex> ul2(m2);
        printf("fun1 get m2\n");
        cv2.wait(ul2, []{return false;}); // 阻塞
    }
}

void fun2(){
    unique_lock<mutex> ul1(m1);
    printf("fun2 get m1\n");
    deadlock.store(false);
}


int main(){
    deadlock.store(true);
    thread t1(fun1);
    thread t2(fun2);
    this_thread::sleep_for(3s);
    if(deadlock) printf("deadlock!\n");
    t1.detach();
    t2.detach();
    return 0;
}