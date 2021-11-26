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
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT 5                   // 超时时间, 单位s
#define INTERVAL 50                 // 检查定时器时间间隔，单位ms


SOCKET senderSocket;
SOCKADDR_IN recverAddr;


std::atomic_uint base;         
std::atomic_uint nextseqnum;   
std::vector<rdt_t*> sndpkt;
std::condition_variable notFull;    // 滑动窗口非满条件变量
std::mutex winMutex;                // 滑动窗口锁
Timer timer(TIMEOUT);               // 定时器，单位s，目前不是线程安全的



void recv_task(){
    printf("recv_task enter!\n");
    SOCKADDR_IN sock;
    int len = sizeof(sock);
    int result;
    rdt_t pktBuf;
    while(true){
        result = recvfrom(senderSocket, (char*)&pktBuf, sizeof(rdt_t), 0, (SOCKADDR*)&sock, &len);
        if (result == SOCKET_ERROR) {
            printf("Receive error %d\n", WSAGetLastError());
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
            }else {          
                #ifdef DEBUG  
                    printf("ACK %u\n", pktBuf.seqnum);
                #endif
                base.store(pktBuf.seqnum + 1);
                notFull.notify_all();
                // TODO: 删除base之前的节点，释放内存
                if(base == nextseqnum) {
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
            auto beg = base.load();
            auto end = nextseqnum.load();
            printf("timeout: resend from %d to %d\n", beg, end - 1);
            for(auto i = beg; i < end; i++){
                sendto(senderSocket, (char*)sndpkt[i], sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            }
            timer.rewind();
        }
    }
    printf("resend_task quit!\n");
}



void rdt_send(char* data, uint16_t dataLen, uint8_t flag = 0){
    std::unique_lock<std::mutex> ul(winMutex);
    while(true){
        if (nextseqnum < base + N){
            rdt_t* pktBuf = new rdt_t();
            make_pkt(pktBuf, flag, nextseqnum, (uint8_t*)data, dataLen);
            sndpkt.push_back(pktBuf);
            sendto(senderSocket, (char*)pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            if(base == nextseqnum){
                // starttimer
                timer.rewind();
            }
            nextseqnum.store((nextseqnum.load() + 1));
            break;
        }
        else {
            // refuse data
            notFull.wait(ul); // 阻塞，并释放锁
        }
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

    base.store(0);
    nextseqnum.store(0);
    std::thread timerThread(resend_task);
    std::thread recvThread(recv_task);

    char sendBuf[DATA_SIZE];
    memset(sendBuf, 0, DATA_SIZE);
    std::ifstream fin(inputfile, std::ios::binary); 
    while(fin)
    {
        fin.read(sendBuf, DATA_SIZE);
        // printf("gcount: %d\n", fin.gcount());
        rdt_send(sendBuf, fin.gcount());
        memset(sendBuf, 0, DATA_SIZE);
    }   
    fin.close();

    rdt_send(0, 0, FIN_FLAG);

    
    timerThread.detach();
    recvThread.join();

    closesocket(senderSocket);
    WSACleanup();

    return 0;
}