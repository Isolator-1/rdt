#include "circlequeue.hpp"

    
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

template<class T>
CirCleQueue<T>::CirCleQueue(uint32_t _capacity): capacity(_capacity)
{
    head = 0;
    tail = 0;
    que = new T(capacity + 1);
    memset(que, 0, sizeof(T)*(capacity + 1));
}

template<class T>
CirCleQueue<T>::~CirCleQueue(){
    delete[] que;
}

template<class T>
bool CirCleQueue<T>::enqueue(T* ele){
    if(!full()){
        que[tail] = *ele;
        tail = (tail + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template<class T>
void CirCleQueue<T>::pop(T* top){
    if(!empty()){
        *top = que[head];
        head = (head + 1) % (capacity + 1);
    } else {
        top = nullptr;
    }
}

template<class T>
bool CirCleQueue<T>::full(){
    return (tail - head + capacity + 1) % (capacity + 1) == capacity;
}

template<class T>
bool CirCleQueue<T>::empty(){
    return head == tail;
}

template<class T>
uint32_t CirCleQueue<T>::size(){
    return (tail - head + capacity + 1) % (capacity + 1);
}

template<class T>
void CirCleQueue<T>::rewind(){
    ind = head;
}

template<class T>
void CirCleQueue<T>::getNext(T* next){
    if(ind != tail){
        *next = que[ind];
        ind = (ind + 1);
    } else{
        next = nullptr;
    }
}

