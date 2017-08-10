#ifndef UNPAUSE_ASYNC_RUN_LOOP_HPP
#define UNPAUSE_ASYNC_RUN_LOOP_HPP

#include <condition_variable>
#include <thread>
#include <mutex>

namespace unpause { namespace async {

	class run_loop {

	public:
		run_loop() {};
		~run_loop() {};

	private:
		std::thread looper;
		std::condition_variable cond;
		
	}
}
}

#endif