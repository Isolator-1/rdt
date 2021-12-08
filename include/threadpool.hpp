/**
 * @file threadpool.hpp
 * @author 贝蒂儿·施密特等《并行程序概念与设计》
 * @brief 线程池
 * @version 0.1
 * @date 2021-12-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */


#ifndef CHATTING_THREAD_POOL
#define CHATTING_THREAD_POOL

#include <cstdint>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class ThreadPool
{
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void(void)>> tasks; // 任务队列
    std::mutex mutex;
    std::condition_variable cv, cv2;

    bool stop_pool;
    std::atomic<uint32_t> active_threads;
    const uint32_t capacity;

    template <
        typename Func,
        typename... Args,
        typename Rtrn = typename std::result_of<Func(Args...)>::type>
        auto make_task(
            Func&& func,
            Args &&...args) -> std::packaged_task<Rtrn(void)>
    {
        auto aux = std::bind(std::forward<Func>(func),
            std::forward<Args>(args)...);
        return std::packaged_task<Rtrn(void)>(aux);
    }

    void before_task_hook()
    {
        active_threads++;
    }

    void after_task_hook()
    {
        active_threads--;
        /*
        if (active_threads == 0 && tasks.empty())
        {
            stop_pool = true;
            cv2.notify_one();
        }
        */
    }

public:
    ThreadPool(uint32_t _capacity) : stop_pool(false),
        active_threads(0),
        capacity(_capacity)
    {
        auto wait_loop = [this]() -> void {
            while (true)
            {
                std::function<void(void)> task;

                { // lock this section for waiting
                    std::unique_lock<std::mutex> unique_lock(mutex);
                    auto predicate = [this]() -> bool {
                        return stop_pool || !tasks.empty();
                    };
                    cv.wait(unique_lock, predicate); // 当线程池未被停止时，等待新的任务唤醒

                    if (stop_pool && tasks.empty())
                        return;

                    task = std::move(tasks.front());
                    tasks.pop();
                    before_task_hook();
                } // release the lock

                task(); // execute the task in parallel

                {
                    std::lock_guard<std::mutex> lock_guard(mutex);
                    after_task_hook();
                } // release the lock
            }
        };
        // 初始化capacity个线程，并都在等待任务
        for (uint32_t id = 0; id < capacity; id++)
        {
            threads.emplace_back(wait_loop);
        }
    }

    ~ThreadPool()
    {
        if(!stop_pool){
            stop();
        }
    }

    
    uint32_t active(){
        // std::lock_guard<std::mutex> lock_guard(mutex);
        return active_threads;
    }


    template <typename Func,
        typename... Args,
        typename Rtrn = typename std::result_of<Func(Args...)>::type>
        auto enqueue(Func&& func,
            Args &&...args) -> std::future<Rtrn>
    {
        auto task = make_task(func, args...);                              // 创建打包任务
        auto future = task.get_future();                                   // 分派相关的future变量
        auto task_ptr = std::make_shared<decltype(task)>(std::move(task)); // 使用共享指针包装任务
        {
            std::lock_guard<std::mutex> lock_guard(mutex);
            if (stop_pool)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            auto payload = [task_ptr]() -> void {
                task_ptr->operator()();
            };
            tasks.emplace(payload); // 加入任务队列
        } // release the lock
        cv.notify_one(); // 尝试唤醒一个线程
        return future;
    }

    /**
     * 结束线程池任务
     **/
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock_guard(mutex);
            stop_pool = true;
        }
        cv.notify_all();
        for (auto& thread : threads){
            if(thread.joinable()){
                thread.join();
            }
        }
    }

    /**
     * wait for pool being set to stop
     **/
    void wait_and_stop()
    {
        std::unique_lock<std::mutex> unique_lock(mutex);
        auto predicate = [&]() -> bool {
            return stop_pool;
        };
        cv2.wait(unique_lock, predicate);
    }

    
    template <typename Func, typename... Args>
    bool spawn(Func&& func, Args &&...args)
    {
        if (active_threads < capacity)
        {
            enqueue(func, args...); // enqueue if idling
            return true;
        }
        else
        {
            func(args...); // process sequentially
            return false;
        }
    }



};
#endif