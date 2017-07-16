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

#include <queue>
#include <atomic>

namespace unpause { namespace async {
    struct task_queue
    {
        task_queue() : complete(false) {};
        ~task_queue() { complete = true; task_mutex.lock(); };
        
        template<class R, class... Args>
        void add(task<R, Args...>& t) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(t);
            tasks.push(std::move(nt));
        }
        
        void add(std::unique_ptr<detail::task_container>&& task) {
            tasks.push(std::move(task));
        }
        
        template<class R, class... Args>
        void add(R&& r, Args&&... a) {
            
            std::unique_ptr<detail::task_container> t;
            t = std::make_unique<task<R, Args...>>(std::forward<R>(r), std::forward<Args...>(a)...);
            tasks.push(std::move(t));
        
        }
        
        bool next() {
            if(tasks.size() > 0) {
                auto f = std::move(tasks.front());
                tasks.pop();
                f->run_v();
            }
            return has_next();
        }
        
        bool has_next() {
            return tasks.size() > 0;
        }
        
        std::queue<std::unique_ptr<detail::task_container>> tasks;
        std::atomic<bool> complete;
        std::mutex task_mutex;
    };
}
}

#endif /* UNPAUSE_ASYNC_TASK_QUEUE_HPP */
