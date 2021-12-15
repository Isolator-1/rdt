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
std::condition_variable canSend; // 滑动窗口非满且接收方缓存有空余
std::mutex winMutex;             // 滑动窗口锁
Timer timer(TIMEOUT);            // 定时器，单位s，在winMutex的保护下，线程安全
std::atomic_bool isConnect;      // 已连接

CircleQueue<rdt_t *> recvBuf(SENDER_RECV_BUF); // 接收缓冲
std::condition_variable notFull;
std::condition_variable notEmpty;
std::mutex bufLock;

// 都是在获得到winMutex的前提下修改一下值，因此可以不必使用原子量
// std::atomic_ushort recvRwnd; // 接收方的rwnd
uint16_t recvRwnd;

#define SLOW_START 0     // 慢启动
#define WRED 1           // 拥塞避免
#define QUICK_RECOVERY 2 // 快速恢复

// 三个线程都是在获得到winMutex的前提下修改一下值，因此可以不必使用原子量
// std::atomic_ushort cwnd;       // 用于拥塞控制的窗口
// std::atomic_ushort ssthresh;   // 慢启动阈值
// std::atomic_uchar dupAckCount; // 冗余ACK个数
// std::atomic_uchar state;       // 当前状态
uint16_t cwnd = 1;                 // 用于拥塞控制的窗口
uint16_t ssthresh = INIT_SSTHRESH; // 慢启动阈值
uint8_t dupAckCount = DUP_ACK;     // 冗余ACK个数
uint8_t state = SLOW_START;        // 当前状态

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
        result = recvfrom(senderSocket, tmp, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, &len);
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
 * @brief 解析线程任务
 *
 */
void parse_task()
{
    printf("parse_task enter!\n");
    SOCKADDR_IN sock;
    int len = sizeof(sock);
    int result;
    rdt_t *recvPkt = nullptr;
    while (true)
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
        if (!not_corrupt(recvPkt))
        {
            printf("corrupt package %u\n");
            continue;
        }
#ifdef FLOW_CONTROL
        {
            std::unique_lock<std::mutex> ul(winMutex);
            // recvRwnd.store(recvPkt->rwnd);
            recvRwnd = recvPkt->rwnd;
            LOG(printf("recvRwnd: %u\n", recvPkt->rwnd));
            if (recvPkt->rwnd > 0)
            {
                timer.conti();
                canSend.notify_one();
            }
        }

#endif
        if (isAck(recvPkt))
        {
            if (isFin(recvPkt))
            {
                // 先不在这做拥塞控制
                LOG(printf("FIN ACK %u\n", recvPkt->seqnum))
                sendEnd = system_clock::now();
                break;
            }
            else if (isSyn(recvPkt))
            {
                // 先不在这做拥塞控制
                LOG(printf("SYN ACK %u\n", recvPkt->seqnum))
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
                auto dist = (recvPkt->seqnum - lastBase + NUM_SEQNUM) % NUM_SEQNUM;
                if (dist > 0 && dist <= N)
                {
                    dupAckCount = 0;
#ifdef CONGESTION_CONTROL
                    // new ACK
                    LOG(printf("new ACK, state: %u, cwnd: %u, ssthresh: %u, dupACKCount: %u\n", state, cwnd, ssthresh, dupAckCount))
                    static uint16_t dcwnd; // 拥塞避免阶段，控制cwnd增加速度的状态量
                    switch (state)
                    {
                    case SLOW_START:
                        cwnd++;
                        if (cwnd >= ssthresh)
                        {
                            state = WRED;
                        }
                        dcwnd = 0;
                        break;
                    case WRED:
                        dcwnd++;
                        if (dcwnd == cwnd)
                        {
                            cwnd++;
                            dcwnd = 0;
                        }
                        break;
                    case QUICK_RECOVERY:
                        cwnd = ssthresh;
                        state = WRED;
                        break;
                    }
#endif
                    base = recvPkt->seqnum;
                    LOG(printf("ACK [%u, %u)\n", lastBase, recvPkt->seqnum))
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
                    if (dupAckCount < DUP_ACK)
                    {
                        dupAckCount++;
                    }
#ifdef FAST_RETRANSMIT
                    if (dupAckCount == DUP_ACK)
                    {
                        LOG(printf("FAST RETRANSMIT\n"))
                        timer.stop_earlier();
                    }
#endif
#ifdef CONGESTION_CONTROL
                    // DUP ACK
                    LOG(printf("dup ACK, state: %u, cwnd: %u, ssthresh: %u, dupACKCount: %u\n", state, cwnd, ssthresh, dupAckCount))
                    switch (state)
                    {
                    case SLOW_START:
                    case WRED:
                        if (dupAckCount == DUP_ACK)
                        {
                            ssthresh = std::max(cwnd / 2, 1);
                            cwnd = ssthresh + DUP_ACK;
                            state = QUICK_RECOVERY;
                        }
                        break;
                    case QUICK_RECOVERY:
                        cwnd++;
                        break;
                    }
#endif
                    LOG(printf("DUP ACK %u\n", recvPkt->seqnum))
                }
                LOG(
                    assert(b);
                    auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                    if (size != sendWin.size()) {
                        printf("recv_task: size: %u, sendWin.size(): %u\n", size, sendWin.size());
                    })
                canSend.notify_one();
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
    delete recvPkt;
    recvPkt = nullptr;
    printf("parse_task quit!\n");
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
            // 在超时范围内，队列没有被清空
            std::lock_guard<std::mutex> lg(winMutex);
            dupAckCount = 0;
#ifdef CONGESTION_CONTROL
            // timeout
            LOG(printf("dup ACK, state: %u, cwnd: %u, ssthresh: %u, dupACKCount: %u\n", state, cwnd, ssthresh, dupAckCount))
            switch (state)
            {
            case SLOW_START:
                ssthresh = std::max(cwnd / 2, 1);
                cwnd = 1;
                break;
            case WRED:
            case QUICK_RECOVERY:
                ssthresh = std::max(cwnd / 2, 1);
                cwnd = 1;
                state = SLOW_START;
                break;
            }

#endif
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

void rdt_send(char *data, uint16_t dataLen, uint16_t flag = 0)
{
    while (true)
    {
        std::unique_lock<std::mutex> ul(winMutex);
#ifdef FLOW_CONTROL
#ifdef CONGESTION_CONTROL
        if (!sendWin.full() && recvRwnd >= sendWin.size() && cwnd >= sendWin.size())
#else
        if (!sendWin.full() && recvRwnd >= sendWin.size())
#endif // CONGESTION_CONTROL
#else
#ifdef CONGESTION_CONTROL
        if (!sendWin.full() && cwnd >= sendWin.size())
#else
        if (!sendWin.full())
#endif // CONGESTION_CONTROL
#endif // FLOW_CONTROL
        {
            LOG(
                auto size = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
                if (size != sendWin.size()) {
                    printf("rdt_send: size: %u, sendWin.size(): %u\n", size, sendWin.size());
                })
            rdt_t *sendPkt = new rdt_t();
            rdt_t *abandoned = nullptr;
            make_pkt(sendPkt, flag, nextseqnum, (uint8_t *)data, dataLen);
            auto index = (nextseqnum - base + NUM_SEQNUM) % NUM_SEQNUM;
            auto b = false;
            if (index == sendWin.size())
            {
                b = sendWin.enqueue(sendPkt);
                LOG(assert(b))
            }
            else if (index < sendWin.size())
            {
                abandoned = sendPkt;
            }
            sendto(senderSocket, (char *)sendPkt, sizeof(rdt_t), 0, (SOCKADDR *)&recverAddr, sizeof(SOCKADDR));
            delete abandoned;
            abandoned = nullptr;
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
#ifdef FLOW_CONTROL
#ifdef CONGESTION_CONTROL
            LOG(printf("rdt_send blocked! sendWin(%u): %u, recvRwnd: %u, cwnd: %u\n", N, sendWin.size(), recvRwnd, cwnd))
#else
            LOG(printf("rdt_send blocked! sendWin(%u): %u, recvRwnd: %u\n", N, sendWin.size(), recvRwnd))
            // if (recvRwnd < 1)
            // {
            // 理论上，还要暂停重发线程，并且设置新的定时器，超时后，向对方发送一个探索报文，获得对方更新的窗口信息。
            // 但在这里我不停止超时重发的报文段了，因为它既是对方不给我发送的根源，又能作为定时探索报文，何必再新建一个定时器呢？
            // timer.stop();
            // }
#endif // CONGESTION_CONTROL
#else
#ifdef CONGESTION_CONTROL
            LOG(printf("rdt_send blocked! sendWin(%u): %u, cwnd: %u\n", N, sendWin.size(), cwnd))
#else
            LOG(printf("rdt_send blocked! sendWin(%u): %u\n", N, sendWin.size()));
#endif                        // CONGESTION_CONTROL
#endif                        // FLOW_CONTROL
            canSend.wait(ul); // 阻塞，并释放锁
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

    // 启动计时线程，接收线程，解析线程
    isConnect.store(false);
    std::thread timerThread(resend_task);
    std::thread recvThread(recv_task);
    std::thread parseThread(parse_task);

    timerThread.detach();
    recvThread.detach();

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

    // 断开连接
    printf("waiting to disconnect...\n");
    rdt_send(0, 0, FIN_FLAG);
    parseThread.join();

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