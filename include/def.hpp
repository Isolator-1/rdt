/**
 * @file def.hpp
 * @author zqs
 * @brief autogeneration
 * @version 0.1
 * @date Thu Dec 16 01:36:06 2021
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef DEF_HPP
#define DEF_HPP
#define SENDER_PORT 9997
#define RECVER_PORT 9999
#define DEFAULT_ADDR "127.0.0.1"
#define N 32
#define NUM_SEQNUM 16
#define TIMEOUT 200
#define INTERVAL 50
#define FLOW_CONTROL
#define DELAY
#define ACK_DELAY 500
#define SENDER_RECV_BUF 16
#define RECVER_RECV_BUF 16
#define CONGESTION_CONTROL
#define INIT_SSTHRESH 4
#define DUP_ACK 3
#define FAST_RETRANSMIT

#ifdef LOG
#undef LOG
#define LOG(s) \
    do         \
    {          \
        s;     \
    } while (0);
#else
#define LOG(s) \
    do         \
    {          \
    } while (0);
#endif
#endif
