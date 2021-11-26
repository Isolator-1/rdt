#include "../include/rdt.hpp"
#include <stdio.h>
#include <fstream>
#include <assert.h>
int main(){
    char buf[1024];
    rdt_t rdt;
    printf("sizeof(rdt_t): %u\n", sizeof(rdt_t));
    print_pkt(&rdt);
    uint8_t data[4] = {0x0, 0x1, 0x2, 0x3};
    make_pkt(&rdt, 0, 0, data, sizeof(data));
    print_pkt(&rdt);
    // rdt.flag++;
    if(not_corrupt(&rdt)){
        printf("YES\n");
    }else{
        printf("NO\n");
    }
    print_pkt(&rdt);

    // rdt_t rdt;
    // rdt_t rdt2;
    // std::ifstream fin("input/bytes.bin", std::ios::binary);
    // fin.read((char*)&rdt, sizeof(rdt));
    // assert(fin.gcount() == sizeof(rdt));
    // make_pkt(&rdt2, rdt.flag, rdt.seqnum, rdt.data, rdt.dataLen);
    // if(not_corrupt(&rdt2)){
    //     printf("YES\n");
    // }else{
    //     printf("NO\n");
    // }
    // print_pkt(&rdt2);

    // printf("checksum: %04x\n", rdt2.sum);

    // fin.close();
    return 0;
}