#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>

namespace helios{

struct rwlock_node{
	rwlock_node *previous;
	rwlock_node *next;
	std::thread::id id;
};

class rwmutex{
	int state = 0;
	rwlock_node *queue_head = nullptr;
	rwlock_node *queue_tail = nullptr;
	size_t queue_size = 0;
	size_t writers_waiting = 0;
	std::mutex m;
	std::condition_variable cv;
	
	friend class rwlock;
	friend class wlock;
	void lock_reader(rwlock_node &);
	void unlock_reader(rwlock_node &);
	void upgrade();
	void downgrade();
	void lock_writer(rwlock_node &);
	void unlock_writer(rwlock_node &);
	void push(rwlock_node &);
	void remove(rwlock_node &);
	typedef std::unique_lock<std::mutex> UL;
};

class rwlock{
	rwmutex &m;
	int state;
	rwlock_node node;
public:
	rwlock(rwmutex &);
	~rwlock();
	rwlock(const rwlock &) = delete;
	rwlock &operator=(const rwlock &) = delete;
	rwlock(rwlock &&) = delete;
	rwlock &operator=(rwlock &&) = delete;
	void upgrade();
	void downgrade();
};

class wlock{
	rwmutex &m;
	rwlock_node node;
public:
	wlock(rwmutex &m): m(m){
		node.id = std::this_thread::get_id();
		m.lock_writer(node);
	}
	~wlock(){
		m.unlock_writer(node);
	}
	wlock(const wlock &) = delete;
	wlock &operator=(const wlock &) = delete;
	wlock(wlock &&) = delete;
	wlock &operator=(wlock &&) = delete;
};

class rwlock_writer_region{
	rwlock &lock;
public:
	rwlock_writer_region(rwlock &lock): lock(lock){
		lock.upgrade();
	}
	~rwlock_writer_region(){
		lock.downgrade();
	}
	rwlock_writer_region(const rwlock_writer_region &) = delete;
	rwlock_writer_region &operator=(const rwlock_writer_region &) = delete;
	rwlock_writer_region(rwlock_writer_region &&) = delete;
	rwlock_writer_region &operator=(rwlock_writer_region &&) = delete;
};

}
