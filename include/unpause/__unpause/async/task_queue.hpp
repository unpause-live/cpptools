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
#include <chrono>
#include <memory>
#include <atomic>
#include <deque>
#include <mutex>
#include <assert.h>

namespace unpause { namespace async {

    class task_queue 
    {

    public:
        task_queue() : complete(false), head(nullptr), tail(nullptr) {};
        task_queue(const task_queue& other) = delete;
        task_queue(task_queue&& other) = delete;

        void add(std::unique_ptr<detail::task_container>&& task) {
            sort_push_mutex_.lock();
            node* new_head = new node;
            new_head->task = std::move(task);
            new_head->next = head.load(std::memory_order_relaxed);
            while(!head.compare_exchange_strong(new_head->next, new_head, std::memory_order_release, std::memory_order_relaxed));

            // if we don't have a tail, set it to the head.
            if(!new_head->next) {
                node* old_tail = nullptr;
                tail.compare_exchange_strong(old_tail, new_head, std::memory_order_release, std::memory_order_relaxed);    
                assert(new_head->next == old_tail);
            } else {
                new_head->next->prev = new_head;
            }
            sort_push_mutex_.unlock();
        }

        template<class R, class... Args>
        void add(task<R, Args...>& t) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(std::forward<task<R, Args...>>(t));
            add(std::move(nt));
        }

        template<class R, class... Args>
        void add(R&& r, Args&&... a) {
            std::unique_ptr<detail::task_container> nt = std::make_unique<task<R, Args...>>(std::forward<R>(r), std::forward<Args>(a)...);
            add(std::move(nt));
        }

        bool next() {
            auto f = next_pop();
            if(f && !complete.load()) {
                f->run_v();
            }
            return has_next();
        }   

        bool has_next() {
            return head.load(std::memory_order_acquire) != nullptr;
        }

        std::unique_ptr<detail::task_container> next_pop() {
            sort_pop_mutex_.lock();
            std::unique_ptr<detail::task_container> f;

            node* old_tail = tail.load(std::memory_order_acquire);

            if(old_tail) {
                tail.store(old_tail->prev, std::memory_order_release);
                f = std::move(old_tail->task);
                node* old_head = head.load(std::memory_order_acquire);
                if(old_head == old_tail) {
                    head.store(nullptr, std::memory_order_release);
                }
                delete old_tail;
            }
            sort_pop_mutex_.unlock();
            return f;
        }

        std::chrono::steady_clock::time_point next_dispatch_time() {
            node* t = tail.load(std::memory_order_acquire);
            if(t) {
                return t->task->dispatch_time;
            } else {
                return std::chrono::steady_clock::time_point::min();
            }
        }

        void sort(std::function<bool(const detail::task_container& lhs, const detail::task_container& rhs)> predicate) {
            sort_push_mutex_.lock();
            sort_pop_mutex_.lock();
            node* n = head.load(std::memory_order_relaxed);
            while(n) {
                if(n->next) {
                    bool swap = predicate(*n->task, *n->next->task);
                    if(swap) {
                        auto f1 = std::move(n->task);
                        n->task = std::move(n->next->task);
                        n->next->task = std::move(f1);
                    }
                }
                n = n->next;
            }
            sort_push_mutex_.unlock();
            sort_pop_mutex_.unlock();
        }
        std::mutex task_mutex;
        std::atomic<bool> complete;

    private:
        struct node {
            node() : next(nullptr), prev(nullptr) {};
            std::unique_ptr<detail::task_container> task;
            node* next;
            node* prev;
        };

        std::atomic<node*> head;
        std::atomic<node*> tail;

        std::mutex sort_push_mutex_;
        std::mutex sort_pop_mutex_;
    };
}
}

#endif /* UNPAUSE_ASYNC_TASK_QUEUE_HPP */
