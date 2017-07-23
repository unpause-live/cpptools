#include <unpause/async>
#include <stdio.h>
#include <assert.h>
#include <vector>

#define log_v(x, ...) printf("%3d:\t" x "\n", __LINE__, ##__VA_ARGS__);
#define log(x) printf("%3d:\t" x "\n", __LINE__);

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
		int val = 0;
		const int n = 10000;
		for(int i = 1 ; i <= n ; i++) {
			queue.add([&](int in) { val += in; }, (int)i);
		}
		while(queue.next());
		log_v("val=%d n=%d t=%d", val, n, (n*(n+1)/2));
		assert(val==(n*(n+1)/2));
		log("OK");
	}

	{
		log("task passing with after");
		async::task_queue queue;
		int val = 0;
		const int n = 10000;
		for(int i = 1 ; i <= n ; i++) {
			auto t = async::make_task([&](int in) { val += in; return in; }, (int)i);
			t.after = [&](int i){ val += i; };
			queue.add(t);
		}
		while(queue.next());
		log_v("val=%d n=%d t=%d", val, n, (n*(n+1)/2)*2);
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
		std::atomic<int> val(0);
		const int n = 10000;
		std::atomic<int> ct(n);
		{
			async::thread_pool pool;
			for(int i = 1 ; i <= n ; i++)  {
				async::run(pool, [&](int in) { val += in; --ct; }, (int)i);
			}
			while(ct.load() > 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		log_v("val=%d n=%d t=%d", val.load(), n, (n*(n+1)/2));
		assert(val==(n*(n+1)/2));
		log("OK");
	}
	{
		log("sync dispatch on any thread");
		std::atomic<int> val(0);
		const int n = 10000;
		std::atomic<int> ct(n);
		{
			async::thread_pool pool;
			for(int i = 1 ; i <= n ; i++)  {
				async::run_sync(pool, [&](int in) { val += in; --ct;}, (int)i);
			}
			while(ct.load() > 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		log_v("val=%d n=%d t=%d", val.load(), n, (n*(n+1)/2));
		assert(val==(n*(n+1)/2));
		log("OK");
	}
	{
		log("async dispatch in a serial queue");
		std::vector<int> res;
		const int n = 10000;
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
				/*if(val%1000) {
					log_v("%d", val);
				}*/
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

int main(void)
{
	task_test();
	task_queue_test();
	thread_pool_test();
	return 0;
}