/**
 * @file circlequeue.tpp
 * @author zqs
 * @brief 可随机访问的循环队列模板
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef CIRCLE_QUEUE_HPP
#define CIRCLE_QUEUE_HPP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

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
class CircleQueue
{
private:
    uint32_t head;
    uint32_t tail;
    T *que;
    uint32_t ind;
    uint32_t capacity;

public:
    CircleQueue(uint32_t _capacity);
    ~CircleQueue();

    /**
     * @brief 在队尾插入元素
     *
     * @param ele 插入的元素
     * @return true 插入成功
     * @return false 插入失败（队满）
     */
    bool enqueue(T ele);

    /**
     * @brief 弹出队首元素
     *
     * @param top 弹出的元素
     * @return true 弹出成功
     * @return false 弹出失败（队空）
     */
    bool pop(T &top);

    /**
     * @brief 尝试弹出队首元素，如果队空，什么都不做，该重载函数配合top使用
     * @see top
     */
    void pop();

    /**
     * @brief 返回队首元素
     *
     * @param ele 队首元素
     * @return true 返回成功
     * @return false 返回失败，队空
     */
    bool top(T &ele);

    /**
     * @brief 替换队伍中的一个元素
     *
     * @param index 从队首（index=0）起，第index个被替换
     * @param ele 新的元素
     * @return true 替换成功
     * @return false 替换失败（索引越界）
     */
    bool replace(uint32_t index, T ele);

    /**
     * @brief 索引队伍中的一个元素
     *
     * @param ind 从队首（index=0）起，第ind个被索引
     * @param ele 索引出的元素
     * @return true 索引成功
     * @return false 索引失败
     */
    bool index(uint32_t ind, T &ele);
    bool full();
    bool empty();
    uint32_t size();
    void rewind();
    bool getNext(T &next);
    bool hasNext();
    uint32_t getHead() const { return head; }
    uint32_t getTail() const { return tail; }
};

template <class T>
CircleQueue<T>::CircleQueue(uint32_t _capacity) : capacity(_capacity)
{
    head = 0;
    tail = 0;
    ind = 0;
    que = new T[capacity + 1];
    memset(que, 0, sizeof(T) * (capacity + 1));
}

template <class T>
CircleQueue<T>::~CircleQueue()
{
    delete[] que;
}

template <class T>
bool CircleQueue<T>::enqueue(T ele)
{
    if (!full())
    {
        que[tail] = ele;
        tail = (tail + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template <class T>
bool CircleQueue<T>::pop(T &top)
{
    if (!empty())
    {
        top = que[head];
        head = (head + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template <class T>
void CircleQueue<T>::pop()
{
    if (!empty())
    {
        head = (head + 1) % (capacity + 1);
    }
}

template <class T>
bool CircleQueue<T>::top(T& ele)
{
    if(!empty()){
        ele = que[head];
        return true;
    }
    return false;
}

template <class T>
bool CircleQueue<T>::full()
{
    return (tail - head + capacity + 1) % (capacity + 1) == capacity;
}

template <class T>
bool CircleQueue<T>::empty()
{
    return head == tail;
}

template <class T>
uint32_t CircleQueue<T>::size()
{
    return (tail - head + capacity + 1) % (capacity + 1);
}

template <class T>
void CircleQueue<T>::rewind()
{
    ind = head;
}

template <class T>
bool CircleQueue<T>::getNext(T &next)
{
    if (ind != tail && head != tail)
    {
        next = que[ind];
        ind = (ind + 1) % (capacity + 1);
        return true;
    }
    return false;
}

template <class T>
bool CircleQueue<T>::hasNext()
{
    return ind != tail && head != tail;
}

// template<class T>
// bool CirCleQueue<T>::isBegin(){
//     return ind == head;
// }

template <class T>
bool CircleQueue<T>::replace(uint32_t index, T ele)
{
    if (index >= size())
    {
        return false;
    }
    auto i = (index + head) % (capacity + 1);
    que[i] = ele;
    return true;
}

template <class T>
bool CircleQueue<T>::index(uint32_t ind, T &ele)
{
    if (ind >= size())
    {
        return false;
    }
    auto i = (ind + head) % (capacity + 1);
    ele = que[i];
    return true;
}

#endif