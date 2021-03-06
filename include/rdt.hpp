#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

/**
 * @file rdt.hpp
 * @author zqs
 * @brief 协议数据结构及相关方法
 * @version 0.1
 * @date 2021-12-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <stdint.h>

#define DATA_SIZE 10228
#define ACK_FLAG 0x1
#define FIN_FLAG 0x2
#define SYN_FLAG 0x4

#pragma pack(1)

struct rdt_t
{
    uint16_t sum = 0;           // 校验和
    uint16_t flag = 0;          // 标志位
    uint32_t seqnum = 0;        // 序列号
    uint16_t rwnd = 0;          // 缓冲区剩余，单位：个数据包
    uint16_t dataLen = 0;       // 数据长度
    uint8_t data[DATA_SIZE] = {0}; // 数据
};

#pragma pack()

/**
 * 校验和回卷
 **/
void rollback(uint32_t& sum);

/**
 * 打包数据包
 **/
void make_pkt(rdt_t* pktBuf, uint16_t flag, uint32_t seqnum, uint8_t* data, uint16_t dataLen, uint16_t rwnd=0);

/**
 * 使用校验和检查
 * @return 校验通过返回true
 **/
bool not_corrupt(rdt_t* pktBuf);

/**
 * 打印数据包
 **/
void print_pkt(rdt_t* pktBuf);


bool isAck(rdt_t* pktBuf);

bool isFin(rdt_t* pktBuf);

bool isSyn(rdt_t* pktBuf);

#endif