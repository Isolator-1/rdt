/**
 * @file rdtsend_gbn.cpp
 * @author zqs
 * @brief 使用类TCP协议的发送方
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) 2021
 *
 */

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


using namespace std::chrono;
system_clock::time_point sendBegin;
system_clock::time_point sendEnd;
uint64_t filesize;

SOCKET senderSocket;
SOCKADDR_IN recverAddr;

uint32_t base = 0;
uint32_t nextseqnum = 0;
CircleQueue<rdt_t *> sendWin(N); // 保存已发送但是待确认数据包的循环队列
std::condition_variable notFull; // 滑动窗口非满条件变量
std::mutex winMutex;             // 滑动窗口锁
Timer timer(TIMEOUT);            // 定时器，单位s，目前不是线程安全的
std::atomic_bool isConnect;      // 已连接

void recv_task()
{
    printf("recv_task enter!\n");
    SOCKADDR_IN sock;
    int len = sizeof(sock);
    int result;
    rdt_t pktBuf;
    while (true)
    {
        result = recvfrom(senderSocket, (char *)&pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&sock, &len);
        if (result == SOCKET_ERROR)
        {
            int e = WSAGetLastError();
            if (e == 10054)
            {
                // 未发现远程主机错误，出现该错误通常是因为先启动发送方，后启动接收方，因此不因为该错误结束接收线程
                printf("Cannot find recver\n");
                continue;
            }
            printf("Receive error %d\n", e);
            break;
        }
        if (result == 0)
        {
            printf("The connection is closed\n");
            break;
        }
        if (!not_corrupt(&pktBuf))
        {
            printf("corrupt package %u\n");
            continue;
        }
        if (isAck(&pktBuf))
        {
            if (isFin(&pktBuf))
            {
                LOG(printf("FIN ACK %u\n", pktBuf.seqnum))
                sendEnd = system_clock::now();
                break;
            }
            else if (isSyn(&pktBuf))
            {
                LOG(printf("SYN ACK %u\n", pktBuf.seqnum))
                isConnect.store(true);
                continue;
            }
            else
            {
                std::lock_guard<std::mutex> lg(winMutex);
                // 收到确认，如果确认号比先前的位置大不过N，则前推滑动窗口（累积确认）
                auto lastBase = base;
                rdt_t *abandoned = nullptr;
                bool b = true;
                auto dist = (pktBuf.seqnum - lastBase + NUM_SEQNUM) % NUM_SEQNUM;
                if (dist > 0 && dist <= N)
                {
                    base = pktBuf.seqnum;
                    LOG(printf("ACK [%u, %u)\n", lastBase, pktBuf.seqnum))
                    for (auto i = 0; i < dist; i++)
                    {
                        sendWin.pop(abandoned);
                        if (abandoned != nullptr)
                        {
                            delete abandoned;
                            abandoned = nullptr;
                        }
                    }
                }
                else
                {
                    LOG(printf("DUP ACK %u\n", pktBuf.seqnum))
                }
                LOG(
                    assert(b);
                    auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                    if (size != sendWin.size()) {
                        printf("recv_task: size: %u, sendWin.size(): %u\n", size, sendWin.size());
                    })
                notFull.notify_all();
                if (sendWin.empty())
                {
                    timer.stop();
                }
                else
                {
                    timer.rewind();
                }
            }
        }
        else
        {
            printf("Not ACK\n");
            exit(1);
        }
    }
    printf("recv_task quit!\n");
}

void resend_task()
{
    printf("resend_task enter!\n");
    // 忙等待
    while (true)
    {
        if (timer.check())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
        }
        else
        {
            std::lock_guard<std::mutex> lg(winMutex);
            // 在超时范围内，队列没有被清空
            LOG(
                printf("timeout: resend seq %u\n", base);
                auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                if (size != sendWin.size()) {
                    printf("resend_task: size: %u, sendWin.size(): %u\n", size, sendWin.size());
                })
            // 只重传一个
            rdt_t *first;
            sendWin.top(first);
            sendto(senderSocket, (char *)first, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            timer.rewind();
        }
    }
    printf("resend_task quit!\n");
}

void rdt_send(char *data, uint16_t dataLen, uint8_t flag = 0)
{
    std::unique_lock<std::mutex> ul(winMutex);
    while (true)
    {
        if (!sendWin.full())
        {
            LOG(
                auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                if (size != sendWin.size()) {
                    printf("rdt_send: size: %u, sendWin.size(): %u\n", size, sendWin.size());
                })
            rdt_t *pktBuf = new rdt_t();
            rdt_t *abandoned = nullptr;
            make_pkt(pktBuf, flag, nextseqnum, (uint8_t *)data, dataLen);
            auto index = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
            auto b = false;
            if (index == sendWin.size())
            {
                b = sendWin.enqueue(pktBuf);
                LOG(assert(b))
            }
            else if (index < sendWin.size())
            {
                abandoned = pktBuf;
            }
            sendto(senderSocket, (char *)pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            if (abandoned != nullptr)
            {
                delete abandoned;
            }
            if (base == nextseqnum)
            {
                // 队列中第一个待发送的包！
                timer.rewind();
                sendWin.rewind();
            }
            nextseqnum = (nextseqnum + 1) % NUM_SEQNUM;
            break;
        }
        else
        {
            // refuse data
            notFull.wait(ul); // 阻塞，并释放锁
        }
    }
}

void connect()
{
    rdt_t pktBuf;
    make_pkt(&pktBuf, SYN_FLAG, 0, 0, 0);
    while (!isConnect)
    {
        sendto(senderSocket, (char *)&pktBuf, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL));
    }
}

int main(int argc, char **argv)
{
    char inputfile[64] = "input/test.txt";
    uint16_t senderPort = SENDER_PORT;
    uint16_t recverPort = RECVER_PORT;
    char recverAddrStr[16] = DEFAULT_ADDR;
    switch (argc)
    {
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
    senderSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // 创建客户端用于通信的Socket
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
    sendBegin = system_clock::now();
    while (fin)
    {
        fin.read(sendBuf, DATA_SIZE);
        rdt_send(sendBuf, fin.gcount());
        filesize += fin.gcount();
        memset(sendBuf, 0, DATA_SIZE);
    }
    fin.close();

    timerThread.detach();
    // 断开连接
    printf("waiting to disconnect...\n");
    rdt_send(0, 0, FIN_FLAG);
    recvThread.join();
    auto duration = duration_cast<microseconds>(sendEnd - sendBegin);
    double costTime = duration.count() / 1000.0;
    double throughput = (filesize * 8) / (costTime / 1000) / 1e6;
    printf("finished!\nFile size: %u Bytes. Cost %.2f ms\n"
           "Throughput rate: %.2f Mb/s\n",
           filesize, costTime, throughput);
    closesocket(senderSocket);
    WSACleanup();

    return 0;
}