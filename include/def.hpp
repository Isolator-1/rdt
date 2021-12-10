/**
 * @file def.hpp
 * @author zqs
 * @brief 接收方和发送方、不同接收方、不同发送方共同定义值
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef DEF_HPP
#define DEF_HPP

#define SENDER_PORT 9997
#define RECVER_PORT 9999
#define DEFAULT_ADDR "127.0.0.1"
#define N 16          // 滑动窗口大小
#define NUM_SEQNUM 32 // 序列号数目 至少为2N

#define TIMEOUT 0.1 // 发送方超时时间, 单位s
#define INTERVAL 50 // 检查定时器时间间隔，单位ms

#define FLOW_CONTROL // 启用流量控制（仅发送方实现）

#define SENDER_RECV_BUF 16  // 发送方接收缓冲区大小，单位：包
#define RECVER_RECV_BUF 16  // 接收方接收缓冲区大小，单位：包

#define LOG // 是否打印日志，进行assert检查

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