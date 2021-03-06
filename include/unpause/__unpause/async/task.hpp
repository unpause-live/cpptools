/* Copyright (c) 2020 Unpause, SAS.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */

#ifndef UNPAUSE_ASYNC_TASK_HPP
#define UNPAUSE_ASYNC_TASK_HPP

#include <functional>
#include <utility>
#include <chrono>
#include <memory>
#include <atomic>
#include <tuple>

namespace unpause { namespace async {
    
    namespace detail {
        template<class R>
        struct task_after
        {
            using function_type = std::function<void(R&)>;
        };
        
        template<>
        struct task_after<void>
        {
            using function_type = std::function<void()>;
        };
        struct task_container {
            task_container() : dispatch_time(std::chrono::steady_clock::now()) {};
            task_container(task_container&& other)
            : before_internal(std::move(other.before_internal))
            , after_internal(std::move(other.after_internal))
            , dispatch_time(std::move(other.dispatch_time))
            , token(std::move(other.token))
            , use_token(other.use_token) 
            { other.token.reset(); other.use_token = false; }; 

            task_container(const task_container& other) = delete;

            virtual ~task_container() {};

            virtual void run_v() = 0;
            std::function<void()> before_internal; // used for task_queue
            std::function<void()> after_internal; // used for task_queue
            std::chrono::steady_clock::time_point dispatch_time; // used for run_loop
            std::weak_ptr<std::atomic<bool>> token;
            bool use_token {false};
        };
    }
    
    template<class R, class... Args>
    struct task : public detail::task_container
    {
        using result_type = typename std::result_of<R(Args...)>::type;
        using after_type = typename detail::task_after<result_type>::function_type;
        
        task(R&& r, Args&&... a) : func(std::move(r)), args(std::forward<Args>(a)...) {};
        task(task<R, Args...>&& rhs)
        : detail::task_container(std::forward<detail::task_container>(rhs))
        , func(std::move(rhs.func))
        , args(std::move(rhs.args))
        , after(std::move(rhs.after)) {};
        task(const task& other) = delete;

        virtual ~task() {};

        virtual void run_v() { (*this)(); }
        
        result_type operator()() {
            return run(std::integral_constant<bool, std::is_same<result_type, void>::value>(), std::index_sequence_for<Args...>{});
        }
        
        std::function<result_type (Args...)> func;
        std::tuple<Args...> args;
        after_type after;

    private:
        template<std::size_t... I>
        result_type run(std::true_type, std::index_sequence<I...>) {
            
            if(before_internal) {
                before_internal();
            }
            auto t = token.lock();
            if(!use_token || (use_token && t && t->load(std::memory_order_acquire))) {
                func(std::get<I>(std::forward<std::tuple<Args...>>(args)) ...);
                if(after) {
                    after();
                }
            }
            if(after_internal) {
                after_internal();
            } 
            
        }
        
        template<std::size_t... I>
        result_type run(std::false_type, std::index_sequence<I...>) {
            result_type res = result_type();
            
            if(before_internal) {
                before_internal();
            }
            auto t = token.lock();
            if(!use_token || (use_token && t && t->load(std::memory_order_acquire))) {
                res = func(std::get<I>(std::forward<std::tuple<Args...>>(args)) ...);
                if(after) {
                    after(res);
                }
            } 
            if(after_internal) {
                after_internal();
            }
        
            return res;
        }
    };
    
    template<class R, class... Args>
    inline task<R,Args...> make_task(R&& r, Args&&... args)
    {
        return task<R, Args...>(std::forward<R>(r), std::forward<Args>(args)...);
    }
}
}
#endif /* UNPAUSE_ASYNC_TASK_HPP */
