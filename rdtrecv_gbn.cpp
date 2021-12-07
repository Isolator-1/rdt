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
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"

#pragma comment(lib, "ws2_32.lib")


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

    WSADATA WSAData;
    uint32_t expectedseqnum = 0;
    rdt_t sendBuf; // 接收方发送的ACK包
    rdt_t recvBuf; // 接收方接收的数据包
    make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        printf("WSAStartup failure");
        exit(1);
    }
    SOCKET recverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
    SOCKADDR_IN senderAddr;
    int len = sizeof(SOCKADDR);
    std::ofstream fout(outpurfile, std::ios::binary);
    while (fout.is_open())
    {
        int result = recvfrom(recverSocket, (char *)&recvBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, &len);
        if (result > 0)
        {
            bool b1 = not_corrupt(&recvBuf);
            bool b2 = recvBuf.seqnum == expectedseqnum;
            if (b1 && b2)
            {
                LOG(printf("SEQ %d\n", recvBuf.seqnum))
                if (isFin(&recvBuf))
                {
                    LOG(printf("FIN\n"))
                    filesize = fout.tellp();
                    fout.close();
                    make_pkt(&sendBuf, ACK_FLAG | FIN_FLAG, expectedseqnum, 0, 0);
                    sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    break;
                }
                else if (isSyn(&recvBuf))
                {
                    LOG(printf("SYN\n"))
                    make_pkt(&sendBuf, ACK_FLAG | SYN_FLAG, 0, 0, 0);
                    sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    continue;
                }
                else
                {
                    if (fout.is_open())
                    {
                        fout.write((char *)recvBuf.data, recvBuf.dataLen);
                    }
                }
                make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
                sendto(recverSocket, (char *)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
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
                    LOG(printf("expect %d but receive %d\n", expectedseqnum, recvBuf.seqnum))
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
    closesocket(recverSocket);
    WSACleanup();
    printf("finished!\nTotal size: %u Bytes.\n", filesize);
    return 0;
}