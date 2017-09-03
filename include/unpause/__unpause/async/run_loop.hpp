/* Copyright (c) 2017 Unpause, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */
#ifndef UNPAUSE_ASYNC_RUN_LOOP_HPP
#define UNPAUSE_ASYNC_RUN_LOOP_HPP

#include <condition_variable>
#include <thread>
#include <mutex>
#include <atomic>

namespace unpause { namespace async {

    class run_loop {

    public:
        run_loop() : exiting_(false), dirty_(false), looper_(&run_loop::loop, this) {};
        ~run_loop() {
            exiting_ = true;
            cond_.notify_one();
            if(looper_.joinable()) {
                looper_.join();
            }
        };
        
        void notify() {
            queue.sort([](const detail::task_container& lhs, const detail::task_container& rhs) {
                return lhs.dispatch_time < rhs.dispatch_time;
            });
            dirty_ = true;
            cond_.notify_one();
        };
        
        task_queue queue;
        
    private:
        void loop() {
            while(!exiting_.load()) {
                std::unique_lock<std::mutex> lk (mutex_);
                auto next_time = queue.next_dispatch_time();
                if(next_time == std::chrono::steady_clock::time_point::min()) {
                    cond_.wait(lk, [=]{ return exiting_.load() || queue.has_next(); });
                } else if(std::chrono::steady_clock::now() < next_time) {
                    cond_.wait_until(lk, next_time, [=] {
                        auto now = std::chrono::steady_clock::now();
                        return exiting_.load() || queue.next_dispatch_time() <= now || dirty_.load();
                    });
                }
                dirty_ = false;
                while(queue.next_dispatch_time() <= std::chrono::steady_clock::now() &&
                      queue.next_dispatch_time() != std::chrono::steady_clock::time_point::min() &&
                      !exiting_.load()) {
                    queue.next();
                }
            }
        };
        
        
    private:
        std::atomic<bool> exiting_;
        std::atomic<bool> dirty_;
        std::thread looper_;
        std::condition_variable cond_;
        std::mutex mutex_;
    };
}
}

#endif
