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
        queue.add(std::forward<R>(r), std::forward<Args>(a)...);
        queue.next();
    }
    
    // run(thread_pool...)
    namespace detail {
        inline void run(thread_pool& pool, std::unique_ptr<detail::task_container>&& task) {
            pool.tasks.add(std::move(task));
            pool.task_waiter.notify_one();
        }
    }
    
    template<class R, class... Args>
    void run(thread_pool& pool, task<R, Args...>& t) {
        std::lock_guard<std::mutex> guard(pool.task_mutex);
        pool.tasks.add(t);
        pool.task_waiter.notify_one();
    }
    
    template<class R, class... Args>
    void run(thread_pool& pool, R&& r, Args&&... a) {
        std::lock_guard<std::mutex> guard(pool.task_mutex);
        pool.tasks.add(std::forward<R>(r), std::forward<Args>(a)...);
        pool.task_waiter.notify_one();
    }
    
    // run(thread_pool, task_queue...)
    template<class R, class... Args>
    void run(thread_pool& pool, task_queue& queue, task<R, Args...>& t)
    {
        std::weak_ptr<std::atomic<bool>> token = queue.token;
        if(!token.expired() && !queue.complete.load()) {
            auto after = std::move(t.after_internal);
            auto try_next = [&, token] {
                if(!token.expired() && queue.has_next() && queue.task_mutex.try_lock()) {
                    auto next = queue.next_pop();
                    if(next) {
                        queue.inc_lock(); // add in-flight
                        detail::run(pool, std::move(next));
                    } else {
                        queue.task_mutex.unlock();
                    }
                } 
            };

            t.after_internal = [&, after = std::move(after), try_next] {
                if(after) {
                    after();
                }
                queue.task_mutex.unlock();
                try_next();
                queue.dec_lock();
            };
            queue.add(t);
            try_next();
        }
    }
    template<class R, class... Args>
    void run(thread_pool& pool, task_queue& queue, R&& r, Args&&... a) {
        if(!queue.complete.load()) {
            auto t = make_task(std::forward<R>(r), std::forward<Args>(a)...);
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
        
        t.after_internal = [&, after = std::move(after)] {
            if(after) {
                after();
            }
            std::lock_guard<std::mutex> guard(m);
            d = true;
            v.notify_one();
        };
        
        run(pool, t);
        std::unique_lock<std::mutex> lk(m);
        v.wait(lk, [&] { return d.load(); });
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
            auto after = std::move(t.after_internal);

            t.after_internal = [&, after = std::move(after)] {
                if(after) {
                    after();
                }
                std::lock_guard<std::mutex> guard(m);
                d = true;
                v.notify_one();
            };
            
            run(pool, queue, t);
            std::unique_lock<std::mutex> lk(m);
            v.wait(lk, [&] { return d.load(); });
        }
    }
    
    template<class R, class... Args>
    void run_sync(thread_pool& pool, task_queue& queue, R&& r, Args&&... a) {
        auto t = make_task(r, a...);
        run_sync(pool, queue, t);
    }
    
    // schedule
    
    template<class R, class... Args>
    void schedule(thread_pool& pool, std::chrono::steady_clock::time_point point, task<R, Args...>&& t) {
        
        auto w = make_task([] (thread_pool& pool, task<R, Args...>&& tsk){
            run(pool, tsk);
        }, pool, std::move(t));
        
        w.dispatch_time = point;
        if(!pool.runloop) {
            pool.runloop.emplace();
        }
        pool.runloop->queue.add(w);
        pool.runloop->notify();
    }
    
    template<class R, class... Args>
    void schedule(thread_pool& pool, std::chrono::steady_clock::time_point point, R&& r, Args&&... a) {
        schedule(pool, point, make_task(std::forward<R>(r), std::forward<Args>(a)...));
    }
    
    template<class R, class... Args>
    void schedule(thread_pool& pool, task_queue& queue, std::chrono::steady_clock::time_point point, task<R, Args...>&& t) {
        std::weak_ptr<std::atomic<bool>> token = queue.token;
        auto w = make_task([token] (thread_pool& pool, task_queue& queue, task<R, Args...>&& t){
            if(!token.expired()) {
                run(pool, queue, t);
            }
        }, pool, queue, std::move(t));
        w.dispatch_time = point;
        if(!pool.runloop) {
            pool.runloop.emplace();
        }
        pool.runloop->queue.add(w);
        pool.runloop->notify();
    }
    template<class R, class... Args>
    void schedule(thread_pool& pool, task_queue& queue, std::chrono::steady_clock::time_point point, R&& r, Args&&... a) {
        schedule(pool, queue, point, make_task(std::forward<R>(r), std::forward<Args>(a)...));
    }
    
    template<class R, class... Args>
    void schedule(run_loop& loop, task_queue& queue, std::chrono::steady_clock::time_point point, task<R, Args...>&& t) {
        std::weak_ptr<std::atomic<bool>> token = queue.token;
        auto w = make_task([token] (thread_pool& pool, task_queue& queue, task<R, Args...>&& t){
            if(!token.expired()) {
                run(queue, t);
            }
        }, queue, std::move(t));
        w.dispatch_time = point;
        loop.queue.add(w);
        loop.notify();
    }
    
    template<class R, class... Args>
    void schedule(run_loop& loop, task_queue& queue, std::chrono::steady_clock::time_point point, R&& r, Args&&... a) {
        schedule(loop, queue, point, make_task(std::forward<R>(r), std::forward<Args>(a)...));
    }
    
    template<class R, class... Args>
    void schedule(run_loop& loop, std::chrono::steady_clock::time_point point, task<R, Args...>&& t) {
        t.dispatch_time = point;
        loop.queue.add(t);
        loop.notify();
    }
    
    template<class R, class... Args>
    void schedule(run_loop& loop, std::chrono::steady_clock::time_point point, R&& r, Args&&... a) {
        schedule(loop, point, make_task(std::forward<R>(r), std::forward<Args>(a)...));
    }
}
}
#endif /* UNPAUSE_ASYNC_RUN_HPP */
