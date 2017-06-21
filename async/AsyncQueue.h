//
//  AsyncQueue.hpp
//  unpause.tools
//
//  Created by James Hurley on 6/18/17.
//  Copyright © 2017 Unpause, Inc. All rights reserved.
//

#ifndef __unpause_async_AsyncQueue_h
#define __unpause_async_AsyncQueue_h

#include <future>
#include <queue>
#include <string>
#include <memory>
#include <type_traits>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <list>
#include <assert.h>

namespace unpause { namespace async {

    class AsyncQueue;
    
    class SharedAsyncQueue
    {
    public:
        ~SharedAsyncQueue()
        {
            m_end = true;
            
            m_cv.notify_all();

            for(auto& t: m_threads)
            {
                t.join();
            }
        };
        
    private:
        struct TaskQueue {
            std::queue<std::function<void()>> fq;
            std::mutex m;
            std::mutex qm;
        };
    private:
        friend class AsyncQueue;
        SharedAsyncQueue(int threadCount = 8)
        : m_end(false)
        {
            for(int i = 0 ; i < threadCount ; ++i)
            {
                m_threads.insert(m_threads.end(), std::thread(std::bind(&SharedAsyncQueue::t, this)));
            }
        };

        static SharedAsyncQueue& instance(int threadCount = 8)
        {
            if(!s_saq)
            {
                s_saq.reset(new SharedAsyncQueue(threadCount));
            }
            return *s_saq;
        }
        
        
        template<typename Function, typename... Args>
        void blany(Function&& f, Args&&... args)
        {
            std::mutex l;
            std::unique_lock<std::mutex> lk(l);
            std::condition_variable cv;
            bool done = false;
            auto bf = [&]() {
                if(!m_end.load())
                {
                    f(args...);
                    done = true;
                    cv.notify_all();
                }
            };
            
            m_nfm.lock();
            m_nf.push(std::move(bf));
            m_nfm.unlock();
            m_cv.notify_one();
            cv.wait(lk, [&] { return done; });
        }
        
        template<typename Function, typename... Args>
        void blq(std::string qname, Function&& f, Args&&... args)
        {
            std::mutex l;
            std::unique_lock<std::mutex> lk(l);
            std::condition_variable cv;
            bool done = false;
            auto bf = [&] {
                if(!m_end.load())
                {
                    f(args...);
                    next(qname);
                    done = true;
                    cv.notify_all();
                }
            };
            m_queues[qname].qm.lock();
            m_queues[qname].fq.push(std::move(bf));
            m_queues[qname].qm.unlock();
            auto it = m_queues.find(qname);
            if(it != m_queues.end() && it->second.m.try_lock())
            {
                next(qname);
            }
            
            cv.wait(lk, [&] { return done; });
        }
        
        template<typename Function, typename... Args>
        void any(Function&& f, Args&&... args)
        {
            auto bf = [=]() {
                if(!m_end.load())
                {
                    f(args...);
                }
            };
            
            m_nfm.lock();
            m_nf.push(std::move(bf));
            m_nfm.unlock();
            m_cv.notify_one();
        }
        
        template<typename Function, typename... Args>
        void q(std::string qname, Function&& f, Args&&... args)
        {
            auto bf = [=](){
                if(!m_end.load())
                {
                    f(args...);
                    next(qname);
                }
            };
            m_queues[qname].qm.lock();
            m_queues[qname].fq.push(std::move(bf));
            m_queues[qname].qm.unlock();
            auto it = m_queues.find(qname);
            if(it != m_queues.end() && it->second.m.try_lock())
            {
                next(qname);
            }
        }
        void next(std::string qname)
        {
            auto it = m_queues.find(qname);
            if(it != m_queues.end())
            {
                if(!it->second.fq.empty())
                {
                    it->second.qm.lock();
                    auto f = std::move(it->second.fq.front());
                    assert(f != nullptr);
                    it->second.fq.pop();
                    it->second.qm.unlock();
                    m_nfm.lock();
                    m_nf.push(std::move(f));
                    m_nfm.unlock();
                    m_cv.notify_one();
                }
                else
                {
                    it->second.m.unlock();
                }
            }
        }
        void t()
        {
            while(!m_end.load())
            {
                std::unique_lock<std::mutex> lk(m_qm);
                m_cv.wait(lk, [this]{ return !m_nf.empty() || m_end.load(); });
                
                if(!m_nf.empty())
                {
                    m_nfm.lock();
                    assert(m_nf.front() != nullptr);
                    auto f = std::move(m_nf.front());
                    m_nf.pop();
                    m_nfm.unlock();
                    lk.unlock();
                    f();
                }
            }
        }
    private:
        static std::unique_ptr<SharedAsyncQueue> s_saq;
        std::map<std::string, TaskQueue> m_queues;
        std::mutex m_qm;
        std::mutex m_nfm;
        std::condition_variable m_cv;
        std::queue<std::function<void()>> m_nf; // next function queue
        std::list<std::thread> m_threads;
        std::atomic<bool> m_end;
        
    };
    
    //
    //  Create a serial task queue
    //
    //
    class AsyncQueue
    {
    public:
        AsyncQueue(std::string name) : m_name(name) {};
        
        //
        //  Run a task synchronously on this serial queue
        //
        //
        template<typename Function, typename... Args>
        void blq(Function&& f, Args&&... args)
        {
            SharedAsyncQueue::instance(s_threadCount).blq(m_name, f, args...);
        }
        
        //
        //  Run a task synchronously on any queue
        //
        //
        template<typename Function, typename... Args>
        static void blany(Function&& f, Args&&... args)
        {
            SharedAsyncQueue::instance(s_threadCount).blany(f, args...);
        }

        
        //
        //  Run a task asynchronously on this serial queue
        //
        //
        template<typename Function, typename... Args>
        void q(Function&& f, Args&&... args)
        {
            SharedAsyncQueue::instance(s_threadCount).q(m_name, f, args...);
        }
        
        //
        //  Run a task asynchronously on any queue
        //
        //
        template<typename Function, typename... Args>
        static void any(Function&& f, Args&&... args)
        {
            SharedAsyncQueue::instance(s_threadCount).any(f, args...);
        }
        
        // This must be called before any tasks are enqueued. It can't be changed later.
        static void setThreadCount(int threadCount) { s_threadCount = threadCount; };
        
    private:
        std::string m_name;
        static int s_threadCount;
    };
    
}
}

#endif /* __unpause_async_AsyncQueue_h */
