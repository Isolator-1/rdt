#include "../include/circlequeue.tpp"
#include <stdio.h>

#define N 3

template <class T>
void print_cq(CircleQueue<T>& cq){
    printf("%-10u%-10u%-10u%-10u%-10u\n", cq.getHead(), cq.getTail(), cq.size(), cq.full(), cq.empty());
}


template <class T>
void f(CircleQueue<T>& cq){
    int i = 0;
    int* ele = new int(i++);
    while(!cq.full()){
        cq.enqueue(ele);
        print_cq(cq);
        ele = new int(i++);
    }
}

template <class T>
void p(CircleQueue<T>& cq){
    int* ele = nullptr;
    while(!cq.empty()){
        cq.pop(ele);
        printf("top: %u\n", *ele);
        delete ele;
        ele = nullptr;
        print_cq(cq);
    }
}

int main(){
    CircleQueue<int*> cq(N);
    printf("%-10s%-10s%-10s%-10s%-10s\n", "head", "tail", "size", "full", "empty");
    f(cq);
    p(cq);
    f(cq);
    p(cq);
    return 0;
}