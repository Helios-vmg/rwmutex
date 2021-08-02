/*
 * This program tests that the mutex permits the maximum possible concurrency.
 * Readers must always be able to run in parallel, but at most a single
 * writer should run at any time.
 */

#include "../src/rwmutex.hpp"
#include <iostream>
#include <mutex>
#include <type_traits>
#include <condition_variable>
#include <thread>
#include <map>
#include <set>
#include <mutex>

typedef typename std::make_signed<size_t>::type ssize_t;

helios::rwmutex mutex;

void simulate_work(){
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

class AutoResetEvent{
	ssize_t state;
	ssize_t waiting = 0;
	std::mutex mutex;
	std::condition_variable cv;
public:
	AutoResetEvent(ssize_t initial_state = 0): state(initial_state){}
	void signal(){
		std::lock_guard<std::mutex> lock(this->mutex);
		this->state++;
		this->cv.notify_one();
	}
	void signal_all(){
		std::lock_guard<std::mutex> lock(this->mutex);
		this->state += this->waiting;
		this->cv.notify_all();
	}
	void wait(){
		std::unique_lock<std::mutex> lock(this->mutex);
		this->waiting++;
		while (this->state <= 0)
			this->cv.wait(lock);
		this->waiting--;
		this->state--;
	}
	void set_state(ssize_t state){
		this->state = state;
	}
};

class Counter{
	int state = 0;
	int max_state = 0;
	std::mutex m;
public:
	void increment(){
		std::lock_guard<std::mutex> lg(m);
		max_state = std::max(max_state, ++state);
	}
	void decrement(){
		std::lock_guard<std::mutex> lg(m);
		--state;
	}
	int get(){
		std::lock_guard<std::mutex> lg(m);
		return max_state;
	}
};

class AutoCounter{
	Counter &c;
public:
	AutoCounter(Counter &c): c(c){
		c.increment();
	}
	~AutoCounter(){
		c.decrement();
	}
};

void read(Counter &shared, Counter &unique){
	helios::rwlock l(mutex);
	{
		AutoCounter ac(shared);
		simulate_work();
	}
}

void read_then_write(Counter &shared, Counter &unique){
	helios::rwlock l(mutex);
	AutoCounter ac1(shared);
	simulate_work();
	l.upgrade();
	AutoCounter ac2(unique);
	simulate_work();
}

void read_then_write_then_read(Counter &shared, Counter &unique){
	helios::rwlock l(mutex);
	{
		AutoCounter ac1(shared);
		simulate_work();
		{
			helios::rwlock_writer_region r(l);
			AutoCounter ac2(unique);
			simulate_work();
		}
		simulate_work();
	}
}

void write(Counter &shared, Counter &unique){
	helios::wlock l(mutex);
	{
		AutoCounter ac2(unique);
		simulate_work();
	}
}

void test(AutoResetEvent &event1, AutoResetEvent &event2, int f, Counter &shared, Counter &unique){
	event1.signal();
	event2.wait();
	switch (f){
		case 0:
			read(shared, unique);
			break;
		case 1:
			read_then_write(shared, unique);
			break;
		case 2:
			read_then_write_then_read(shared, unique);
			break;
		case 3:
			write(shared, unique);
			break;
	}
}

const char *function_to_string(int f){
	switch (f){
		case 0:
			return "read";
		case 1:
			return "read_then_write";
		case 2:
			return "read_then_write_then_read";
		case 3:
			return "write";
	}
	return nullptr;
}

void failed_test(int f1, int f2, int shared, int unique){
	std::cout << "Test " << function_to_string(f1) << " + " << function_to_string(f2) << " failed: " << shared << ", " << unique << std::endl;
}

bool contains(const std::set<int> &set, int i){
	return set.find(i) != set.end();
}

const int r = 0;
const int rw = 1;
const int rwr = 2;
const int w = 3;

int main(){
	std::map<std::pair<int, int>, std::pair<std::set<int>, std::set<int>>> data = {
		{{r  , r  }, {{2}, {0}}},
		{{r  , rw }, {{2}, {1}}},
		{{r  , rwr}, {{2}, {1}}},
		{{r  , w  }, {{1}, {1}}},
		
		{{rw , r  }, {{2}, {1}}},
		{{rw , rw }, {{2}, {1}}},
		{{rw , rwr}, {{2}, {1}}},
		{{rw , w  }, {{1}, {1}}},
		
		{{rwr, r  }, {{2}, {1}}},
		{{rwr, rw }, {{2}, {1}}},
		{{rwr, rwr}, {{2}, {1}}},
		{{rwr, w  }, {{1}, {1}}},
		
		{{w  , r  }, {{1}, {1}}},
		{{w  , rw }, {{1}, {1}}},
		{{w  , rwr}, {{1}, {1}}},
		{{w  , w  }, {{0}, {1}}},
	};
	
	bool ok = true;
	int i = 0;
	for (int f1 = 0; f1 < 4; f1++){
		for (int f2 = 0; f2 < 4; f2++){
			std::cout << "Test " << i++ << std::endl;
			
			AutoResetEvent event1(-1);
			AutoResetEvent event2;
			Counter shared;
			Counter unique;
			std::thread t1([&event1, &event2, f = f1, &shared, &unique](){
				test(event1, event2, f, shared, unique);
			});
			std::thread t2([&event1, &event2, f = f2, &shared, &unique](){
				test(event1, event2, f, shared, unique);
			});
			event1.wait();
			event2.signal_all();
			t1.join();
			t2.join();

			auto &pair = data.find({ f1, f2 })->second;
			
			if (!contains(pair.first, shared.get()) || !contains(pair.second, unique.get())){
				failed_test(f1, f2, shared.get(), unique.get());
				ok = false;
			}
		}
	}
	if (ok)
		std::cout << "OK\n";
}
