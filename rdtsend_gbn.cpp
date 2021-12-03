#include <winsock2.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <assert.h>
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"
#include "./include/circlequeue.tpp"

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT 0.5                 // 超时时间, 单位s
#define INTERVAL 50                 // 检查定时器时间间隔，单位ms


SOCKET senderSocket;
SOCKADDR_IN recverAddr;


uint32_t base = 0;
uint32_t nextseqnum = 0;
CircleQueue<rdt_t*> sndpkt(N);      // 保存已发送但是待确认数据包的循环队列
std::condition_variable notFull;    // 滑动窗口非满条件变量
std::mutex winMutex;                // 滑动窗口锁
Timer timer(TIMEOUT);               // 定时器，单位s，目前不是线程安全的
std::atomic_bool isConnect;         // 已连接


void recv_task(){
    printf("recv_task enter!\n");
    SOCKADDR_IN sock;
    int len = sizeof(sock);
    int result;
    rdt_t pktBuf;
    while(true){
        result = recvfrom(senderSocket, (char*)&pktBuf, sizeof(rdt_t), 0, (SOCKADDR*)&sock, &len);
        if (result == SOCKET_ERROR) {
            int e =  WSAGetLastError();
            if (e == 10054){
                // 远程主机未发现错误
                continue;
            }
            printf("Receive error %d\n", e);
            break;
        }
        if (result == 0) {
            printf("The connection is closed\n");
            break;
        }
        if(!not_corrupt(&pktBuf)){
            printf("corrupt package %u\n");
            continue;
        }
        if(isAck(&pktBuf)) {
            if(isFin(&pktBuf)){
                #ifdef DEBUG
                    printf("FIN ACK %u\n", pktBuf.seqnum);
                #endif
                break;
            }else if(isSyn(&pktBuf)) {
                #ifdef DEBUG
                    printf("SYN ACK %u\n", pktBuf.seqnum);
                #endif
                isConnect.store(true);
                continue;
            }
            else {        
                std::lock_guard<std::mutex> lg(winMutex);  
                // 收到确认，如果确认号是滑动窗口第一个，则前推滑动窗口
                auto lastBase = base;
                base = (pktBuf.seqnum + 1) % NUM_SEQNUM;
                rdt_t* abandoned = nullptr;
                bool b = true;
                if (base == (lastBase + 1) % NUM_SEQNUM){
                    #ifdef DEBUG  
                        printf("ACK %u\n", pktBuf.seqnum);
                    #endif
                    b = sndpkt.pop(abandoned);
                    if(abandoned != nullptr){
                        delete abandoned;
                        abandoned = nullptr;
                    }
                } else {
                    #ifdef DEBUG
                        printf("DUP ACK %u\n");
                    #endif
                }
                #ifdef DEBUG
                    assert(b);
                    auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                    if(size != sndpkt.size()){
                        printf("recv_task: size: %u, sndpkt.size(): %u\n", size, sndpkt.size());
                    }
                #endif

                notFull.notify_all();
                // TODO: 删除base之前的节点，释放内存
                if(sndpkt.empty()) {
                    timer.stop();
                }
                else {
                    timer.rewind();
                }
            }
        }
    }
    printf("recv_task quit!\n");
}

void resend_task(){
    printf("resend_task enter!\n");
    // 忙等待
    while(true){
        if(timer.check()){
            std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
        } else {
            std::lock_guard<std::mutex> lg(winMutex);
            // 在超时范围内，队列没有被清空
            #ifdef DEBUG
                printf("timeout: resend size %d\n", sndpkt.size());
                auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                if(size != sndpkt.size()){
                    printf("resend_task: size: %u, sndpkt.size(): %u\n", size, sndpkt.size());
                }
            #endif
            rdt_t* next;
            sndpkt.rewind();
            while(sndpkt.hasNext()){
                sndpkt.getNext(next);
                #ifdef DEBUG 
                    assert(next != nullptr);
                #endif
                sendto(senderSocket, (char*)next, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            }
            timer.rewind();
        }
    }
    printf("resend_task quit!\n");
}

void rdt_send(char* data, uint16_t dataLen, uint8_t flag = 0){
    std::unique_lock<std::mutex> ul(winMutex);
    while(true){
        if (!sndpkt.full()){
            #ifdef DEBUG
                auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                if(size != sndpkt.size()){
                    printf("rdt_send: size: %u, sndpkt.size(): %u\n", size, sndpkt.size());
                }
            #endif
            rdt_t* pktBuf = new rdt_t();
            make_pkt(pktBuf, flag, nextseqnum, (uint8_t*)data, dataLen);
            auto index = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
            auto b = false;
            if(index == sndpkt.size()){
                b = sndpkt.enqueue(pktBuf);
            }
            else if(index < sndpkt.size()){
                b = sndpkt.replace(index, pktBuf);
            }
            #ifdef DEBUG
                assert(b);
            #endif
            sendto(senderSocket, (char*)pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            if(base == nextseqnum){
                // 队列中第一个待发送的包！
                timer.rewind();
                sndpkt.rewind();
            }
            nextseqnum = (nextseqnum + 1) % NUM_SEQNUM;
            break;
        }
        else {
            // refuse data
            notFull.wait(ul); // 阻塞，并释放锁
        }
    }
}

void connect(){
    rdt_t pktBuf;
    make_pkt(&pktBuf, SYN_FLAG, 0, 0, 0);
    while(!isConnect){
        sendto(senderSocket, (char*)&pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
    }
}

int main(int argc, char** argv)
{
    char inputfile[64] = "input/test.txt";
    uint16_t senderPort = SENDER_PORT;
    uint16_t recverPort = RECVER_PORT;
    char recverAddrStr[16] = DEFAULT_ADDR;
    switch(argc){
        case 5:
            senderPort = atoi(argv[4]);
        case 4:
            strcpy_s(recverAddrStr, argv[3]);
        case 3:
            recverPort = atoi(argv[2]);
        case 2:
            strcpy_s(inputfile, argv[1]);
        default:
            printf("inputfile: %s, recver port: %u, recver addr: %s, sender port: %u\n",
                    inputfile, recverPort, recverAddrStr, senderPort);
    }  

    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        printf("WSAStartup failed!");
        WSACleanup();
        return -1;
    }

    //初始化
    senderSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // 创建客户端用于通信的Socket
    if (senderSocket == INVALID_SOCKET)
    {
        printf("socket failure %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    recverAddr.sin_family = AF_INET;
    recverAddr.sin_port = htons(recverPort);               
    recverAddr.sin_addr.S_un.S_addr = inet_addr(recverAddrStr); 

    if (senderSocket == INVALID_SOCKET)
    {
        printf("socket failure %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }
    SOCKADDR_IN senderAddr;
    senderAddr.sin_family = AF_INET;
    senderAddr.sin_port = htons(senderPort);
    senderAddr.sin_addr.S_un.S_addr = inet_addr(recverAddrStr);
    if (bind(senderSocket, (SOCKADDR *)&senderAddr, sizeof(senderAddr)) == SOCKET_ERROR)
    {
        printf("bind failure %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // 启动计时线程和接收线程
    isConnect.store(false);
    std::thread timerThread(resend_task);
    std::thread recvThread(recv_task);

    // 发送连接请求
    printf("waiting to connect...\n");
    connect();
    printf("connected\n");

    // 发送文件
    char sendBuf[DATA_SIZE];
    memset(sendBuf, 0, DATA_SIZE);
    std::ifstream fin(inputfile, std::ios::binary); 
    while(fin)
    {
        fin.read(sendBuf, DATA_SIZE);
        rdt_send(sendBuf, fin.gcount());
        memset(sendBuf, 0, DATA_SIZE);
    }   
    fin.close();

    timerThread.detach();
    // 断开连接
    printf("waiting to disconnect...\n");
    rdt_send(0, 0, FIN_FLAG);
    recvThread.join();
    printf("disconnect\n");

    closesocket(senderSocket);
    WSACleanup();

    return 0;
}