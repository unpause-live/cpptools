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

#include <experimental/optional>
#include <chrono>
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
            mutex_internal_.lock();
            tasks_.push_back(std::move(nt));
            mutex_internal_.unlock();
        }
        
        void add(std::unique_ptr<detail::task_container>&& task) {
            mutex_internal_.lock();
            tasks_.push_back(std::move(task));
            mutex_internal_.unlock();
        }
        
        template<class R, class... Args>
        void add(R&& r, Args&&... a) {
            std::unique_ptr<detail::task_container> t;
            t = std::make_unique<task<R, Args...>>(std::forward<R>(r), std::forward<Args>(a)...);
            mutex_internal_.lock();
            tasks_.push_back(std::move(t));
            mutex_internal_.unlock();
        }
        
        bool next() {
            auto f = next_pop();
            
            if(f && !complete.load()) {
                f->run_v();
            }
            
            return has_next();
        }
        
        bool has_next() {
            return tasks_.size() > 0;
        }
        
        std::chrono::steady_clock::time_point next_dispatch_time() {
            std::unique_lock<std::mutex> lk(mutex_internal_);
            if(tasks_.size() > 0) {
                return tasks_.front()->dispatch_time;
            } else {
                return std::chrono::steady_clock::time_point::min();
            }
        }
        
        std::unique_ptr<detail::task_container> next_pop() {
            mutex_internal_.lock();
            std::unique_ptr<detail::task_container> f;
            if(has_next()) {
                f = std::move(tasks_.front());
                tasks_.pop_front();
            }
            mutex_internal_.unlock();
            return f;
        }
        
        void sort(std::function<bool(const detail::task_container& lhs, const detail::task_container& rhs)> predicate) {
            mutex_internal_.lock();
            std::sort(tasks_.begin(), tasks_.end(), [predicate](std::unique_ptr<detail::task_container>& lhs, std::unique_ptr<detail::task_container>& rhs) {
                return predicate(*lhs, *rhs);
            });
            mutex_internal_.unlock();
        }
        
        std::mutex task_mutex;
        std::atomic<bool> complete;
    private:
        std::deque<std::unique_ptr<detail::task_container>> tasks_;
        std::mutex mutex_internal_;
        
    };
}
}

#endif /* UNPAUSE_ASYNC_TASK_QUEUE_HPP */
