/* Copyright (c) 2017 Unpause, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#ifndef UNPAUSE_ASYNC_TASK_QUEUE_HPP
#define UNPAUSE_ASYNC_TASK_QUEUE_HPP

#include <memory>
#include <atomic>
#include <deque>
#include <mutex>

namespace unpause { namespace async {
    struct task_queue
    {
        task_queue() : complete(false) {};
        ~task_queue() { complete = true; task_mutex.lock(); };
        
        template<class R, class... Args>
        void add(task<R, Args...>& t) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(std::forward<task<R, Args...>>(t));
            mutex_internal.lock();
            tasks.push_back(std::move(nt));
            mutex_internal.unlock();
        }
        
        void add(std::unique_ptr<detail::task_container>&& task) {
            mutex_internal.lock();
            tasks.push_back(std::move(task));
            mutex_internal.unlock();
        }
        
        template<class R, class... Args>
        void add(R&& r, Args&&... a) {
            std::unique_ptr<detail::task_container> t;
            t = std::make_unique<task<R, Args...>>(std::forward<R>(r), std::forward<Args>(a)...);
            mutex_internal.lock();
            tasks.push_back(std::move(t));
            mutex_internal.unlock();
        }
        
        bool next() {
            auto f = next_pop();
            
            if(f && !complete.load()) {
                f->run_v();
            }
            
            return has_next();
        }
        
        bool has_next() {
            return tasks.size() > 0;
        }
        
        std::unique_ptr<detail::task_container> next_pop() {
            mutex_internal.lock();
            std::unique_ptr<detail::task_container> f;
            if(has_next()) {
                f = std::move(tasks.front());
                tasks.pop_front();
            }
            mutex_internal.unlock();
            return f;
        }
        
        std::mutex task_mutex;
        std::atomic<bool> complete;
    private:
        std::deque<std::unique_ptr<detail::task_container>> tasks;
        std::mutex mutex_internal;
        
    };
}
}

#endif /* UNPAUSE_ASYNC_TASK_QUEUE_HPP */
