#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include "./include/timer.hpp"
#include "./include/rdt.hpp"
#include "./include/def.hpp"

#pragma comment(lib, "ws2_32.lib")

using namespace std::chrono;

int main(int argc, char** argv)
{
    system_clock::time_point recvBegin;
    system_clock::time_point recvEnd;
    char outpurfile[64] = "output/test.txt";
    uint64_t filesize = 0;
    uint16_t recverPort = RECVER_PORT;
    char recverAddrStr[16] = DEFAULT_ADDR;
    switch(argc){
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
    rdt_t sendBuf;  // 接收方发送的ACK包
    rdt_t recvBuf;  // 接收方接收的数据包
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
        int result = recvfrom(recverSocket, (char*)&recvBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, &len);
        if (result > 0)
        {
            bool b1 = not_corrupt(&recvBuf);
            bool b2 = recvBuf.seqnum == expectedseqnum;
            if(b1 && b2) {
                #ifdef DEBUG
                    printf("SEQ %d\n", recvBuf.seqnum);
                #endif
                if(isFin(&recvBuf)){
                    #ifdef DEBUG
                        printf("FIN\n");
                    #endif
                    recvEnd = system_clock::now();  // 停止传输
                    filesize = fout.tellp();
                    fout.close();
                    make_pkt(&sendBuf, ACK_FLAG | FIN_FLAG, expectedseqnum, 0, 0);
                    sendto(recverSocket, (char*)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                    break;
                }
                else if (isSyn(&recvBuf)){
                    #ifdef DEBUG
                        printf("SYN\n");
                    #endif
                    recvBegin = system_clock::now();  // 开始传输
                    make_pkt(&sendBuf, ACK_FLAG | SYN_FLAG, 0, 0, 0);
                    sendto(recverSocket, (char*)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR*)&senderAddr, sizeof(SOCKADDR));
                    continue;
                }   
                else {
                    if(fout.is_open()){
                        fout.write((char*)recvBuf.data, recvBuf.dataLen);
                    }
                }
                make_pkt(&sendBuf, ACK_FLAG, expectedseqnum, 0, 0);
                sendto(recverSocket, (char*)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
                expectedseqnum = (expectedseqnum + 1) % NUM_SEQNUM;
            }
            else {
                if(!b1){
                    #ifdef DEBUG
                        printf("corrupt package\n");
                    #endif
                }
                if(!b2){
                    #ifdef DEBUG
                        printf("expect %d but receive %d\n", expectedseqnum, recvBuf.seqnum);
                    #endif
                }
                sendto(recverSocket, (char*)&sendBuf, sizeof(rdt_t), 0, (SOCKADDR *)&senderAddr, sizeof(SOCKADDR));
            }
        } else if (result == 0){
            printf("Connection end.\n");
            break;
        } else {
            printf("Receive failure %d\n", WSAGetLastError());
            break;
        }
    }
    closesocket(recverSocket);
    WSACleanup();
    auto duration = duration_cast<microseconds>(recvEnd - recvBegin);
    double costTime = duration.count()/1000.0;
    double throughput = (filesize * 8) / (costTime / 1000) / 1e6;
    printf("finished!\n File size: %u Bytes. Cost %.2f ms\n"
    "Throughput rate: %.2f Mb/s\n", filesize, costTime, throughput);
    return 0;
}