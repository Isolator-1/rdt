/**
 * @file rdtrecv_gbn.cpp
 * @author zqs
 * @brief 使用GBN协议的接收方
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
#include <thread>
#include <condition_variable>
#include <atomic>
#include "../include/timer.hpp"
#include "../include/rdt.hpp"
#include "../include/def.hpp"
#include "../include/circlequeue.tpp"

#pragma comment(lib, "ws2_32.lib")

WSADATA WSAData;
SOCKET recverSocket;
SOCKADDR_IN senderAddr;

CircleQueue<rdt_t *> recvBuf(RECVER_RECV_BUF); // 接收缓冲
std::condition_variable notEmpty;
std::condition_variable notFull;
std::mutex bufLock;

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

int main(int argc, char **argv)
{
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

    uint32_t expectedseqnum = 0;
    rdt_t sendPkt;  // 接收方发送的ACK包
    rdt_t *recvPkt = nullptr; // 接收方接收的数据包
    make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0);
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
    SOCKADDR_IN recverAddr;
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

    int len = sizeof(SOCKADDR);
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
        bool b2 = recvPkt->seqnum == expectedseqnum;
        if (b1 && b2)
        {
            LOG(printf("SEQ %d\n", recvPkt->seqnum))
            if (isFin(recvPkt))
            {
                LOG(printf("FIN\n"))
                filesize = fout.tellp();
                fout.close();
                make_pkt(&sendPkt, ACK_FLAG | FIN_FLAG, expectedseqnum, 0, 0);
                sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                break;
            }
            else if (isSyn(recvPkt))
            {
                LOG(printf("SYN\n"))
                make_pkt(&sendPkt, ACK_FLAG | SYN_FLAG, 0, 0, 0);
                sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                continue;
            }
            else
            {
                if (fout.is_open())
                {
                    fout.write((char *)recvPkt->data, recvPkt->dataLen);
                }
            }
            make_pkt(&sendPkt, ACK_FLAG, expectedseqnum, 0, 0);
            sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
            expectedseqnum = (expectedseqnum + 1) % NUM_SEQNUM;
        }
        else
        {
            if (!b1)
            {
                LOG(printf("corrupt package\n"))
            }
            if (!b2)
            {
                LOG(printf("expect %d but receive %d\n", expectedseqnum, recvPkt->seqnum))
            }
            sendto(recverSocket, (char *)&sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
        }
    }
    closesocket(recverSocket);
    WSACleanup();
    printf("finished!\nTotal size: %u Bytes.\n", filesize);
    return 0;
}