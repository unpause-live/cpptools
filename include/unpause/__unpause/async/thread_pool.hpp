/* Copyright (c) 2020 Unpause, SAS.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#ifndef UNPAUSE_ASYNC_THREAD_POOL_HPP
#define UNPAUSE_ASYNC_THREAD_POOL_HPP

#include <condition_variable>
#include <optional>
#include <atomic>
#include <thread>
#include <list>

namespace unpause { namespace async {
    
    class thread_pool
    {
    public:
        thread_pool(int thread_count = std::thread::hardware_concurrency()) : exiting_(false) {
            for(int i = 0 ; i < thread_count ; i++ ) {
                threads_.push_back(std::thread(std::bind(&thread_pool::thread_func, this)));
            }
        };
        ~thread_pool() {
            exiting_ = true;
            tasks.complete = true;
            task_waiter.notify_all();
            for(auto & it : threads_) {
                if(it.joinable()) {
                    it.join();
                }
            }
        }
        
        task_queue tasks;
        std::condition_variable task_waiter;
        std::mutex task_mutex;
        std::optional<run_loop> runloop;
        
    private:
        void thread_func() {
            while(!exiting_.load()) {
                std::unique_lock<std::mutex> lk(task_mutex);
                task_waiter.wait_for(lk, std::chrono::milliseconds(100), [this]{ return tasks.has_next() || exiting_.load(); });
                auto f = tasks.next_pop();
                lk.unlock();
                if(f && !exiting_.load()) {
                    f->run_v();
                }
            }
        }
        std::atomic<bool> exiting_;
        std::list<std::thread> threads_;
    };
    
}
}


#endif /* UNPAUSE_ASYNC_THREAD_POOL_HPP */
