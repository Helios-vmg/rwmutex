This is a simple multiple-reader, single-writer mutex class for C++14. Reader
locks can be upgraded and then downgraded without being released. It uses a
queue (allocated on the stack) to prevent writer starvation and to resolve lock
acquisition conflicts. Writer lock acquisition requests are always fulfilled in
FIFO order.


Usage examples:

helios::rwmutex m;

void reader(){
    helios::rwlock l(m);
    //reader code
}

void conditional_writer(){
    helios::rwlock l(m);
    //reader code
    if (/*condition*/)
        return;
    l.upgrade();
    //note: calling l.upgrade() again would throw an exception
    //writer code
}

void nested_writer(){
    helios::rwlock l(m);
    //reader code
    if (/*condition*/)
        return;
    {
        helios::rwlock_writer_region r(l);
        //writer code
    }
    //some more reader code
}

void unconditional_writer(){
    helios::wlock l(m);
    //writer code
}
