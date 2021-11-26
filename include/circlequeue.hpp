#ifndef CIRCLE_QUEUE_HPP
#define CIRCLE_QUEUE_HPP

#include <stdint.h>

template <class T>
class CirCleQueue{
private:
    uint32_t head;
    uint32_t tail;
    T* que;
    uint32_t ind;
    const uint32_t capacity;
public:
    CirCleQueue(uint32_t _capacity);
    ~CirCleQueue();
    bool enqueue(T* ele);
    void pop(T* top);
    bool full();
    bool empty();
    uint32_t size();
    void rewind();
    void getNext(T* next);
};

#endif