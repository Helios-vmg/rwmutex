#include "rwmutex.hpp"
#include <stdexcept>

namespace helios{

rwlock::rwlock(rwmutex &m): m(m){
	node.id = std::this_thread::get_id();
	m.lock_reader(node);
	state = 1;
}

rwlock::~rwlock(){
	if (state == 1)
		m.unlock_reader(node);
	else
		m.unlock_writer(node);
}

void rwlock::upgrade(){
	if (state != 1)
		throw std::runtime_error("incorrect use");
	m.upgrade();
	state = 2;
}

void rwlock::downgrade(){
	if (state != 2)
		throw std::runtime_error("incorrect use");
	m.upgrade();
	state = 1;
}

void rwmutex::lock_reader(rwlock_node &node){
	UL lg(m);
	push(node);
	while (state == 2)
		cv.wait(lg);
	state = 1;
}

void rwmutex::unlock_reader(rwlock_node &node){
	UL lg(m);
	remove(node);
	if (!queue_size)
		state = 0;
	cv.notify_all();
}

void rwmutex::upgrade(){
	auto id = std::this_thread::get_id();
	UL lg(m);
	writers_waiting++;
	cv.notify_all();
	while (queue_head->id != id)
		cv.wait(lg);
	while (queue_size > writers_waiting && state != 0)
		cv.wait(lg);
	writers_waiting--;
	state = 2;
}

void rwmutex::downgrade(){
	UL lg(m);
	state = 1;
	cv.notify_all();
}

void rwmutex::lock_writer(rwlock_node &node){
	auto id = std::this_thread::get_id();
	UL lg(m);
	push(node);
	writers_waiting++;
	cv.notify_all();
	while (queue_head->id != id)
		cv.wait(lg);
	while (queue_size > writers_waiting && state != 0)
		cv.wait(lg);
	writers_waiting--;
	state = 2;
}

void rwmutex::unlock_writer(rwlock_node &node){
	UL lg(m);
	remove(node);
	state = !!queue_size;
	cv.notify_all();
}

void rwmutex::push(rwlock_node &node){
	queue_size++;
	node.next = nullptr;
	if (queue_size == 1){
		queue_head = &node;
		queue_tail = &node;
		node.previous = nullptr;
		return;
	}
	queue_tail->next = &node;
	node.previous = queue_tail;
	queue_tail = &node;
}

void rwmutex::remove(rwlock_node &node){
	queue_size--;
	if (node.previous)
		node.previous->next = node.next;
	else
		queue_head = node.next;
	if (node.next)
		node.next->previous = node.previous;
	else
		queue_tail = node.previous;
}

}
