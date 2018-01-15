/*
 * threadpool.cpp
 *
 *  Created on: Oct 20, 2017
 *      Author: yliu
 */

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

class Worker;

// Class: ThreadPool
class ThreadPool {

  public:

    ThreadPool(size_t);
    template<typename F>
    void enqueue(F f);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator = (const ThreadPool&) = delete;
    bool isQueueEmpty();
  private:

    friend class Worker;

    ::std::vector< ::std::thread > _threads;
    ::std::deque< ::std::function<void()> > _tasks;
    ::std::mutex _mutex;
    ::std::condition_variable _condition;
    ::std::atomic_bool _terminate {false};
};

class Worker {

	public:

	// Constructor.
	Worker(ThreadPool* master) : _master{master} {};

	// Functor.
	void operator ()() {

	  ::std::function<void()> task;

	  while(true) {
		{ // Acquire lock.
		  ::std::unique_lock<::std::mutex> lock(_master->_mutex);

		  _master->_condition.wait(
			lock,
			[&] () { return !_master->_tasks.empty() || _master->_terminate.load(); }
		  );

		  if(_master->_terminate.load()) return;

		  task = _master->_tasks.front();
		  _master->_tasks.pop_front();
		} // Release lock.

		task();
	  }
	}

	private:

	ThreadPool* _master;
};

bool ThreadPool::isQueueEmpty() {
	std::lock_guard<std::mutex> locker(_mutex);
	bool empty = _tasks.empty();
	return empty;
}

// Constructor
ThreadPool::ThreadPool(size_t threads) {
  for(size_t i = 0;i<threads;++i) {
    _threads.push_back(::std::thread(Worker(this)));
  }
}

// Destructor.
ThreadPool::~ThreadPool() {
  _terminate.store(true);
  _condition.notify_all();
  for(auto& t : _threads) t.join();
}

// Procedure: enqueue
template<typename F>
void ThreadPool::enqueue(F f) {

  { // Acquire lock
    ::std::unique_lock<::std::mutex> guard(_mutex);
    _tasks.push_back(::std::function<void()>(f));
  } // Release lock

  // wake up one thread
  _condition.notify_one();
}

void foo() {
  printf("hello by %p\n", ::std::this_thread::get_id());
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main() {
	unsigned int n = std::thread::hardware_concurrency();
	std::cout<<"thread pool size: " << n <<std::endl;
  ThreadPool tp(n);
  int i = 20;
  while(i>0) {

    printf("main %p: enque, index: %d\n", ::std::this_thread::get_id(), i);
    tp.enqueue(foo);
    std::this_thread::sleep_for(std::chrono::seconds(0));
    i--;
  }

  while(!tp.isQueueEmpty()) {
	  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return 0;
}


