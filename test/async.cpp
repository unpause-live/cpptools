/* Copyright (c) 2017 Unpause, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3.0 of the License, or (at your option) any later version.
 *  See the file LICENSE included with this distribution for more
 *  information.
 */


#include <iostream>
#include <random>
#include <atomic>

#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

static std::atomic<int> s_order(0);

static int order() { return s_order.fetch_add(1); }

#define log_v(x, ...) printf("[%d] %3d:\t" x "\n",  order(), __LINE__, ##__VA_ARGS__); fflush(stdout);
#define log(x) printf("[%d] %3d:\t" x "\n", order(), __LINE__); fflush(stdout);

#include <unpause/async>

static const uint64_t iterations = 500000;

void task_test() {
    log("------- Testing async::task -------");
    using namespace unpause;
    
    {
        int val = 0;
        log("lambda capture with return value...");
        auto t = async::make_task([&]{ return val+1; });
        int res = t();
        log_v("res=%d", res);
        assert(res == (val+1));
        log("OK");
    }
    {
        log("argument passing with return value...");
        auto t = async::make_task([](int val){ return val+1; }, 1);
        int res = t();
        log_v("res=%d", res);
        assert(res == 2);
        log("OK");
    }
    {
        log("argument passing with return value and after lambda with capture...");
        int val = 0;
        auto t = async::make_task([](int val){ return val+1; }, (int)val); // variables are default pass-by-reference
        t.after = [&](int result){ val = result; };
        int res = t();
        log_v("res=%d val=%d", res, val);
        assert(res == val);
        log("OK");
    }
    {
        log("move with argument passing and return value along with lambda capture");
        int val = 2;
        auto t = async::make_task([](int val){ return val+1; }, (int)val); // val should be 3
        t.after = [&](int result){ val *= result; }; // val should be 6
        auto t2 = std::move(t);
        int res = t2();
        log_v("res=%d val=%d", res, val);
        assert(res==(val/2));
        log("OK");
    }
    log("------- Testing async::detail::task_container -------");
    {
        log("before_internal and after_internal");
        int res = 1;
        auto t = async::make_task([](int& val) { return ++val; }, res); // res should = 6 at this point.
        t.after = [&] (int ret) { res += ret; }; // res should = 12
        t.before_internal = [&] { res = res * 5; }; // res should = 5 at this point
        t.after_internal = [&] { res = res * 5; }; // res should = 60 at this point.
        t();
        log_v("res=%d", res);
        assert(res == 60);
        log("OK");
    }
    {
        log("before_internal and after_internal with move");
        int res = 1;
        auto t = async::make_task([](int& val) { return ++val; }, res); // res should = 6 at this point.
        t.after = [&] (int ret) { res += ret; }; // res should = 12
        t.before_internal = [&] { res = res * 5; }; // res should = 5 at this point
        t.after_internal = [&] { res = res * 5; }; // res should = 60 at this point.
        auto t2 = std::move(t);
        t2();
        log_v("res=%d", res);
        assert(res == 60);
        log("OK");
    }
}

void task_queue_test()
{
    using namespace unpause;
    log("------- Testing async::task_queue -------");
    {
        log("argument passing tasks");
        async::task_queue queue;
        uint64_t val = 0;
        const uint64_t n = iterations;
        for(uint64_t i = 1 ; i <= n ; i++) {
            queue.add([&](uint64_t in) { val += in; }, (uint64_t)i);
        }
        while(queue.next());
        log_v("val=%" PRId64 " n=%" PRId64 " t=%" PRId64, val, n, (n*(n+1)/2));
        assert(val==(n*(n+1)/2));
        log("OK");
    }
    
    {
        log("task passing with after");
        async::task_queue queue;
        uint64_t val = 0;
        const uint64_t n = iterations;
        for(uint64_t i = 1 ; i <= n ; i++) {
            auto t = async::make_task([&](uint64_t in) { val += in; return in; }, (uint64_t)i);
            t.after = [&](uint64_t i){ val += i; };
            queue.add(t);
        }
        while(queue.next());
        log_v("val=%" PRId64 " n=%" PRId64 " t=%" PRId64, val, n, (n*(n+1)/2));
        assert(val==(n*(n+1)/2)*2);
        log("OK");
    }
}

void thread_pool_test()
{
    using namespace unpause;
    log("------- Testing async::thread_pool -------");
    {
        log("async dispatch on any thread");
        std::atomic<uint64_t> val(0);
        const uint64_t n = iterations;
        std::atomic<uint64_t> ct(n);
        {
            async::thread_pool pool;
            for(uint64_t i = 1 ; i <= n ; i++)  {
                async::run(pool, [&](uint64_t in) { val += in; --ct; }, (uint64_t)i);
            }
            while(ct.load() > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        log_v("val=%" PRId64 " n=%" PRId64 " t=%" PRId64, val.load(), n, (n*(n+1)/2));
        assert(val==(n*(n+1)/2));
        log("OK");
    }
    {
        log("sync dispatch on any thread");
        std::atomic<uint64_t> val(0);
        const uint64_t n = iterations;
        std::atomic<uint64_t> ct(n);
        {
            async::thread_pool pool;
            auto div = n / 4;
            for(uint64_t i = 1 ; i <= n ; i++)  {
                async::run_sync(pool, [&](uint64_t in) { val += in; --ct; }, (uint64_t)i);
                if(!(i%div)) {
                    log_v("ct=%" PRId64, n-i);
                }
            }
            
        }
        log_v("val=%" PRId64 " n=%" PRId64 " t=%" PRId64, val.load(), n, (n*(n+1)/2));
        assert(val==(n*(n+1)/2));
        log("OK");
    }
    {
        log("async dispatch in a serial queue");
        std::vector<int> res;
        const int n = iterations;
        res.reserve(n);
        std::atomic<int> ct(n);
        std::mutex m;
        async::thread_pool pool;
        async::task_queue queue;
        for(int i = 0 ; i < n; i++) {
            async::run(pool, queue, [&](int val) {
                m.lock();
                res.push_back(val);
                m.unlock();
                --ct;
            }, (int)i);
        }
        while(ct.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        log("Checking order");
        for(int i = 0 ; i < n ; ++i ) {
            assert(i == res.at(i));
        }
        log("OK");
    }
}

void run_loop_test() {
    log("------- Testing async::run_loop -------");
    using namespace unpause;
    {
        log("schedule with thread pool");
        async::thread_pool pool;
        auto start = std::chrono::steady_clock::now();
        auto end1 = start;
        auto end2 = start;
        auto end3 = start;
        
        
        async::schedule(pool, std::chrono::seconds(3) + std::chrono::steady_clock::now(), [&end2] {
            log("3s...");
            end2 = std::chrono::steady_clock::now();
        });
        async::schedule(pool, std::chrono::milliseconds(2500) + std::chrono::steady_clock::now(), [&end1] {
            log("2.5s...");
            end1 = std::chrono::steady_clock::now();
        });
        async::schedule(pool, std::chrono::seconds(4) + std::chrono::steady_clock::now(), [&end3] {
            log("4s...");
            end3 = std::chrono::steady_clock::now();
        });
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto diff1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start).count();
        log_v("Diff=%" PRId64, diff1);
        assert(diff1 <= 3000000 && diff1 > 2500000);
        auto diff2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start).count();
        log_v("Diff2=%" PRId64, diff2);
        assert(diff2 <= 3500000 && diff2 > 3000000);
        auto diff3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start).count();
        log_v("Diff3=%" PRId64, diff3);
        assert(diff3 <= 4500000 && diff3 > 4000000);
    }
    {
        log("schedule with thread pool and queue");
        async::thread_pool pool;
        async::task_queue queue;
        auto start = std::chrono::steady_clock::now();
        auto end1 = start;
        auto end2 = start;
        auto end3 = start;
        
        
        async::schedule(pool, queue, std::chrono::seconds(3) + std::chrono::steady_clock::now(), [&end2] {
            log("3s (sleeping for 1500ms)...");
            end2 = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        });
        async::schedule(pool, queue, std::chrono::milliseconds(2500) + std::chrono::steady_clock::now(), [&end1] {
            log("2.5s...");
            end1 = std::chrono::steady_clock::now();
        });
        async::schedule(pool, queue, std::chrono::seconds(4) + std::chrono::steady_clock::now(), [&end3] {
            log("4s (should be closer to 4.5s)...");
            end3 = std::chrono::steady_clock::now();
        });
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto diff1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start).count();
        log_v("Diff=%" PRId64, diff1);
        assert(diff1 <= 3000000 && diff1 > 2500000);
        auto diff2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start).count();
        log_v("Diff2=%" PRId64, diff2);
        assert(diff2 <= 4000000 && diff2 > 3000000);
        auto diff3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start).count();
        log_v("Diff3=%" PRId64, diff3);
        assert(diff3 <= 5000000 && diff3 > 4500000);
    }
    {
        log("schedule with loop");
        async::run_loop loop;
        auto start = std::chrono::steady_clock::now();
        auto end1 = start;
        auto end2 = start;
        auto end3 = start;
        async::schedule(loop, std::chrono::seconds(3) + std::chrono::steady_clock::now(), [&end2] {
            log("3s...");
            end2 = std::chrono::steady_clock::now();
        });
        async::schedule(loop, std::chrono::milliseconds(2500) + std::chrono::steady_clock::now(), [&end1] {
            log("2.5s...");
            end1 = std::chrono::steady_clock::now();
        });
        async::schedule(loop, std::chrono::seconds(4) + std::chrono::steady_clock::now(), [&end3] {
            log("4s...");
            end3 = std::chrono::steady_clock::now();
        });

        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto diff1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start).count();
        log_v("Diff=%" PRId64, diff1);
        assert(diff1 <= 3000000 && diff1 > 2500000);
        auto diff2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start).count();
        log_v("Diff2=%" PRId64, diff2);
        assert(diff2 <= 3500000 && diff2 > 3000000);
        auto diff3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start).count();
        log_v("Diff3=%" PRId64, diff3);
        assert(diff3 <= 4500000 && diff3 > 4000000);
    }
}

void interleave_test() {

    log("------- Testing interleaving queues with thread pool -------");
    using namespace unpause;

    async::thread_pool p(4);
    async::task_queue q1;
    async::task_queue q2;

    for(int i = 0 ; i < 10000 ; i++) {
        std::atomic<int> val1(0);
        std::atomic<int> val2(0);
        
        async::run(p, q1, [&val1]{
            assert(val1.load() == 0);
            val1++;
        
        });
        async::run(p, q1, [&val1]{
            assert(val1.load() == 1);
            val1++;
        });
        async::run(p, q2, [&val2]{
            assert(val2.load() == 0);
            val2++;
        });
        
        async::run(p, q1, [&q2, &p, &val1]{
            assert(val1.load() == 2);
            val1++;
            async::run_sync(p, q2, [&val1]{
                assert(val1.load() == 3);
                val1++;
            });
        });

        async::run_sync(p, q1, [&val1]{
            assert(val1.load() == 4);
        });
    }
    log("OK");
}

void abrupt_exit_test() {
    log("------- Testing abrupt dealloc of queue -------");
    using namespace unpause;
    async::thread_pool p;
    for(int i = 0 ; i < 10000 ; i++)
    {
        {
            async::task_queue q;    

            for(int j = 0 ; j < 100 ; j++) {
                async::run(p, q, [i, j] {
                    return i*j;
                });
            }
        }
    }

    log("OK");
    
}
int main(void)
{
    task_test();
    task_queue_test();
    thread_pool_test();
    run_loop_test();
    interleave_test();
    abrupt_exit_test();
    return 0;
}
