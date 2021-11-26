#ifndef CIRCLE_QUEUE_HPP
#define CIRCLE_QUEUE_HPP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
    
/*
maxsize: 3      arraysize: 3+1
op      head    tail    size
        0       0       0       empty
enq     0       1       1
enq     0       2       2
enq     0       3       3       full
pop     1       3       2
pop     2       3       1  
pop     3       3       0       empty
enq     3       0       1
enq     3       1       2
enq     3       2       3       full
...
*/

template <class T>
class CircleQueue{
private:
    uint32_t head;
    uint32_t tail;
    T* que;
    uint32_t ind;
    uint32_t capacity;
public:
    CircleQueue(uint32_t _capacity);
    ~CircleQueue();
    bool enqueue(T ele);
    bool pop(T& top);
    bool full();
    bool empty();
    uint32_t size();
    void rewind();
    bool getNext(T& next);
    bool hasNext();
    // bool isBegin();
    uint32_t getHead() const {return head;}
    uint32_t getTail() const {return tail;}
    // uint32_t getInd() const {return ind;} 
};


template<class T>
CircleQueue<T>::CircleQueue(uint32_t _capacity): capacity(_capacity)
{
    head = 0;
    tail = 0;
    ind = 0;
    que = new T[capacity + 1];
    memset(que, 0, sizeof(T)*(capacity + 1));
}

template<class T>
CircleQueue<T>::~CircleQueue(){
    delete[] que; 
}

template<class T>
bool CircleQueue<T>::enqueue(T ele){
    if(!full()){
        que[tail] = ele;
        tail = (tail + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template<class T>
bool CircleQueue<T>::pop(T& top){
    if(!empty()){
        top = que[head];
        head = (head + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template<class T>
bool CircleQueue<T>::full(){
    return (tail - head + capacity + 1) % (capacity + 1) == capacity;
}

template<class T>
bool CircleQueue<T>::empty(){
    return head == tail;
}

template<class T>
uint32_t CircleQueue<T>::size(){
    return (tail - head + capacity + 1) % (capacity + 1);
}

template<class T>
void CircleQueue<T>::rewind(){
    ind = head;
}

template<class T>
bool CircleQueue<T>::getNext(T& next){
    if(ind != tail){
        next = que[ind];
        ind = (ind + 1);
        return true;
    } 
    return false;
}

template<class T>
bool CircleQueue<T>::hasNext(){
    return ind != tail;
}

// template<class T>
// bool CirCleQueue<T>::isBegin(){
//     return ind == head;
// }


#endif