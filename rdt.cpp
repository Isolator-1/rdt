#include "./include/rdt.hpp"
#include "assert.h"
#include "string.h"
#include "stdio.h"

void rollback(uint32_t& sum){
    if(sum & 0xFFFF0000){
        sum &= 0xFFFF;
        sum ++;
    }
}

void make_pkt(rdt_t* pktBuf, uint8_t flag, uint32_t seqnum, uint8_t* data, uint16_t dataLen){
    assert(dataLen <= DATA_SIZE);
    pktBuf->sum = 0;
    pktBuf->flag = flag;
    pktBuf->seqnum = seqnum;
    pktBuf->dataLen = dataLen;
    memcpy_s(pktBuf->data, DATA_SIZE, data, dataLen);
    uint16_t validLen = sizeof(rdt_t) - DATA_SIZE + dataLen;
    uint16_t i = 0;
    uint16_t* p = (uint16_t*)pktBuf;
    uint32_t sum = 0;
    for(; i < validLen; i += 2){
        sum += *p; 
        rollback(sum);
        p++;
    }
    if (i > validLen) {
        p--;
        // printf("ckpt1\n");
        sum += *(uint8_t*)p;
    }
    pktBuf->sum = ~(sum & 0xFFFF);
}

// 检查校验和
bool not_corrupt(rdt_t* pktBuf){
    uint32_t sum = 0;
    uint16_t i = 0;
    uint16_t* p = (uint16_t*)pktBuf;
    uint16_t validLen = sizeof(rdt_t) - DATA_SIZE + pktBuf->dataLen;
    for(; i < validLen; i += 2){
        // printf("adder : %04x\n", *p);
        sum += *p; 
        rollback(sum);
        p++;
    }
    if (i > validLen) {
        p--;
        // printf("adder : %02x\n", *(uint8_t*)p);
        sum += *(uint8_t*)p;
    }
    return sum == 0xFFFF;
}

void print_pkt(rdt_t* pktBuf){
    // sprintf_s(cstr, len, "checksum: %04x\tflag: %02x\tseqnum:%08x\tdataLen:%04x\n",
    // pktBuf->sum, pktBuf->flag, pktBuf->seqnum, pktBuf->dataLen);
    uint8_t* p = (uint8_t*) pktBuf;
    for(uint32_t i = 0; i < sizeof(rdt_t) - DATA_SIZE + pktBuf->dataLen; i++){
        if (i % 16 == 0){
            printf("\n");
        }
        printf("%02x ", *(p+i));

    }
    printf("\n");
}

bool isAck(rdt_t* pktBuf){
    return pktBuf->flag & ACK_FLAG;
}

bool isFin(rdt_t* pktBuf){
    return pktBuf->flag & FIN_FLAG;
}