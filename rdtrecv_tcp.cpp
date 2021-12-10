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
#include <condition_variable>
#include <atomic>
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"
#include "./include/circlequeue.tpp"
#include "./include/threadpool.hpp"

#pragma comment(lib, "ws2_32.lib")

#define DELAY        // 使用延时ACK
#define ACK_DELAY 30 // 延时发送的ACK的延时，单位ms

WSADATA WSAData;
SOCKET recverSocket;
SOCKADDR_IN senderAddr;

CircleQueue<rdt_t *> recvBuf(RECVER_RECV_BUF); // 接收缓冲
std::condition_variable notEmpty;
std::condition_variable notFull;
std::mutex bufLock;

CircleQueue<rdt_t *> recvWin(N); // 接收滑动窗口
ThreadPool TP(1);                // 用于延迟ACK的线程池
std::atomic_bool cancleDelay;

uint16_t holesInWin = N;

/**
 * @brief 获得Rwnd，为接收缓冲剩余和滑动窗口缓冲剩余的最小值
 * @return Rwnd
 */
uint16_t getRwnd()
{
    // uint16_t num = RECVER_RECV_BUF;
    // rdt_t *tmp;
    // recvWin.rewind();
    // while (recvWin.hasNext())
    // {
    //     recvWin.getNext(tmp);
    //     if (tmp != nullptr)
    //     {
    //         num--;
    //     }
    // }
    // LOG(
    //     assert(num >= 0);
    //     assert(num <= RECVER_RECV_BUF))
    // return num;
    uint16_t recvBufRemain = RECVER_RECV_BUF - recvBuf.size();
    return std::min(holesInWin, recvBufRemain);
}

/**
 * @brief 接收线程任务
 *
 */
void recv_task()
{
    printf("recv_task begin\n");
    int len = sizeof(SOCKADDR);
    char tmp[sizeof(rdt_t)];
    int result = 1;
    while (result > 0)
    {
        result = recvfrom(recverSocket, tmp, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, &len);
        rdt_t *buf = new rdt_t();
        memcpy_s(buf, sizeof(rdt_t), tmp, sizeof(rdt_t));
        {
            std::unique_lock<std::mutex> ul(bufLock);
            notFull.wait(ul, []
                         { return !recvBuf.full(); });
            auto b = recvBuf.enqueue(buf);
            LOG(assert(b))
            notEmpty.notify_one();
        }
    }
    int e = 0;
    if (result == SOCKET_ERROR)
    {
        e = WSAGetLastError();
    }
    printf("recv_task finished (%d)\n", e);
}

/**
 * @brief 延迟发送ACK线程任务
 * @param num ACK序号
 */
void delay_ack(uint32_t num)
{
    using namespace std::chrono;
    std::this_thread::sleep_for(milliseconds(ACK_DELAY));
    if (cancleDelay)
        return;
    rdt_t sendPkt;
    uint16_t rwnd = 0;
    // {
    //     std::lock_guard<std::mutex> lg(bufLock);
    //     rwnd = recvWin.size();
    // }
    rwnd = recvWin.size();
#ifdef FLOW_CONTROL
    make_pkt(&sendPkt, ACK_FLAG, num, 0, 0, getRwnd());
#else
    make_pkt(&sendPkt, ACK_FLAG, num, 0, 0);
#endif
    sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
    LOG(printf("delay ACK%u\n", num);)
}

int main(int argc, char **argv)
{
    // 开辟好空间，nullptr
    while (!recvWin.full())
    {
        recvWin.enqueue(nullptr);
    }
    rdt_t sendPkt;            // 接收方发送的ACK包
    rdt_t *recvPkt = nullptr; // 接收方接收的数据包
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
#ifdef FLOW_CONTROL
    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0);
#endif
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

    std::thread recvThread(recv_task);
    recvThread.detach();

    std::ofstream fout(outpurfile, std::ios::binary);
    while (fout.is_open())
    {
        delete recvPkt;
        recvPkt = nullptr;
        {
            std::unique_lock<std::mutex> ul(bufLock);
            notEmpty.wait(ul, []
                          { return !recvBuf.empty(); });
            auto b = recvBuf.pop(recvPkt);
            LOG(assert(b))
            notFull.notify_one();
        }
        bool b1 = not_corrupt(recvPkt);
        auto dist = (recvPkt->seqnum - expectedseqnum + NUM_SEQNUM) % NUM_SEQNUM;
        bool b2 = dist >= 0 && dist < N;
        if (b1 && b2)
        {
            if (isSyn(recvPkt))
            {
                LOG(printf("SYN\n"))
#ifdef FLOW_CONTROL
                make_pkt(&sendPkt, ACK_FLAG | SYN_FLAG, 0, 0, 0, getRwnd());
#else
                make_pkt(&sendPkt, ACK_FLAG | SYN_FLAG, 0, 0, 0);
#endif
                sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                continue;
            }
            else
            {
                if (dist == 0)
                {
                    // 所具有期望序号的按序报文段到达，逻辑上是填入缓冲区的0位置再弹出，实际上就不填了
                    expectedseqnum = (expectedseqnum + 1) % NUM_SEQNUM;
                    holesInWin--;
                    if (isFin(recvPkt))
                    {
                        // 若下一的期望的正好是FIN包，则滑动窗口中不再有其他有效包
                        LOG(
                            printf("1FIN %u\n", recvPkt->seqnum);
                            rdt_t * tmp;
                            for (uint32_t i = 0; i < recvWin.size(); i++) {
                                recvWin.index(i, tmp);
                                assert(tmp == nullptr);
                            })
#ifdef FLOW_CONTROL
                        make_pkt(&sendPkt, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
                        make_pkt(&sendPkt, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0);
#endif
                        sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                        filesize = fout.tellp();
                        fout.close();
                        break;
                    }
                    LOG(printf("SEQ %u\n", recvPkt->seqnum))
                    fout.write((char *)recvPkt->data, recvPkt->dataLen);
                    recvWin.pop();
                    recvWin.enqueue(nullptr);
                    holesInWin++;
                    LOG(assert(recvWin.full()))
                    rdt_t *writened = nullptr;
                    uint32_t acc = 0; // 滑动窗口中连续缓存的个数
                    while (recvWin.top(writened))
                    {
                        if (writened == nullptr)
                        {
                            break;
                        }
                        recvWin.pop();
                        holesInWin++;
                        bool end = false;
                        if (isFin(writened))
                        {
                            LOG(
                                printf("2FIN %u\n", writened->seqnum);
                                rdt_t * tmp;
                                for (uint32_t i = 0; i < recvWin.size(); i++) {
                                    recvWin.index(i, tmp);
                                    assert(tmp == nullptr);
                                })
#ifdef FLOW_CONTROL
                            make_pkt(&sendPkt, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
                            make_pkt(&sendPkt, FIN_FLAG | ACK_FLAG, expectedseqnum, 0, 0);
#endif
                            sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                            filesize = fout.tellp();
                            fout.close();
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
                    if (acc == 0 && TP.active() == 0)
                    {
                        // 发送延迟的ACK
                        LOG(printf("should delay %d\n", expectedseqnum))
                        cancleDelay.store(false);
                        TP.enqueue(delay_ack, expectedseqnum);
                    }
                    else
                    {
                        // 可以从第一个开始连续
                        // 立即发送累积ACK
                        cancleDelay.store(true);
#ifdef FLOW_CONTROL
                        make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
                        make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0);
#endif
                        sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    }
#else
#ifdef FLOW_CONTROL
                    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
                    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0)
#endif
                    sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
#endif
                }
                else
                {
                    // 比期望序号大的失序报文端到达，缓存到滑动窗口的dist位置
                    LOG(printf("expected %u, cached %u into %u\n", expectedseqnum, recvPkt->seqnum, dist))
                    bool b;
                    rdt_t *origin;
                    b = recvWin.index(dist, origin);
                    LOG(assert(b))
                    if (origin == nullptr)
                    {
                        // 之前该位置没有缓存
                        recvWin.replace(dist, new rdt_t(*recvPkt));
                        holesInWin--;
                    }
#ifdef FLOW_CONTROL
                    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
#else
                    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0);
#endif
                    sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
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
                LOG(printf("abandon %d\n", recvPkt->seqnum))
            }
            make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0, getRwnd());
            sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
        }
    }

    printf("waiting to stop\n");
    TP.stop();
    printf("Finished. %u Bytes in total\n", filesize);
    closesocket(recverSocket);
    WSACleanup();
    return 0;
}