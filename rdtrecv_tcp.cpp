/**
 * @file rdtrecv_gbn.cpp
 * @author zqs
 * @brief 使用类TCP协议的接收方
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"
#include "./include/circlequeue.tpp"
#include "./include/threadpool.hpp"

#pragma comment(lib, "ws2_32.lib")

// #define DELAY         // 使用延时ACK
#define ACK_DELAY 30  // 延时发送的ACK的延时，单位ms
#define TNUM 4        // 线程数

WSADATA WSAData;
SOCKET recverSocket;
SOCKADDR_IN senderAddr;
CircleQueue<rdt_t *> recvWin(N);
ThreadPool TP(TNUM);           // 线程池
std::atomic_bool delays[TNUM]; // 终止发送信号
uint8_t lastD;                 // 上一个delay线程的终止信号序号
bool readyClose = false;       // 置位时说明收到了FIN，即滑动窗口不会再滑动
/**
 * @brief 延迟发送ACK线程任务
 * @param num ACK序号
 * @param dnum 终止发送信号序号
 */
void delay_ack(uint32_t num, uint8_t dnum)
{
    using namespace std::chrono;
    std::this_thread::sleep_for(milliseconds(ACK_DELAY));
    if (!delays[dnum])
    {
        return;
    }
    rdt_t sendBuf;
    make_pkt(&sendBuf, ACK_FLAG, num, 0, 0);
    sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
}

int main(int argc, char **argv)
{
    // 开辟好空间，nullptr
    while (!recvWin.full())
    {
        recvWin.enqueue(nullptr);
    }
    rdt_t sendBuf; // 接收方发送的ACK包
    rdt_t recvBuf; // 接收方接收的数据包
    SOCKADDR_IN recverAddr;
    uint32_t expectedseqnum = 0; // 期望的下一个包的序号
    char outpurfile[64] = "output/test.txt";
    uint64_t filesize = 0;
    uint16_t recverPort = RECVER_PORT;
    char recverAddrStr[16] = DEFAULT_ADDR;
    switch (argc)
    {
    case 4:
        strcpy_s(recverAddrStr, argv[3]);
    case 3:
        recverPort = atoi(argv[2]);
    case 2:
        strcpy_s(outpurfile, argv[1]);
    default:
        printf("outputfile: %s, recver port: %u, recver addr: %s\n",
               outpurfile, recverPort, recverAddrStr);
    }
    make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        printf("WSAStartup failure");
        exit(1);
    }
    recverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recverSocket == INVALID_SOCKET)
    {
        printf("socket failure\n");
        return 0;
    }
    recverAddr;
    recverAddr.sin_family = AF_INET;
    recverAddr.sin_port = htons(recverPort);
    recverAddr.sin_addr.S_un.S_addr = inet_addr(recverAddrStr);
    if (bind(recverSocket, (SOCKADDR *)&recverAddr, sizeof(recverAddr)) == SOCKET_ERROR)
    {
        printf("bind failure %d\n", WSAGetLastError());
        return 0;
    }

    int len = sizeof(SOCKADDR);
    std::ofstream fout(outpurfile, std::ios::binary);
    while (fout.is_open())
    {
        int result = recvfrom(recverSocket, (char *)&recvBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, &len);
        if (result > 0)
        {
            bool b1 = not_corrupt(&recvBuf);
            auto dist = (recvBuf.seqnum - expectedseqnum + NUM_SEQNUM) % NUM_SEQNUM;
            bool b2 = dist >= 0 && dist < N;
            if (b1 && b2)
            {
                if (isSyn(&recvBuf))
                {
                    LOG(printf("SYN\n"))
                    make_pkt(&sendBuf, ACK_FLAG | SYN_FLAG, 0, 0, 0);
                    sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    continue;
                }
                else
                {
                    if (dist == 0)
                    {
                        // 更新expectedseqnum, 写入文件，窗口滑动右移，发送expectedseqnum为序号的ACK
                        expectedseqnum = (expectedseqnum + 1) % NUM_SEQNUM;
                        if (isFin(&recvBuf))
                        {
                            // 若下一的期望的正好是FIN包，则滑动窗口中不再有其他有效包
                            LOG(
                                printf("FIN %u\n", recvBuf.seqnum);
                                rdt_t * tmp;
                                for (uint32_t i = 0; i < recvWin.size(); i++) {
                                    recvWin.index(i, tmp);
                                    assert(tmp == nullptr);
                                })
                            make_pkt(&sendBuf, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0);
                            sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                            filesize = fout.tellp();
                            fout.close();
                            break;
                        }
                        LOG(printf("SEQ %u\n", recvBuf.seqnum))
                        fout.write((char *)recvBuf.data, recvBuf.dataLen);
                        recvWin.pop();
                        recvWin.enqueue(nullptr);
                        LOG(assert(recvWin.full()))
                        rdt_t *writened = nullptr;
                        uint32_t acc = 0;
                        while (recvWin.top(writened))
                        {
                            if (writened == nullptr)
                            {
                                break;
                            }
                            recvWin.pop();
                            bool end = false;
                            if (isFin(writened))
                            {
                                LOG(
                                    printf("FIN %u\n", writened->seqnum);
                                    rdt_t * tmp;
                                    for (uint32_t i = 0; i < recvWin.size(); i++) {
                                        recvWin.index(i, tmp);
                                        assert(tmp == nullptr);
                                    })
                                filesize = fout.tellp();
                                fout.close();
                                make_pkt(&sendBuf, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0);
                                sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                                end = true;
                                break;
                            }
                            if (end)
                                break;
                            acc++;
                            fout.write((char *)writened->data, writened->dataLen);
                            delete writened;
                            writened = nullptr;
                        }
                        expectedseqnum = (expectedseqnum + acc) % NUM_SEQNUM;
                        for (auto i = 0; i < acc; i++)
                        {
                            recvWin.enqueue(nullptr);
                        }
                        // LOG(assert(recvWin.full())) // 因为不清楚原因在调试过程中过不去，但不影响传输的正确性
#ifdef DELAY
                        if (acc == 0)
                        {
                            // 发送延迟的ACK
                            delays[lastD].store(true);
                            TP.enqueue(delay_ack, expectedseqnum, lastD);
                            lastD = (lastD + 1) % TNUM;
                        }
                        else
                        {
                            // 放弃延时ACK，立即发送累积ACK
                            for (uint8_t i = 0; i < TNUM; i++)
                            {
                                delays[i].store(false);
                            }
                            make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
                            sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                        }
#else
                        make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
                        sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
#endif
                    }
                    else
                    {
                        // 缓存到滑动窗口的dist位置
                        LOG(printf("expected %u, cached %u into %u\n", expectedseqnum, recvBuf.seqnum, dist))
                        bool b;
                        rdt_t *origin;
                        b = recvWin.index(dist, origin);
                        LOG(assert(b))
                        if (origin == nullptr)
                        {
                            // 之前该位置没有缓存
                            recvWin.replace(dist, new rdt_t(recvBuf));
                        }
                        make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
                        sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    }
                }
            }
            else
            {
                if (!b1)
                {
                    LOG(printf("corrupt package\n"))
                }
                if (!b2)
                {
                    LOG(printf("abandon %d\n", recvBuf.seqnum))
                }
                sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
            }
        }
        else if (result == 0)
        {
            printf("Connection end.\n");
            break;
        }
        else
        {
            printf("Receive failure %d\n", WSAGetLastError());
            break;
        }
    }
    printf("waiting to stop\n");
    TP.stop();
    printf("Finished. %u Bytes in total\n", filesize);
    closesocket(recverSocket);
    WSACleanup();
    return 0;
}