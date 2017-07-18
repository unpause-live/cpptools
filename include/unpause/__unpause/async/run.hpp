/* Copyright (c) 2017 Unpause, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#ifndef UNPAUSE_ASYNC_RUN_HPP
#define UNPAUSE_ASYNC_RUN_HPP

#include <unpause/__unpause/async/thread_pool.hpp>

namespace unpause { namespace async {
    
    // run(task_queue...)
    template<class R, class... Args>
    void run(task_queue& queue, task<R, Args...>& t)
    {
        queue.add(t);
        queue.next();
    }
    
    template<class R, class... Args>
    void run(task_queue& queue, R&& r, Args&&... a) {
        queue.add(std::forward<R>(r), std::forward<Args...>(a)...);
        queue.next();
    }
    
    // run(thread_pool...)
    namespace detail {
        void run(thread_pool& pool, std::unique_ptr<detail::task_container>& task) {
            pool.tasks.add(std::move(task));
            pool.task_waiter.notify_one();
        }
    }
    
    template<class R, class... Args>
    void run(thread_pool& pool, task<R, Args...>& t) {
        pool.tasks.add(t);
        pool.task_waiter.notify_one();
    }
    
    template<class R, class... Args>
    void run(thread_pool& pool, R&& r, Args&&... a) {
        pool.tasks.add(std::forward<R>(r), std::forward<Args...>(a)...);
        pool.task_waiter.notify_one();
    }
    
    // run(thread_pool, task_queue...)
    template<class R, class... Args>
    void run(thread_pool& pool, task_queue& queue, task<R, Args...>& t)
    {
        if(!queue.complete.load()) {
            auto after = std::move(t.after_internal);
            t.after_internal = [&, after = std::move(after)] {
                if(after) {
                    after();
                }
                if(!queue.complete.load() && queue.has_next()) {
                    auto next = std::move(queue.tasks.front());
                    queue.tasks.pop();
                    detail::run(pool, next);
                } else {
                    queue.task_mutex.unlock();
                }
            };
            queue.add(t);
            if(queue.task_mutex.try_lock()) {
                auto next = std::move(queue.tasks.front());
                queue.tasks.pop();
                detail::run(pool, next);
            }
        }
    }
    template<class R, class... Args>
    void run(thread_pool& pool, task_queue& queue, R&& r, Args&&... a) {
        if(!queue.complete.load()) {
            auto t = make_task(std::forward<R>(r), std::forward<Args...>(a)...);
            run(pool, queue, t);
        }
    }
    
    // run_sync
    template<class R, class... Args>
    void run_sync(thread_pool& pool, task<R, Args...>& t) {
        std::mutex m;
        std::condition_variable v;
        std::atomic<bool> d(false);
        std::function<void()> after = std::move(t.after_internal);
        
        t.after_internal = [&v,&d, after = std::move(after)] {
            d = true;
            if(after) {
                after();
            }
            v.notify_one();
        };
        run(pool, t);
        
        std::unique_lock<std::mutex> lk(m);
        v.wait(lk, [&d] { return d.load(); });
    }
    
    template<class R, class... Args>
    void run_sync(thread_pool& pool, R&& r, Args&&... a) {
        auto t = make_task(r, a...);
        run_sync(pool, t);
    }
    
    template<class R, class... Args>
    void run_sync(thread_pool& pool, task_queue& queue, task<R, Args...>& t)
    {
        if(!queue.complete.load()) {
            std::mutex m;
            std::condition_variable v;
            std::atomic<bool> d(false);
            std::function<void()> after = std::move(t.after_internal);
            
            t.after_internal = [&v,&d, after = std::move(after)] {
                d = true;
                if(after) {
                    after();
                }
                v.notify_one();
            };
            
            run(pool, queue, t);
            
            std::unique_lock<std::mutex> lk(m);
            v.wait(lk, [&d] { return d.load(); });
        }
    }
    
    template<class R, class... Args>
    void run_sync(thread_pool& pool, task_queue& queue, R&& r, Args&&... a) {
        auto t = make_task(r, a...);
        run_sync(pool, queue, t);
    }
}
}
#endif /* UNPAUSE_ASYNC_RUN_HPP */
