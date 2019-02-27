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
#include <algorithm>
#include <cassert>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <deque>
#include <mutex>

namespace unpause { namespace async {

    struct task_queue
    {
        task_queue() : token(std::make_shared<std::atomic<bool>>(true)), complete(false), end_sem_(0), count_(0) {};
        task_queue(const task_queue& other) = delete;
        task_queue(task_queue&& other) = delete;
        task_queue& operator=(const task_queue& other) = delete;
        task_queue& operator=(task_queue&& other) = delete;

        // TODO: replace with a more robust semaphore implementation.
        // Final tasks have 5 seconds to finish.  If it needs more time, use run_sync.
        ~task_queue() { 
            mutex_internal_.lock();
            token->store(false, std::memory_order_release);
            complete = true; 
            token.reset();
            tasks_.clear();
            mutex_internal_.unlock();
            auto start = std::chrono::steady_clock::now();
            while(end_sem_.load() > 0 && ((std::chrono::steady_clock::now() - start) < std::chrono::seconds(5))) { std::this_thread::yield(); }
        };
        
        template<class R, class... Args>
        void add(task<R, Args...>& t) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(std::forward<task<R, Args...>>(t));
            add(std::move(nt));
        }
        
        void add(std::unique_ptr<detail::task_container>&& task) {
            std::weak_ptr<std::atomic<bool>> tkn = token;
            
            if(!tkn.expired() && !complete.load()) {
                inc_lock();
                {
                    std::lock_guard<std::mutex> lk(mutex_internal_);
                    if(!tkn.expired() && !complete.load()) {
                        if(!task->use_token) {
                            task->token = token;
                            task->use_token = true;
                        }
                        tasks_.push_back(std::move(task));
                        std::atomic_thread_fence(std::memory_order_release);
                        count_.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                dec_lock();
            }
            
        }
        
        template<class R, class... Args>
        void add(R&& r, Args&&... a) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(std::forward<R>(r), std::forward<Args>(a)...);
            add(std::move(nt));
            
        }
        
        void inc_lock() {
            ++end_sem_;
        }
        
        void dec_lock() {
            --end_sem_;
        }

        bool next() {
            inc_lock();
            auto f = next_pop();
            if(f && !complete.load()) {
                f->run_v();
            }
            dec_lock();
            return has_next();
        }
        
        bool has_next() {
            if(!complete.load()) {
                std::atomic_thread_fence(std::memory_order_acquire);
                auto count = count_.load(std::memory_order_relaxed);
                return count > 0;
            }
            return false;
        }
        
        std::chrono::steady_clock::time_point next_dispatch_time() {
            std::weak_ptr<std::atomic<bool>> tkn = token;
            
            if(!tkn.expired() && !complete.load()) {
                inc_lock();
                {

                    std::lock_guard<std::mutex> lk(mutex_internal_);
                    if(!tkn.expired() && has_next()) {
                        return tasks_.front()->dispatch_time;
                    }
                }
                dec_lock();
            }
            
            return std::chrono::steady_clock::time_point::min();
        }
        
        std::unique_ptr<detail::task_container> next_pop() {
            std::unique_ptr<detail::task_container> f = nullptr;
            std::weak_ptr<std::atomic<bool>> tkn = token;
            
            if(!tkn.expired() && !complete.load()) {
                inc_lock(); 
                {
                    std::lock_guard<std::mutex> lk(mutex_internal_);
                    if(!tkn.expired() && has_next()) {
                        std::atomic_thread_fence(std::memory_order_acquire);
                        auto& task = tasks_.front();
                        f = std::move(task);
                        tasks_.pop_front();
                        count_.fetch_sub(1, std::memory_order_relaxed);
                    }
                }
                dec_lock();
            }
            
            return f;
        }
        
        void sort(std::function<bool(const detail::task_container& lhs, const detail::task_container& rhs)> predicate) {
            std::weak_ptr<std::atomic<bool>> tkn = token;
            if(!tkn.expired() && !complete.load()) {
                inc_lock();
                mutex_internal_.lock();
                if(!tkn.expired() && !complete.load()) {
                    std::sort(tasks_.begin(), tasks_.end(), [predicate](std::unique_ptr<detail::task_container>& lhs, std::unique_ptr<detail::task_container>& rhs) {
                        return predicate(*lhs, *rhs);
                    });
                }   
                mutex_internal_.unlock();
                dec_lock();
            }
            std::atomic_thread_fence(std::memory_order_release);
        }
        
        void set_name(const std::string& name) {
            name_ = name;
        }

        const std::string name() const { return name_; }

        std::shared_ptr<std::atomic<bool>> token;
        std::mutex task_mutex;
        std::atomic<bool> complete;
    private:
        std::deque<std::unique_ptr<detail::task_container>> tasks_;
        std::mutex mutex_internal_;
        std::atomic<int> end_sem_;
        std::atomic<int64_t> count_;
        std::string name_;
    };
}
}

#endif /* UNPAUSE_ASYNC_TASK_QUEUE_HPP */
